// SPDX-License-Identifier: MIT
// drmprobe: checks /dev/dri end to end without libdrm
// version, gpu info, a mapped gem buffer, then PRIME, syncobj and sync_file passing

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// the linux uapi layouts and ioctl numbers, which match the BSD encoding
struct drm_version {
	int version_major, version_minor, version_patchlevel;
	size_t name_len;
	char *name;
	size_t date_len;
	char *date;
	size_t desc_len;
	char *desc;
};
#define DRM_IOCTL_VERSION	_IOWR('d', 0x00, struct drm_version)
#define DRM_COMMAND_BASE	0x40

union drm_amdgpu_gem_create {
	struct { uint64_t bo_size, alignment, domains, domain_flags; } in;
	struct { uint32_t handle, pad; } out;
};
#define DRM_IOCTL_AMDGPU_GEM_CREATE	_IOWR('d', DRM_COMMAND_BASE + 0x00, union drm_amdgpu_gem_create)

union drm_amdgpu_gem_mmap {
	struct { uint32_t handle, pad; } in;
	struct { uint64_t addr_ptr; } out;
};
#define DRM_IOCTL_AMDGPU_GEM_MMAP	_IOWR('d', DRM_COMMAND_BASE + 0x01, union drm_amdgpu_gem_mmap)

struct drm_amdgpu_info {
	uint64_t return_pointer;
	uint32_t return_size, query;
	uint32_t pad[4];
};
#define DRM_IOCTL_AMDGPU_INFO	_IOW('d', DRM_COMMAND_BASE + 0x05, struct drm_amdgpu_info)
#define AMDGPU_INFO_DEV_INFO	0x16

struct drm_prime_handle {
	uint32_t handle, flags;
	int32_t fd;
};
#define DRM_IOCTL_PRIME_HANDLE_TO_FD	_IOWR('d', 0x2d, struct drm_prime_handle)
#define DRM_IOCTL_PRIME_FD_TO_HANDLE	_IOWR('d', 0x2e, struct drm_prime_handle)
#define DRM_CLOEXEC	O_CLOEXEC
#define DRM_RDWR	O_RDWR

struct drm_syncobj_create {
	uint32_t handle, flags;
};
#define DRM_SYNCOBJ_CREATE_SIGNALED	(1 << 0)
#define DRM_IOCTL_SYNCOBJ_CREATE	_IOWR('d', 0xbf, struct drm_syncobj_create)

struct drm_syncobj_handle {
	uint32_t handle, flags;
	int32_t fd;
	uint32_t pad;
};
#define DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE	(1 << 0)
#define DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD	_IOWR('d', 0xc1, struct drm_syncobj_handle)
#define DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE	_IOWR('d', 0xc2, struct drm_syncobj_handle)

struct sync_file_info {
	char name[32];
	int32_t status;
	uint32_t flags, num_fences, pad;
	uint64_t sync_fence_info;
};
#define SYNC_IOC_FILE_INFO	_IOWR('>', 4, struct sync_file_info)

#define AMDGPU_GEM_DOMAIN_GTT	0x2
#define AMDGPU_GEM_DOMAIN_VRAM	0x4

static int
try_buffer(int fd, const char *what, uint64_t domain)
{
	union drm_amdgpu_gem_create create = { .in = { .bo_size = 1 << 20, .alignment = 4096, .domains = domain } };
	union drm_amdgpu_gem_mmap map = { 0 };
	uint32_t *p;
	size_t i, bad = 0;

	if (ioctl(fd, DRM_IOCTL_AMDGPU_GEM_CREATE, &create) != 0) {
		printf("drmprobe: %s gem create failed: %s\n", what, strerror(errno));
		return 1;
	}
	map.in.handle = create.out.handle;
	if (ioctl(fd, DRM_IOCTL_AMDGPU_GEM_MMAP, &map) != 0) {
		printf("drmprobe: %s gem mmap offset failed: %s\n", what, strerror(errno));
		return 1;
	}
	p = mmap(NULL, 1 << 20, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)map.out.addr_ptr);
	if (p == MAP_FAILED) {
		printf("drmprobe: %s mmap at 0x%llx failed: %s\n", what,
		    (unsigned long long)map.out.addr_ptr, strerror(errno));
		return 1;
	}
	for (i = 0; i < (1 << 20) / 4; i++)
		p[i] = (uint32_t)(i * 2654435761u);
	for (i = 0; i < (1 << 20) / 4; i++)
		bad += p[i] != (uint32_t)(i * 2654435761u);
	printf("drmprobe: %s buffer handle %u mapped at %p, 1 MiB written and read back, %zu bad words\n",
	    what, create.out.handle, (void *)p, bad);
	munmap(p, 1 << 20);
	return bad != 0;
}

