#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <wayland-client.h>

#include "swc_snap-client-protocol.h"

enum { CAPTURE_FLAGS = SWC_SNAP_FLAGS_OVERLAY_CURSOR };

struct shm_buffer {
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;
	void *data;
	size_t size;
	int fd;
	int32_t width;
	int32_t height;
	int32_t stride;
};

struct app {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_shm *shm;
	struct swc_snap *snap;
	uint32_t width;
	uint32_t height;
	bool have_buffer_info;
	bool capture_ready;
	bool capture_failed;
	uint32_t failure_reason;
};

static void
usage(FILE *stream, const char *argv0)
{
	fprintf(stream,
	        "usage:\n"
	        "  %s image [output.ppm]\n"
	        "  %s record [-o output.raw|-] [-n frames] [-r fps]\n",
	        argv0, argv0);
}

static ssize_t
write_all(int fd, const void *data, size_t size)
{
	const uint8_t *p = data;
	size_t written = 0;

	while (written < size) {
		ssize_t rc = write(fd, p + written, size - written);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		written += (size_t)rc;
	}

	return (ssize_t)written;
}

static int
create_shm_file(size_t size)
{
	char path[] = "/tmp/swcsnap-XXXXXX";
	int fd = mkstemp(path);

	if (fd < 0) {
		return -1;
	}

	unlink(path);

	if (ftruncate(fd, (off_t)size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static bool
shm_buffer_create(struct shm_buffer *buffer, struct wl_shm *shm,
                  int32_t width, int32_t height, uint32_t format)
{
	size_t size;

	memset(buffer, 0, sizeof(*buffer));
	buffer->fd = -1;
	buffer->width = width;
	buffer->height = height;
	buffer->stride = width * 4;
	size = (size_t)buffer->stride * buffer->height;
	buffer->size = size;

	buffer->fd = create_shm_file(size);
	if (buffer->fd < 0) {
		return false;
	}

	buffer->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
	                    buffer->fd, 0);
	if (buffer->data == MAP_FAILED) {
		close(buffer->fd);
		buffer->fd = -1;
		buffer->data = NULL;
		return false;
	}

	buffer->pool = wl_shm_create_pool(shm, buffer->fd, (int32_t)size);
	if (!buffer->pool) {
		munmap(buffer->data, size);
		close(buffer->fd);
		buffer->fd = -1;
		buffer->data = NULL;
		return false;
	}

	buffer->buffer = wl_shm_pool_create_buffer(
	    buffer->pool, 0, width, height, buffer->stride, format);
	if (!buffer->buffer) {
		wl_shm_pool_destroy(buffer->pool);
		munmap(buffer->data, size);
		close(buffer->fd);
		buffer->pool = NULL;
		buffer->fd = -1;
		buffer->data = NULL;
		return false;
	}

	return true;
}

static void
shm_buffer_destroy(struct shm_buffer *buffer)
{
	if (buffer->buffer) {
		wl_buffer_destroy(buffer->buffer);
	}
	if (buffer->pool) {
		wl_shm_pool_destroy(buffer->pool);
	}
	if (buffer->data) {
		munmap(buffer->data, buffer->size);
	}
	if (buffer->fd >= 0) {
		close(buffer->fd);
	}
	memset(buffer, 0, sizeof(*buffer));
	buffer->fd = -1;
}

static bool
write_ppm(const char *path, const struct shm_buffer *buffer)
{
	FILE *f;
	uint8_t rgb[3];

	f = fopen(path, "wb");
	if (!f) {
		perror("fopen");
		return false;
	}

	if (fprintf(f, "P6\n%d %d\n255\n", buffer->width, buffer->height) < 0) {
		fclose(f);
		return false;
	}

	for (int32_t y = 0; y < buffer->height; y++) {
		const uint8_t *row =
		    (const uint8_t *)buffer->data + (size_t)y * buffer->stride;
		for (int32_t x = 0; x < buffer->width; x++) {
			const uint8_t *px = row + (size_t)x * 4;

			rgb[0] = px[2];
			rgb[1] = px[1];
			rgb[2] = px[0];
			if (fwrite(rgb, 1, sizeof(rgb), f) != sizeof(rgb)) {
				fclose(f);
				return false;
			}
		}
	}

	return fclose(f) == 0;
}

static void
handle_snap_buffer(void *data, struct swc_snap *snap, uint32_t width,
                   uint32_t height, uint32_t format)
{
	struct app *app = data;
	(void)snap;

	app->width = width;
	app->height = height;
	app->have_buffer_info = true;
}

static void
handle_snap_ready(void *data, struct swc_snap *snap, uint32_t tv_sec,
                  uint32_t tv_nsec)
{
	struct app *app = data;
	(void)snap;
	(void)tv_sec;
	(void)tv_nsec;

	app->capture_ready = true;
}

static void
handle_snap_failed(void *data, struct swc_snap *snap, uint32_t reason)
{
	struct app *app = data;
	(void)snap;

	app->capture_failed = true;
	app->failure_reason = reason;
}

static const struct swc_snap_listener snap_listener = {
    .buffer = handle_snap_buffer,
    .ready = handle_snap_ready,
    .failed = handle_snap_failed,
};

static void
handle_global(void *data, struct wl_registry *registry, uint32_t name,
              const char *interface, uint32_t version)
{
	struct app *app = data;

	if (strcmp(interface, wl_shm_interface.name) == 0) {
		app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
		return;
	}

	if (strcmp(interface, "swc_snap") == 0) {
		uint32_t bind_version = version < 2 ? version : 2;

		app->snap =
		    wl_registry_bind(registry, name, &swc_snap_interface, bind_version);
		if (app->snap) {
			swc_snap_add_listener(app->snap, &snap_listener, app);
		}
	}
}

static void
handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static bool
app_connect(struct app *app)
{
	memset(app, 0, sizeof(*app));

	app->display = wl_display_connect(NULL);
	if (!app->display) {
		fprintf(stderr, "swcsnap: failed to connect to Wayland display\n");
		return false;
	}

	app->registry = wl_display_get_registry(app->display);
	wl_registry_add_listener(app->registry, &registry_listener, app);

	if (wl_display_roundtrip(app->display) < 0 ||
	    wl_display_roundtrip(app->display) < 0) {
		fprintf(stderr, "swcsnap: roundtrip failed\n");
		return false;
	}

	if (!app->shm || !app->snap || !app->have_buffer_info) {
		fprintf(stderr, "swcsnap: missing wl_shm/swc_snap metadata\n");
		return false;
	}

	return true;
}

static void
app_disconnect(struct app *app)
{
	if (app->snap) {
		swc_snap_destroy(app->snap);
	}
	if (app->shm) {
		wl_shm_destroy(app->shm);
	}
	if (app->registry) {
		wl_registry_destroy(app->registry);
	}
	if (app->display) {
		wl_display_disconnect(app->display);
	}
	memset(app, 0, sizeof(*app));
}

static bool
capture_once(struct app *app, struct shm_buffer *buffer)
{
	app->capture_ready = false;
	app->capture_failed = false;
	app->failure_reason = 0;

	swc_snap_capture(app->snap, buffer->buffer, CAPTURE_FLAGS);
	wl_display_flush(app->display);

	while (!app->capture_ready && !app->capture_failed) {
		if (wl_display_dispatch(app->display) < 0) {
			return false;
		}
	}

	return !app->capture_failed;
}

static void
timespec_add_ns(struct timespec *ts, long ns)
{
	ts->tv_nsec += ns;
	while (ts->tv_nsec >= 1000000000L) {
		ts->tv_nsec -= 1000000000L;
		ts->tv_sec += 1;
	}
}

static void
sleep_until(const struct timespec *deadline)
{
	struct timespec now;
	struct timespec remaining;

	for (;;) {
		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
			return;
		}

		if (now.tv_sec > deadline->tv_sec ||
		    (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec)) {
			return;
		}

		remaining.tv_sec = deadline->tv_sec - now.tv_sec;
		remaining.tv_nsec = deadline->tv_nsec - now.tv_nsec;
		if (remaining.tv_nsec < 0) {
			remaining.tv_nsec += 1000000000L;
			remaining.tv_sec--;
		}

		if (nanosleep(&remaining, NULL) == 0 || errno != EINTR) {
			return;
		}
	}
}

static int
cmd_image(const char *prog, int argc, char **argv)
{
	struct app app;
	struct shm_buffer buffer = {.fd = -1};
	const char *output = "shot.ppm";
	int ret = 1;

	if (argc > 2) {
		fprintf(stderr, "usage: %s image [output.ppm]\n", prog);
		return 1;
	}
	if (argc == 2) {
		output = argv[1];
	}

	if (!app_connect(&app)) {
		goto out0;
	}

	if (!shm_buffer_create(&buffer, app.shm, (int32_t)app.width,
	                       (int32_t)app.height, WL_SHM_FORMAT_ARGB8888)) {
		fprintf(stderr, "swcsnap: failed to allocate shm buffer\n");
		goto out1;
	}

	if (!capture_once(&app, &buffer)) {
		fprintf(stderr, "swcsnap: capture failed (%u)\n", app.failure_reason);
		goto out2;
	}

	if (!write_ppm(output, &buffer)) {
		fprintf(stderr, "swcsnap: failed to write %s\n", output);
		goto out2;
	}

	ret = 0;

out2:
	shm_buffer_destroy(&buffer);
out1:
	app_disconnect(&app);
out0:
	return ret;
}

static int
cmd_record(const char *prog, int argc, char **argv)
{
	struct app app;
	struct shm_buffer buffer = {.fd = -1};
	unsigned fps = 60;
	unsigned frames = 600;
	const char *output_path = "-";
	int out_fd = STDOUT_FILENO;
	struct timespec deadline;
	long frame_ns;
	int ret = 1;
	int opt;

	optind = 1;
	while ((opt = getopt(argc, argv, "o:n:r:")) != -1) {
		switch (opt) {
		case 'o':
			output_path = optarg;
			break;
		case 'n':
			frames = (unsigned)strtoul(optarg, NULL, 10);
			break;
		case 'r':
			fps = (unsigned)strtoul(optarg, NULL, 10);
			break;
		default:
			fprintf(stderr,
			        "usage: %s record [-o output.raw|-] [-n frames] [-r fps]\n",
			        prog);
			return 1;
		}
	}

	if (optind != argc) {
		fprintf(stderr,
		        "usage: %s record [-o output.raw|-] [-n frames] [-r fps]\n",
		        prog);
		return 1;
	}

	if (fps == 0) {
		fprintf(stderr, "swcsnap: fps must be > 0\n");
		return 1;
	}

	if (strcmp(output_path, "-") != 0) {
		out_fd = open(output_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
		if (out_fd < 0) {
			perror("open");
			return 1;
		}
	}

	if (!app_connect(&app)) {
		goto out0;
	}

	if (!shm_buffer_create(&buffer, app.shm, (int32_t)app.width,
	                       (int32_t)app.height, WL_SHM_FORMAT_ARGB8888)) {
		fprintf(stderr, "swcsnap: failed to allocate shm buffer\n");
		goto out1;
	}

	fprintf(stderr, "swcsnap: rawvideo stream %ux%u bgra @ %u fps (%u frames)\n",
	        app.width, app.height, fps, frames);

	frame_ns = 1000000000L / (long)fps;
	clock_gettime(CLOCK_MONOTONIC, &deadline);

	for (unsigned i = 0; i < frames;) {
		if ((uint32_t)buffer.width != app.width ||
		    (uint32_t)buffer.height != app.height) {
			shm_buffer_destroy(&buffer);
			if (!shm_buffer_create(&buffer, app.shm, (int32_t)app.width,
			                       (int32_t)app.height,
			                       WL_SHM_FORMAT_ARGB8888)) {
				fprintf(stderr, "swcsnap: failed to reallocate shm buffer\n");
				goto out2;
			}
			fprintf(stderr, "swcsnap: resized to %ux%u\n", app.width,
			        app.height);
		}

		if (!capture_once(&app, &buffer)) {
			if (app.capture_failed &&
			    app.failure_reason == SWC_SNAP_FAILURE_REASON_SIZE_MISMATCH) {
				continue;
			}
			fprintf(stderr, "swcsnap: capture failed (%u)\n",
			        app.failure_reason);
			goto out2;
		}

		if (write_all(out_fd, buffer.data, buffer.size) < 0) {
			perror("write");
			goto out2;
		}

		i++;
		timespec_add_ns(&deadline, frame_ns);
		sleep_until(&deadline);
	}

	ret = 0;

out2:
	shm_buffer_destroy(&buffer);
out1:
	app_disconnect(&app);
out0:
	if (out_fd != STDOUT_FILENO && out_fd >= 0) {
		close(out_fd);
	}
	return ret;
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		usage(stderr, argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "image") == 0) {
		return cmd_image(argv[0], argc - 1, argv + 1);
	}
	if (strcmp(argv[1], "record") == 0) {
		return cmd_record(argv[0], argc - 1, argv + 1);
	}

	usage(stderr, argv[0]);
	return 1;
}
