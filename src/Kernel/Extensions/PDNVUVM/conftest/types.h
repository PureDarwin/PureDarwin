// types.h - the linuxkpi type shapes nvidia-uvm asks about, by hand for PDNVUVM
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_NVUVM_CONFTEST_TYPES_H
#define PD_NVUVM_CONFTEST_TYPES_H
#define NV_VM_FAULT_T_IS_PRESENT
// linuxkpi has the DMA page iterator and for_each_sgtable_dma_page
#define NV_SG_DMA_PAGE_ITER_PRESENT
#define NV_FOR_EACH_SGTABLE_DMA_PAGE_PRESENT
// page_pgmap comes from pd_uvm_linux.h
#define NV_PAGE_PGMAP_PRESENT
// linuxkpi keeps Linux's three argument __vmalloc, with the protection
#define NV_VMALLOC_HAS_PGPROT_T_ARG
#define NV_VM_OPS_FAULT_REMOVED_VMA_ARG
#endif
