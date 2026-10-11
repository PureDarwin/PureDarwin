// libdrm-virtgpu: mesa venus's libdrm calls on IOVirtIOGPU's venus client, one connection (3D context) per render
// node fd. fences are pipes a waiter thread makes readable once retired, all mesa's simulated syncobjs need

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>
#include <virtgpu_drm.h>

#include "IOVirtIOGPU3DShared.h"
#include "pd_virtgpu_drm.h"

#define SERVICE_CLASS		"IOVirtIOGPU"
#define MAX_FILES		16
#define PRIME_MAGIC		0x47565044	// 'PDVG'
#define WAITER_SLICE_US		20000

struct gpu_file {
	int fd;
	dev_t dev;
	ino_t ino;
	io_connect_t conn;
	struct PDVenusInfo info;
};

struct gpu_map {
	void *addr;
	size_t len;
	io_connect_t conn;
	uint32_t handle;
};

// a fence somebody holds a sync fd for: the write end of its pipe, closed once the host retires it
struct pending_fence {
	io_connect_t conn;
	uint64_t fence;
	int wfd;
};

// what a prime fd carries: the descriptor is a sparse temporary file starting with this. the token
// is the device's, so the fd works in any process it is passed to
struct prime_token {
	uint32_t magic;
	uint32_t pad;
	uint64_t token;
	uint64_t size;
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct gpu_file files[MAX_FILES];
static struct gpu_map *maps;
static size_t nmaps, capmaps;

static pthread_mutex_t fence_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fence_cond = PTHREAD_COND_INITIALIZER;
static struct pending_fence *pending;
static size_t npending, cappending;
static bool waiter_running;

static int
fail(int err)
{
	errno = err;
	return -1;
}

static int
kr_errno(kern_return_t kr)
{
	switch (kr) {
	case kIOReturnBadArgument:
		return EINVAL;
	case kIOReturnNoMemory:
	case kIOReturnNoResources:
	case kIOReturnNoSpace:
		return ENOMEM;
	case kIOReturnNotPermitted:
	case kIOReturnNotPrivileged:
		return EPERM;
	case kIOReturnUnsupported:
		return ENOTTY;
	case kIOReturnNotFound:
		return ENOENT;
	case kIOReturnBusy:
		return EBUSY;
	case kIOReturnTimeout:
		return ETIME;
	case kIOReturnNotReady:
		return EAGAIN;
	default:
		return EIO;
	}
}

// PD_DRM_DEBUG=1 logs calls that fail and device discovery to stderr
static bool
debug_enabled(void)
{
	const char *v = getenv("PD_DRM_DEBUG");

	return v != NULL && *v != '\0' && *v != '0';
}

// PD_DRM_DEBUG=2 also traces blob create, map, unmap and close: who owned a mapped address
static bool
trace_enabled(void)
{
	const char *v = getenv("PD_DRM_DEBUG");

	return v != NULL && strchr(v, '2') != NULL;
}

// unbuffered, so the lines survive a crash
static void
trace(const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > 0)
		write(2, buf, n < (int)sizeof(buf) ? (size_t)n : sizeof(buf) - 1);
}

static int
call(io_connect_t conn, uint32_t sel, const uint64_t *in, uint32_t nin, const void *sin, size_t nsin,
    uint64_t *out, uint32_t nout, void *sout, size_t *nsout)
{
	uint32_t outcnt = nout;
	kern_return_t kr = IOConnectCallMethod(conn, sel, in, nin, sin, nsin, out, out ? &outcnt : NULL, sout, nsout);

	if (kr != KERN_SUCCESS && debug_enabled())
		fprintf(stderr, "libdrm-virtgpu: selector %u failed, kr 0x%x\n", sel, kr);
	return kr == KERN_SUCCESS ? 0 : fail(kr_errno(kr));
}

static io_service_t
gpu_service(void)
{
	return IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching(SERVICE_CLASS));
}

