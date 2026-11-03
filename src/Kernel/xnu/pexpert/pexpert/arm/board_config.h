/*
 * Copyright (c) 2007-2014 Apple Inc. All rights reserved.
 * Copyright (c) 2005-2006 Apple Computer, Inc. All rights reserved.
 */
#ifndef _PEXPERT_ARM_BOARD_CONFIG_H
#define _PEXPERT_ARM_BOARD_CONFIG_H

#ifdef ARM_BOARD_CONFIG_T8002
#define ARMA7
#include <pexpert/arm/T8002.h>
#define MAX_CPUS                    2
#define MAX_CPU_CLUSTERS            1
#define ARM_BOARD_WFE_TIMEOUT_NS 1000
#define MAX_L2_CLINE              6
#define ARM_BOARD_CLASS_T8002
#define PEXPERT_NO_3X_IMAGES    1
#endif  /* ARM_BOARD_CONFIG_T8002 */

#ifdef ARM_BOARD_CONFIG_T8004
#define ARMA7
#include <pexpert/arm/T8002.h>
#define MAX_CPUS                    2
#define MAX_CPU_CLUSTERS            1
#define ARM_BOARD_WFE_TIMEOUT_NS 1000
#define MAX_L2_CLINE              6
#define ARM_BOARD_CLASS_T8002
#define PEXPERT_NO_3X_IMAGES    1
#endif  /* ARM_BOARD_CONFIG_T8004 */

#ifdef ARM_BOARD_CONFIG_BCM2835
#define ARM1176
#include <pexpert/arm/BCM2835.h>
#define MAX_CPUS                    1
#define MAX_CPU_CLUSTERS            1
#define MAX_L2_CLINE              5
#define ARM_BOARD_CLASS_BCM2835
#define PEXPERT_NO_3X_IMAGES    1
#endif  /* ARM_BOARD_CONFIG_BCM2835 */

#ifdef ARM_BOARD_CONFIG_VIRTA7
#define ARMA7
#if defined(ARM_BOARD_CONFIG_RV1106)
#include <pexpert/arm/RV1106.h>
#elif defined(ARM_BOARD_CONFIG_RK3506)
#include <pexpert/arm/RK3506.h>
#elif defined(ARM_BOARD_CONFIG_SUN7I)
#include <pexpert/arm/SUN7I.h>
#else
#include <pexpert/arm/VIRTA7.h>
#endif
#define MAX_CPUS                    4	// qemu virt up to 4, rk3506 3, a20 2; the loader's /cpus decides
#define MAX_CPU_CLUSTERS            1
#define MAX_L2_CLINE              6
#define ARM_BOARD_CLASS_VIRTA7
#define PEXPERT_NO_3X_IMAGES    1
#endif  /* ARM_BOARD_CONFIG_VIRTA7 */

#endif /* ! _PEXPERT_ARM_BOARD_CONFIG_H */
