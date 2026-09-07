/* swc: data_device.h
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

#ifndef SWC_DATA_DEVICE_H
#define SWC_DATA_DEVICE_H

#include <stdbool.h>
#include <wayland-server.h>

enum { DATA_DEVICE_EVENT_SELECTION_CHANGED };

struct data_device {
	/* The data source corresponding to the current selection. */
	struct wl_resource *selection;
	struct wl_listener selection_destroy_listener;

	struct wl_signal event_signal;
	struct wl_list resources;
};

struct data_device *
data_device_create(void);
void
data_device_destroy(struct data_device *data_device);
struct wl_resource *
data_device_bind(struct data_device *data_device, struct wl_client *client,
                 uint32_t version, uint32_t id);
void
data_device_offer_selection(struct data_device *data_device,
                            struct wl_client *client);

#endif
