// libdrm-pdpvr: the libdrm calls mesa's powervr winsys makes, on the PDPowerVR user client. each render node descriptor gets its own connection,
// powervr ioctls pass through and generic drm ones are emulated

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "pd_pvr_drm.h"
#include "pd_pvr_internal.h"
#include "pd_pvr_uapi.h"

#define MAX_FILES		16
#define MAX_COMPATIBLE		8
#define SUBMIT_POLL_US		100

struct pvr_file {
	int fd;
	dev_t dev;
	ino_t ino;
	io_connect_t conn;
	int kind;
	// display files: the write end of the pipe their events are read from
	int evfd;
};

struct pvr_map {
	void *addr;
	size_t len;
	io_connect_t conn;
	uint32_t type;
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct pvr_file files[MAX_FILES];
static struct pvr_map *maps;
static size_t nmaps, capmaps;

static int
fail(int err)
{
	errno = err;
	return -1;
}

static int
kr_errno(kern_return_t kr)
{
	int err = PD_PVR_ERRNO(kr);

	if (err > 0)
		return err;

	switch (kr) {
	case kIOReturnBadArgument:
		return EINVAL;
	case kIOReturnNoMemory:
		return ENOMEM;
	case kIOReturnNotPermitted:
	case kIOReturnNotPrivileged:
		return EPERM;
	case kIOReturnUnsupported:
		return ENOTTY;
	case kIOReturnNotOpen:
		return EBADF;
	default:
		return EIO;
	}
}

static uint64_t
mono_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// PD_DRM_DEBUG=1 logs device discovery to stderr
static bool
debug_enabled(void)
{
	const char *v = getenv("PD_DRM_DEBUG");

	return v != NULL && *v != '\0' && *v != '0';
}

static io_service_t
gpu_service(void)
{
	return IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching(PD_PVR_SERVICE_CLASS));
}

static int
node_kind(const struct stat *st)
{
	struct stat node;

	if (!S_ISCHR(st->st_mode))
		return PD_FILE_NONE;
	if (stat(PD_PVR_DRM_NODE, &node) == 0 && st->st_rdev == node.st_rdev)
		return PD_FILE_RENDER;
	if (stat(PD_PVR_DRM_DISPLAY_NODE, &node) == 0 && st->st_rdev == node.st_rdev)
		return PD_FILE_DISPLAY;
	return PD_FILE_NONE;
}

// drop the mappings of a connection, called with the lock held
static void
forget_maps(io_connect_t conn)
{
	size_t i = 0;

	while (i < nmaps) {
		if (maps[i].conn == conn)
			maps[i] = maps[--nmaps];
		else
			i++;
	}
}

static void
release_file(struct pvr_file *f)
{
	forget_maps(f->conn);
	pd_kms_forget(f->fd);
	IOServiceClose(f->conn);
	if (f->evfd > 0)
		close(f->evfd);
	memset(f, 0, sizeof(*f));
	f->fd = -1;
}

// the user mappings come out cacheable and the gpu is not coherent: clean and invalidate them all
// around submits and waits until the kext maps them write-combined. PD_DRM_CACHE_FLUSH=0 disables it
static void
flush_maps(void)
{
	const char *v = getenv("PD_DRM_CACHE_FLUSH");

	if (v == NULL || *v != '1')
		return;

	pthread_mutex_lock(&lock);
	__asm__ volatile ("dsb sy" ::: "memory");
	for (size_t i = 0; i < nmaps; i++) {
		uintptr_t p = (uintptr_t)maps[i].addr & ~(uintptr_t)63, end = (uintptr_t)maps[i].addr + maps[i].len;

		for (; p < end; p += 64) {
			__asm__ volatile ("dc civac, %0" :: "r" (p) : "memory");
		}
	}
	__asm__ volatile ("dsb sy" ::: "memory");
	pthread_mutex_unlock(&lock);
}

// the descriptor is swapped for a pipe so a closed and reused number shows up as a new file, a
// display file keeps the write end for its page flip events
static io_connect_t
bind_file(int fd, struct pvr_file *slot, int kind)
{
	io_service_t svc = gpu_service();
	io_connect_t conn = IO_OBJECT_NULL;
	int p[2], fdflags = fcntl(fd, F_GETFD);
	struct stat st;
	kern_return_t kr;

	if (svc == IO_OBJECT_NULL)
		return IO_OBJECT_NULL;

	kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
	IOObjectRelease(svc);
	if (debug_enabled())
		fprintf(stderr, "libdrm-pdpvr: fd %d opens the user client: kr 0x%x\n", fd, kr);
	if (kr != KERN_SUCCESS)
		return IO_OBJECT_NULL;

	if (pipe(p) != 0 || dup2(p[0], fd) < 0) {
		IOServiceClose(conn);
		return IO_OBJECT_NULL;
	}
	close(p[0]);
	if (kind == PD_FILE_DISPLAY) {
		fcntl(p[1], F_SETFD, FD_CLOEXEC);
		fcntl(p[1], F_SETFL, O_NONBLOCK);
		slot->evfd = p[1];
	} else {
		close(p[1]);
		slot->evfd = -1;
	}
	if (fdflags >= 0)
		fcntl(fd, F_SETFD, fdflags);
	fstat(fd, &st);
	slot->fd = fd;
	slot->dev = st.st_dev;
	slot->ino = st.st_ino;
	slot->conn = conn;
	slot->kind = kind;
	return conn;
}

