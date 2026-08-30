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
#include <kern/macro_help.h>
#include <os/base_private.h>
#include <os/log.h>
#include <stdbool.h>

#pragma once

extern os_log_t vm_log_handle;
extern bool vm_log_debug_enabled;
extern bool vm_log_to_serial;

#define _vm_log_with_type(type, format, ...) MACRO_BEGIN \
	if (os_unlikely(vm_log_to_serial)) { \
	        printf("vm: " format, ##__VA_ARGS__); \
	} else { \
	        os_log_with_startup_serial_and_type(vm_log_handle, type, "vm: " format, ##__VA_ARGS__); \
	} \
MACRO_END
#define vm_log(format, ...) _vm_log_with_type(OS_LOG_TYPE_DEFAULT, format, ##__VA_ARGS__)
#define vm_log_info(format, ...) _vm_log_with_type(OS_LOG_TYPE_INFO, format, ##__VA_ARGS__)
#define vm_log_debug(format, ...) \
MACRO_BEGIN \
	if (os_unlikely(vm_log_debug_enabled)) { \
	        _vm_log_with_type(OS_LOG_TYPE_DEBUG, format, ##__VA_ARGS__); \
	} \
MACRO_END
#define vm_log_error(format, ...) _vm_log_with_type(OS_LOG_TYPE_ERROR, format, ##__VA_ARGS__)
#define vm_log_fault(format, ...) _vm_log_with_type(OS_LOG_TYPE_FAULT, format, ##__VA_ARGS__)
