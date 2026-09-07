/* wld: intel/mi.h
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

#ifndef WLD_INTEL_MI_H
#define WLD_INTEL_MI_H

#define INTEL_CLIENT_MI 0x0

#define MI_OP(opcode) (               \
    INTEL_CLIENT_MI << 29 /* 31:29 */ \
    | opcode << 23        /* 28:23 */ \
)

#define MI_NOOP MI_OP(0x00)
#define MI_FLUSH MI_OP(0x04)
#define MI_BATCH_BUFFER_END MI_OP(0x0A)

/* MI_NOOP */
#define MI_NOOP_IDENTIFICATION_NUMBER(number) (1 << 22 | number)

/* MI_FLUSH */
#define MI_FLUSH_ENABLE_PROTECTED_MEMORY (1 << 6)
#define MI_FLUSH_DISABLE_INDIRECT_STATE_POINTERS (1 << 5)
#define MI_FLUSH_CLEAR_GENERIC_MEDIA_STATE (1 << 4)
#define MI_FLUSH_RESET_GLOBAL_SNAPSHOT_COUNT (1 << 3)
#define MI_FLUSH_INHIBIT_RENDER_CACHE_FLUSH (1 << 2)
#define MI_FLUSH_INVALIDATE_STATE_INSTRUCTION_CACHE (1 << 1)

#endif
