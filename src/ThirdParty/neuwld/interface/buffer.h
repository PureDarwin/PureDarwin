/* wld: interface/buffer.h
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

static bool buffer_map(struct buffer *drawable);
static bool buffer_unmap(struct buffer *drawable);
#ifdef BUFFER_IMPLEMENTS_FLUSH
static void buffer_flush(struct buffer *drawable);
#endif
static void buffer_destroy(struct buffer *drawable);

static const struct wld_buffer_impl wld_buffer_impl = {
	.map = &buffer_map,
	.unmap = &buffer_unmap,
#ifdef BUFFER_IMPLEMENTS_FLUSH
	.flush = &buffer_flush,
#endif
	.destroy = &buffer_destroy
};
