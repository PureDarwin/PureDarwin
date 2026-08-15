/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
 */

#ifndef _PEXPERT_ARM_AIC_H
#define _PEXPERT_ARM_AIC_H

#ifndef ASSEMBLER

#include <stdint.h>

static inline uint32_t
_aic_read32(uintptr_t addr)
{
	return *(volatile uint32_t *)addr;
}

static inline void
_aic_write32(uintptr_t addr, uint32_t data)
{
	*(volatile uint32_t *)(addr) = data;
}

#define aic_read32(offset, data) (_aic_read32(pic_base + (offset)))
#define aic_write32(offset, data) (_aic_write32(pic_base + (offset), (data)))

#endif

// AIC timebase registers (timer base address in DT node is setup as AIC_BASE + 0x1000)
#define kAICMainTimLo                           (0x20)
#define kAICMainTimHi                           (0x28)

/*
 * The rest of the AIC register map, as spelled in the 32-bit pexpert header
 * (pexpert/arm/AIC.h), which cannot be included here - both files use the same
 * include guard. Only what is needed to quiesce the controller is repeated.
 */
#define kAICAicCap0                             (0x0004)
#define kAICAicCap0Int(n)                       ((n) & 0x3FF)
#define kAICAicCap0Proc(n)                      ((((n) >> 16) & 0x1F) + 1)
#define kAICIack                                (0x2004)
#define kAICIntMaskSet(n)                       (0x4100 + (n) * 4)
#define kAICIntMaskClr(n)                       (0x4180 + (n) * 4)

#endif /* ! _PEXPERT_ARM_AIC_H */
