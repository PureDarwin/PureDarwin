// pd_uvm_common.c - PD's own uvm_common for nvidia-uvm, written from the declarations in NVIDIA's MIT uvm_common.h
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// NVIDIA's uvm_common.c is the package's one GPL file. It is left out unread: everything here is PD's.
#include "uvm_common.h"
#include "uvm_linux.h"

// what NVIDIA exposes as module parameters, with PD's fixed defaults: release asserts on, everything else off
int uvm_enable_builtin_tests = 0;
int uvm_release_asserts = 1;
int uvm_release_asserts_dump_stack = 0;
int uvm_release_asserts_set_global_error = 0;
bool uvm_release_asserts_set_global_error_for_tests = false;
static int pd_uvm_debug_prints = 0;

extern int PE_parse_boot_argn(const char *arg_string, void *arg_ptr, int max_arg);

// the pduvmdebug boot-arg turns on nvidia-uvm's own error and debug messages, as its uvm_debug_prints parameter
bool uvm_debug_prints_enabled(void)
{
    static int read;

    if (!read) {
        int value = 0;

        if (PE_parse_boot_argn("pduvmdebug", &value, sizeof(value)))
            pd_uvm_debug_prints = value;
        read = 1;
    }
    return pd_uvm_debug_prints != 0;
}

// places to break on in a debugger; they do nothing themselves
void on_uvm_test_fail(void)
{
}

void on_uvm_assert(void)
{
}

// xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx, lower-case hex
void uvm_uuid_string(char *buffer, const NvProcessorUuid *uuid)
{
    static const char hex[] = "0123456789abcdef";
    const NvU8 *b = uuid->uuid;
    char *out = buffer;
    int i;

    for (i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            *out++ = '-';
        *out++ = hex[b[i] >> 4];
        *out++ = hex[b[i] & 0xf];
    }
    *out = '\0';
}

// how long a spin may run before its caller is told to warn, and again after each warning
#define PD_UVM_SPIN_WARN_NS (10ULL * 1000 * 1000 * 1000)

NV_STATUS uvm_spin_loop(uvm_spin_loop_t *spin)
{
    NvU64 now;

    if (!in_interrupt())
        cond_resched();
    else
        cpu_relax();

    now = NV_GETTIME();
    if (now - spin->print_time_ns >= PD_UVM_SPIN_WARN_NS) {
        spin->print_time_ns = now;
        return NV_ERR_TIMEOUT_RETRY;
    }
    return NV_OK;
}

// the process and thread ids of the caller, as numbers that are only printed and compared
unsigned uvm_get_stale_process_id(void)
{
    // the process is its thread group leader
    struct task_struct *leader = current->group_leader ? current->group_leader : current;
    return (unsigned)task_pid_nr(leader);
}

unsigned uvm_get_stale_thread_id(void)
{
    return (unsigned)task_pid_nr(current);
}

// errno values and NV_STATUS codes, both ways; anything unlisted is a generic error
static const struct {
    int err;
    NV_STATUS status;
} pd_uvm_errno_map[] = {
    { 0,          NV_OK },
    { -E2BIG,     NV_ERR_INVALID_ARGUMENT },
    { -EINVAL,    NV_ERR_INVALID_ARGUMENT },
    { -EBUSY,     NV_ERR_BUSY_RETRY },
    { -EAGAIN,    NV_ERR_BUSY_RETRY },
    { -EEXIST,    NV_ERR_IN_USE },
    { -EFAULT,    NV_ERR_INVALID_ADDRESS },
    { -ENOMEM,    NV_ERR_NO_MEMORY },
    { -ENOENT,    NV_ERR_OBJECT_NOT_FOUND },
    { -ENODEV,    NV_ERR_INVALID_DEVICE },
    { -ENOSPC,    NV_ERR_INSUFFICIENT_RESOURCES },
    { -ENOTTY,    NV_ERR_NOT_SUPPORTED },
    { -ENOSYS,    NV_ERR_NOT_SUPPORTED },
    { -EOPNOTSUPP, NV_ERR_NOT_SUPPORTED },
    { -EPERM,     NV_ERR_INSUFFICIENT_PERMISSIONS },
    { -EACCES,    NV_ERR_INSUFFICIENT_PERMISSIONS },
    { -ERANGE,    NV_ERR_OUT_OF_RANGE },
    { -ETIMEDOUT, NV_ERR_TIMEOUT },
    { -EINTR,     NV_ERR_SIGNAL_PENDING },
    { -ERESTARTSYS, NV_ERR_SIGNAL_PENDING },
    { -EIO,       NV_ERR_GENERIC },
    { -EBADF,     NV_ERR_INVALID_ARGUMENT },
};

NV_STATUS errno_to_nv_status(int errnoCode)
{
    size_t i;

    if (errnoCode > 0)
        errnoCode = -errnoCode;
    for (i = 0; i < ARRAY_SIZE(pd_uvm_errno_map); i++)
        if (pd_uvm_errno_map[i].err == errnoCode)
            return pd_uvm_errno_map[i].status;
    return NV_ERR_GENERIC;
}

int nv_status_to_errno(NV_STATUS status)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(pd_uvm_errno_map); i++)
        if (pd_uvm_errno_map[i].status == status)
            return pd_uvm_errno_map[i].err;
    return -EIO;
}