// the user client connection behind fd, binding a freshly opened render or display node when allowed
io_connect_t
pd_file_lookup(int fd, bool bind, int *kind, int *evfd)
{
	struct pvr_file *free_slot = NULL;
	io_connect_t conn = IO_OBJECT_NULL;
	struct stat st;
	bool have_st;
	int k;

	if (kind != NULL)
		*kind = PD_FILE_NONE;
	if (evfd != NULL)
		*evfd = -1;
	if (fd < 0)
		return IO_OBJECT_NULL;

	have_st = fstat(fd, &st) == 0;
	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_FILES; i++) {
		struct pvr_file *f = &files[i];

		if (f->conn == IO_OBJECT_NULL) {
			if (free_slot == NULL)
				free_slot = f;
			continue;
		}
		if (f->fd != fd)
			continue;

		if (have_st && st.st_dev == f->dev && st.st_ino == f->ino) {
			conn = f->conn;
			if (kind != NULL)
				*kind = f->kind;
			if (evfd != NULL)
				*evfd = f->evfd;
			break;
		}
		release_file(f);
		if (free_slot == NULL)
			free_slot = f;
	}
	if (conn == IO_OBJECT_NULL && bind && have_st && free_slot != NULL &&
	    (k = node_kind(&st)) != PD_FILE_NONE) {
		conn = bind_file(fd, free_slot, k);
		if (conn != IO_OBJECT_NULL && kind != NULL)
			*kind = k;
		if (conn != IO_OBJECT_NULL && evfd != NULL)
			*evfd = free_slot->evfd;
	}
	pthread_mutex_unlock(&lock);
	return conn;
}

static io_connect_t
file_conn(int fd, bool bind)
{
	return pd_file_lookup(fd, bind, NULL, NULL);
}

int
pd_fail(int err)
{
	return fail(err);
}

int
pd_kr_errno(kern_return_t kr)
{
	return kr_errno(kr);
}

bool
pd_debug(void)
{
	return debug_enabled();
}

// unmap every buffer before closing the user clients, so no page is freed while still mapped
__attribute__((destructor)) static void
close_all(void)
{
	pthread_mutex_lock(&lock);
	for (size_t i = 0; i < nmaps; i++) {
		IOConnectUnmapMemory64(maps[i].conn, maps[i].type, mach_task_self(),
		    (mach_vm_address_t)(uintptr_t)maps[i].addr);
	}
	nmaps = 0;
	for (int i = 0; i < MAX_FILES; i++) {
		if (files[i].conn != IO_OBJECT_NULL)
			release_file(&files[i]);
	}
	pthread_mutex_unlock(&lock);
}

int
pd_call(io_connect_t conn, uint32_t sel, const uint64_t *in, uint32_t nin, uint64_t *out, uint32_t nout)
{
	uint32_t outcnt = nout;
	kern_return_t kr = IOConnectCallScalarMethod(conn, sel, in, nin, out, out ? &outcnt : NULL);

	if (kr != KERN_SUCCESS && debug_enabled())
		fprintf(stderr, "libdrm-pdpvr: selector 0x%x failed, kr 0x%x\n", sel, kr);
	return kr == KERN_SUCCESS ? 0 : fail(kr_errno(kr));
}

static int
call_scalar(io_connect_t conn, uint32_t sel, uint64_t in, uint64_t *out)
{
	uint32_t outcnt = out ? 1 : 0;
	kern_return_t kr = IOConnectCallScalarMethod(conn, sel, &in, 1, out, out ? &outcnt : NULL);

	if (kr != KERN_SUCCESS && debug_enabled())
		fprintf(stderr, "libdrm-pdpvr: selector 0x%x (%llu) failed, kr 0x%x\n", sel, (unsigned long long)in, kr);
	return kr == KERN_SUCCESS ? 0 : fail(kr_errno(kr));
}

// one kernel wait, returns 0, or an errno with ETIMEDOUT for a pending fence
static int
kernel_wait(io_connect_t conn, const uint32_t *handles, uint32_t count, bool all, uint64_t timeout_us,
    uint32_t *first)
{
	struct pd_pvr_syncobj_wait w = { 0 };
	size_t sz = sizeof(w);
	kern_return_t kr;

	w.handles = (uint64_t)(uintptr_t)handles;
	w.count = count;
	w.flags = all ? PD_PVR_SYNCOBJ_WAIT_ALL : 0;
	w.timeout_us = timeout_us;
	kr = IOConnectCallStructMethod(conn, PD_PVR_SEL_SYNCOBJ_WAIT, &w, sizeof(w), &w, &sz);
	if (kr != KERN_SUCCESS)
		return kr_errno(kr);

	if (first)
		*first = w.first_signaled;
	return 0;
}

// drm takes an absolute CLOCK_MONOTONIC deadline and can wait for fences not yet submitted
static int
syncobj_wait(io_connect_t conn, struct drm_syncobj_wait *a)
{
	const uint32_t *handles = (const uint32_t *)(uintptr_t)a->handles;
	bool all = a->flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL;
	bool for_submit = a->flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;
	int err;

	if (a->count_handles == 0)
		return fail(EINVAL);

	for (;;) {
		uint64_t now = mono_ns();
		uint64_t left = a->timeout_nsec > 0 && (uint64_t)a->timeout_nsec > now ?
		    ((uint64_t)a->timeout_nsec - now) / 1000 : 0;

		err = kernel_wait(conn, handles, a->count_handles, all, left, &a->first_signaled);
		if (err == 0) {
			flush_maps();
			return 0;
		}

		if (err == ETIMEDOUT)
			return fail(ETIME);

		if (err != EINVAL || !for_submit || left == 0)
			return fail(err);

		usleep(SUBMIT_POLL_US);
	}
}

