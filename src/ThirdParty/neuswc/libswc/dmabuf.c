/* swc: dmabuf.c
 *
 * Copyright (c) 2019 Michael Forney
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

#include "dmabuf.h"
#include "drm.h"
#include "internal.h"
#include "util.h"
#include "wayland_buffer.h"

#include "linux-dmabuf-unstable-v1-server-protocol.h"
#include <drm_fourcc.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <wld/drm.h>
#include <wld/wld.h>

struct params {
	struct wl_resource *resource;
	int fd[4];
	uint32_t offset[4];
	uint32_t stride[4];
	uint64_t modifier[4];
	bool created;
};

static void
add(struct wl_client *client, struct wl_resource *resource, int32_t fd,
    uint32_t i, uint32_t offset, uint32_t stride, uint32_t modifier_hi,
    uint32_t modifier_lo)
{
	struct params *params = wl_resource_get_user_data(resource);

	if (params->created) {
		wl_resource_post_error(resource,
		                       ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
		                       "buffer already created");
		return;
	}
	if (i > ARRAY_LENGTH(params->fd)) {
		wl_resource_post_error(resource,
		                       ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
		                       "plane index too large");
		return;
	}
	if (params->fd[i] != -1) {
		wl_resource_post_error(resource,
		                       ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
		                       "buffer plane already set");
		return;
	}
	params->fd[i] = fd;
	params->offset[i] = offset;
	params->stride[i] = stride;
	params->modifier[i] = (uint64_t)modifier_hi << 32 | modifier_lo;
}

static void
create_immed(struct wl_client *client, struct wl_resource *resource,
             uint32_t id, int32_t width, int32_t height, uint32_t format,
             uint32_t flags)
{
	struct params *params = wl_resource_get_user_data(resource);
	struct wld_buffer *buffer;
	struct wl_resource *buffer_resource;
	union wld_object object;
	int num_planes, i;

	if (params->created) {
		wl_resource_post_error(resource,
		                       ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
		                       "buffer already created");
		return;
	}
	params->created = true;
	switch (format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		num_planes = 1;
		break;
	default:
		wl_resource_post_error(resource,
		                       ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
		                       "unsupported format %#" PRIx32, format);
		return;
	}
	for (i = 0; i < num_planes; ++i) {
		if (params->fd[i] == -1) {
			wl_resource_post_error(resource,
			                       ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
			                       "missing plane %d", i);
		}
	}
	for (; i < ARRAY_LENGTH(params->fd); ++i) {
		if (params->fd[i] != -1) {
			wl_resource_post_error(resource,
			                       ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
			                       "too many planes");
		}
	}
	object.i = params->fd[0];
	buffer =
	    wld_import_buffer(swc.drm->context, WLD_DRM_OBJECT_PRIME_FD, object,
	                      width, height, format, params->stride[0]);
	for (i = 0; i < num_planes; ++i) {
		close(params->fd[i]);
		params->fd[i] = -1;
	}
	if (!buffer) {
		zwp_linux_buffer_params_v1_send_failed(resource);
	}

	buffer_resource = wayland_buffer_create_resource(client, 1, id, buffer);
	if (!buffer_resource) {
		if (buffer) {
			wld_buffer_unreference(buffer);
		}
		wl_resource_post_no_memory(resource);
		return;
	}
	if (id == 0 && buffer) {
		zwp_linux_buffer_params_v1_send_created(resource, buffer_resource);
	}
}

static void
create(struct wl_client *client, struct wl_resource *resource, int32_t width,
       int32_t height, uint32_t format, uint32_t flags)
{
	create_immed(client, resource, 0, width, height, format, flags);
}

static const struct zwp_linux_buffer_params_v1_interface params_impl = {
    .destroy = destroy_resource,
    .add = add,
    .create = create,
    .create_immed = create_immed,
};

static void
params_destroy(struct wl_resource *resource)
{
	struct params *params = wl_resource_get_user_data(resource);
	int i;

	for (i = 0; i < ARRAY_LENGTH(params->fd); ++i) {
		close(params->fd[i]);
	}
}

static void
create_params(struct wl_client *client, struct wl_resource *resource,
              uint32_t id)
{
	struct params *params;
	int i;

	params = malloc(sizeof(*params));
	if (!params) {
		goto error0;
	}
	params->created = false;
	params->resource =
	    wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
	                       wl_resource_get_version(resource), id);
	if (!params->resource) {
		goto error1;
	}
	for (i = 0; i < ARRAY_LENGTH(params->fd); ++i) {
		params->fd[i] = -1;
	}
	wl_resource_set_implementation(params->resource, &params_impl, params,
	                               params_destroy);
	return;

error1:
	free(params);
error0:
	wl_resource_post_no_memory(resource);
}

static const struct zwp_linux_dmabuf_v1_interface dmabuf_impl = {
    .destroy = destroy_resource,
    .create_params = create_params,
};

static void
bind_dmabuf(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	static const uint32_t formats[] = {
	    DRM_FORMAT_XRGB8888,
	    DRM_FORMAT_ARGB8888,
	};
	/*it appears we can only handle linear*/
	uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
	struct wl_resource *resource;
	size_t i;

	resource =
	    wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &dmabuf_impl, NULL, NULL);
	for (i = 0; i < ARRAY_LENGTH(formats); ++i) {
		if (version >= 3) {
			zwp_linux_dmabuf_v1_send_modifier(
			    resource, formats[i], modifier >> 32, modifier & 0xffffffff);
		} else {
			zwp_linux_dmabuf_v1_send_format(resource, formats[i]);
		}
	}
}

struct wl_global *
swc_dmabuf_create(struct wl_display *display)
{
	return wl_global_create(display, &zwp_linux_dmabuf_v1_interface, 3, NULL,
	                        &bind_dmabuf);
}
