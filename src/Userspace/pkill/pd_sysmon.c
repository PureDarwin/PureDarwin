// pd_sysmon.c - the process table pkill asks libsysmon for, from proc_pidinfo and KERN_PROCARGS2
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "sysmon.h"

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/proc_info.h>
#include <libproc.h>
#include <stdlib.h>
#include <string.h>
#include <Block.h>

enum { PD_SYSMON_REQUEST = 1, PD_SYSMON_TABLE };

struct pd_sysmon_request {
	int refs;
	int kind;
	void (^handler)(sysmon_table_t);
};

struct pd_sysmon_row {
	xpc_object_t values[SYSMON_ATTR_PROC_COUNT];
};

struct pd_sysmon_table {
	int refs;
	int kind;
	size_t count;
	struct pd_sysmon_row *rows;
};

// requests and tables both start with a reference count and their kind
struct pd_sysmon_object {
	int refs;
	int kind;
};

sysmon_request_t
sysmon_request_create(sysmon_request_type_t type, void (^handler)(sysmon_table_t table))
{
	struct pd_sysmon_request *request = calloc(1, sizeof(*request));

	(void)type;
	if (request == NULL)
		return NULL;
	request->refs = 1;
	request->kind = PD_SYSMON_REQUEST;
	request->handler = Block_copy(handler);
	return request;
}

// the table always carries every attribute, asking for one changes nothing
void
sysmon_request_add_attribute(sysmon_request_t request, sysmon_attribute_t attribute)
{
	(void)request;
	(void)attribute;
}

// argv as KERN_PROCARGS2 lays it out: argc, the executable path, padding, then argc strings
static xpc_object_t
copy_arguments(pid_t pid)
{
	int mib[3] = { CTL_KERN, KERN_PROCARGS2, pid };
	int argmax_mib[2] = { CTL_KERN, KERN_ARGMAX };
	int argmax = 0, argc;
	size_t size = sizeof(argmax);
	char *buf, *p, *end;
	xpc_object_t args;

	if (sysctl(argmax_mib, 2, &argmax, &size, NULL, 0) != 0 || argmax <= 0)
		return NULL;
	buf = malloc((size_t)argmax);
	if (buf == NULL)
		return NULL;
	size = (size_t)argmax;
	if (sysctl(mib, 3, buf, &size, NULL, 0) != 0 || size < sizeof(int)) {
		free(buf);
		return NULL;
	}
	end = buf + size;
	memcpy(&argc, buf, sizeof(argc));
	p = buf + sizeof(argc);
	p += strnlen(p, (size_t)(end - p));
	while (p < end && *p == '\0')
		p++;
	args = xpc_array_create(NULL, 0);
	while (argc-- > 0 && p < end) {
		size_t len = strnlen(p, (size_t)(end - p));
		char *arg = strndup(p, len);

		if (arg != NULL) {
			xpc_object_t value = xpc_string_create(arg);

			xpc_array_append_value(args, value);
			xpc_release(value);
			free(arg);
		}
		p += len + 1;
	}
	free(buf);
	return args;
}

static void
fill_row(struct pd_sysmon_row *row, const struct proc_bsdinfo *info)
{
	const char *name = info->pbi_name[0] != '\0' ? info->pbi_name : info->pbi_comm;
	char comm[sizeof(info->pbi_name) + 1];

	strlcpy(comm, name, sizeof(comm));
	row->values[SYSMON_ATTR_PROC_PID] = xpc_uint64_create(info->pbi_pid);
	row->values[SYSMON_ATTR_PROC_FLAGS] = xpc_uint64_create(info->pbi_flags);
	row->values[SYSMON_ATTR_PROC_UID] = xpc_uint64_create(info->pbi_uid);
	row->values[SYSMON_ATTR_PROC_COMM] = xpc_string_create(comm);
	row->values[SYSMON_ATTR_PROC_ARGUMENTS] = copy_arguments((pid_t)info->pbi_pid);
	row->values[SYSMON_ATTR_PROC_RUID] = xpc_uint64_create(info->pbi_ruid);
	row->values[SYSMON_ATTR_PROC_RGID] = xpc_uint64_create(info->pbi_rgid);
	row->values[SYSMON_ATTR_PROC_PPID] = xpc_uint64_create(info->pbi_ppid);
	row->values[SYSMON_ATTR_PROC_PGID] = xpc_uint64_create(info->pbi_pgid);
	row->values[SYSMON_ATTR_PROC_TDEV] = xpc_uint64_create(info->e_tdev);
	// xpc dates are nanoseconds since 1970
	row->values[SYSMON_ATTR_PROC_START] = xpc_date_create((int64_t)info->pbi_start_tvsec * 1000000000LL +
	    (int64_t)info->pbi_start_tvusec * 1000LL);
}

void
sysmon_request_execute(sysmon_request_t request)
{
	struct pd_sysmon_table *table = calloc(1, sizeof(*table));
	pid_t *pids = NULL;
	int n, cap = 0;

	if (table == NULL)
		return;
	table->refs = 1;
	table->kind = PD_SYSMON_TABLE;
	// the process count can grow between the two calls, so ask again with room to spare
	for (int tries = 0; tries < 4; tries++) {
		n = proc_listallpids(NULL, 0);
		if (n <= 0)
			break;
		cap = n + 64;
		free(pids);
		pids = calloc((size_t)cap, sizeof(*pids));
		if (pids == NULL)
			break;
		n = proc_listallpids(pids, cap * (int)sizeof(*pids));
		if (n > 0 && n < cap)
			break;
	}
	if (pids != NULL && n > 0) {
		table->rows = calloc((size_t)n, sizeof(*table->rows));
		for (int i = 0; table->rows != NULL && i < n; i++) {
			struct proc_bsdinfo info;

			if (pids[i] <= 0)
				continue;
			if (proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != (int)sizeof(info))
				continue;
			fill_row(&table->rows[table->count++], &info);
		}
	}
	free(pids);
	request->handler(table);
	sysmon_release(table);
}

void *
sysmon_retain(void *object)
{
	if (object != NULL)
		((struct pd_sysmon_object *)object)->refs++;
	return object;
}

void
sysmon_release(void *object)
{
	struct pd_sysmon_object *header = object;

	if (header == NULL || --header->refs > 0)
		return;
	if (header->kind == PD_SYSMON_TABLE) {
		struct pd_sysmon_table *table = object;

		for (size_t i = 0; i < table->count; i++) {
			for (int a = 0; a < SYSMON_ATTR_PROC_COUNT; a++) {
				if (table->rows[i].values[a] != NULL)
					xpc_release(table->rows[i].values[a]);
			}
		}
		free(table->rows);
	} else if (header->kind == PD_SYSMON_REQUEST) {
		Block_release(((struct pd_sysmon_request *)object)->handler);
	}
	free(object);
}

size_t
sysmon_table_get_count(sysmon_table_t table)
{
	return table != NULL ? table->count : 0;
}

sysmon_row_t
sysmon_table_get_row(sysmon_table_t table, size_t index)
{
	return table != NULL && index < table->count ? &table->rows[index] : NULL;
}

xpc_object_t
sysmon_row_get_value(sysmon_row_t row, sysmon_attribute_t attribute)
{
	if (row == NULL || attribute < 0 || attribute >= SYSMON_ATTR_PROC_COUNT)
		return NULL;
	return row->values[attribute];
}
