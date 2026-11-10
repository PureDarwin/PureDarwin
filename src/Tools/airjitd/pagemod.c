// pagemod.c - which pages of the bridge the CPU wrote, from the kernel's dirty bits, as page generations
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <stdlib.h>
#include <string.h>
#include "pagemod.h"

#define PAGE 4096u
// each region of the bridge is asked about at most once per epoch (command buffer)
#define REGION (2u << 20)

static int state; // 0 untried, 1 works, -1 not there
static const uint8_t *arena;
static uint32_t arena_size;
static uint64_t *gen, now = 1, epoch = 1, *region_epoch;
static uint8_t *bits;

// machdep call 7, pd_page_modified64: one bit per page written since the last ask, nonzero when there is none
static long pagemod_call(uintptr_t addr, uint64_t len, uint8_t *out) {
#if defined(__APPLE__) && defined(__x86_64__)
  long rax = (3l << 24) | 7;

  __asm__ volatile("syscall" : "+a"(rax) : "D"(addr), "S"(len), "d"(out) : "rcx", "r11", "memory");
  return rax;
#else
  (void)addr, (void)len, (void)out;
  return -1;
#endif
}

static int setup(const uint8_t *base, uint32_t size) {
  if (state < 0)
    return -1;
  if (arena == base && arena_size == size && gen)
    return 0;
  free(gen), free(bits), free(region_epoch);
  arena = base, arena_size = size;
  gen = calloc(size / PAGE + 2, sizeof(*gen));
  bits = malloc(size / PAGE / 8 + 2);
  region_epoch = calloc(size / REGION + 2, sizeof(*region_epoch));
  if (!gen || !bits || !region_epoch)
    return state = -1;
  return 0;
}

// asks the kernel about [off, off + len), stamping written pages with a new generation
static int take(uint32_t off, uint32_t len) {
  uintptr_t lo = (uintptr_t)(arena + off), first = lo / PAGE, apage = (uintptr_t)arena / PAGE;
  uintptr_t n = (lo + len + PAGE - 1) / PAGE - first;

  if (pagemod_call(lo, len, bits))
    return state = -1;
  state = 1, now++;
  for (uintptr_t i = 0; i < n; i++) {
    if (bits[i / 8] & (1u << (i % 8)))
      gen[first - apage + i] = now;
  }
  return 0;
}

int pagemod_fresh(const uint8_t *base, uint32_t size, uint32_t off, uint32_t len) {
  if (!len || off > size || len > size - off || setup(base, size))
    return -1;
  for (uint32_t r = off / REGION; r <= (off + len - 1) / REGION; r++) {
    uint32_t lo = r * REGION, n = size - lo < REGION ? size - lo : REGION;

    if (region_epoch[r] == epoch)
      continue;
    if (take(lo, n))
      return -1;
    region_epoch[r] = epoch;
  }
  return 0;
}

uint64_t pagemod_newest(uint32_t off, uint32_t len) {
  uintptr_t lo = ((uintptr_t)(arena + off)) / PAGE, hi = ((uintptr_t)(arena + off) + len - 1) / PAGE;
  uintptr_t apage = (uintptr_t)arena / PAGE;
  uint64_t g = 0;

  for (uintptr_t p = lo; len && p <= hi; p++) {
    if (gen[p - apage] > g)
      g = gen[p - apage];
  }
  return g;
}

uint64_t pagemod_now(void) {
  return now;
}

int pagemod_settle(uint32_t off, uint32_t len) {
  if (state < 1 || !len)
    return -1;
  return take(off, len);
}

void pagemod_epoch(void) {
  epoch++;
}
