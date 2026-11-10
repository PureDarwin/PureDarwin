// sysmon.h - the slice of libsysmon's process table that Apple's pkill reads
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// pd_sysmon.c answers it from sysctl and proc_pidinfo, every value an XPC object, so pkill.c builds unchanged

#ifndef PD_SYSMON_H
#define PD_SYSMON_H

#include <xpc/xpc.h>

typedef enum {
	SYSMON_REQUEST_TYPE_PROCESS = 1,
} sysmon_request_type_t;

typedef enum {
	SYSMON_ATTR_PROC_PID,
	SYSMON_ATTR_PROC_FLAGS,
	SYSMON_ATTR_PROC_UID,
	SYSMON_ATTR_PROC_COMM,
	SYSMON_ATTR_PROC_ARGUMENTS,
	SYSMON_ATTR_PROC_RUID,
	SYSMON_ATTR_PROC_RGID,
	SYSMON_ATTR_PROC_PPID,
	SYSMON_ATTR_PROC_PGID,
	SYSMON_ATTR_PROC_TDEV,
	SYSMON_ATTR_PROC_START,
	SYSMON_ATTR_PROC_COUNT,
} sysmon_attribute_t;

typedef struct pd_sysmon_request *sysmon_request_t;
typedef struct pd_sysmon_table *sysmon_table_t;
typedef struct pd_sysmon_row *sysmon_row_t;

sysmon_request_t sysmon_request_create(sysmon_request_type_t type, void (^handler)(sysmon_table_t table));
void sysmon_request_add_attribute(sysmon_request_t request, sysmon_attribute_t attribute);
void sysmon_request_execute(sysmon_request_t request);

void *sysmon_retain(void *object);
void sysmon_release(void *object);

size_t sysmon_table_get_count(sysmon_table_t table);
sysmon_row_t sysmon_table_get_row(sysmon_table_t table, size_t index);
xpc_object_t sysmon_row_get_value(sysmon_row_t row, sysmon_attribute_t attribute);

#endif