// a buffer out through a dma-buf fd and back in to the same handle
static int
try_prime(int fd)
{
	union drm_amdgpu_gem_create create = { .in = { .bo_size = 1 << 16, .alignment = 4096,
	    .domains = AMDGPU_GEM_DOMAIN_GTT } };
	struct drm_prime_handle out = { 0 }, in = { 0 };

	if (ioctl(fd, DRM_IOCTL_AMDGPU_GEM_CREATE, &create) != 0) {
		printf("drmprobe: prime gem create failed: %s\n", strerror(errno));
		return 1;
	}
	out.handle = create.out.handle;
	out.flags = DRM_CLOEXEC | DRM_RDWR;
	if (ioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &out) != 0) {
		printf("drmprobe: PRIME_HANDLE_TO_FD failed: %s\n", strerror(errno));
		return 1;
	}
	in.fd = out.fd;
	if (ioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &in) != 0) {
		printf("drmprobe: PRIME_FD_TO_HANDLE failed: %s\n", strerror(errno));
		close(out.fd);
		return 1;
	}
	close(out.fd);
	printf("drmprobe: prime: handle %u exported as fd %d, imported back as handle %u\n",
	    out.handle, out.fd, in.handle);
	return in.handle != out.handle;
}

// a signaled syncobj out as a syncobj fd and as a sync_file, and back in
static int
try_syncobj(int fd)
{
	struct drm_syncobj_create create = { .flags = DRM_SYNCOBJ_CREATE_SIGNALED };
	struct drm_syncobj_handle out = { 0 }, in = { 0 }, sf = { 0 };
	struct sync_file_info info = { 0 };

	if (ioctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &create) != 0) {
		printf("drmprobe: SYNCOBJ_CREATE failed: %s\n", strerror(errno));
		return 1;
	}
	out.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, &out) != 0) {
		printf("drmprobe: SYNCOBJ_HANDLE_TO_FD failed: %s\n", strerror(errno));
		return 1;
	}
	in.fd = out.fd;
	if (ioctl(fd, DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE, &in) != 0) {
		printf("drmprobe: SYNCOBJ_FD_TO_HANDLE failed: %s\n", strerror(errno));
		return 1;
	}
	close(out.fd);
	sf.handle = create.handle;
	sf.flags = DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE;
	if (ioctl(fd, DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, &sf) != 0) {
		printf("drmprobe: sync_file export failed: %s\n", strerror(errno));
		return 1;
	}
	if (ioctl(sf.fd, SYNC_IOC_FILE_INFO, &info) != 0) {
		printf("drmprobe: SYNC_IOC_FILE_INFO failed: %s\n", strerror(errno));
		close(sf.fd);
		return 1;
	}
	close(sf.fd);
	printf("drmprobe: syncobj: handle %u round-tripped as fd (now handle %u), "
	    "sync_file status %d with %u fence(s)\n", create.handle, in.handle, info.status,
	    info.num_fences);
	return info.status != 1;
}

int
main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/dev/dri/renderD128";
	char name[64] = "", date[64] = "", desc[128] = "";
	struct drm_version version = { .name_len = sizeof(name) - 1, .name = name,
	    .date_len = sizeof(date) - 1, .date = date, .desc_len = sizeof(desc) - 1, .desc = desc };
	uint32_t dev_info[64];
	struct drm_amdgpu_info info = { .return_pointer = (uintptr_t)dev_info,
	    .return_size = sizeof(dev_info), .query = AMDGPU_INFO_DEV_INFO };
	int fd, i, failed = 0;

	// the driver loads from the aux KC a little after launchd starts us
	for (i = 0; i < 120 && access(path, F_OK) != 0; i++)
		usleep(500000);
	// the node exists from minor allocation on, the open works once drm registered it
	for (i = 0; i < 120; i++) {
		fd = open(path, O_RDWR);
		if (fd >= 0 || errno != ENODEV)
			break;
		usleep(500000);
	}
	if (fd < 0) {
		printf("drmprobe: open %s failed: %s\n", path, strerror(errno));
		return 1;
	}
	if (ioctl(fd, DRM_IOCTL_VERSION, &version) != 0) {
		printf("drmprobe: DRM_IOCTL_VERSION failed: %s\n", strerror(errno));
		return 1;
	}
	printf("drmprobe: %s is %s %d.%d.%d (%s, %s)\n", path, name, version.version_major,
	    version.version_minor, version.version_patchlevel, desc, date);

	memset(dev_info, 0, sizeof(dev_info));
	if (ioctl(fd, DRM_IOCTL_AMDGPU_INFO, &info) != 0)
		printf("drmprobe: AMDGPU_INFO_DEV_INFO failed: %s\n", strerror(errno));
	else
		printf("drmprobe: device id 0x%x, chip rev %u, family %u\n", dev_info[0],
		    dev_info[1], dev_info[4]);

	failed |= try_buffer(fd, "gtt", AMDGPU_GEM_DOMAIN_GTT);
	failed |= try_buffer(fd, "vram", AMDGPU_GEM_DOMAIN_VRAM);
	failed |= try_prime(fd);
	failed |= try_syncobj(fd);
	close(fd);
	printf("drmprobe: %s\n", failed ? "FAILED" : "all checks passed");
	return failed;
}
