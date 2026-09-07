/* swc: libswc/shm.c
 *
 * Copyright (c) 2013-2020 Michael Forney
 *
 * Based in part upon wayland-shm.c from wayland, which is:
 *
 *     Copyright © 2008 Kristian Høgsberg
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

#include "shm.h"
#include "internal.h"
#include "util.h"
#include "wayland_buffer.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wld/pixman.h>
#include <wld/wld.h>

struct pool {
	struct wl_resource *resource;
	struct swc_shm *shm;
	void *data;
	uint32_t size;
	int fd;
	bool writable;
	unsigned references;
};

struct pool_reference {
	struct wld_destructor destructor;
	struct pool *pool;
};

struct shm_buffer_record {
	struct wl_list link;
	struct wl_listener destroy_listener;
	struct wl_resource *resource;
	struct pool *pool;
	uint32_t offset;
	int32_t width;
	int32_t height;
	int32_t stride;
	uint32_t format;
};

static struct wl_list shm_buffer_records;
static bool shm_buffer_records_initialized;

static void
ensure_shm_buffer_records(void)
{
	if (!shm_buffer_records_initialized) {
		wl_list_init(&shm_buffer_records);
		shm_buffer_records_initialized = true;
	}
}

static void
handle_shm_buffer_resource_destroy(struct wl_listener *listener, void *data)
{
	struct shm_buffer_record *record =
	    wl_container_of(listener, record, destroy_listener);
	(void)data;

	wl_list_remove(&record->link);
	free(record);
}

static void *
swc_mremap(struct pool *pool, void *oldp, size_t oldsize, size_t newsize)
{
#ifdef __NetBSD__
	return mremap(oldp, oldsize, NULL, newsize, 0);
#elif defined(__linux__)
	return mremap(oldp, oldsize, newsize, MREMAP_MAYMOVE);
#else
	void *newp;

	newp = mmap(NULL, newsize, PROT_READ, MAP_SHARED, pool->fd, 0);
	if (newp == MAP_FAILED) {
		return MAP_FAILED;
	}

	(void)munmap(oldp, oldsize);
	return newp;
#endif
}

static void
unref_pool(struct pool *pool)
{
	if (--pool->references > 0) {
		return;
	}

	munmap(pool->data, pool->size);
	close(pool->fd);
	free(pool);
}

static void
destroy_pool_resource(struct wl_resource *resource)
{
	struct pool *pool = wl_resource_get_user_data(resource);
	unref_pool(pool);
}

static void
handle_buffer_destroy(struct wld_destructor *destructor)
{
	struct pool_reference *reference =
	    wl_container_of(destructor, reference, destructor);
	unref_pool(reference->pool);
}

static inline uint32_t
format_shm_to_wld(uint32_t format)
{
	switch (format) {
	case WL_SHM_FORMAT_ARGB8888:
		return WLD_FORMAT_ARGB8888;
	case WL_SHM_FORMAT_XRGB8888:
		return WLD_FORMAT_XRGB8888;
	default:
		return format;
	}
}

static void
create_buffer(struct wl_client *client, struct wl_resource *resource,
              uint32_t id, int32_t offset, int32_t width, int32_t height,
              int32_t stride, uint32_t format)
{
	struct pool *pool = wl_resource_get_user_data(resource);
	struct pool_reference *reference;
	struct shm_buffer_record *record = NULL;
	struct wld_buffer *buffer;
	struct wl_resource *buffer_resource;
	union wld_object object;

	if (offset > pool->size || offset < 0) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE,
		                       "offset is too big or negative");
		return;
	}

	object.ptr = (void *)((uintptr_t)pool->data + offset);
	buffer =
	    wld_import_buffer(pool->shm->context, WLD_OBJECT_DATA, object, width,
	                      height, format_shm_to_wld(format), stride);

	if (!buffer) {
		goto error0;
	}

	buffer_resource = wayland_buffer_create_resource(
	    client, wl_resource_get_version(resource), id, buffer);

	if (!buffer_resource) {
		goto error1;
	}

	ensure_shm_buffer_records();
	record = malloc(sizeof(*record));
	if (!record) {
		goto error2;
	}
	record->resource = buffer_resource;
	record->pool = pool;
	record->offset = (uint32_t)offset;
	record->width = width;
	record->height = height;
	record->stride = stride;
	record->format = format;
	record->destroy_listener.notify = &handle_shm_buffer_resource_destroy;
	wl_resource_add_destroy_listener(buffer_resource, &record->destroy_listener);
	wl_list_insert(&shm_buffer_records, &record->link);

	if (!(reference = malloc(sizeof(*reference)))) {
		goto error3;
	}

	reference->pool = pool;
	reference->destructor.destroy = &handle_buffer_destroy;
	wld_buffer_add_destructor(buffer, &reference->destructor);
	++pool->references;

	return;

error3:
	if (record) {
		wl_list_remove(&record->destroy_listener.link);
		wl_list_remove(&record->link);
		free(record);
	}
error2:
	wl_resource_destroy(buffer_resource);
error1:
	wld_buffer_unreference(buffer);
error0:
	wl_resource_post_no_memory(resource);
}

static void
resize(struct wl_client *client, struct wl_resource *resource, int32_t size)
{
	struct pool *pool = wl_resource_get_user_data(resource);
	void *data;
	struct stat st;

	if (fstat(pool->fd, &st) != 0) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
		                       "fstat failed: %s", strerror(errno));
		return;
	}
	if (st.st_size < size) {
		if (ftruncate(pool->fd, size) != 0) {
			int saved = errno;
			/* some clients seal memfd if size is already fine,
		 * alloc will fail */
			if ((saved == EPERM || saved == EACCES) &&
			    fstat(pool->fd, &st) == 0 && st.st_size >= size) {
				goto remap;
			}
			wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
			                       "ftruncate failed: %s", strerror(saved));
			return;
		}
	}

