/* wld: interface/drm.h
 *
 * Copyright (c) 2013, 2014 Michael Forney
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

#ifndef DRM_DRIVER_NAME
#error "You must define DRM_DRIVER_NAME before including interface/drm.h"
#endif

/* DRM driver */
static bool driver_device_supported(uint32_t vendor_id, uint32_t device_id);
static struct wld_context *driver_create_context(int drm_fd);

#define EXPAND(f, x) f(x)
#define VAR(name) name##_drm_driver
#define STRING(name) #name
const struct drm_driver EXPAND(VAR, DRM_DRIVER_NAME) = {
	.name = EXPAND(STRING, DRM_DRIVER_NAME),
	.device_supported = &driver_device_supported,
	.create_context = &driver_create_context,
};
#undef VAR
#undef STRING
#undef EXPAND
