// SPDX-License-Identifier: MIT
// haiku's private interrupt helpers
#ifndef _PD_HAIKU_INTERRUPTS_H
#define _PD_HAIKU_INTERRUPTS_H
#include <KernelExport.h>
#ifdef __cplusplus
extern "C" {
#endif
bool are_interrupts_enabled(void);
#ifdef __cplusplus
}
#endif
#endif
