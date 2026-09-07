/* swc: drm.c
 *
 * Copyright (c) 2013-2020 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "drm.h"
#include "dmabuf.h"
#include "event.h"
#include "internal.h"
#include "launch.h"
#include "output.h"
#include "plane.h"
#include "screen.h"
#include "util.h"
#include "wayland_buffer.h"

#include "wayland-drm-server-protocol.h"
#include <dirent.h>
#include <drm.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wld/drm.h>
#include <wld/wld.h>
#include <xf86drm.h>

struct swc_drm swc_drm;

static struct {
	char *path;

	struct wl_global *global;
	struct wl_global *dmabuf;
	struct wl_event_source *event_source;
} drm;

static void
authenticate(struct wl_client *client, struct wl_resource *resource,
             uint32_t magic)
{
	wl_drm_send_authenticated(resource);
}

static void
create_buffer(struct wl_client *client, struct wl_resource *resource,
              uint32_t id, uint32_t name, int32_t width, int32_t height,
              uint32_t stride, uint32_t format)
{
	wl_resource_post_error(
	    resource, WL_DRM_ERROR_INVALID_NAME,
	    "GEM names are not supported, use a PRIME fd instead");
}

static void
create_planar_buffer(struct wl_client *client, struct wl_resource *resource,
                     uint32_t id, uint32_t name, int32_t width, int32_t height,
                     uint32_t format, int32_t offset0, int32_t stride0,
                     int32_t offset1, int32_t stride1, int32_t offset2,
                     int32_t stride2)
{
	wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_FORMAT,
	                       "planar buffers are not supported\n");
}

static void
create_prime_buffer(struct wl_client *client, struct wl_resource *resource,
                    uint32_t id, int32_t fd, int32_t width, int32_t height,
                    uint32_t format, int32_t offset0, int32_t stride0,
                    int32_t offset1, int32_t stride1, int32_t offset2,
                    int32_t stride2)
{
	struct wld_buffer *buffer;
	struct wl_resource *buffer_resource;
	union wld_object object = {.i = fd};

	buffer = wld_import_buffer(swc.drm->context, WLD_DRM_OBJECT_PRIME_FD,
	                           object, width, height, format, stride0);
	close(fd);

	if (!buffer) {
		goto error0;
	}

	buffer_resource = wayland_buffer_create_resource(
	    client, wl_resource_get_version(resource), id, buffer);

	if (!buffer_resource) {
		goto error1;
	}

	return;

error1:
	wld_buffer_unreference(buffer);
error0:
	wl_resource_post_no_memory(resource);
}

static const struct wl_drm_interface drm_impl = {
    .authenticate = authenticate,
    .create_buffer = create_buffer,
    .create_planar_buffer = create_planar_buffer,
    .create_prime_buffer = create_prime_buffer,
};

static int
select_card(const struct dirent *entry)
{
	unsigned num;
	return sscanf(entry->d_name, "card%u", &num) == 1;
}

static bool
find_primary_drm_device(char *path, size_t size)
{
	struct dirent **cards, *card = NULL;
	int num_cards, ret;
	unsigned index;
	FILE *file;
	unsigned char boot_vga;

	num_cards = scandir("/dev/dri", &cards, &select_card, &alphasort);

	if (num_cards == -1) {
		return false;
	}

	for (index = 0; index < num_cards; ++index) {
		snprintf(path, size, "/sys/class/drm/%s/device/boot_vga",
		         cards[index]->d_name);

		if ((file = fopen(path, "r"))) {
			ret = fscanf(file, "%hhu", &boot_vga);
			fclose(file);

			if (ret == 1 && boot_vga) {
				free(card);
				card = cards[index];
				DEBUG("/dev/dri/%s is the primary GPU\n", card->d_name);
				break;
			}
		}

		if (!card) {
			card = cards[index];
		} else {
			free(cards[index]);
		}
	}

	free(cards);

	if (!card) {
		return false;
	}

	if (snprintf(path, size, "/dev/dri/%s", card->d_name) >= size) {
		return false;
	}

	free(card);
	return true;
}

static bool
find_available_crtc(drmModeRes *resources, drmModeConnector *connector,
                    uint32_t taken_crtcs, int *crtc_index)
{
	int i, j;
	uint32_t possible_crtcs;
	drmModeEncoder *encoder;

	for (i = 0; i < connector->count_encoders; ++i) {
		encoder = drmModeGetEncoder(swc.drm->fd, connector->encoders[i]);
		if (!encoder) {
			continue;
		}
		possible_crtcs = encoder->possible_crtcs;
		drmModeFreeEncoder(encoder);

		for (j = 0; j < resources->count_crtcs; ++j) {
			if ((possible_crtcs & (1 << j)) && !(taken_crtcs & (1 << j))) {
				*crtc_index = j;
				return true;
			}
		}
	}

	return false;
}

static void
handle_vblank(int fd, unsigned int sequence, unsigned int sec,
              unsigned int usec, void *data)
{
}

static void
handle_page_flip(int fd, unsigned int sequence, unsigned int sec,
                 unsigned int usec, unsigned int crtc_id, void *data)
{
	struct drm_handler *handler = data;

	handler->page_flip(handler, sec * 1000 + usec / 1000);
}

static drmEventContext event_context = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .vblank_handler = handle_vblank,
    .page_flip_handler2 = handle_page_flip,
};

static int
handle_data(int fd, uint32_t mask, void *data)
{
	drmHandleEvent(fd, &event_context);
	return 1;
}

static void
bind_drm(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &wl_drm_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &drm_impl, NULL, NULL);

	if (version >= 2) {
		wl_drm_send_capabilities(resource, WL_DRM_CAPABILITY_PRIME);
	}

	wl_drm_send_device(resource, drm.path);
	wl_drm_send_format(resource, WL_DRM_FORMAT_XRGB8888);
	wl_drm_send_format(resource, WL_DRM_FORMAT_ARGB8888);
}

bool
drm_initialize(void)
{
	uint64_t val;
	char primary[PATH_MAX];

	if (!find_primary_drm_device(primary, sizeof(primary))) {
		ERROR("Could not find DRM device\n");
		goto error0;
	}

	swc.drm->fd = launch_open_device(primary, O_RDWR | O_CLOEXEC);
	if (swc.drm->fd == -1) {
		ERROR("Could not open DRM device at %s\n", primary);
		goto error0;
	}
	if (drmSetClientCap(swc.drm->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0) {
		ERROR("Could not enable DRM universal planes\n");
		goto error1;
	}
	if (drmGetCap(swc.drm->fd, DRM_CAP_CURSOR_WIDTH, &val) < 0) {
		val = 64;
	}
	swc.drm->cursor_w = val;
	if (drmGetCap(swc.drm->fd, DRM_CAP_CURSOR_HEIGHT, &val) < 0) {
		val = 64;
	}
	swc.drm->cursor_h = val;
	swc.backend = &swc.drm->backend;
	swc.backend->cursor_width = swc.drm->cursor_w;
	swc.backend->cursor_height = swc.drm->cursor_h;

	drm.path = drmGetRenderDeviceNameFromFd(swc.drm->fd);
	if (!drm.path) {
		ERROR("Could not determine render node path\n");
		goto error1;
	}

	if (!(swc.drm->context = wld_drm_create_context(swc.drm->fd))) {
		ERROR("Could not create WLD DRM context\n");
		goto error1;
	}

	if (!(swc.drm->renderer = wld_create_renderer(swc.drm->context))) {
		ERROR("Could not create WLD DRM renderer\n");
		goto error2;
	}
	swc.backend->context = swc.drm->context;
	swc.backend->renderer = swc.drm->renderer;

	drm.event_source = wl_event_loop_add_fd(
	    swc.event_loop, swc.drm->fd, WL_EVENT_READABLE, &handle_data, NULL);

	if (!drm.event_source) {
		ERROR("Could not create DRM event source\n");
		goto error3;
	}

	if (!wld_drm_is_dumb(swc.drm->context)) {
		drm.global = wl_global_create(swc.display, &wl_drm_interface, 2, NULL,
		                              &bind_drm);
		if (!drm.global) {
			ERROR("Could not create wl_drm global\n");
			goto error4;
		}

		drm.dmabuf = swc_dmabuf_create(swc.display);
		if (!drm.dmabuf) {
			WARNING("Could not create wp_linux_dmabuf global\n");
		}
	}

	return true;

error4:
	wl_event_source_remove(drm.event_source);
error3:
	wld_destroy_renderer(swc.drm->renderer);
error2:
	wld_destroy_context(swc.drm->context);
error1:
	close(swc.drm->fd);
error0:
	return false;
}

void
drm_finalize(void)
{
	if (drm.global) {
		wl_global_destroy(drm.global);
	}
	wl_event_source_remove(drm.event_source);
	wld_destroy_renderer(swc.drm->renderer);
	wld_destroy_context(swc.drm->context);
	free(drm.path);
	close(swc.drm->fd);
}

bool
drm_create_screens(struct wl_list *screens)
{
	drmModePlaneRes *plane_ids;
	drmModeRes *resources;
	drmModeConnector *connector;
	struct plane *plane, *cursor_plane;
	struct output *output;
	uint32_t i, taken_crtcs = 0;
	struct wl_list planes;

	plane_ids = drmModeGetPlaneResources(swc.drm->fd);
	if (!plane_ids) {
		ERROR("Could not get DRM plane resources\n");
		return false;
	}
	wl_list_init(&planes);
	for (i = 0; i < plane_ids->count_planes; ++i) {
		plane = plane_new(plane_ids->planes[i]);
		if (plane) {
			wl_list_insert(&planes, &plane->link);
		}
	}
	drmModeFreePlaneResources(plane_ids);

	resources = drmModeGetResources(swc.drm->fd);
	if (!resources) {
		struct plane *p, *ptmp;

		wl_list_for_each_safe(p, ptmp, &planes, link)
			plane_destroy(p);
		ERROR("Could not get DRM resources\n");
		return false;
	}
	for (i = 0; i < resources->count_connectors;
	     ++i, drmModeFreeConnector(connector)) {
		connector = drmModeGetConnector(swc.drm->fd, resources->connectors[i]);

		if (connector->connection == DRM_MODE_CONNECTED) {
			int crtc_index;

			if (!find_available_crtc(resources, connector, taken_crtcs,
			                         &crtc_index)) {
				WARNING("Could not find CRTC for connector %d\n", i);
				continue;
			}

			cursor_plane = NULL;
			wl_list_for_each(plane, &planes, link)
			{
				if (plane->type == DRM_PLANE_TYPE_CURSOR &&
				    plane->possible_crtcs & 1 << crtc_index) {
					wl_list_remove(&plane->link);
					cursor_plane = plane;
					break;
				}
			}
			if (!cursor_plane) {
				WARNING("Could not find cursor plane for CRTC %d\n",
				        crtc_index);
			}

			if (!(output = output_new(connector))) {
				continue;
			}

			output->screen =
			    screen_new(resources->crtcs[crtc_index], output, cursor_plane);
			output->screen->id = crtc_index;
			taken_crtcs |= 1 << crtc_index;

			wl_list_insert(screens, &output->screen->link);
		}
	}
	drmModeFreeResources(resources);

	return true;
}

enum { WLD_USER_OBJECT_FRAMEBUFFER = WLD_USER_ID };

struct framebuffer {
	struct wld_exporter exporter;
	struct wld_destructor destructor;
	uint32_t id;
};

static bool
framebuffer_export(struct wld_exporter *exporter, struct wld_buffer *buffer,
                   uint32_t type, union wld_object *object)
{
	struct framebuffer *framebuffer =
	    wl_container_of(exporter, framebuffer, exporter);

	switch (type) {
	case WLD_USER_OBJECT_FRAMEBUFFER:
		object->u32 = framebuffer->id;
		break;
	default:
		return false;
	}

	return true;
}

static void
framebuffer_destroy(struct wld_destructor *destructor)
{
	struct framebuffer *framebuffer =
	    wl_container_of(destructor, framebuffer, destructor);

	drmModeRmFB(swc.drm->fd, framebuffer->id);
	free(framebuffer);
}

uint32_t
drm_get_framebuffer(struct wld_buffer *buffer)
{
	struct framebuffer *framebuffer;
	union wld_object object;
	int ret;

	if (!buffer) {
		return 0;
	}

	if (wld_export(buffer, WLD_USER_OBJECT_FRAMEBUFFER, &object)) {
		return object.u32;
	}

	if (!wld_export(buffer, WLD_DRM_OBJECT_HANDLE, &object)) {
		ERROR("Could not get buffer handle\n");
		return 0;
	}

	if (!(framebuffer = malloc(sizeof(*framebuffer)))) {
		return 0;
	}

	ret = drmModeAddFB2(swc.drm->fd, buffer->width, buffer->height,
	                    buffer->format, (uint32_t[4]){object.u32},
	                    (uint32_t[4]){buffer->pitch}, (uint32_t[4]){0},
	                    &framebuffer->id, 0);
	if (ret < 0) {
		free(framebuffer);
		return 0;
	}

	framebuffer->exporter.export = &framebuffer_export;
	wld_buffer_add_exporter(buffer, &framebuffer->exporter);
	framebuffer->destructor.destroy = &framebuffer_destroy;
	wld_buffer_add_destructor(buffer, &framebuffer->destructor);

	return framebuffer->id;
}
