/* swc: libswc/wayland_buffer.c
 *
 * Copyright (c) 2013 Michael Forney
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

#include "wayland_buffer.h"
#include "internal.h"
#include "shm.h"
#include "util.h"

#include <wld/pixman.h>
#include <wld/wld.h>

static const struct wl_buffer_interface buffer_impl = {
    .destroy = destroy_resource,
};

struct wld_buffer *
wayland_buffer_get(struct wl_resource *resource)
{
	if (wl_resource_instance_of(resource, &wl_buffer_interface, &buffer_impl)) {
		return wl_resource_get_user_data(resource);
	}

	return NULL;
}

static void
destroy_buffer(struct wl_resource *resource)
{
	struct wld_buffer *buffer = wl_resource_get_user_data(resource);
	wld_buffer_unreference(buffer);
}

struct wl_resource *
wayland_buffer_create_resource(struct wl_client *client, uint32_t version,
                               uint32_t id, struct wld_buffer *buffer)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &wl_buffer_interface, version, id);
	if (resource) {
		wl_resource_set_implementation(resource, &buffer_impl, buffer,
		                               &destroy_buffer);
	}
	return resource;
}
