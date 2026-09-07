/* swc: libswc/shm.h
 *
 * Copyright (c) 2013-2019 Michael Forney
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

#ifndef SWC_SHM_H
#define SWC_SHM_H

#include <stdbool.h>
#include <stdint.h>

struct wl_display;
struct wl_resource;

struct swc_shm {
	struct wl_global *global;
	struct wld_context *context;
	struct wld_renderer *renderer;
};

struct swc_shm_buffer_info {
	void *data;
	int32_t width;
	int32_t height;
	int32_t stride;
	uint32_t format;
	bool writable;
};

struct swc_shm *
shm_create(struct wl_display *display);
void
shm_destroy(struct swc_shm *shm);
bool
shm_buffer_get_info(struct wl_resource *resource, struct swc_shm_buffer_info *info);

#endif
