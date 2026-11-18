// SDM670.h - the pixel 3a (qualcomm sdm670): the sc8280xp configuration plus its debug uart for start.s
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef _PEXPERT_ARM_SDM670_H
#define _PEXPERT_ARM_SDM670_H

#include <pexpert/arm64/SC8280XP.h>

// QUP1 SE4, the usb-c sideband debug uart: start.s writes a byte here before the MMU is on
#define PD_EARLY_GENI_UART_PHYS   0xa90000
// define this to send EL1 faults to start.s's dump before xnu has a console. Off now that the console works
// #define PD_EARLY_FAULT_CATCH

#endif /* ! _PEXPERT_ARM_SDM670_H */
