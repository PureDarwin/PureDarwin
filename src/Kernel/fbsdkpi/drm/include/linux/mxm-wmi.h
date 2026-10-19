// SPDX-License-Identifier: MIT
// mxm wmi methods some optimus laptops use for the mux, absent on PD
#ifndef _PD_LINUX_MXM_WMI_H_
#define _PD_LINUX_MXM_WMI_H_

#include <linux/types.h>

#define MXM_MXDS_ADAPTER_0	0x0
#define MXM_MXDS_ADAPTER_IGD	0x10

static inline bool mxm_wmi_supported(void) { return false; }
static inline int mxm_wmi_call_mxmx(int adapter) { return 0; }
static inline int mxm_wmi_call_mxds(int adapter) { return 0; }

#endif
