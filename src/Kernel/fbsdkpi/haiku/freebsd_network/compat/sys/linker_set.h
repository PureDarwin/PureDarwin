/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1999 John D. Polstra
 * Copyright (c) 1999,2001 Peter Wemm <peter@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _FBSD_COMPAT_SYS_LINKER_SET_H_
#define _FBSD_COMPAT_SYS_LINKER_SET_H_


#ifndef _FBSD_COMPAT_SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

/*
 * The following macros are used to declare global sets of objects, which
 * are collected by the linker into a `linker_set' as defined below.
 * For ELF, this is done by constructing a separate segment for each set.
 */

/*
 * Private macros, not to be used outside this header file.
 */
#if defined(__PUREDARWIN__) && defined(__MACH__)
/* mach-o section names are 16 bytes at most, so each set gets a short name */
#define __PDSET___freebsd_sysinit	"fbsd_sysinit"
#define __PDSET___freebsd_sysuninit	"fbsd_sysuninit"
#define __PDSET_crypto_set	"wlan_crypto"
#define __PDSET_scanner_set	"wlan_scanner"
#define __PDSET_acl_set	"wlan_acl"
#define __PDSET_auth_set	"wlan_auth"
#define __PDSET_ratectl_set	"wlan_ratectl"
#define __PDSET_ieee80211_ioctl_getset	"wlan_ioctl_get"
#define __PDSET_ieee80211_ioctl_setset	"wlan_ioctl_set"
#define __MAKE_SET(set, sym)						\
	static void const * const __set_##set##_sym_##sym 		\
	__section("__DATA," __PDSET_##set) __used = &sym
#elif defined(__GNUCLIKE___SECTION)
#define __MAKE_SET(set, sym)						\
	static void const * const __set_##set##_sym_##sym 		\
	__section("set_" #set) __used = &sym
#else /* !__GNUCLIKE___SECTION */
#ifndef lint
#error this file needs to be ported to your compiler
#endif /* lint */
#define __MAKE_SET(set, sym)	extern void const * const (__set_##set##_sym_##sym)
#endif /* __GNUCLIKE___SECTION */

/*
 * Public macros.
 */
#define TEXT_SET(set, sym)	__MAKE_SET(set, sym)
#define DATA_SET(set, sym)	__MAKE_SET(set, sym)
#define BSS_SET(set, sym)	__MAKE_SET(set, sym)
#define ABS_SET(set, sym)	__MAKE_SET(set, sym)
#define SET_ENTRY(set, sym)	__MAKE_SET(set, sym)

/*
 * Initialize before referring to a given linker set.
 */
#if defined(__PUREDARWIN__) && defined(__MACH__)
#define SET_DECLARE(set, ptype)						\
	extern ptype *__CONCAT(__start_set_,set)			\
		__asm("section$start$__DATA$" __PDSET_##set);		\
	extern ptype *__CONCAT(__stop_set_,set)			\
		__asm("section$end$__DATA$" __PDSET_##set)
#else
#define SET_DECLARE(set, ptype)						\
	extern ptype *__CONCAT(__start_set_,set);			\
	extern ptype *__CONCAT(__stop_set_,set)
#endif

#define SET_BEGIN(set)							\
	(&__CONCAT(__start_set_,set))
#define SET_LIMIT(set)							\
	(&__CONCAT(__stop_set_,set))

/*
 * Iterate over all the elements of a set.
 *
 * Sets always contain addresses of things, and "pvar" points to words
 * containing those addresses.  Thus is must be declared as "type **pvar",
 * and the address of each set item is obtained inside the loop by "*pvar".
 */
#define SET_FOREACH(pvar, set)						\
	for (pvar = SET_BEGIN(set); pvar < SET_LIMIT(set); pvar++)

#endif /* _FBSD_COMPAT_SYS_LINKER_SET_H_ */
