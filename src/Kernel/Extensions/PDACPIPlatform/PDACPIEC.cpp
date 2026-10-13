/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// The ACPI embedded controller (PNP0C09): AML reaches its fields through the EmbeddedControl address space,
// and its GPE asks for a query whose number names the _Qxx method to run, which is how laptop buttons notify

#include <IOKit/IOLib.h>
#include <kern/thread_call.h>
#include <architecture/i386/pio.h>

extern "C" {
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/opregion.h>
#include <uacpi/resources.h>
#include <uacpi/utilities.h>
}

extern "C" void kprintf(const char *fmt, ...);

// status register
enum {
    kEcObf    = 1u << 0,    // output buffer full, data to read
    kEcIbf    = 1u << 1,    // input buffer full, the EC has not taken the last byte
    kEcSciEvt = 1u << 5,    // an event is waiting for a query
};

// commands
enum {
    kEcRead  = 0x80,
    kEcWrite = 0x81,
    kEcQuery = 0x84,
};

static uacpi_u16 sDataPort, sCmdPort;
static uacpi_namespace_node *sEcNode;
static uacpi_u16 sGpe;
static bool sHaveGpe;
static IOLock *sEcLock;
static thread_call_t sQueryCall;

static bool ec_wait(uacpi_u8 mask, uacpi_u8 want)
{
    for (int i = 0; i < 20000; i++) {
        if ((inb(sCmdPort) & mask) == want)
            return true;
        IODelay(10);
    }
    return false;
}

static bool ec_send_cmd(uacpi_u8 cmd)
{
    if (!ec_wait(kEcIbf, 0))
        return false;
    outb(sCmdPort, cmd);
    return true;
}

static bool ec_send_data(uacpi_u8 v)
{
    if (!ec_wait(kEcIbf, 0))
        return false;
    outb(sDataPort, v);
    return true;
}

static bool ec_recv_data(uacpi_u8 *v)
{
    if (!ec_wait(kEcObf, kEcObf))
        return false;
    *v = inb(sDataPort);
    return true;
}

static bool ec_read(uacpi_u8 addr, uacpi_u8 *v)
{
    return ec_send_cmd(kEcRead) && ec_send_data(addr) && ec_recv_data(v);
}

static bool ec_write(uacpi_u8 addr, uacpi_u8 v)
{
    return ec_send_cmd(kEcWrite) && ec_send_data(addr) && ec_send_data(v) && ec_wait(kEcIbf, 0);
}

// a multi-byte access is that many single-byte transactions, lowest address first
static uacpi_status ec_region(uacpi_region_op op, uacpi_handle op_data)
{
    if (op != UACPI_REGION_OP_READ && op != UACPI_REGION_OP_WRITE)
        return UACPI_STATUS_OK;

    uacpi_region_rw_data *rw = (uacpi_region_rw_data *)op_data;
    bool ok = true;

    IOLockLock(sEcLock);
    if (op == UACPI_REGION_OP_READ)
        rw->value = 0;
    for (uacpi_u8 i = 0; i < rw->byte_width && ok; i++) {
        uacpi_u8 addr = (uacpi_u8)(rw->offset + i), b;

        if (op == UACPI_REGION_OP_READ) {
            ok = ec_read(addr, &b);
            rw->value |= (uacpi_u64)b << (8 * i);
        } else {
            ok = ec_write(addr, (uacpi_u8)(rw->value >> (8 * i)));
        }
    }
    IOLockUnlock(sEcLock);
    if (!ok)
        kprintf("PDACPIPlatform: EC %s at 0x%llx timed out\n", op == UACPI_REGION_OP_READ ? "read" : "write",
                (unsigned long long)rw->offset);
    return ok ? UACPI_STATUS_OK : UACPI_STATUS_HARDWARE_TIMEOUT;
}