static int
syncobj_array(io_connect_t conn, uint32_t sel, struct drm_syncobj_array *a)
{
	const uint32_t *handles = (const uint32_t *)(uintptr_t)a->handles;

	if (a->pad)
		return fail(EINVAL);

	for (uint32_t i = 0; i < a->count_handles; i++) {
		if (call_scalar(conn, sel, handles[i], NULL) != 0)
			return -1;
	}
	return 0;
}

// the kernel retains and combines fences, a transfer must not wait for GPU completion.
static int
syncobj_transfer(io_connect_t conn, struct drm_syncobj_transfer *a)
{
	uint64_t in[3] = { a->src_handle, a->dst_handle, a->dst_point != 0 };
	if (a->flags)
		return fail(EINVAL);
	return pd_call(conn, PD_PVR_SEL_SYNCOBJ_TRANSFER, in, 3, NULL, 0);
}

static char *
dup_string(const char *s, int *len)
{
	*len = (int)strlen(s);
	return strdup(s);
}

static int
get_version(struct drm_version *v)
{
	static const char *name = "powervr", *date = "20230904", *desc = "PowerVR (PDPowerVR)";
	const char *src[3] = { name, date, desc };
	char *dst[3] = { v->name, v->date, v->desc };
	__kernel_size_t *len[3] = { &v->name_len, &v->date_len, &v->desc_len };

	v->version_major = 1;
	v->version_minor = 0;
	v->version_patchlevel = 0;
	for (int i = 0; i < 3; i++) {
		size_t n = strlen(src[i]);

		if (dst[i] != NULL && *len[i] > 0)
			memcpy(dst[i], src[i], n < *len[i] ? n : *len[i]);
		*len[i] = n;
	}
	return 0;
}

static int
get_cap(struct drm_get_cap *c, int kind)
{
	switch (c->capability) {
	case DRM_CAP_SYNCOBJ:
		c->value = 1;
		return 0;
	case DRM_CAP_PRIME:
		c->value = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT;
		return 0;
	case DRM_CAP_DUMB_BUFFER:
	case DRM_CAP_TIMESTAMP_MONOTONIC:
	case DRM_CAP_CRTC_IN_VBLANK_EVENT:
		c->value = kind == PD_FILE_DISPLAY;
		return 0;
	case DRM_CAP_SYNCOBJ_TIMELINE:
	case DRM_CAP_ADDFB2_MODIFIERS:
	case DRM_CAP_ASYNC_PAGE_FLIP:
		c->value = 0;
		return 0;
	default:
		return fail(EINVAL);
	}
}

// the leading fields of the powervr vm map, vm unmap and submit arguments, for debug logging
struct log_vm_args {
	uint32_t vm_context_handle, flags;
	uint64_t device_addr;
	uint32_t handle, pad;
	uint64_t offset, size;
};

struct log_obj_array {
	uint32_t stride, count;
	uint64_t array;
};

static void
log_ioctl(uint32_t nr, const void *arg)
{
	const struct log_vm_args *vm = arg;

	if (nr == 0x45) {
		fprintf(stderr, "libdrm-pdpvr: vm map 0x%llx size 0x%llx bo %u offset 0x%llx\n",
		    (unsigned long long)vm->device_addr, (unsigned long long)vm->size, vm->handle,
		    (unsigned long long)vm->offset);
	} else if (nr == 0x46) {
		fprintf(stderr, "libdrm-pdpvr: vm unmap 0x%llx size 0x%llx\n", (unsigned long long)vm->device_addr,
		    (unsigned long long)((const uint64_t *)arg)[2]);
	} else if (nr == 0x4d) {
		const struct log_obj_array *jobs = arg;

		for (uint32_t i = 0; i < jobs->count; i++) {
			const uint32_t *job = (const uint32_t *)(uintptr_t)(jobs->array + (uint64_t)i * jobs->stride);

			fprintf(stderr, "libdrm-pdpvr: submit job %u type %u context %u flags 0x%x\n", i, job[0], job[1],
			    job[2]);
		}
	}
}

// buffers made while set are mapped cacheable (the kext cleans them around submits and waits): for memory the
// CPU reads back, which a write-combined map makes slow. The caller says when, the bit is PVR_BO_PD_CPU_CACHED
#define PD_BO_CPU_CACHED (1ull << 61)
static int cpu_cached_hint;

void
pd_drm_set_cpu_cached(int on)
{
	cpu_cached_hint = on;
}

