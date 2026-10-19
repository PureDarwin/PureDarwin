#include "fb.h"
#include "compositor.h"
#include "mode.h"
#include "internal.h"
#include "output.h"
#include "pointer.h"
#include "screen.h"
#include "seat.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <string.h>
#include <wayland-server.h>
#include <wld/pixman.h>
#include <wld/wld.h>

struct swc_fb swc_fb;

// the frame's damage and the cursor rectangle of the last present, so only those are copied
static struct {
	pixman_region32_t region;
	bool initialized, valid;
	int32_t cursor_x, cursor_y;
	uint32_t cursor_w, cursor_h;
	bool cursor_shown;
	struct wld_buffer *last_buffer;
	uint32_t last_width, last_height;
} fb_damage;

void
fb_set_damage(const pixman_region32_t *damage)
{
	if (!fb_damage.initialized) {
		pixman_region32_init(&fb_damage.region);
		fb_damage.initialized = true;
	}
	if (damage) {
		pixman_region32_copy(&fb_damage.region, (pixman_region32_t *)damage);
		fb_damage.valid = true;
	} else {
		fb_damage.valid = false;
	}
}

/* for the cursor */ 
static uint32_t
blend(uint32_t background, uint32_t foreground)
{
	uint32_t alpha = foreground >> 24;
	uint32_t inverse = 255 - alpha;
	uint32_t red = ((foreground >> 16) & 0xff) +
	               (((background >> 16) & 0xff) * inverse + 127) / 255;
	uint32_t green = ((foreground >> 8) & 0xff) +
	                 (((background >> 8) & 0xff) * inverse + 127) / 255;
	uint32_t blue = (foreground & 0xff) +
	                ((background & 0xff) * inverse + 127) / 255;
	return (MIN(red, 255) << 16) | (MIN(green, 255) << 8) | MIN(blue, 255);
}

#ifdef __PUREDARWIN__
// ARGON_GFX=cpu|gles|auto picks the compositor's renderer (SWC_GPU_COMPOSITOR still means gles).
// auto takes GLES only on a GPU whose resources can be scanned out, and never a software rasteriser.
// Unset means cpu until the GL path has been tested on virgl.
// ARGON_GPU=soft|<render node>|<pci id>|<gallium driver> composites on that device and reads frames back.
enum { GPU_OFF, GPU_AUTO, GPU_ON };

static struct {
	int mode;
	bool active, direct, direct_tried;
	// ARGON_GPU named a render device, which may not be the display's, so frames are read back
	const char *device;
	bool separate;
	// rendering straight into the display's flip pair, and what the last frame changed
	bool flip;
	pixman_region32_t last_damage;
} gpu;

static int
gpu_mode(void)
{
	const char *mode = getenv("ARGON_GFX");

	gpu.device = getenv("ARGON_GPU");
	gpu.separate = gpu.device && *gpu.device && strcmp(gpu.device, "auto");
	// a chosen render device means gl, unless ARGON_GFX says otherwise
	if (!mode || !*mode)
		return getenv("SWC_GPU_COMPOSITOR") || gpu.separate ? GPU_ON : GPU_OFF;
	if (!strcmp(mode, "cpu"))
		return GPU_OFF;
	if (!strcmp(mode, "gles") || !strcmp(mode, "gl"))
		return GPU_ON;
	return GPU_AUTO;
}

