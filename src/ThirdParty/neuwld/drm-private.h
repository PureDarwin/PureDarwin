/* wld: drm-private.h
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

#ifndef WLD_DRM_PRIVATE_H
#define WLD_DRM_PRIVATE_H

#include "wld-private.h"

struct drm_driver {
	const char *name;
	bool (*device_supported)(uint32_t vendor_id, uint32_t device_id);
	struct wld_context *(*create_context)(int drm_fd);
};

#if WITH_DRM_INTEL
extern const struct drm_driver intel_drm_driver;
#endif
#if WITH_DRM_NOUVEAU
extern const struct drm_driver nouveau_drm_driver;
#endif
extern const struct drm_driver dumb_drm_driver;
extern const struct wld_context_impl *dumb_context_impl;

#endif
