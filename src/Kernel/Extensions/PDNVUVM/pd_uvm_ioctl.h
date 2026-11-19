// pd_uvm_ioctl.h - how a Linux nvidia-uvm ioctl reaches PDNVUVM on Darwin, shared with elftrans
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// nvidia-uvm's ioctl numbers carry no size or direction, so Darwin would hand the driver neither the argument nor
// its address. elftrans sends each one as PD_UVM_IOCTL instead: the Linux number and the argument's address in the
// caller, which the driver reads and writes directly. elftrans (src/Tools/elftrans/syscalls.c) keeps its own copy
// of these definitions, which must match.
#ifndef PD_UVM_IOCTL_H
#define PD_UVM_IOCTL_H

#include <stdint.h>
#include <sys/ioccom.h>

struct pd_uvm_ioctl {
	uint32_t cmd;       // nvidia-uvm's own number, UVM_INITIALIZE and the rest
	uint32_t reserved;
	uint64_t arg;       // the argument's address in the calling process
};

#define PD_UVM_IOCTL _IOW('U', 0x01, struct pd_uvm_ioctl)

#endif