static bool
is_node(const struct stat *st)
{
	struct stat node;

	return S_ISCHR(st->st_mode) && stat(PD_VIRTGPU_DRM_NODE, &node) == 0 && st->st_rdev == node.st_rdev;
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
release_file(struct gpu_file *f)
{
	forget_maps(f->conn);
	IOServiceClose(f->conn);
	memset(f, 0, sizeof(*f));
	f->fd = -1;
}

// the descriptor is swapped for a pipe so a closed and reused number shows up as a new file
static io_connect_t
bind_file(int fd, struct gpu_file *slot)
{
	io_service_t svc = gpu_service();
	io_connect_t conn = IO_OBJECT_NULL;
	int p[2], fdflags = fcntl(fd, F_GETFD);
	size_t len = sizeof(slot->info);
	struct stat st;
	kern_return_t kr;

	if (svc == IO_OBJECT_NULL)
		return IO_OBJECT_NULL;

	kr = IOServiceOpen(svc, mach_task_self(), kIOVirtIOGPUVenusConnectType, &conn);
	IOObjectRelease(svc);
	if (debug_enabled())
		fprintf(stderr, "libdrm-virtgpu: fd %d opens the venus client: kr 0x%x\n", fd, kr);
	if (kr != KERN_SUCCESS)
		return IO_OBJECT_NULL;

	if (IOConnectCallStructMethod(conn, kPDVenus_GetInfo, NULL, 0, &slot->info, &len) != KERN_SUCCESS ||
	    pipe(p) != 0 || dup2(p[0], fd) < 0) {
		IOServiceClose(conn);
		return IO_OBJECT_NULL;
	}
	close(p[0]);
	close(p[1]);
	if (fdflags >= 0)
		fcntl(fd, F_SETFD, fdflags);
	fstat(fd, &st);
	slot->fd = fd;
	slot->dev = st.st_dev;
	slot->ino = st.st_ino;
	slot->conn = conn;
	return conn;
}

// the venus connection behind fd, binding a freshly opened render node when allowed
static struct gpu_file *
file_lookup(int fd, bool bind)
{
	struct gpu_file *free_slot = NULL, *found = NULL;
	struct stat st;
	bool have_st;

	if (fd < 0)
		return NULL;

	have_st = fstat(fd, &st) == 0;
	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_FILES; i++) {
		struct gpu_file *f = &files[i];

		if (f->conn == IO_OBJECT_NULL) {
			if (free_slot == NULL)
				free_slot = f;
			continue;
		}
		if (f->fd != fd)
			continue;

		if (have_st && st.st_dev == f->dev && st.st_ino == f->ino) {
			found = f;
			break;
		}
		release_file(f);
		if (free_slot == NULL)
			free_slot = f;
	}
	if (found == NULL && bind && have_st && free_slot != NULL && is_node(&st) &&
	    bind_file(fd, free_slot) != IO_OBJECT_NULL)
		found = free_slot;
	pthread_mutex_unlock(&lock);
	return found;
}

// unmap every blob before closing the connections
__attribute__((destructor)) static void
close_all(void)
{
	pthread_mutex_lock(&lock);
	for (size_t i = 0; i < nmaps; i++)
		IOConnectUnmapMemory64(maps[i].conn, maps[i].handle, mach_task_self(), (mach_vm_address_t)(uintptr_t)maps[i].addr);
	nmaps = 0;
	for (int i = 0; i < MAX_FILES; i++) {
		if (files[i].conn != IO_OBJECT_NULL)
			release_file(&files[i]);
	}
	pthread_mutex_unlock(&lock);
}

// one thread per process watches every fence that has a sync fd and closes its write end, with a byte
// in the pipe, once the host retires it
static void *
fence_waiter(void *arg)
{
	uint64_t gen = 0;

	(void)arg;
	pthread_mutex_lock(&fence_lock);
	for (;;) {
		while (npending == 0)
			pthread_cond_wait(&fence_cond, &fence_lock);

		io_connect_t conn = pending[0].conn;
		pthread_mutex_unlock(&fence_lock);

		uint64_t in[2] = { gen, WAITER_SLICE_US }, out = gen;
		call(conn, kPDVenus_WaitProgress, in, 2, NULL, 0, &out, 1, NULL, NULL);
		gen = out;

		pthread_mutex_lock(&fence_lock);
		size_t i = 0;
		while (i < npending) {
			uint8_t retired = 0;
			size_t rlen = 1;
			struct pending_fence *p = &pending[i];

			if (IOConnectCallStructMethod(p->conn, kPDVenus_FenceStatus, &p->fence, sizeof(p->fence),
			    &retired, &rlen) != KERN_SUCCESS || retired) {
				char b = 1;

				(void)write(p->wfd, &b, 1);
				close(p->wfd);
				pending[i] = pending[--npending];
			} else
				i++;
		}
	}
	return NULL;
}

