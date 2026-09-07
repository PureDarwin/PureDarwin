/* swc: libswc/mode.c
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

#include "mode.h"

#ifdef ENABLE_DRM
bool
mode_initialize(struct mode *mode, drmModeModeInfo *mode_info)
{
	mode->width = mode_info->hdisplay;
	mode->height = mode_info->vdisplay;
	mode->refresh = mode_info->vrefresh * 1000;
	mode->preferred = mode_info->type & DRM_MODE_TYPE_PREFERRED;
	mode->info = *mode_info;
	return true;
}
#endif

void
mode_initialize_simple(struct mode *mode, uint16_t width, uint16_t height,
                       uint32_t refresh)
{
	mode->width = width;
	mode->height = height;
	mode->refresh = refresh;
	mode->preferred = true;
}

bool
mode_equal(const struct mode *mode1, const struct mode *mode2)
{
	return mode1->width == mode2->width && mode1->height == mode2->height &&
	       mode1->refresh == mode2->refresh;
}