static struct wld_renderer *
gpu_renderer(void)
{
	struct wld_renderer *renderer;
	const char *name;

	if (gpu.separate) {
		renderer = wld_gl_create_renderer_on(gpu.device);
		if (!renderer) {
			fprintf(stderr, "neuswc: no GL on ARGON_GPU %s, compositing on the CPU\n", gpu.device);
			return NULL;
		}
		// the frame is copied into the display's framebuffer, the minimal render-elsewhere model
		gpu.direct_tried = true;
		fprintf(stderr, "neuswc: compositing with GL on %s, read back into the framebuffer\n",
		        wld_gl_renderer_name(renderer));
		return renderer;
	}
	if (gpu.mode == GPU_AUTO && !framebuffer_gpu_scanout_available()) {
		fprintf(stderr, "neuswc: the framebuffer cannot scan out GPU resources, compositing on the CPU\n");
		return NULL;
	}
	// no GPU display and no render node means no GPU driver came up, so mesa is not worth loading
	if (!framebuffer_gpu_scanout_available() && access("/dev/dri/renderD128", F_OK) != 0) {
		fprintf(stderr, "neuswc: no GPU driver (no /dev/dri, no GPU scanout), compositing on the CPU\n");
		return NULL;
	}
	fprintf(stderr, "neuswc: the framebuffer can scan out GPU resources, starting GL\n");
	renderer = wld_gl_create_renderer();
	if (!renderer) {
		fprintf(stderr, "neuswc: no GL renderer, compositing on the CPU\n");
		return NULL;
	}
	name = wld_gl_renderer_name(renderer);
	if (strstr(name, "llvmpipe") || strstr(name, "softpipe") || strstr(name, "Software") ||
	    strstr(name, "lavapipe")) {
		fprintf(stderr, "neuswc: GL is %s, a software rasteriser, compositing on the CPU\n", name);
		wld_destroy_renderer(renderer);
		return NULL;
	}
	fprintf(stderr, "neuswc: compositing with GL on %s\n", name);
	return renderer;
}

// the GPU path failed: back to pixman, the scanout back to the framebuffer, and one whole frame
static void
gpu_fall_back(const char *why)
{
	struct wld_renderer *pixman = wld_create_renderer(swc_fb.backend.context);

	fprintf(stderr, "neuswc: GL compositing failed (%s), back to the CPU\n", why);
	if (!pixman)
		return;
	if (gpu.direct)
		framebuffer_set_scanout_resource(0, 0, 0);
	if (gpu.flip)
		pixman_region32_fini(&gpu.last_damage);
	gpu.flip = false;
	wld_destroy_renderer(swc_fb.backend.renderer);
	swc_fb.backend.renderer = pixman;
	gpu.active = gpu.direct = false;
	compositor_damage_all();
}
#endif

bool
fb_initialize(void)
{
	if (!framebuffer_initialize(&swc_fb)) {
		return false;
	}
	swc_fb.pitch = (size_t)swc_fb.width * sizeof(*swc_fb.pixels);
	swc_fb.pixels = calloc(swc_fb.height, swc_fb.pitch);
	if (!swc_fb.pixels) {
		goto error0;
	}
	swc_fb.backend.context = wld_pixman_create_context();
	if (!swc_fb.backend.context) {
		goto error1;
	}
	#ifdef __PUREDARWIN__
	// a compositor killed while scanning out a GL resource leaves the display showing it
	if (framebuffer_gpu_scanout_available())
		framebuffer_set_scanout_resource(0, 0, 0);
	gpu.mode = gpu_mode();
	if (gpu.mode != GPU_OFF) {
		swc_fb.backend.renderer = gpu_renderer();
		if (swc_fb.backend.renderer)
			gpu.active = true;
	}
	if (!swc_fb.backend.renderer) {
		// the GL attempt already said why it fell back
		if (gpu.mode == GPU_OFF)
			fprintf(stderr, "neuswc: compositing on the CPU\n");
	#endif
		swc_fb.backend.renderer = wld_create_renderer(swc_fb.backend.context);
	#ifdef __PUREDARWIN__
	}
	#endif
	if (!swc_fb.backend.renderer) {
		goto error2;
	}
	swc_fb.backend.cursor_width = 64;
	swc_fb.backend.cursor_height = 64;
	swc.backend = &swc_fb.backend;
	return true;

error2:
	wld_destroy_context(swc_fb.backend.context);
error1:
	free(swc_fb.pixels);
error0:
	framebuffer_finalize(&swc_fb);
	return false;
}

void
fb_finalize(void)
{
	wld_destroy_renderer(swc_fb.backend.renderer);
	wld_destroy_context(swc_fb.backend.context);
	free(swc_fb.pixels);
	framebuffer_finalize(&swc_fb);
}

