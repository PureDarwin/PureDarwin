/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
 */
#ifndef _PEXPERT_RISCV_BOARD_CONFIG_H
#define _PEXPERT_RISCV_BOARD_CONFIG_H

#if defined(RISCV64_BOARD_CONFIG_VIRT)
// qemu virt with opensbi, sv39, harts and the plic come from the device tree
#define MAX_CPUS                        8
#define MAX_CPU_CLUSTERS                1
#define PLATFORM_HAS_SBI_TIMER          1
#define PLATFORM_HAS_PLIC               1
#endif /* RISCV64_BOARD_CONFIG_VIRT */

#endif /* _PEXPERT_RISCV_BOARD_CONFIG_H */