// a pollable descriptor that becomes readable when the fence retires
static int
fence_fd(io_connect_t conn, uint64_t fence)
{
	int p[2];
	char b = 1;

	if (pipe(p) != 0)
		return -1;
	fcntl(p[0], F_SETFD, FD_CLOEXEC);
	fcntl(p[1], F_SETFD, FD_CLOEXEC);

	if (fence == 0) {
		(void)write(p[1], &b, 1);
		close(p[1]);
		return p[0];
	}

	pthread_mutex_lock(&fence_lock);
	if (npending == cappending) {
		size_t cap = cappending ? cappending * 2 : 64;
		struct pending_fence *n = realloc(pending, cap * sizeof(*n));

		if (n == NULL) {
			pthread_mutex_unlock(&fence_lock);
			close(p[0]);
			close(p[1]);
			return -1;
		}
		pending = n;
		cappending = cap;
	}
	pending[npending++] = (struct pending_fence){ conn, fence, p[1] };
	if (!waiter_running) {
		pthread_t t;

		if (pthread_create(&t, NULL, fence_waiter, NULL) == 0) {
			pthread_detach(t);
			waiter_running = true;
		}
	}
	pthread_cond_signal(&fence_cond);
	pthread_mutex_unlock(&fence_lock);
	return p[0];
}

