/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#pragma once

#if !XNU_KERNEL_PRIVATE
#error upsi.h is XNU private
#endif

#if (DEVELOPMENT || DEBUG)
#define HAS_UPSI_FAILURE_INJECTION 1
#endif

#if HAS_UPSI_FAILURE_INJECTION
/* Enumeration of the injectable failure locations/stages - Must be kept in sync with iBoot's "power_boot_stages.h"
 * The "stage" macros specify points where failure injection is possible
 */
__enum_decl(failure_injection_stage_t, uint64_t, {
	XNU_STAGE_ARM_INIT              = 0x31,
	XNU_STAGE_BOOTSTRAP_START       = 0x32,
	XNU_STAGE_SCHEDULER_START       = 0x33,
	XNU_STAGE_BSD_INIT_START        = 0x34,
	XNU_STAGE_BSD_INIT_END          = 0x35,
	XNU_STAGE_HIBERNATE_ENTRY       = 0x37,
});

/* Enumeration of the injectable failure actions
 *
 *  ACTION_WATCHDOG_TIMEOUT and ACTION_DEADLOOP look functionally equivalent.
 * However they are different in the way iBoot configures the system watchdog.
 *
 * ACTION_WATCHDOG_TIMEOUT -> Deadloops with the system watchdog enabled
 * ACTION_DEADLOOP         -> Deadloops with the system watchdog disabled
 * The watchdog behavior is configured by iBoot. Convey'd to XNU through the wdt=-1 boot-arg
 */
__enum_decl(failure_injection_action_t, uint64_t, {
	INJECTION_ACTION_PANIC                    = 0x01,
	INJECTION_ACTION_WATCHDOG_TIMEOUT         = 0x02,
	INJECTION_ACTION_DEADLOOP                 = 0x03,
});

extern uint64_t xnu_upsi_injection_stage;
extern uint64_t xnu_upsi_injection_action;

#ifdef __cplusplus
extern "C" {
#endif
void check_for_failure_injection(failure_injection_stage_t req);
#ifdef __cplusplus
}
#endif
#endif // HAS_UPSI_FAILURE_INJECTION