bool
fb_create_screens(struct wl_list *screens)
{
	struct fb_mode found[64];
	struct mode modes[64];
	struct output *output;
	struct screen *screen;
	int count = framebuffer_modes(found, 64), current = 0;

	// every mode the framebuffer offers, the one it is in now as the preferred
	for (int i = 0; i < count; i++) {
		mode_initialize_simple(&modes[i], found[i].width, found[i].height, 60000);
		modes[i].fb_id = found[i].id;
		if (found[i].current || (found[i].width == swc_fb.width && found[i].height == swc_fb.height))
			current = i;
	}
	if (count == 0) {
		mode_initialize_simple(&modes[0], swc_fb.width, swc_fb.height, 60000);
		count = 1;
	}
	output = output_new_fb_modes(modes, count, current, framebuffer_name());
	if (!output) {
		return false;
	}
	screen = screen_new(output);
	if (!screen) {
		output_destroy(output);
		return false;
	}
	screen->id = 0;
	output->screen = screen;
	wl_list_insert(screens, &screen->link);
	return true;
}

// the backend switches first, then the composition buffer follows its new size
bool
fb_set_mode(uint32_t mode_id)
{
	uint32_t *pixels;

	if (!framebuffer_set_mode(&swc_fb, mode_id))
		return false;
	pixels = calloc(swc_fb.height, (size_t)swc_fb.width * sizeof(*pixels));
	if (!pixels)
		return false;
	free(swc_fb.pixels);
	swc_fb.pixels = pixels;
	swc_fb.pitch = (size_t)swc_fb.width * sizeof(*swc_fb.pixels);
	return true;
}

// ARGON_GFX_STATS: presents, pixels copied to the scanout copy and time per present, every 5 s
static struct {
	int on;
	unsigned long presents;
	unsigned long long pixels, nanos;
	struct timespec since;
	struct rusage usage;
	struct timespec frame_cpu;
	unsigned long long frame_cpu_nanos;
} fb_stats = { -1 };

static unsigned long long
thread_cpu_nanos(void)
{
	struct timespec t;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
	return (unsigned long long)t.tv_sec * 1000000000ull + t.tv_nsec;
}

void
fb_frame_begin(void)
{
	if (fb_stats.on)
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &fb_stats.frame_cpu);
}

static double
cpu_us(const struct rusage *u)
{
	return (double)(u->ru_utime.tv_sec + u->ru_stime.tv_sec) * 1e6 + u->ru_utime.tv_usec + u->ru_stime.tv_usec;
}

static void
fb_stats_add(unsigned long long pixels, const struct timespec *start)
{
	struct timespec now;

	if (fb_stats.on < 0) {
		fb_stats.on = getenv("ARGON_GFX_STATS") != NULL;
		clock_gettime(CLOCK_MONOTONIC, &fb_stats.since);
		getrusage(RUSAGE_SELF, &fb_stats.usage);
	}
	if (!fb_stats.on)
		return;
	clock_gettime(CLOCK_MONOTONIC, &now);
	fb_stats.presents++;
	fb_stats.frame_cpu_nanos += thread_cpu_nanos() -
	    ((unsigned long long)fb_stats.frame_cpu.tv_sec * 1000000000ull + fb_stats.frame_cpu.tv_nsec);
	fb_stats.pixels += pixels;
	fb_stats.nanos += (unsigned long long)(now.tv_sec - start->tv_sec) * 1000000000ull + (now.tv_nsec - start->tv_nsec);
	if (now.tv_sec - fb_stats.since.tv_sec < 5)
		return;
	struct rusage usage;
	getrusage(RUSAGE_SELF, &usage);
	// frame cpu is the compositor thread from repaint to presented, process cpu all of neuswc over the window
#ifdef __PUREDARWIN__
	const char *path = gpu.flip ? "gles-flip" : gpu.active ? "gles" : "cpu";
#else
	const char *path = "cpu";
#endif
	fprintf(stderr, "ARGON-FB %s presents %lu copied %llu Kpx present %.1f us avg frame cpu %.1f us process cpu %.1f us per frame\n",
	        path, fb_stats.presents, fb_stats.pixels / 1000,
	        fb_stats.presents ? (double)fb_stats.nanos / 1000.0 / (double)fb_stats.presents : 0.0,
	        fb_stats.presents ? (double)fb_stats.frame_cpu_nanos / 1000.0 / (double)fb_stats.presents : 0.0,
	        fb_stats.presents ? (cpu_us(&usage) - cpu_us(&fb_stats.usage)) / (double)fb_stats.presents : 0.0);
	fb_stats.usage = usage;
	fb_stats.presents = 0;
	fb_stats.pixels = fb_stats.nanos = fb_stats.frame_cpu_nanos = 0;
	fb_stats.since = now;
}

