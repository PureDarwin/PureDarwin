// SPDX-License-Identifier: MIT
// maps and unmaps drm dumb buffers in a loop and reports free pages, for the cdev mmap pager

#include <fcntl.h>
#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct create_dumb { uint32_t height, width, bpp, flags, handle, pitch; uint64_t size; };
struct map_dumb { uint32_t handle, pad; uint64_t offset; };
struct destroy_dumb { uint32_t handle; };

// the system's own macros, linux and darwin encode the direction bits differently
#define CREATE_DUMB _IOWR('d', 0xb2, struct create_dumb)
#define MAP_DUMB _IOWR('d', 0xb3, struct map_dumb)
#define DESTROY_DUMB _IOWR('d', 0xb4, struct destroy_dumb)

static unsigned long
free_pages(void)
{
	vm_statistics64_data_t vm;
	mach_msg_type_number_t n = HOST_VM_INFO64_COUNT;

	if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &n) != KERN_SUCCESS)
		return 0;
	return vm.free_count;
}

int
main(int argc, char **argv)
{
	int loops = argc > 1 ? atoi(argv[1]) : 3000;
	// mode: create (no mmap), map (map and unmap), touch1 (write one page), touchall (every page)
	const char *mode = argc > 2 ? argv[2] : "touchall";
	int do_map = strcmp(mode, "create") != 0;
	int pages = strcmp(mode, "touchall") == 0 ? -1 : strcmp(mode, "touch1") == 0 ? 1 : 0;
	int fd;

	// line buffered, a run cut short still shows how far it got
	setvbuf(stdout, NULL, _IOLBF, 0);
	fd = open("/dev/dri/card0", O_RDWR);

	if (fd < 0) {
		perror("pdmmaploop: /dev/dri/card0");
		return 1;
	}
	printf("pdmmaploop: %s start, %d loops, free pages %lu\n", mode, loops, free_pages());
	for (int i = 1; i <= loops; i++) {
		struct create_dumb c = { .height = 512, .width = 512, .bpp = 32 };
		struct map_dumb m = { 0 };
		struct destroy_dumb d = { 0 };

		if (ioctl(fd, CREATE_DUMB, &c) != 0) {
			perror("pdmmaploop: create");
			return 1;
		}
		if (do_map) {
			m.handle = c.handle;
			if (ioctl(fd, MAP_DUMB, &m) != 0) {
				perror("pdmmaploop: map");
				return 1;
			}
			uint8_t *p = mmap(NULL, c.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)m.offset);
			if (p == MAP_FAILED) {
				perror("pdmmaploop: mmap");
				return 1;
			}
			for (uint64_t o = 0; o < c.size && (pages < 0 || o < (uint64_t)pages * 4096); o += 4096)
				p[o] = (uint8_t)i;
			munmap(p, c.size);
		}
		d.handle = c.handle;
		ioctl(fd, DESTROY_DUMB, &d);
		if (i % 500 == 0)
			printf("pdmmaploop: %s %d loops, free pages %lu\n", mode, i, free_pages());
	}
	close(fd);
	printf("pdmmaploop: %s closed, free pages %lu\n", mode, free_pages());
	// the pager drops its pages from deferred work, give it time before the last count
	sleep(5);
	printf("pdmmaploop: %s done after 5 s, free pages %lu\n", mode, free_pages());
	return 0;
}
