// PDEventFD.h - the setup ioctl /dev/eventfd takes after open, shared with elftrans
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PD_EVENTFD_H
#define PD_EVENTFD_H

#include <stdint.h>
#include <sys/ioccom.h>

#define PD_EVENTFD_SEMAPHORE 1

typedef struct {
	uint64_t initial;
	uint32_t flags;
	uint32_t pad;
} PDEventFDSetup;

#define PD_EVENTFD_SETUP _IOW('E', 1, PDEventFDSetup)

#endif