#ifdef __PUREDARWIN__
// the display's flip pair imported as GL render targets, so frames are composited straight into scanout memory
static void
gpu_try_flip_pair(void)
{
	int fds[2], count;
	uint32_t width, height, pitch;

	count = framebuffer_scanout_dmabufs(fds, &width, &height, &pitch);
	if (count != 2) {
		fprintf(stderr, "neuswc: the framebuffer did not export its flip pair\n");
		return;
	}
	gpu.flip = width == swc_fb.width && height == swc_fb.height &&
	           wld_gl_set_scanout_dmabufs(swc_fb.backend.renderer, fds, count, width, height, pitch);
	close(fds[0]);
	close(fds[1]);
	if (!gpu.flip) {
		fprintf(stderr, "neuswc: GL could not import the flip pair (%ux%u pitch %u)\n", width, height, pitch);
		return;
	}
	gpu.direct_tried = true;
	pixman_region32_init(&gpu.last_damage);
	fprintf(stderr, "neuswc: compositing straight into the display's flip pair\n");
}

// the back buffer is two frames old, so it takes this frame's damage and the last one's
static bool
gpu_present_flip(struct wld_buffer *buffer, struct wld_buffer *cursor, int32_t cursor_x, int32_t cursor_y,
                 pixman_region32_t *damage, bool whole, const struct timespec *start)
{
	int back = framebuffer_back_index();
	pixman_region32_t both;
	enum wld_gl_present_result result;
	bool all = whole || buffer->width != swc_fb.width || buffer->height != swc_fb.height;

	if (back < 0) {
		fprintf(stderr, "neuswc: no back buffer to composite into\n");
		return false;
	}
	pixman_region32_init(&both);
	if (all) {
		pixman_region32_union_rect(&both, &both, 0, 0, buffer->width, buffer->height);
		pixman_region32_copy(&gpu.last_damage, &both);
	} else {
		pixman_region32_union(&both, damage, &gpu.last_damage);
		pixman_region32_copy(&gpu.last_damage, damage);
	}
	if (!pixman_region32_not_empty(&both)) {
		pixman_region32_fini(&both);
		fb_stats_add(0, start);
		return true;
	}
	result = wld_gl_present_scanout(swc_fb.backend.renderer, buffer, cursor, cursor_x, cursor_y,
	                                all ? NULL : &both, back);
	const pixman_box32_t *e = pixman_region32_extents(&both);
	unsigned long long pixels = (unsigned long long)(e->x2 - e->x1) * (e->y2 - e->y1);
	pixman_region32_fini(&both);
	if (result == WLD_GL_PRESENT_FAILED)
		return false;
	if (!framebuffer_flip(back)) {
		fprintf(stderr, "neuswc: the presenter refused a flip to buffer %d\n", back);
		return false;
	}
	fb_stats_add(pixels, start);
	return true;
}

