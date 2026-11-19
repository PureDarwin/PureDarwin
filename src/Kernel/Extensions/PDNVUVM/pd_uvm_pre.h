// pd_uvm_pre.h - included ahead of each nvidia-uvm source: what it expects linuxkpi to have in scope
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_UVM_PRE_H
#define PD_UVM_PRE_H

// nv-linux.h takes the kernel configuration from generated/autoconf.h, PDNVUVM's own (include/generated)
#define NV_GENERATED_AUTOCONF_H_PRESENT 1

// linuxkpi's efi.h reads FreeBSD's efi_boot, which PD has not got: nvidia-uvm only asks whether EFI booted, it never acts on it
#define efi_boot 0

// headers Linux's own pull in on the way, which nvidia's shared ones count on
#include <linux/kernel.h>
// linuxkpi follows Linux 6.14 (the drm-kmod base), which is what nvidia's version checks are asked about
#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(6, 14, 0)
#endif
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/preempt.h>
#include <linux/rwsem.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/mm.h>

// linuxkpi's pgprot_t is FreeBSD's protection bits as a plain word
typedef unsigned long vm_flags_t;
#ifndef pgprot_val
#define pgprot_val(prot) (prot)
#endif
#ifndef __pgprot
#define __pgprot(value) ((pgprot_t)(value))
#endif
// the page size bit in an x86 page table entry
#ifndef _PAGE_PSE
#define _PAGE_PSE (1UL << 7)
#endif

// the context queries nvidia's locking asserts make
#ifndef in_irq
#define in_irq() in_interrupt()
#endif
#ifndef irqs_disabled
#define irqs_disabled() (!ml_get_interrupts_enabled())
extern int ml_get_interrupts_enabled(void);
#endif
// PD's sx keeps only its exclusive owner, a shared hold is not visible here
#ifndef rwsem_is_locked
#define rwsem_is_locked(sem) sx_xlocked(&(sem)->sx)
#endif

static inline struct timespec64 timespec64_add(struct timespec64 a, struct timespec64 b)
{
    return ns_to_timespec64(timespec64_to_ns(&a) + timespec64_to_ns(&b));
}

static inline struct timespec64 timespec64_sub(struct timespec64 a, struct timespec64 b)
{
    return ns_to_timespec64(timespec64_to_ns(&a) - timespec64_to_ns(&b));
}

// nv-platform.h names nv_state_t before nv-linux.h has brought in nv.h
typedef struct nv_state_t nv_state_t;

// FreeBSD's libnv sys/nv.h guards itself with _NV_H_ as NVIDIA's nv.h does. Both are read here, libnv's first,
// then NVIDIA's with the guard cleared, so each is in and no later include of either one is skipped for the other
#include <sys/nv.h>
#undef _NV_H_
#include "nv.h"

// nv-linux.h's nv_is_control_device reads a Linux inode's device number; linuxkpi's inode is PD's vnode, which keeps
// a cdev. nvidia-uvm never calls it: the field below only lets the inline compile, it is never run
#define i_rdev v_type

// linuxkpi hides vmf_insert_pfn_prot behind an assert that steers callers to the locked form; nv-linux.h's
// nv_insert_pfn calls it once per fault, which is the unlocked form's use
#undef vmf_insert_pfn_prot

#include "pd_uvm_linux.h"

// NVIDIA's shared headers pick Linux's forms of the UVM callbacks on __linux__, which the interface layer and
// nvidia-uvm must agree on. Set only now, after every FreeBSD header linuxkpi needs has been read
#ifndef __linux__
#define __linux__ 1
#endif

// nv-time.h's nv_sleep_ms keeps a local called jiffies, which linuxkpi's jiffies macro would turn into a call:
// it is read once here with the macro set aside, its include guard keeps it from coming in again
#pragma push_macro("jiffies")
#undef jiffies
#include "nv-time.h"
#pragma pop_macro("jiffies")

#endif