static int
pvr_ioctl(io_connect_t conn, int kind, unsigned long request, void *arg)
{
	uint32_t nr = request & 0xff;

	if (((request >> 8) & 0xff) != DRM_IOCTL_BASE)
		return fail(ENOTTY);

	if (nr >= PD_PVR_SEL_IOCTL_FIRST && nr <= PD_PVR_SEL_IOCTL_LAST) {
		size_t len = IOCPARM_LEN(request), outlen = len;
		kern_return_t kr;

		if (debug_enabled())
			log_ioctl(nr, arg);
		if (nr == 0x4d)
			flush_maps();
		// create_bo: flags at byte 16
		if (nr == 0x41 && cpu_cached_hint && len >= 24)
			((uint64_t *)arg)[2] |= PD_BO_CPU_CACHED;
		kr = IOConnectCallStructMethod(conn, nr, arg, len, arg, &outlen);
		if (nr == 0x41 && len >= 24)
			((uint64_t *)arg)[2] &= ~PD_BO_CPU_CACHED;

		if (kr != KERN_SUCCESS && debug_enabled())
			fprintf(stderr, "libdrm-pdpvr: powervr ioctl 0x%x failed, kr 0x%x errno %d\n", nr, kr, kr_errno(kr));
		return kr == KERN_SUCCESS ? 0 : fail(kr_errno(kr));
	}

	switch (request) {
	case DRM_IOCTL_VERSION:
		return get_version(arg);
	case DRM_IOCTL_GET_CAP:
		return get_cap(arg, kind);
	case DRM_IOCTL_SET_CLIENT_CAP:
		return 0;
	case DRM_IOCTL_MODE_CREATE_DUMB: {
		struct drm_mode_create_dumb *a = arg;
		uint64_t in = (uint64_t)a->width * ((a->bpp + 7) / 8) * a->height, out[2];

		if (kind != PD_FILE_DISPLAY || in == 0)
			return fail(EINVAL);
		if (pd_call(conn, PD_PVR_SEL_DUMB_CREATE, &in, 1, out, 2) != 0)
			return -1;
		a->handle = (uint32_t)out[0];
		a->pitch = a->width * ((a->bpp + 7) / 8);
		a->size = (in + 0x3fff) & ~0x3fffULL;
		if (debug_enabled())
			fprintf(stderr, "libdrm-pdpvr: dumb buffer %u, %llu bytes, %scontiguous\n", a->handle,
			    (unsigned long long)a->size, out[1] ? "" : "NOT ");
		return 0;
	}
	case DRM_IOCTL_MODE_DESTROY_DUMB:
		return call_scalar(conn, PD_PVR_SEL_GEM_CLOSE, ((struct drm_mode_destroy_dumb *)arg)->handle, NULL);
	case DRM_IOCTL_MODE_MAP_DUMB: {
		struct drm_mode_map_dumb *a = arg;

		// as for powervr buffers, the mmap offset is the handle
		a->offset = a->handle;
		return 0;
	}
	case DRM_IOCTL_GEM_CLOSE:
		return call_scalar(conn, PD_PVR_SEL_GEM_CLOSE, ((struct drm_gem_close *)arg)->handle, NULL);
	case DRM_IOCTL_SYNCOBJ_CREATE: {
		struct drm_syncobj_create *a = arg;
		uint64_t handle = 0;

		if (a->flags & ~DRM_SYNCOBJ_CREATE_SIGNALED)
			return fail(EINVAL);

		if (call_scalar(conn, PD_PVR_SEL_SYNCOBJ_CREATE,
		    (a->flags & DRM_SYNCOBJ_CREATE_SIGNALED) ? PD_PVR_SYNCOBJ_CREATE_SIGNALED : 0, &handle) != 0)
			return -1;

		a->handle = (uint32_t)handle;
		return 0;
	}
	case DRM_IOCTL_SYNCOBJ_DESTROY:
		return call_scalar(conn, PD_PVR_SEL_SYNCOBJ_DESTROY, ((struct drm_syncobj_destroy *)arg)->handle,
		    NULL);
	case DRM_IOCTL_SYNCOBJ_WAIT:
		return syncobj_wait(conn, arg);
	case DRM_IOCTL_SYNCOBJ_RESET:
		return syncobj_array(conn, PD_PVR_SEL_SYNCOBJ_RESET, arg);
	case DRM_IOCTL_SYNCOBJ_SIGNAL:
		return syncobj_array(conn, PD_PVR_SEL_SYNCOBJ_SIGNAL, arg);
	case DRM_IOCTL_SYNCOBJ_TRANSFER:
		return syncobj_transfer(conn, arg);
	case DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT:
	case DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL:
	case DRM_IOCTL_SYNCOBJ_QUERY:
	case DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD:
	case DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE:
	case DRM_IOCTL_SYNCOBJ_EVENTFD:
		return fail(EOPNOTSUPP);
	case DRM_IOCTL_PRIME_HANDLE_TO_FD: {
		struct drm_prime_handle *a = arg;

		return pd_prime_export(conn, a->handle, a->flags, &a->fd);
	}
	case DRM_IOCTL_PRIME_FD_TO_HANDLE: {
		struct drm_prime_handle *a = arg;

		return pd_prime_import(conn, a->fd, &a->handle);
	}
	default:
		return fail(ENOTTY);
	}
}

