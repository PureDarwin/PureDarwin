/* swc: libswc/pointer_constraints.h
 *
 * Copyright (c) 2026 The PureDarwin Project
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

#ifndef SWC_POINTER_CONSTRAINTS_H
#define SWC_POINTER_CONSTRAINTS_H

#include <stdbool.h>
#include <wayland-server.h>

struct pointer;

// zwp_relative_pointer_manager_v1 and zwp_pointer_constraints_v1 for mouse-look
bool
pointer_constraints_create(struct wl_display *display);

// raw device motion: sends relative events, then returns false while the pointer is locked
// or else clamps the target (*x, *y) into an active confinement
bool
pointer_constraints_motion(struct pointer *pointer, uint32_t time, wl_fixed_t dx,
                           wl_fixed_t dy, wl_fixed_t *x, wl_fixed_t *y);

// re-evaluates which constraint is active after motion, focus or region changes
void
pointer_constraints_update(struct pointer *pointer);

#endif
