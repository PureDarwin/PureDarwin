/*
 * Copyright 2026, PureDarwin Project. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
// included ahead of every iwlwifi source, after pd_lkpi_pre.h
#ifndef _PD_IWLWIFI_PRE_H
#define _PD_IWLWIFI_PRE_H

// linux puts kconfig.h in front of every file
#include <linux/kconfig.h>

// a global sx lock set up when the kext starts
#define SX_SYSINIT_FLAGS(name, sxa, desc, flags) \
	static void name##_sx_sysinit(void *arg) { sx_init_flags((sxa), (desc), (flags)); } \
	SYSINIT(name##_sx_sysinit, SI_SUB_LOCK, SI_ORDER_MIDDLE, name##_sx_sysinit, NULL)
#define SX_SYSINIT(name, sxa, desc) SX_SYSINIT_FLAGS(name, sxa, desc, 0)

// haiku's pcivar has only D0 to D3
#ifndef PCI_POWERSTATE_D3_COLD
#define PCI_POWERSTATE_D3_HOT 3
#define PCI_POWERSTATE_D3_COLD 4
// newer linux log2.h helper our linuxkpi lacks
#define rounddown_pow_of_two(n) \
	((n) == 0 ? 0UL : 1UL << (63 - __builtin_clzl((unsigned long)(n))))

#endif

// linux clock ids, iwlwifi only compares them
#ifndef CLOCK_BOOTTIME
#define CLOCK_BOOTTIME 7
// newer linux log2.h helper our linuxkpi lacks
#define rounddown_pow_of_two(n) \
	((n) == 0 ? 0UL : 1UL << (63 - __builtin_clzl((unsigned long)(n))))

#endif

// FreeBSD's ephemeral port range, for linuxkpi's net/ip.h
#ifndef IPPORT_EPHEMERALFIRST
#define IPPORT_EPHEMERALFIRST 10000
#define IPPORT_EPHEMERALLAST 65535
// newer linux log2.h helper our linuxkpi lacks
#define rounddown_pow_of_two(n) \
	((n) == 0 ? 0UL : 1UL << (63 - __builtin_clzl((unsigned long)(n))))

#endif

// FreeBSD in6.h names linuxkpi's net/addrconf.h uses, on haiku's in6_addr
#define s6_addr8 in6_u.u6_addr8
#define IPV6_ADDR_INT16_MLL __builtin_bswap16(0xff02)
#define IPV6_ADDR_INT32_ONE __builtin_bswap32(1)

// newer linux log2.h helper our linuxkpi lacks
#define rounddown_pow_of_two(n) \
	((n) == 0 ? 0UL : 1UL << (63 - __builtin_clzl((unsigned long)(n))))

#endif
