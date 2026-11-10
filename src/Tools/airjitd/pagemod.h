// pagemod.h - which pages of the bridge the CPU wrote, from the kernel's dirty bits
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PAGEMOD_H
#define PAGEMOD_H

#include <stdint.h>

// brings the page generations of [off, off + len) up to this epoch, -1 when the kernel cannot tell
int pagemod_fresh(const uint8_t *base, uint32_t size, uint32_t off, uint32_t len);
// the newest generation any page of the range was written in
uint64_t pagemod_newest(uint32_t off, uint32_t len);
// the generation the latest ask stamped
uint64_t pagemod_now(void);
// asks again right away, after the executor itself wrote the range (a readback)
int pagemod_settle(uint32_t off, uint32_t len);
// a command buffer ended: regions may be asked about again
void pagemod_epoch(void);

#endif
