#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct USBHIDMouseEvent {
	uint32_t sequence;
	uint8_t mouseIndex;
	uint8_t buttons;
	int8_t dx;
	int8_t dy;
	int8_t wheel;
	uint8_t reserved[3];
};

int
main(int argc, char **argv)
{
	const char *path = "/dev/usb_hid_mouse";
	int fd;

	if (argc > 1) {
		path = argv[1];
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "mousemon: open %s failed: %s\n",
		    path, strerror(errno));
		return 1;
	}

	printf("mousemon: reading %s\n", path);
	for (;;) {
		struct USBHIDMouseEvent event;
		ssize_t n = read(fd, &event, sizeof(event));

		if (n == (ssize_t)sizeof(event)) {
			printf("seq=%u mouse=%u buttons=%02x dx=%d dy=%d wheel=%d\n",
			    event.sequence, event.mouseIndex, event.buttons,
			    event.dx, event.dy, event.wheel);
			continue;
		}
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			usleep(50000);
			continue;
		}
		if (n == 0) {
			usleep(50000);
			continue;
		}

		fprintf(stderr, "mousemon: read failed: %s\n",
		    n < 0 ? strerror(errno) : "short read");
		close(fd);
		return 1;
	}
}
