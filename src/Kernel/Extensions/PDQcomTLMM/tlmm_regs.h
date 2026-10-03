/*
 * Qualcomm TLMM (top-level mode mux)
 */
#ifndef PD_TLMM_REGS_H
#define PD_TLMM_REGS_H

#include <libkern/OSTypes.h>

// each pin has its own 4 KiB block in the TLMM window, pin 0 first
enum {
  kTlmmPinStride = 0x1000,
  kTlmmPinControl = 0x0, // function select, pull, drive: never written here
  kTlmmPinInOut = 0x4,   // bit 0: the level on the pin now
  kTlmmPinIrqConfig = 0x8,
  kTlmmPinIrqStatus = 0xc, // bit 0: latched; write 0 to clear
};

// kTlmmPinIrqConfig
enum {
  kTlmmIrqEnable = 1u << 0, // latches reach the summary interrupt
  kTlmmIrqActiveHigh =
      1u << 1, // high level, or any edge (the trigger field picks which). clear for low level
  kTlmmIrqTriggerShift =
      2, // 2 bits: 0 level, 1 rising, 2 falling, 3 both edges
  kTlmmIrqRawStatus = 1u << 4, // latch even while not enabled
  kTlmmIrqTargetShift = 5,     // 3 bits: which processor the latch is for
  kTlmmIrqTargetApps = 3,      // the application processors
};

enum {
  kTlmmTriggerLevel = 0,
  kTlmmTriggerRising = 1,
  kTlmmTriggerFalling = 2,
  kTlmmTriggerBoth = 3,
};

static inline UInt32 tlmmPin(UInt32 pin, UInt32 reg) {
  return pin * kTlmmPinStride + reg;
}

#endif
