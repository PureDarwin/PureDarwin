/* wld: interface/surface.h
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

static pixman_region32_t *surface_damage(struct wld_surface *surface,
                                         pixman_region32_t *new_damage);
static struct buffer *surface_back(struct wld_surface *surface);
static struct buffer *surface_take(struct wld_surface *surface);
static bool surface_release(struct wld_surface *surface,
                            struct buffer *buffer);
static bool surface_swap(struct wld_surface *surface);
static void surface_destroy(struct wld_surface *surface);

static const struct wld_surface_impl wld_surface_impl = {
	.damage = &surface_damage,
	.back = &surface_back,
	.take = &surface_take,
	.release = &surface_release,
	.swap = &surface_swap,
	.destroy = &surface_destroy
};
