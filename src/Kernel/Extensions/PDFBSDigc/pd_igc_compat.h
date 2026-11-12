// SPDX-License-Identifier: MIT
// freebsd kpi bits igc uses that the haiku compat layer lacks, force-included into the driver
#ifndef PD_IGC_COMPAT_H
#define PD_IGC_COMPAT_H

#ifndef PRIx64
#define PRIx64 "llx"
#endif

// pcireg.h l1 pm substates capability, for the i225 and i226 l1.2 erratum
#ifndef PCIZ_L1PM
#define PCIZ_L1PM                   0x001e
#define PCIR_L1PM_CTL1              0x08
#define PCIM_L1PM_CTL1_PCIPM_L1_2   0x00000001
#define PCIM_L1PM_CTL1_ASPM_L1_2    0x00000004
#endif

#endif
