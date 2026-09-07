/* swc: libswc/mode.h
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

#ifndef SWC_MODE_H
#define SWC_MODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef ENABLE_DRM
#include <xf86drmMode.h>
#endif

struct mode {
	uint16_t width, height;
	uint32_t refresh;

	bool preferred;

#ifdef ENABLE_DRM
	drmModeModeInfo info;
#endif
};

#ifdef ENABLE_DRM
bool
mode_initialize(struct mode *mode, drmModeModeInfo *mode_info);
#endif
void
mode_initialize_simple(struct mode *mode, uint16_t width, uint16_t height,
                       uint32_t refresh);
bool
mode_equal(const struct mode *mode1, const struct mode *mode2);

#endif