int
drmIoctl(int fd, unsigned long request, void *arg)
{
	int kind;
	io_connect_t conn = pd_file_lookup(fd, true, &kind, NULL);
	int ret;

	if (conn != IO_OBJECT_NULL) {
		uint64_t t0 = debug_enabled() ? mono_ns() : 0;

		ret = pvr_ioctl(conn, kind, request, arg);
		if (ret != 0 && debug_enabled())
			fprintf(stderr, "libdrm-pdpvr: drm ioctl 0x%lx failed, errno %d\n", request & 0xff, errno);
		// a timeline: when each call started (ms) and how long it took
		if (debug_enabled())
			fprintf(stderr, "libdrm-pdpvr: t %llu.%03llu ioctl 0x%lx %llu us\n", (unsigned long long)(t0 / 1000000),
			    (unsigned long long)(t0 / 1000 % 1000), request & 0xff, (unsigned long long)((mono_ns() - t0) / 1000));
		return ret;
	}

	do {
		ret = ioctl(fd, request, arg);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
	return ret;
}

void
drmFree(void *p)
{
	free(p);
}

drmVersionPtr
drmGetVersion(int fd)
{
	struct drm_version v = { 0 };
	drmVersionPtr r;

	if (file_conn(fd, true) == IO_OBJECT_NULL) {
		errno = ENODEV;
		return NULL;
	}

	get_version(&v);
	r = calloc(1, sizeof(*r));
	if (r == NULL)
		return NULL;

	r->version_major = v.version_major;
	r->version_minor = v.version_minor;
	r->version_patchlevel = v.version_patchlevel;
	r->name = dup_string("powervr", &r->name_len);
	r->date = dup_string("20230904", &r->date_len);
	r->desc = dup_string("PowerVR (PDPowerVR)", &r->desc_len);
	return r;
}

void
drmFreeVersion(drmVersionPtr v)
{
	if (v == NULL)
		return;

	free(v->name);
	free(v->date);
	free(v->desc);
	free(v);
}

int
drmGetCap(int fd, uint64_t capability, uint64_t *value)
{
	struct drm_get_cap c = { .capability = capability };
	int ret = drmIoctl(fd, DRM_IOCTL_GET_CAP, &c);

	if (ret == 0)
		*value = c.value;
	return ret;
}

int
drmIsKMS(int fd)
{
	int kind;

	return pd_file_lookup(fd, true, &kind, NULL) != IO_OBJECT_NULL && kind == PD_FILE_DISPLAY;
}

int
drmGetNodeTypeFromFd(int fd)
{
	int kind;

	if (pd_file_lookup(fd, true, &kind, NULL) == IO_OBJECT_NULL)
		return fail(ENODEV);
	return kind == PD_FILE_DISPLAY ? DRM_NODE_PRIMARY : DRM_NODE_RENDER;
}

char *
drmGetRenderDeviceNameFromFd(int fd)
{
	int kind;

	if (pd_file_lookup(fd, true, &kind, NULL) == IO_OBJECT_NULL || kind != PD_FILE_RENDER)
		return NULL;
	return strdup(PD_PVR_DRM_NODE);
}

char *
drmGetPrimaryDeviceNameFromFd(int fd)
{
	int kind;

	if (pd_file_lookup(fd, true, &kind, NULL) == IO_OBJECT_NULL)
		return NULL;
	return strdup(kind == PD_FILE_DISPLAY ? PD_PVR_DRM_DISPLAY_NODE : PD_PVR_DRM_NODE);
}

static int
add_strings(char **list, int n, const char *buf, size_t len)
{
	size_t off = 0;

	while (off < len && n < MAX_COMPATIBLE) {
		size_t sl = strnlen(buf + off, len - off);

		if (sl > 0)
			list[n++] = strndup(buf + off, sl);
		off += sl + 1;
	}
	return n;
}

static int
add_data_property(char **list, int n, io_registry_entry_t entry, const char *key)
{
	CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
	CFTypeRef v = IORegistryEntryCreateCFProperty(entry, k, kCFAllocatorDefault, 0);

	CFRelease(k);
	if (v == NULL)
		return n;

	if (CFGetTypeID(v) == CFDataGetTypeID()) {
		n = add_strings(list, n, (const char *)CFDataGetBytePtr(v), (size_t)CFDataGetLength(v));
	} else if (CFGetTypeID(v) == CFStringGetTypeID()) {
		char buf[128];

		if (CFStringGetCString(v, buf, sizeof(buf), kCFStringEncodingUTF8))
			n = add_strings(list, n, buf, strlen(buf));
	}
	CFRelease(v);
	return n;
}

static bool
names_a733(char **list, int n)
{
	bool hit = false;

	for (int i = 0; i < n; i++) {
		if (!hit && (strstr(list[i], "sun60iw2") != NULL || strstr(list[i], "sun60i-a733") != NULL)) {
			hit = true;
			if (debug_enabled())
				fprintf(stderr, "libdrm-pdpvr: soc from device tree node %s\n", list[i]);
		}
		free(list[i]);
	}
	return hit;
}

// the bsp device tree calls the a733 gpu just "img,gpu" and pd's root claims a virtual mac, so
// look for a device tree node of the soc and name the gpu the way mainline does
static int
add_soc_compatible(char **list, int n)
{
	io_registry_entry_t root = IORegistryEntryFromPath(MACH_PORT_NULL, "IODeviceTree:/");
	io_iterator_t it = IO_OBJECT_NULL;
	io_registry_entry_t e;
	bool a733 = false;

	if (root == IO_OBJECT_NULL)
		return n;

	if (IORegistryEntryCreateIterator(root, kIODeviceTreePlane, kIORegistryIterateRecursively, &it) == KERN_SUCCESS) {
		while (!a733 && (e = IOIteratorNext(it)) != IO_OBJECT_NULL) {
			char *c[MAX_COMPATIBLE] = { 0 };

			a733 = names_a733(c, add_data_property(c, 0, e, "compatible"));
			IOObjectRelease(e);
		}
		IOObjectRelease(it);
	}
	IOObjectRelease(root);
	if (a733 && n < MAX_COMPATIBLE)
		list[n++] = strdup("allwinner,sun60i-a733-gpu");
	return n;
}

static void
free_device(drmDevicePtr d)
{
	if (d == NULL)
		return;

	if (d->nodes != NULL) {
		for (int i = 0; i < DRM_NODE_MAX; i++) {
			free(d->nodes[i]);
		}
		free(d->nodes);
	}
	if (d->deviceinfo.platform != NULL) {
		for (char **c = d->deviceinfo.platform->compatible; c != NULL && *c != NULL; c++) {
			free(*c);
		}
		free(d->deviceinfo.platform->compatible);
		free(d->deviceinfo.platform);
	}
	free(d->businfo.platform);
	free(d);
}

// a platform device with a render node, carrying the compatible strings of the gpu's provider
static drmDevicePtr
make_device(io_service_t svc)
{
	io_registry_entry_t parent = IO_OBJECT_NULL;
	io_name_t name = "gpu", loc = "";
	char *compat[MAX_COMPATIBLE] = { 0 };
	drmDevicePtr d = calloc(1, sizeof(*d));
	int n = 0;

	if (d == NULL)
		return NULL;

	d->bustype = DRM_BUS_PLATFORM;
	d->available_nodes = 1 << DRM_NODE_RENDER;
	d->nodes = calloc(DRM_NODE_MAX, sizeof(char *));
	d->businfo.platform = calloc(1, sizeof(*d->businfo.platform));
	d->deviceinfo.platform = calloc(1, sizeof(*d->deviceinfo.platform));
	if (d->nodes == NULL || d->businfo.platform == NULL || d->deviceinfo.platform == NULL) {
		free_device(d);
		return NULL;
	}

	// mesa stats the primary node path of the render device as well
	d->nodes[DRM_NODE_PRIMARY] = strdup(PD_PVR_DRM_NODE);
	d->nodes[DRM_NODE_RENDER] = strdup(PD_PVR_DRM_NODE);

	n = add_soc_compatible(compat, n);
	if (IORegistryEntryGetParentEntry(svc, kIOServicePlane, &parent) == KERN_SUCCESS) {
		n = add_data_property(compat, n, parent, "compatible");
		IORegistryEntryGetName(parent, name);
		IORegistryEntryGetLocationInPlane(parent, kIOServicePlane, loc);
		IOObjectRelease(parent);
	}
	if (n == 0)
		compat[n++] = strdup("img,img-rogue");
	d->deviceinfo.platform->compatible = calloc(n + 1, sizeof(char *));
	if (d->deviceinfo.platform->compatible == NULL) {
		for (int i = 0; i < n; i++) {
			free(compat[i]);
		}
		free_device(d);
		return NULL;
	}

	memcpy(d->deviceinfo.platform->compatible, compat, n * sizeof(char *));
	if (debug_enabled()) {
		for (int i = 0; i < n; i++) {
			fprintf(stderr, "libdrm-pdpvr: gpu compatible %s\n", compat[i]);
		}
	}
	snprintf(d->businfo.platform->fullname, sizeof(d->businfo.platform->fullname), "%s%s%s", loc,
	    loc[0] ? "." : "", name);
	return d;
}

// the display engine as a kms-only platform device, its primary node binds display files
static drmDevicePtr
make_display_device(void)
{
	drmDevicePtr d = calloc(1, sizeof(*d));

	if (d == NULL)
		return NULL;
	d->bustype = DRM_BUS_PLATFORM;
	d->available_nodes = 1 << DRM_NODE_PRIMARY;
	d->nodes = calloc(DRM_NODE_MAX, sizeof(char *));
	d->businfo.platform = calloc(1, sizeof(*d->businfo.platform));
	d->deviceinfo.platform = calloc(1, sizeof(*d->deviceinfo.platform));
	if (d->nodes == NULL || d->businfo.platform == NULL || d->deviceinfo.platform == NULL ||
	    (d->deviceinfo.platform->compatible = calloc(2, sizeof(char *))) == NULL) {
		free_device(d);
		return NULL;
	}
	d->nodes[DRM_NODE_PRIMARY] = strdup(PD_PVR_DRM_DISPLAY_NODE);
	d->deviceinfo.platform->compatible[0] = strdup("allwinner,display-engine-v352");
	snprintf(d->businfo.platform->fullname, sizeof(d->businfo.platform->fullname), "display-engine");
	return d;
}

static bool
have_display(void)
{
	io_service_t svc;

	if (getenv("PD_DRM_NO_DISPLAY") != NULL)
		return false;
	svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching(PD_DISPLAY_SERVICE_CLASS));
	if (svc == IO_OBJECT_NULL)
		return false;
	IOObjectRelease(svc);
	return true;
}