// composite on the GPU: the frame's damage and the cursor's old and new places go to the scanout
// copy, then either the display shows that GPU resource directly or the damaged rows are read back
static bool
gpu_present(struct wld_buffer *buffer, struct wld_buffer *cursor, int32_t cursor_x, int32_t cursor_y,
            const struct timespec *start)
{
	pixman_region32_t damage;
	bool whole = !fb_damage.valid || buffer->width != fb_damage.last_width || buffer->height != fb_damage.last_height;
	enum wld_gl_present_result result;

	pixman_region32_init(&damage);
	if (whole)
		pixman_region32_init_rect(&damage, 0, 0, buffer->width, buffer->height);
	else {
		pixman_region32_copy(&damage, &fb_damage.region);
		if (fb_damage.cursor_shown)
			pixman_region32_union_rect(&damage, &damage, fb_damage.cursor_x, fb_damage.cursor_y,
			                           fb_damage.cursor_w, fb_damage.cursor_h);
		if (cursor)
			pixman_region32_union_rect(&damage, &damage, cursor_x, cursor_y, cursor->width, cursor->height);
		pixman_region32_intersect_rect(&damage, &damage, 0, 0, buffer->width, buffer->height);
	}
	fb_damage.valid = false;
	fb_damage.last_width = buffer->width;
	fb_damage.last_height = buffer->height;
	fb_damage.cursor_shown = cursor != NULL;
	if (cursor) {
		fb_damage.cursor_x = cursor_x, fb_damage.cursor_y = cursor_y;
		fb_damage.cursor_w = cursor->width, fb_damage.cursor_h = cursor->height;
	}

	// ARGON_GFX_TRACE: each GL present's damage and outcome
	static int trace = -1;
	if (trace < 0)
		trace = getenv("ARGON_GFX_TRACE") != NULL;
	if (trace) {
		const pixman_box32_t *e = pixman_region32_extents(&damage);
		fprintf(stderr, "ARGON-GL present %ux%u damage %d,%d-%d,%d%s\n", buffer->width, buffer->height,
		        e->x1, e->y1, e->x2, e->y2, whole ? " whole" : "");
	}

	if (!gpu.direct_tried && !getenv("ARGON_GFX_READBACK"))
		gpu_try_flip_pair();
	if (gpu.flip) {
		bool ok = gpu_present_flip(buffer, cursor, cursor_x, cursor_y, &damage, whole, start);
		pixman_region32_fini(&damage);
		if (!ok)
			gpu_fall_back("flip");
		return true;
	}

	// direct scanout needs the resource id from the driver, the first present finds out
	bool read_back = !gpu.direct && gpu.direct_tried;
	if (read_back && buffer->width == swc_fb.width && buffer->height == swc_fb.height)
		result = wld_gl_present_damage(swc_fb.backend.renderer, buffer, cursor, cursor_x, cursor_y, whole ? NULL : &damage,
		                               swc_fb.pixels, swc_fb.pitch);
	else
		result = wld_gl_present_damage(swc_fb.backend.renderer, buffer, cursor, cursor_x, cursor_y, whole ? NULL : &damage,
		                               NULL, 0);
	if (trace)
		fprintf(stderr, "ARGON-GL rendered, result %d\n", result);
	if (result == WLD_GL_PRESENT_FAILED) {
		pixman_region32_fini(&damage);
		gpu_fall_back("present");
		return true;
	}

	if (!gpu.direct_tried) {
		// ARGON_GFX_READBACK=1 skips direct scanout, to test reading the damage back
		uint32_t resource = getenv("ARGON_GFX_READBACK") ? 0 : wld_gl_scanout_resource(swc_fb.backend.renderer);

		gpu.direct_tried = true;
		gpu.direct = resource && framebuffer_set_scanout_resource(resource, buffer->width, buffer->height);
		fprintf(stderr, "neuswc: %s\n", gpu.direct ? "scanning out the GL composite directly"
		                                          : "no direct scanout, reading the damage back");
		if (!gpu.direct) {
			// this frame went only to the GPU, so read the whole of the next one back
			fb_damage.last_width = 0;
			compositor_damage_all();
		}
	}

	pixman_box32_t *extents = pixman_region32_extents(&damage);
	bool any = pixman_region32_not_empty(&damage);
	bool ok = true;

	if (result == WLD_GL_PRESENT_ON_GPU && gpu.direct && any)
		ok = framebuffer_present_rect(extents->x1, extents->y1, extents->x2 - extents->x1, extents->y2 - extents->y1);
	else if (result == WLD_GL_PRESENT_READ_BACK && any) {
		swc_fb.damage_y1 = extents->y1;
		swc_fb.damage_y2 = extents->y2;
		ok = framebuffer_present(&swc_fb);
	}
	fb_stats_add(any ? (unsigned long long)(extents->x2 - extents->x1) * (extents->y2 - extents->y1) : 0, start);
	pixman_region32_fini(&damage);
	if (!ok)
		gpu_fall_back("scanout");
	return true;
}
#endif