// every waiting event in turn, then the GPE goes back on
static void ec_query(thread_call_param_t, thread_call_param_t)
{
    for (int n = 0; n < 32; n++) {
        uacpi_u8 q = 0;
        bool ok;

        IOLockLock(sEcLock);
        ok = (inb(sCmdPort) & kEcSciEvt) && ec_send_cmd(kEcQuery) && ec_recv_data(&q);
        IOLockUnlock(sEcLock);
        if (!ok || q == 0)
            break;

        char method[5] = { '_', 'Q', "0123456789ABCDEF"[q >> 4], "0123456789ABCDEF"[q & 0xf], 0 };
        uacpi_status st = uacpi_eval(sEcNode, method, UACPI_NULL, UACPI_NULL);
        if (st != UACPI_STATUS_OK && st != UACPI_STATUS_NOT_FOUND)
            kprintf("PDACPIPlatform: EC %s failed: %s\n", method, uacpi_status_to_string(st));
    }
    uacpi_finish_handling_gpe(UACPI_NULL, sGpe);
}

static uacpi_interrupt_ret ec_gpe(uacpi_handle, uacpi_namespace_node *, uacpi_u16)
{
    thread_call_enter(sQueryCall);
    return UACPI_INTERRUPT_HANDLED;
}

struct ec_ports {
    int count;
    uacpi_u16 port[2];
};

static uacpi_iteration_decision ec_port_res(void *user, uacpi_resource *res)
{
    ec_ports *p = (ec_ports *)user;

    if (p->count >= 2)
        return UACPI_ITERATION_DECISION_BREAK;
    if (res->type == UACPI_RESOURCE_TYPE_IO)
        p->port[p->count++] = res->io.minimum;
    else if (res->type == UACPI_RESOURCE_TYPE_FIXED_IO)
        p->port[p->count++] = res->fixed_io.address;
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uacpi_iteration_decision ec_found(void *, uacpi_namespace_node *node, uacpi_u32)
{
    ec_ports ports = { 0, { 0, 0 } };

    if (sEcNode != UACPI_NULL)
        return UACPI_ITERATION_DECISION_BREAK;
    uacpi_for_each_device_resource(node, "_CRS", ec_port_res, &ports);
    if (ports.count < 2) {
        kprintf("PDACPIPlatform: EC without its two ports\n");
        return UACPI_ITERATION_DECISION_CONTINUE;
    }
    // the first is data, the second command and status
    sDataPort = ports.port[0];
    sCmdPort = ports.port[1];
    sEcNode = node;
    return UACPI_ITERATION_DECISION_BREAK;
}

// after the namespace is loaded and before it is initialized, so _INI and _REG methods can use the EC
void pd_acpi_ec_init(void)
{
    uacpi_u64 gpe = 0;
    uacpi_status st;

    uacpi_find_devices("PNP0C09", ec_found, UACPI_NULL);
    if (sEcNode == UACPI_NULL) {
        kprintf("PDACPIPlatform: no embedded controller\n");
        return;
    }
    sEcLock = IOLockAlloc();
    sQueryCall = thread_call_allocate(ec_query, NULL);
    if (sEcLock == NULL || sQueryCall == NULL)
        return;

    st = uacpi_install_address_space_handler(sEcNode, UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER, ec_region,
                                             UACPI_NULL);
    kprintf("PDACPIPlatform: EC data 0x%x command 0x%x, address space %s\n", sDataPort, sCmdPort,
            uacpi_status_to_string(st));

    if (uacpi_eval_simple_integer(sEcNode, "_GPE", &gpe) == UACPI_STATUS_OK) {
        sGpe = (uacpi_u16)gpe;
        st = uacpi_install_gpe_handler(UACPI_NULL, sGpe, UACPI_GPE_TRIGGERING_EDGE, ec_gpe, UACPI_NULL);
        if (st == UACPI_STATUS_OK)
            st = uacpi_enable_gpe(UACPI_NULL, sGpe);
        sHaveGpe = st == UACPI_STATUS_OK;
        kprintf("PDACPIPlatform: EC GPE %u %s\n", sGpe, uacpi_status_to_string(st));
    }
}

// an event may already be waiting from before the GPE was armed
void pd_acpi_ec_start(void)
{
    if (sHaveGpe && sQueryCall != NULL)
        thread_call_enter(sQueryCall);
}
