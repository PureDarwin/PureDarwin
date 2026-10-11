// pdvklife: buffer lifetime against PDPowerVR through libdrm-pdpvr. closes a buffer while it is still
// mapped, keeps writing through the mapping, then exits with mappings live and skips the shim's unmap

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>

#include "pd_pvr_drm.h"
#include "pvr_drm_min.h"

#define BO_SIZE		0x10000
#define NBO		4

static int
create_bo(int fd, uint32_t *handle, uint64_t *offset)
{
	struct pvr_create_bo cb = { .size = BO_SIZE, .flags = PVR_BO_ALLOW_CPU_USERSPACE_ACCESS };
	struct pvr_mmap_offset mo = { 0 };

	if (drmIoctl(fd, PVR_IOCTL_CREATE_BO, &cb) != 0)
		return -1;

	mo.handle = cb.handle;
	if (drmIoctl(fd, PVR_IOCTL_GET_BO_MMAP_OFFSET, &mo) != 0)
		return -1;

	*handle = cb.handle;
	*offset = mo.offset;
	return 0;
}

int
main(void)
{
	uint32_t handle[NBO];
	uint64_t offset[NBO];
	volatile uint32_t *map[NBO];
	drmVersionPtr v;
	int fd;

	setvbuf(stdout, NULL, _IONBF, 0);
	fd = open(PD_PVR_DRM_NODE, O_RDWR);
	v = fd >= 0 ? drmGetVersion(fd) : NULL;
	if (v == NULL) {
		printf("pdvklife: no gpu\nFAIL\n");
		return 1;
	}
	drmFreeVersion(v);

	for (int i = 0; i < NBO; i++) {
		if (create_bo(fd, &handle[i], &offset[i]) != 0) {
			printf("pdvklife: create bo %d failed\nFAIL\n", i);
			return 1;
		}
		map[i] = pd_drm_mmap(NULL, BO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset[i]);
		if ((void *)map[i] == MAP_FAILED) {
			printf("pdvklife: map bo %d failed\nFAIL\n", i);
			return 1;
		}
		for (int w = 0; w < BO_SIZE / 4; w++) {
			map[i][w] = 0x11110000u | w;
		}
	}
	printf("pdvklife: %d buffers created and mapped\n", NBO);

	// close two buffers while their mappings are live, then keep using those mappings
	for (int i = 0; i < 2; i++) {
		struct drm_gem_close gc = { .handle = handle[i] };

		printf("pdvklife: gem close %u while mapped: %d\n", handle[i], drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &gc));
	}
	for (int i = 0; i < 2; i++) {
		uint32_t bad = 0;

		for (int w = 0; w < BO_SIZE / 4; w++) {
			map[i][w] ^= 0xffffffffu;
			if (map[i][w] != (~(0x11110000u | w)))
				bad++;
		}
		printf("pdvklife: closed buffer %d still readable and writable, %u bad words\n", i, bad);
	}

	// exit with every mapping live and without the shim's exit-time unmap
	printf("pdvklife: exiting with %d live mappings\nPASS\n", NBO);
	_exit(0);
}