void
drmFreeDevice(drmDevicePtr *device)
{
	if (device == NULL)
		return;

	free_device(*device);
	*device = NULL;
}

void
drmFreeDevices(drmDevicePtr devices[], int count)
{
	if (devices == NULL)
		return;

	for (int i = 0; i < count; i++) {
		drmFreeDevice(&devices[i]);
	}
}

// one gpu: every render node descriptor binds to the first PDPowerVR service, with a scanout
// driver there is a display device too, whose files go to the same service
int
drmGetDevices2(uint32_t flags, drmDevicePtr devices[], int max_devices)
{
	io_service_t svc = gpu_service();
	int n = 0, total;
	drmDevicePtr d;

	(void)flags;

	if (svc == IO_OBJECT_NULL) {
		if (debug_enabled())
			fprintf(stderr, "libdrm-pdpvr: no %s service\n", PD_PVR_SERVICE_CLASS);
		return 0;
	}
	total = have_display() ? 2 : 1;

	if (devices == NULL || max_devices < 1) {
		IOObjectRelease(svc);
		return total;
	}

	d = make_device(svc);
	IOObjectRelease(svc);
	if (d == NULL)
		return -ENOMEM;
	devices[n++] = d;
	if (total > 1 && max_devices > 1 && (d = make_display_device()) != NULL)
		devices[n++] = d;
	return n;
}