bool
fb_present(struct wld_buffer *buffer, int32_t origin_x, int32_t origin_y)
{
	struct timespec present_start;
	clock_gettime(CLOCK_MONOTONIC, &present_start);
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;
	struct wld_buffer *cursor = NULL;
	int32_t cursor_x = 0, cursor_y = 0;
	uint32_t width, height, x, y;
	bool cursor_mapped = false;

	#ifdef __PUREDARWIN__
	if (gpu.active) {
		if (!buffer) return false;
		if (pointer && pointer->cursor.view.buffer) {
			cursor = pointer->cursor.buffer;
			cursor_x = pointer->cursor.view.geometry.x - origin_x;
			cursor_y = pointer->cursor.view.geometry.y - origin_y;
		}
		return gpu_present(buffer, cursor, cursor_x, cursor_y, &present_start);
	}
	#endif

	if (!buffer || !wld_map(buffer)) {
		return false;
	}
	if (pointer && pointer->cursor.view.buffer) {
		cursor = pointer->cursor.buffer;
		cursor_x = pointer->cursor.view.geometry.x - origin_x;
		cursor_y = pointer->cursor.view.geometry.y - origin_y;
		cursor_mapped = wld_map(cursor);
	}

	width = buffer->width < swc_fb.width ? buffer->width : swc_fb.width;
	height = buffer->height < swc_fb.height ? buffer->height : swc_fb.height;
	if (width != swc_fb.width || height != swc_fb.height) {
		memset(swc_fb.pixels, 0, swc_fb.height * swc_fb.pitch);
	}

	// only the frame's damage and where the cursor was and is now, when the screen is otherwise unchanged
	pixman_region32_t copy;
	bool partial = fb_damage.valid && getenv("SWC_FULL_PRESENT") == NULL &&
	               width == fb_damage.last_width && height == fb_damage.last_height &&
	               width == swc_fb.width && height == swc_fb.height;

	pixman_region32_init(&copy);
	if (partial) {
		pixman_region32_copy(&copy, &fb_damage.region);
		if (fb_damage.cursor_shown)
			pixman_region32_union_rect(&copy, &copy, fb_damage.cursor_x, fb_damage.cursor_y,
			                           fb_damage.cursor_w, fb_damage.cursor_h);
		if (cursor_mapped)
			pixman_region32_union_rect(&copy, &copy, cursor_x, cursor_y, cursor->width, cursor->height);
		pixman_region32_intersect_rect(&copy, &copy, 0, 0, width, height);
	} else {
		pixman_region32_init_rect(&copy, 0, 0, width, height);
	}
	fb_damage.valid = false;
	fb_damage.last_width = width;
	fb_damage.last_height = height;
	fb_damage.cursor_shown = cursor_mapped;
	if (cursor_mapped) {
		fb_damage.cursor_x = cursor_x, fb_damage.cursor_y = cursor_y;
		fb_damage.cursor_w = cursor->width, fb_damage.cursor_h = cursor->height;
	}

	int nboxes;
	pixman_box32_t *boxes = pixman_region32_rectangles(&copy, &nboxes);
	pixman_box32_t *extents = pixman_region32_extents(&copy);

	swc_fb.damage_y1 = nboxes ? (uint32_t)extents->y1 : 0;
	swc_fb.damage_y2 = nboxes ? (uint32_t)extents->y2 : 0;
	if (!partial)
		swc_fb.damage_y1 = swc_fb.damage_y2 = 0;
	for (int i = 0; i < nboxes; i++)
	for (y = (uint32_t)boxes[i].y1; y < (uint32_t)boxes[i].y2; ++y) {
		uint32_t *source = (uint32_t *)((uint8_t *)buffer->map +
		                                (size_t)y * buffer->pitch);
		uint32_t *destination = (uint32_t *)((uint8_t *)swc_fb.pixels +
		                                     (size_t)y * swc_fb.pitch);
		for (x = (uint32_t)boxes[i].x1; x < (uint32_t)boxes[i].x2; ++x) {
			uint32_t pixel = source[x];
			int32_t cx = (int32_t)x - cursor_x;
			int32_t cy = (int32_t)y - cursor_y;
			if (cursor_mapped && cx >= 0 && cy >= 0 &&
			    (uint32_t)cx < cursor->width && (uint32_t)cy < cursor->height) {
				uint32_t foreground = *(uint32_t *)((uint8_t *)cursor->map +
				    (size_t)cy * cursor->pitch + (size_t)cx * 4);
				pixel = blend(pixel, foreground);
			}
			destination[x] = pixel;
		}
	}
	unsigned long long copied = 0;
	for (int i = 0; i < nboxes; i++)
		copied += (unsigned long long)(boxes[i].x2 - boxes[i].x1) * (unsigned long long)(boxes[i].y2 - boxes[i].y1);
	pixman_region32_fini(&copy);
	if (cursor_mapped) {
		wld_unmap(cursor);
	}
	wld_unmap(buffer);
	bool ok = (partial && nboxes == 0) || framebuffer_present(&swc_fb);
	fb_stats_add(copied, &present_start);
	return ok;
}