remap:
	data = swc_mremap(pool, pool->data, pool->size, size);
	if (data == MAP_FAILED) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
		                       "mremap failed: %s", strerror(errno));
		return;
	}
	pool->data = data;
	pool->size = size;
}

static const struct wl_shm_pool_interface shm_pool_impl = {
    .create_buffer = create_buffer,
    .destroy = destroy_resource,
    .resize = resize,
};

static void
create_pool(struct wl_client *client, struct wl_resource *resource, uint32_t id,
            int32_t fd, int32_t size)
{
	struct swc_shm *shm = wl_resource_get_user_data(resource);
	struct pool *pool;

	pool = malloc(sizeof(*pool));
	if (!pool) {
		wl_resource_post_no_memory(resource);
		goto error0;
	}
	pool->shm = shm;
	pool->resource = wl_resource_create(client, &wl_shm_pool_interface,
	                                    wl_resource_get_version(resource), id);
	if (!pool->resource) {
		wl_resource_post_no_memory(resource);
		goto error1;
	}
	wl_resource_set_implementation(pool->resource, &shm_pool_impl, pool,
	                               &destroy_pool_resource);
	pool->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	pool->writable = true;
	if (pool->data == MAP_FAILED) {
		pool->data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		pool->writable = false;
		if (pool->data == MAP_FAILED) {
			wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
			                       "mmap failed: %s", strerror(errno));
			goto error2;
		}
	}
	/* close(fd); */
	pool->size = size;
	pool->references = 1;
	pool->fd = fd;
	return;

error2:
	wl_resource_destroy(pool->resource);
error1:
	free(pool);
error0:
	close(fd);
}

static const struct wl_shm_interface shm_impl = {.create_pool = &create_pool};

static void
bind_shm(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct swc_shm *shm = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &wl_shm_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &shm_impl, shm, NULL);

	wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);
	wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
}

struct swc_shm *
shm_create(struct wl_display *display)
{
	struct swc_shm *shm;

	shm = malloc(sizeof(*shm));
	if (!shm) {
		goto error0;
	}
	shm->context = wld_pixman_create_context();
	if (!shm->context) {
		goto error1;
	}
	shm->renderer = wld_create_renderer(shm->context);
	if (!shm->renderer) {
		goto error2;
	}
	shm->global =
	    wl_global_create(display, &wl_shm_interface, 1, shm, &bind_shm);
	if (!shm->global) {
		goto error3;
	}

	return shm;

error3:
	wld_destroy_renderer(shm->renderer);
error2:
	wld_destroy_context(shm->context);
error1:
	free(shm);
error0:
	return NULL;
}

void
shm_destroy(struct swc_shm *shm)
{
	wl_global_destroy(shm->global);
	wld_destroy_renderer(shm->renderer);
	wld_destroy_context(shm->context);
	free(shm);
}

bool
shm_buffer_get_info(struct wl_resource *resource, struct swc_shm_buffer_info *info)
{
	struct shm_buffer_record *record;
	uint64_t size;

	if (!shm_buffer_records_initialized) {
		return false;
	}

	wl_list_for_each(record, &shm_buffer_records, link)
	{
		if (record->resource != resource) {
			continue;
		}

		if (record->width < 0 || record->height < 0 || record->stride < 0) {
			return false;
		}

		size = (uint64_t)record->stride * (uint64_t)record->height;
		if ((uint64_t)record->offset + size > record->pool->size) {
			return false;
		}

		info->data = (uint8_t *)record->pool->data + record->offset;
		info->width = record->width;
		info->height = record->height;
		info->stride = record->stride;
		info->format = record->format;
		info->writable = record->pool->writable;
		return true;
	}

	return false;
}
