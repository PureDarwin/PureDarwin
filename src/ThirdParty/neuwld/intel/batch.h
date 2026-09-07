/* wld: intel/batch.h
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

#ifndef WLD_INTEL_BATCH_H
#define WLD_INTEL_BATCH_H

#include <intel_bufmgr.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#define INTEL_BATCH_MAX_COMMANDS (1 << 13)
#define INTEL_BATCH_RESERVED_COMMANDS 2
#define INTEL_BATCH_SIZE (INTEL_BATCH_MAX_COMMANDS << 2)

enum intel_batch_result {
	INTEL_BATCH_SUCCESS,
	INTEL_BATCH_NO_SPACE
};

struct intel_device_info {
	int gen;
};

#define GEN(b, m) ((b)->device_info->gen >= (m))

struct intel_batch {
	const struct intel_device_info *device_info;
	drm_intel_bo *bo;
	uint32_t commands[INTEL_BATCH_MAX_COMMANDS];
	uint32_t command_count;
};

bool intel_batch_initialize(struct intel_batch *batch,
                            drm_intel_bufmgr *bufmgr);

void intel_batch_finalize(struct intel_batch *batch);

void intel_batch_flush(struct intel_batch *batch);

static inline uint32_t
intel_batch_check_space(struct intel_batch *batch,
                        uint32_t size)
{
	return (INTEL_BATCH_MAX_COMMANDS - INTEL_BATCH_RESERVED_COMMANDS
	        - batch->command_count)
	       >= size;
}

static inline void
intel_batch_ensure_space(struct intel_batch *batch, uint32_t size)
{
	if (!intel_batch_check_space(batch, size))
		intel_batch_flush(batch);
}

static inline void
intel_batch_add_dword(struct intel_batch *batch,
                      uint32_t dword)
{
	batch->commands[batch->command_count++] = dword;
}

static inline void
intel_batch_add_dwords_va(struct intel_batch *batch,
                          uint32_t count, va_list dwords)
{
	while (count--)
		intel_batch_add_dword(batch, va_arg(dwords, uint32_t));
}

static inline void
intel_batch_add_dwords(struct intel_batch *batch,
                       uint32_t count, ...)
{
	va_list dwords;
	va_start(dwords, count);
	intel_batch_add_dwords_va(batch, count, dwords);
	va_end(dwords);
}

static inline uint32_t
intel_batch_offset(struct intel_batch *batch,
                   uint32_t command_index)
{
	return (batch->command_count + command_index) << 2;
}

#endif