int
drmDevicesEqual(drmDevicePtr a, drmDevicePtr b)
{
	if (a == NULL || b == NULL || a->bustype != b->bustype)
		return 0;

	if (a->bustype != DRM_BUS_PLATFORM)
		return 0;

	return strcmp(a->businfo.platform->fullname, b->businfo.platform->fullname) == 0;
}

int
drmGetDevices(drmDevicePtr devices[], int max_devices)
{
	return drmGetDevices2(0, devices, max_devices);
}

int
drmGetDevice2(int fd, uint32_t flags, drmDevicePtr *device)
{
	drmDevicePtr d = NULL;
	int n, kind;

	if (device == NULL || pd_file_lookup(fd, true, &kind, NULL) == IO_OBJECT_NULL)
		return -EINVAL;
	if (kind == PD_FILE_DISPLAY) {
		*device = make_display_device();
		return *device != NULL ? 0 : -ENOMEM;
	}

	n = drmGetDevices2(flags, &d, 1);
	if (n <= 0)
		return n < 0 ? n : -ENODEV;

	*device = d;
	return 0;
}

int
drmGetDevice(int fd, drmDevicePtr *device)
{
	return drmGetDevice2(fd, 0, device);
}

int
drmAuthMagic(int fd, drm_magic_t magic)
{
	(void)magic;

	// every pd client is master: not EACCES, so mesa's display wsi takes the fd
	return file_conn(fd, true) != IO_OBJECT_NULL ? -EINVAL : -EBADF;
}

int
drmGetMagic(int fd, drm_magic_t *magic)
{
	*magic = 0;
	return file_conn(fd, true) != IO_OBJECT_NULL ? 0 : -EBADF;
}

int
drmSetClientCap(int fd, uint64_t capability, uint64_t value)
{
	(void)capability;
	(void)value;

	return file_conn(fd, true) != IO_OBJECT_NULL ? 0 : fail(EBADF);
}

int
drmIsMaster(int fd)
{
	int kind;

	return pd_file_lookup(fd, true, &kind, NULL) != IO_OBJECT_NULL && kind == PD_FILE_DISPLAY;
}

int
drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int *prime_fd)
{
	struct drm_prime_handle a = { .handle = handle, .flags = flags };
	int ret = drmIoctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &a);

	if (ret == 0)
		*prime_fd = a.fd;
	return ret;
}

int
drmPrimeFDToHandle(int fd, int prime_fd, uint32_t *handle)
{
	struct drm_prime_handle a = { .fd = prime_fd };
	int ret = drmIoctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &a);

	if (ret == 0)
		*handle = a.handle;
	return ret;
}

int
drmSyncobjCreate(int fd, uint32_t flags, uint32_t *handle)
{
	struct drm_syncobj_create a = { .flags = flags };
	int ret = drmIoctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &a);

	if (ret == 0)
		*handle = a.handle;
	return ret;
}

int
drmSyncobjDestroy(int fd, uint32_t handle)
{
	struct drm_syncobj_destroy a = { .handle = handle };

	return drmIoctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &a);
}

int
drmSyncobjHandleToFD(int fd, uint32_t handle, int *obj_fd)
{
	struct drm_syncobj_handle a = { .handle = handle };
	int ret = drmIoctl(fd, DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, &a);

	if (ret == 0)
		*obj_fd = a.fd;
	return ret;
}

int
drmSyncobjFDToHandle(int fd, int obj_fd, uint32_t *handle)
{
	struct drm_syncobj_handle a = { .fd = obj_fd };
	int ret = drmIoctl(fd, DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE, &a);

	if (ret == 0)
		*handle = a.handle;
	return ret;
}

int
drmSyncobjImportSyncFile(int fd, uint32_t handle, int sync_file_fd)
{
	struct drm_syncobj_handle a = { .handle = handle, .fd = sync_file_fd,
	    .flags = DRM_SYNCOBJ_FD_TO_HANDLE_FLAGS_IMPORT_SYNC_FILE };

	return drmIoctl(fd, DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE, &a);
}

int
drmSyncobjExportSyncFile(int fd, uint32_t handle, int *sync_file_fd)
{
	struct drm_syncobj_handle a = { .handle = handle, .fd = -1,
	    .flags = DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE };
	int ret = drmIoctl(fd, DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, &a);

	if (ret == 0)
		*sync_file_fd = a.fd;
	return ret;
}

int
drmSyncobjWait(int fd, uint32_t *handles, unsigned num_handles, int64_t timeout_nsec, unsigned flags,
    uint32_t *first_signaled)
{
	struct drm_syncobj_wait a = { .handles = (uintptr_t)handles, .timeout_nsec = timeout_nsec,
	    .count_handles = num_handles, .flags = flags };

	if (drmIoctl(fd, DRM_IOCTL_SYNCOBJ_WAIT, &a) < 0)
		return -errno;

	if (first_signaled)
		*first_signaled = a.first_signaled;
	return 0;
}

int
drmSyncobjReset(int fd, const uint32_t *handles, uint32_t handle_count)
{
	struct drm_syncobj_array a = { .handles = (uintptr_t)handles, .count_handles = handle_count };

	return drmIoctl(fd, DRM_IOCTL_SYNCOBJ_RESET, &a);
}

