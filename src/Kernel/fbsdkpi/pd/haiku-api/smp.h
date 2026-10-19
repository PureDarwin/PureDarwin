// SPDX-License-Identifier: MIT
// haiku's smp queries
#ifndef _PD_HAIKU_SMP_H
#define _PD_HAIKU_SMP_H
#include <SupportDefs.h>
#ifdef __cplusplus
extern "C" {
#endif
int32 smp_get_num_cpus(void);
int32 smp_get_current_cpu(void);
#ifdef __cplusplus
}
#endif
#endif