static int
get_version(struct drm_version *v)
{
	static const char *name = "virtio_gpu", *date = "0", *desc = "virtio GPU (IOVirtIOGPU)";
	const char *src[3] = { name, date, desc };
	char *dst[3] = { v->name, v->date, v->desc };
	__kernel_size_t *len[3] = { &v->name_len, &v->date_len, &v->desc_len };

	v->version_major = 0;
	v->version_minor = 1;
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
getparam(struct gpu_file *f, struct drm_virtgpu_getparam *a)
{
	enum { F_VIRGL = 1 << 0, F_BLOB = 1 << 3, F_CONTEXT_INIT = 1 << 4 };
	uint64_t v;

	switch (a->param) {
	case VIRTGPU_PARAM_3D_FEATURES:
		v = (f->info.features & F_VIRGL) || (f->info.features & F_CONTEXT_INIT);
		break;
	case VIRTGPU_PARAM_CAPSET_QUERY_FIX:
		v = 1;
		break;
	case VIRTGPU_PARAM_RESOURCE_BLOB:
		v = (f->info.features & F_BLOB) != 0;
		break;
	case VIRTGPU_PARAM_HOST_VISIBLE:
		v = f->info.hostmem_size != 0;
		break;
	case VIRTGPU_PARAM_CROSS_DEVICE:
	case VIRTGPU_PARAM_EXPLICIT_DEBUG_NAME:
		v = 0;
		break;
	case VIRTGPU_PARAM_CONTEXT_INIT:
		v = (f->info.features & F_CONTEXT_INIT) != 0;
		break;
	case VIRTGPU_PARAM_SUPPORTED_CAPSET_IDs:
		v = f->info.capset_mask;
		break;
	default:
		return fail(EINVAL);
	}
	// the kernel writes 32 bits
	memcpy((void *)(uintptr_t)a->value, &(uint32_t){ (uint32_t)v }, sizeof(uint32_t));
	return 0;
}

static int
get_caps(struct gpu_file *f, struct drm_virtgpu_get_caps *a)
{
	uint64_t in[2] = { a->cap_set_id, a->cap_set_ver };
	size_t len = a->size;

	if (a->size == 0 || a->addr == 0)
		return fail(EINVAL);
	memset((void *)(uintptr_t)a->addr, 0, a->size);
	return call(f->conn, kPDVenus_GetCaps, in, 2, NULL, 0, NULL, 0, (void *)(uintptr_t)a->addr, &len);
}

static int
context_init(struct gpu_file *f, struct drm_virtgpu_context_init *a)
{
	const struct drm_virtgpu_context_set_param *p = (const void *)(uintptr_t)a->ctx_set_params;
	uint64_t capset = 0, ctx = 0;

	for (uint32_t i = 0; i < a->num_params; i++) {
		switch (p[i].param) {
		case VIRTGPU_CONTEXT_PARAM_CAPSET_ID:
			capset = p[i].value;
			break;
		case VIRTGPU_CONTEXT_PARAM_NUM_RINGS:
			if (p[i].value > 64)
				return fail(EINVAL);
			break;
		case VIRTGPU_CONTEXT_PARAM_POLL_RINGS_MASK:
		case VIRTGPU_CONTEXT_PARAM_DEBUG_NAME:
			break;
		default:
			return fail(EINVAL);
		}
	}
	return call(f->conn, kPDVenus_ContextInit, &capset, 1, NULL, 0, &ctx, 1, NULL, NULL);
}

static int
submit(struct gpu_file *f, const void *cs, size_t len, uint32_t ring, uint32_t flags, uint64_t *fence)
{
	uint64_t in[2] = { ring, flags }, out = 0;
	int ret = call(f->conn, kPDVenus_Submit, in, 2, len ? cs : NULL, len, &out, 1, NULL, NULL);

	if (ret == 0 && fence != NULL)
		*fence = out;
	return ret;
}

static int
create_blob(struct gpu_file *f, struct drm_virtgpu_resource_create_blob *a)
{
	struct PDVenusBlobCreate bc = { a->blob_mem, a->blob_flags, a->blob_id, a->size };
	uint64_t out[2];

	if (a->cmd_size && submit(f, (const void *)(uintptr_t)a->cmd, a->cmd_size, 0, 0, NULL) != 0)
		return -1;
	if (call(f->conn, kPDVenus_CreateBlob, NULL, 0, &bc, sizeof(bc), out, 2, NULL, NULL) != 0)
		return -1;
	a->bo_handle = (uint32_t)out[0];
	a->res_handle = (uint32_t)out[0];
	if (trace_enabled())
		trace("PDTRACE create res %u size 0x%llx mem %u flags 0x%x\n", a->bo_handle,
		    (unsigned long long)a->size, a->blob_mem, a->blob_flags);
	return 0;
}

static int
resource_info(struct gpu_file *f, struct drm_virtgpu_resource_info *a)
{
	uint64_t in = a->bo_handle, out[3];

	if (call(f->conn, kPDVenus_ResourceInfo, &in, 1, NULL, 0, out, 3, NULL, NULL) != 0)
		return -1;
	a->res_handle = a->bo_handle;
	a->size = (uint32_t)out[0];
	a->blob_mem = (uint32_t)out[1];
	return 0;
}

// the host maps the blob now, the mmap offset is the handle, which pd_drm_mmap hands to IOConnectMapMemory64
static int
map_blob(struct gpu_file *f, struct drm_virtgpu_map *a)
{
	uint64_t in = a->handle, out[2];

	if (call(f->conn, kPDVenus_Map, &in, 1, NULL, 0, out, 2, NULL, NULL) != 0)
		return -1;
	a->offset = a->handle;
	return 0;
}

static int
wait_fd(int fd)
{
	struct pollfd p = { .fd = fd, .events = POLLIN };
	int ret;

	do {
		ret = poll(&p, 1, -1);
	} while (ret < 0 && (errno == EINTR || errno == EAGAIN));
	return ret < 0 ? -1 : 0;
}

static int
execbuffer(struct gpu_file *f, struct drm_virtgpu_execbuffer *a)
{
	uint32_t flags = 0;
	uint64_t fence = 0;

	if (a->flags & ~VIRTGPU_EXECBUF_FLAGS || a->num_in_syncobjs || a->num_out_syncobjs)
		return fail(EINVAL);
	// an in fence is waited for here, the host never sees it
	if ((a->flags & VIRTGPU_EXECBUF_FENCE_FD_IN) && a->fence_fd >= 0 && wait_fd(a->fence_fd) != 0)
		return -1;
	if (a->flags & VIRTGPU_EXECBUF_RING_IDX)
		flags |= PD_VENUS_SUBMIT_RING;
	if (a->flags & VIRTGPU_EXECBUF_FENCE_FD_OUT)
		flags |= PD_VENUS_SUBMIT_FENCE;

	if (submit(f, (const void *)(uintptr_t)a->command, a->size, a->ring_idx, flags, &fence) != 0)
		return -1;
	if (a->flags & VIRTGPU_EXECBUF_FENCE_FD_OUT) {
		a->fence_fd = fence_fd(f->conn, fence);
		if (a->fence_fd < 0)
			return fail(EMFILE);
	}
	return 0;
}

// the fd is an unlinked temporary file holding the export token, it stays valid while the exporting
// file is open
static int
prime_export(struct gpu_file *f, struct drm_prime_handle *a)
{
	uint64_t in = a->handle, out[3], token = 0;
	struct prime_token t;
	char path[] = "/tmp/pdvg.XXXXXX";
	int fd;

	if (call(f->conn, kPDVenus_ResourceInfo, &in, 1, NULL, 0, out, 3, NULL, NULL) != 0)
		return -1;
	if (call(f->conn, kPDVenus_Export, &in, 1, NULL, 0, &token, 1, NULL, NULL) != 0)
		return -1;
	t = (struct prime_token){ PRIME_MAGIC, 0, token, out[0] };
	fd = mkstemp(path);
	if (fd < 0)
		return -1;
	unlink(path);
	if (write(fd, &t, sizeof(t)) != sizeof(t) || ftruncate(fd, (off_t)out[0]) != 0) {
		close(fd);
		return fail(EIO);
	}
	lseek(fd, 0, SEEK_SET);
	if (a->flags & DRM_CLOEXEC)
		fcntl(fd, F_SETFD, FD_CLOEXEC);
	a->fd = fd;
	return 0;
}

static int
prime_import(struct gpu_file *f, struct drm_prime_handle *a)
{
	struct prime_token t;
	uint64_t out[4];

	if (pread(a->fd, &t, sizeof(t), 0) != sizeof(t) || t.magic != PRIME_MAGIC)
		return fail(EINVAL);
	if (call(f->conn, kPDVenus_Import, &t.token, 1, NULL, 0, out, 4, NULL, NULL) != 0)
		return -1;
	a->handle = (uint32_t)out[0];
	return 0;
}

static int
virtgpu_ioctl(struct gpu_file *f, unsigned long request, void *arg)
{
	switch (request) {
	case DRM_IOCTL_VERSION:
		return get_version(arg);
	case DRM_IOCTL_SET_CLIENT_CAP:
		return 0;
	case DRM_IOCTL_GET_CAP: {
		struct drm_get_cap *c = arg;

		if (c->capability != DRM_CAP_PRIME)
			return fail(EINVAL);
		c->value = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT;
		return 0;
	}
	case DRM_IOCTL_VIRTGPU_GETPARAM:
		return getparam(f, arg);
	case DRM_IOCTL_VIRTGPU_GET_CAPS:
		return get_caps(f, arg);
	case DRM_IOCTL_VIRTGPU_CONTEXT_INIT:
		return context_init(f, arg);
	case DRM_IOCTL_VIRTGPU_RESOURCE_CREATE_BLOB:
		return create_blob(f, arg);
	case DRM_IOCTL_VIRTGPU_RESOURCE_INFO:
		return resource_info(f, arg);
	case DRM_IOCTL_VIRTGPU_MAP:
		return map_blob(f, arg);
	case DRM_IOCTL_VIRTGPU_EXECBUFFER:
		return execbuffer(f, arg);
	case DRM_IOCTL_VIRTGPU_WAIT:
		// every submit that matters carries a fence fd, a bo has nothing else to wait for
		return 0;
	case DRM_IOCTL_GEM_CLOSE: {
		uint64_t in = ((struct drm_gem_close *)arg)->handle;

		if (trace_enabled())
			trace("PDTRACE close res %llu\n", (unsigned long long)in);

		return call(f->conn, kPDVenus_Unref, &in, 1, NULL, 0, NULL, 0, NULL, NULL);
	}
	case DRM_IOCTL_PRIME_HANDLE_TO_FD:
		return prime_export(f, arg);
	case DRM_IOCTL_PRIME_FD_TO_HANDLE:
		return prime_import(f, arg);
	default:
		return fail(ENOTTY);
	}
}

int
drmIoctl(int fd, unsigned long request, void *arg)
{
	struct gpu_file *f = file_lookup(fd, true);
	int ret;

	if (f != NULL) {
		ret = virtgpu_ioctl(f, request, arg);
		if (ret != 0 && debug_enabled())
			fprintf(stderr, "libdrm-virtgpu: ioctl 0x%lx failed, errno %d\n", request & 0xff, errno);
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

static char *
dup_string(const char *s, int *len)
{
	*len = (int)strlen(s);
	return strdup(s);
}

drmVersionPtr
drmGetVersion(int fd)
{
	drmVersionPtr r;

	if (file_lookup(fd, true) == NULL) {
		errno = ENODEV;
		return NULL;
	}
	r = calloc(1, sizeof(*r));
	if (r == NULL)
		return NULL;

	r->version_major = 0;
	r->version_minor = 1;
	r->name = dup_string("virtio_gpu", &r->name_len);
	r->date = dup_string("0", &r->date_len);
	r->desc = dup_string("virtio GPU (IOVirtIOGPU)", &r->desc_len);
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
	(void)fd;
	return 0;
}

int
drmGetNodeTypeFromFd(int fd)
{
	return file_lookup(fd, true) != NULL ? DRM_NODE_RENDER : fail(ENODEV);
}

char *
drmGetRenderDeviceNameFromFd(int fd)
{
	return file_lookup(fd, true) != NULL ? strdup(PD_VIRTGPU_DRM_NODE) : NULL;
}

char *
drmGetPrimaryDeviceNameFromFd(int fd)
{
	return file_lookup(fd, true) != NULL ? strdup(PD_VIRTGPU_DRM_NODE) : NULL;
}

static void
free_device(drmDevicePtr d)
{
	if (d == NULL)
		return;

	if (d->nodes != NULL) {
		for (int i = 0; i < DRM_NODE_MAX; i++)
			free(d->nodes[i]);
		free(d->nodes);
	}
	free(d->businfo.pci);
	free(d->deviceinfo.pci);
	free(d);
}

// the PCI location of the device the service sits on, "dev,func" in the service plane
static void
pci_location(io_service_t svc, drmPciBusInfoPtr bus)
{
	io_registry_entry_t parent = IO_OBJECT_NULL;
	io_name_t loc = "";
	unsigned dev = 0, func = 0;

	if (IORegistryEntryGetParentEntry(svc, kIOServicePlane, &parent) != KERN_SUCCESS)
		return;
	IORegistryEntryGetLocationInPlane(parent, kIOServicePlane, loc);
	IOObjectRelease(parent);
	if (sscanf(loc, "%x,%x", &dev, &func) >= 1) {
		bus->dev = (uint8_t)dev;
		bus->func = (uint8_t)func;
	}
}

// the virtio-gpu PCI function with a render node, which is what venus looks for
static drmDevicePtr
make_device(io_service_t svc)
{
	drmDevicePtr d = calloc(1, sizeof(*d));

	if (d == NULL)
		return NULL;

	d->bustype = DRM_BUS_PCI;
	d->available_nodes = (1 << DRM_NODE_RENDER) | (1 << DRM_NODE_PRIMARY);
	d->nodes = calloc(DRM_NODE_MAX, sizeof(char *));
	d->businfo.pci = calloc(1, sizeof(*d->businfo.pci));
	d->deviceinfo.pci = calloc(1, sizeof(*d->deviceinfo.pci));
	if (d->nodes == NULL || d->businfo.pci == NULL || d->deviceinfo.pci == NULL) {
		free_device(d);
		return NULL;
	}
	d->nodes[DRM_NODE_PRIMARY] = strdup(PD_VIRTGPU_DRM_NODE);
	d->nodes[DRM_NODE_RENDER] = strdup(PD_VIRTGPU_DRM_NODE);
	d->deviceinfo.pci->vendor_id = 0x1af4;
	d->deviceinfo.pci->device_id = 0x1050;
	d->deviceinfo.pci->subvendor_id = 0x1af4;
	d->deviceinfo.pci->subdevice_id = 0x1100;
	pci_location(svc, d->businfo.pci);
	return d;
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

	for (int i = 0; i < count; i++)
		drmFreeDevice(&devices[i]);
}

// one gpu, offered only when the kext brought venus up
int
drmGetDevices2(uint32_t flags, drmDevicePtr devices[], int max_devices)
{
	io_service_t svc = gpu_service();
	io_connect_t conn = IO_OBJECT_NULL;
	drmDevicePtr d;

	(void)flags;
	if (svc == IO_OBJECT_NULL)
		return 0;
	if (IOServiceOpen(svc, mach_task_self(), kIOVirtIOGPUVenusConnectType, &conn) != KERN_SUCCESS) {
		if (debug_enabled())
			fprintf(stderr, "libdrm-virtgpu: %s has no venus client\n", SERVICE_CLASS);
		IOObjectRelease(svc);
		return 0;
	}
	IOServiceClose(conn);

	if (devices == NULL || max_devices < 1) {
		IOObjectRelease(svc);
		return 1;
	}
	d = make_device(svc);
	IOObjectRelease(svc);
	if (d == NULL)
		return -ENOMEM;
	devices[0] = d;
	return 1;
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
	int n;

	if (device == NULL || file_lookup(fd, true) == NULL)
		return -EINVAL;
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
drmDevicesEqual(drmDevicePtr a, drmDevicePtr b)
{
	if (a == NULL || b == NULL || a->bustype != b->bustype || a->bustype != DRM_BUS_PCI)
		return 0;
	return memcmp(a->businfo.pci, b->businfo.pci, sizeof(*a->businfo.pci)) == 0;
}

int
drmSetClientCap(int fd, uint64_t capability, uint64_t value)
{
	(void)capability;
	(void)value;
	return file_lookup(fd, true) != NULL ? 0 : fail(EBADF);
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

// drm syncobjs: venus simulates its own, but the vulkan runtime links the whole family

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

// unmap tracked blob mappings inside [addr, addr + len), called with the lock held
static void
unmap_range(void *addr, size_t len)
{
	uintptr_t lo = (uintptr_t)addr, hi = lo + len;
	size_t i = 0;

	while (i < nmaps) {
		uintptr_t mlo = (uintptr_t)maps[i].addr, mhi = mlo + maps[i].len;

		if (mlo < hi && lo < mhi) {
			kern_return_t ukr = IOConnectUnmapMemory64(maps[i].conn, maps[i].handle, mach_task_self(), (mach_vm_address_t)mlo);

			if (trace_enabled())
				trace("PDTRACE unmap res %u at 0x%llx len 0x%zx kr 0x%x\n", maps[i].handle,
				    (unsigned long long)mlo, maps[i].len, ukr);
			maps[i] = maps[--nmaps];
		} else
			i++;
	}
}

static int
track_map(void *addr, size_t len, io_connect_t conn, uint32_t handle)
{
	if (nmaps == capmaps) {
		size_t cap = capmaps ? capmaps * 2 : 64;
		struct gpu_map *m = realloc(maps, cap * sizeof(*m));

		if (m == NULL)
			return -1;
		maps = m;
		capmaps = cap;
	}
	maps[nmaps++] = (struct gpu_map){ addr, len, conn, handle };
	return 0;
}

void *
pd_drm_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	struct gpu_file *f = file_lookup(fd, false);
	mach_vm_address_t va = (mach_vm_address_t)(uintptr_t)addr;
	mach_vm_size_t size = 0;
	IOOptionBits opts = 0;
	bool fixed = addr != NULL && (flags & MAP_FIXED);
	kern_return_t kr;

	if (fixed) {
		pthread_mutex_lock(&lock);
		unmap_range(addr, len);
		pthread_mutex_unlock(&lock);
	}
	if (f == NULL)
		return mmap(addr, len, prot, flags, fd, offset);

	// iokit cannot map over a reservation, so free the range first
	if (fixed)
		munmap(addr, len);
	else
		opts |= kIOMapAnywhere;
	if (!(prot & PROT_WRITE))
		opts |= kIOMapReadOnly;

	kr = IOConnectMapMemory64(f->conn, (uint32_t)offset, mach_task_self(), &va, &size, opts);
	if (kr != KERN_SUCCESS) {
		if (debug_enabled())
			fprintf(stderr, "libdrm-virtgpu: map blob %u: kr 0x%x\n", (uint32_t)offset, kr);
		errno = kr_errno(kr);
		return MAP_FAILED;
	}

	pthread_mutex_lock(&lock);
	if (size < len || track_map((void *)(uintptr_t)va, len, f->conn, (uint32_t)offset) != 0) {
		pthread_mutex_unlock(&lock);
		IOConnectUnmapMemory64(f->conn, (uint32_t)offset, mach_task_self(), va);
		errno = size < len ? EINVAL : ENOMEM;
		return MAP_FAILED;
	}
	pthread_mutex_unlock(&lock);
	if (trace_enabled())
		trace("PDTRACE map res %u len 0x%zx at 0x%llx size 0x%llx fixed %d\n", (uint32_t)offset, len,
		    (unsigned long long)va, (unsigned long long)size, fixed);
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
	if (trace_enabled())
		trace("PDTRACE munmap 0x%llx len 0x%zx tracked %d\n", (unsigned long long)(uintptr_t)addr, len, found);
	return found ? 0 : munmap(addr, len);
}