int
drmSyncobjSignal(int fd, const uint32_t *handles, uint32_t handle_count)
{
	struct drm_syncobj_array a = { .handles = (uintptr_t)handles, .count_handles = handle_count };

	return drmIoctl(fd, DRM_IOCTL_SYNCOBJ_SIGNAL, &a);
}

int
drmSyncobjTimelineSignal(int fd, const uint32_t *handles, uint64_t *points, uint32_t handle_count)
{
	struct drm_syncobj_timeline_array a = { .handles = (uintptr_t)handles, .points = (uintptr_t)points,
	    .count_handles = handle_count };

	return drmIoctl(fd, DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL, &a);
}

int
drmSyncobjTimelineWait(int fd, uint32_t *handles, uint64_t *points, unsigned num_handles,
    int64_t timeout_nsec, unsigned flags, uint32_t *first_signaled)
{
	struct drm_syncobj_timeline_wait a = { .handles = (uintptr_t)handles, .points = (uintptr_t)points,
	    .timeout_nsec = timeout_nsec, .count_handles = num_handles, .flags = flags };

	if (drmIoctl(fd, DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, &a) < 0)
		return -errno;

	if (first_signaled)
		*first_signaled = a.first_signaled;
	return 0;
}

int
drmSyncobjQuery2(int fd, uint32_t *handles, uint64_t *points, uint32_t handle_count, uint32_t flags)
{
	struct drm_syncobj_timeline_array a = { .handles = (uintptr_t)handles, .points = (uintptr_t)points,
	    .count_handles = handle_count, .flags = flags };

	return drmIoctl(fd, DRM_IOCTL_SYNCOBJ_QUERY, &a);
}

int
drmSyncobjQuery(int fd, uint32_t *handles, uint64_t *points, uint32_t handle_count)
{
	return drmSyncobjQuery2(fd, handles, points, handle_count, 0);
}

int
drmSyncobjTransfer(int fd, uint32_t dst_handle, uint64_t dst_point, uint32_t src_handle, uint64_t src_point,
    uint32_t flags)
{
	struct drm_syncobj_transfer a = { .src_handle = src_handle, .dst_handle = dst_handle,
	    .src_point = src_point, .dst_point = dst_point, .flags = flags };

	return drmIoctl(fd, DRM_IOCTL_SYNCOBJ_TRANSFER, &a);
}

// unmap tracked buffer mappings inside [addr, addr + len), called with the lock held
static void
unmap_range(void *addr, size_t len)
{
	uintptr_t lo = (uintptr_t)addr, hi = lo + len;
	size_t i = 0;

	while (i < nmaps) {
		uintptr_t mlo = (uintptr_t)maps[i].addr, mhi = mlo + maps[i].len;

		if (mlo < hi && lo < mhi) {
			IOConnectUnmapMemory64(maps[i].conn, maps[i].type, mach_task_self(),
			    (mach_vm_address_t)mlo);
			maps[i] = maps[--nmaps];
		} else {
			i++;
		}
	}
}

static int
track_map(void *addr, size_t len, io_connect_t conn, uint32_t type)
{
	if (nmaps == capmaps) {
		size_t cap = capmaps ? capmaps * 2 : 64;
		struct pvr_map *m = realloc(maps, cap * sizeof(*m));

		if (m == NULL)
			return -1;

		maps = m;
		capmaps = cap;
	}
	maps[nmaps++] = (struct pvr_map){ addr, len, conn, type };
	return 0;
}

void *
pd_drm_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	io_connect_t conn = file_conn(fd, false);
	mach_vm_address_t va = (mach_vm_address_t)(uintptr_t)addr;
	mach_vm_size_t size = 0;
	IOOptionBits opts = 0;
	kern_return_t kr;
	bool fixed = addr != NULL && (flags & MAP_FIXED);

	if (fixed) {
		pthread_mutex_lock(&lock);
		unmap_range(addr, len);
		pthread_mutex_unlock(&lock);
	}
	if (conn == IO_OBJECT_NULL)
		return mmap(addr, len, prot, flags, fd, offset);

	// iokit cannot map over a reservation, so free the range first
	if (fixed)
		munmap(addr, len);
	else
		opts |= kIOMapAnywhere;
	if (!(prot & PROT_WRITE))
		opts |= kIOMapReadOnly;

	kr = IOConnectMapMemory64(conn, (uint32_t)offset, mach_task_self(), &va, &size, opts);
	if (kr != KERN_SUCCESS) {
		errno = kr_errno(kr);
		return MAP_FAILED;
	}

	pthread_mutex_lock(&lock);
	if (size < len || track_map((void *)(uintptr_t)va, len, conn, (uint32_t)offset) != 0) {
		pthread_mutex_unlock(&lock);
		IOConnectUnmapMemory64(conn, (uint32_t)offset, mach_task_self(), va);
		errno = size < len ? EINVAL : ENOMEM;
		return MAP_FAILED;
	}
	pthread_mutex_unlock(&lock);
	return (void *)(uintptr_t)va;
}

int
pd_drm_munmap(void *addr, size_t len)
{
	bool found = false;

	pthread_mutex_lock(&lock);
	for (size_t i = 0; i < nmaps; i++) {
		if (maps[i].addr == addr) {
			found = true;
			break;
		}
	}
	if (found)
		unmap_range(addr, len);
	pthread_mutex_unlock(&lock);
	return found ? 0 : munmap(addr, len);
}
