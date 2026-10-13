/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// HID over I2C devices (PNP0C50) from the ACPI namespace, published the way a device tree describes them:
// a controller nub with its registers, and a child per device with reg, compatible and hid-descr-addr

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>

extern "C" {
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include <uacpi/resources.h>
#include <uacpi/namespace.h>
}

#include "PDACPIPlatformExpert.h"

extern "C" void kprintf(const char *fmt, ...);

// the HID over I2C _DSM, 3CDFF6F7-4267-4555-AD05-B30A3D8938DE, as it sits in a buffer
static const uacpi_u8 kHidI2cDsm[16] = { 0xF7, 0xF6, 0xDF, 0x3C, 0x67, 0x42, 0x55, 0x45,
                                         0xAD, 0x05, 0xB3, 0x0A, 0x3D, 0x89, 0x38, 0xDE };

static IOService *sExpertNub;
static OSArray *sNewControllers;

struct pd_i2c_conn {
    bool found;
    uacpi_u16 addr;
    uacpi_u32 speed;
    char controller[128];
};

struct pd_i2c_regs {
    uacpi_u64 base;
    uacpi_u64 length;
};

static uacpi_iteration_decision pd_i2c_conn_res(void *user, uacpi_resource *res)
{
    pd_i2c_conn *c = (pd_i2c_conn *)user;

    if (res->type != UACPI_RESOURCE_TYPE_SERIAL_I2C_CONNECTION || c->found)
        return UACPI_ITERATION_DECISION_CONTINUE;
    const uacpi_resource_i2c_connection *i2c = &res->i2c_connection;
    if (i2c->addressing_mode != UACPI_I2C_7BIT || i2c->common.source.string == UACPI_NULL)
        return UACPI_ITERATION_DECISION_CONTINUE;
    c->found = true;
    c->addr = i2c->slave_address;
    c->speed = i2c->connection_speed;
    strlcpy(c->controller, i2c->common.source.string, sizeof(c->controller));
    return UACPI_ITERATION_DECISION_BREAK;
}

static uacpi_iteration_decision pd_i2c_mmio_res(void *user, uacpi_resource *res)
{
    pd_i2c_regs *r = (pd_i2c_regs *)user;

    if (r->base != 0)
        return UACPI_ITERATION_DECISION_BREAK;
    if (res->type == UACPI_RESOURCE_TYPE_FIXED_MEMORY32) {
        r->base = res->fixed_memory32.address;
        r->length = res->fixed_memory32.length;
    } else if (res->type == UACPI_RESOURCE_TYPE_MEMORY32) {
        r->base = res->memory32.minimum;
        r->length = res->memory32.length;
    } else if (res->type == UACPI_RESOURCE_TYPE_ADDRESS64) {
        r->base = res->address64.minimum;
        r->length = res->address64.address_length;
    }
    return UACPI_ITERATION_DECISION_CONTINUE;
}

// the register the HID descriptor sits at, from _DSM function 1
static bool pd_i2c_hid_descr(uacpi_namespace_node *node, uacpi_u64 *out)
{
    uacpi_data_view uuid;
    uacpi_object *args[4];
    uacpi_object_array arr, empty = { UACPI_NULL, 0 };
    uacpi_status st;

    uuid.const_bytes = kHidI2cDsm;
    uuid.length = sizeof(kHidI2cDsm);
    args[0] = uacpi_object_create_buffer(uuid);
    args[1] = uacpi_object_create_integer(1);
    args[2] = uacpi_object_create_integer(1);
    args[3] = uacpi_object_create_package(empty);
    arr.objects = args;
    arr.count = 4;
    st = UACPI_STATUS_INVALID_ARGUMENT;
    if (args[0] && args[1] && args[2] && args[3])
        st = uacpi_eval_integer(node, "_DSM", &arr, out);
    for (int i = 0; i < 4; i++) {
        if (args[i])
            uacpi_object_unref(args[i]);
    }
    return st == UACPI_STATUS_OK;
}

static void pd_set_data(IORegistryEntry *e, const char *key, const void *bytes, unsigned len)
{
    OSData *d = OSData::withBytes(bytes, len);
    if (d) {
        e->setProperty(key, d);
        d->release();
    }
}

static void pd_set_u32(IORegistryEntry *e, const char *key, uacpi_u64 v)
{
    UInt32 x = (UInt32)v;
    pd_set_data(e, key, &x, sizeof(x));
}

// one nub per controller, found again by its ACPI path when a second device sits on the same bus
static IOService *pd_i2c_controller(uacpi_namespace_node *ctrl, const char *path)
{
    OSIterator *it = sExpertNub->getChildIterator(gIOServicePlane);
    IORegistryEntry *e;
    IOService *nub = NULL;

    while (it && (e = (IORegistryEntry *)it->getNextObject()) != NULL) {
        OSString *p = OSDynamicCast(OSString, e->getProperty("acpi-path"));
        if (p && p->isEqualTo(path)) {
            nub = OSDynamicCast(IOService, e);
            break;
        }
    }
    OSSafeReleaseNULL(it);
    if (nub)
        return nub;

    pd_i2c_regs regs = { 0, 0 };
    uacpi_id_string *hid = UACPI_NULL;
    char name[16] = "i2c";

    uacpi_for_each_device_resource(ctrl, "_CRS", pd_i2c_mmio_res, &regs);
    if (regs.base == 0 || regs.length == 0) {
        kprintf("PDACPIPlatform: i2c controller %s has no registers\n", path);
        return NULL;
    }
    if (uacpi_eval_hid(ctrl, &hid) == UACPI_STATUS_OK) {
        strlcpy(name, hid->value, sizeof(name));
        uacpi_free_id_string(hid);
    }

    IOPlatformDevice *dev = new IOPlatformDevice;
    if (dev == NULL || !dev->init()) {
        OSSafeReleaseNULL(dev);
        return NULL;
    }
    dev->setName(name);
    dev->setName(name, gIODTPlane);
    dev->setProperty("acpi-path", path);
    pd_set_data(dev, "compatible", name, (unsigned)strlen(name) + 1);
    IODeviceMemory *mem = IODeviceMemory::withRange((IOPhysicalAddress)regs.base, (IOPhysicalLength)regs.length);
    if (mem) {
        OSArray *ranges = OSArray::withObjects((const OSObject **)&mem, 1);
        dev->setDeviceMemory(ranges);
        OSSafeReleaseNULL(ranges);
        mem->release();
    }
    if (!dev->attach(sExpertNub)) {
        dev->release();
        return NULL;
    }
    kprintf("PDACPIPlatform: i2c controller %s (%s) regs 0x%llx+0x%llx\n", path, name, regs.base, regs.length);
    if (sNewControllers)
        sNewControllers->setObject(dev);
    dev->release();
    return dev;
}

static uacpi_iteration_decision pd_found_i2c_hid(void *, uacpi_namespace_node *node, uacpi_u32)
{
    pd_i2c_conn conn;
    uacpi_u32 sta = 0;
    uacpi_u64 descr = 0;
    uacpi_namespace_node *ctrl = UACPI_NULL;
    const uacpi_char *path = uacpi_namespace_node_generate_absolute_path(node);

    bzero(&conn, sizeof(conn));
    if (uacpi_eval_sta(node, &sta) == UACPI_STATUS_OK && !(sta & 1))
        goto out;
    uacpi_for_each_device_resource(node, "_CRS", pd_i2c_conn_res, &conn);
    if (!conn.found) {
        kprintf("PDACPIPlatform: %s has no I2C connection\n", path);
        goto out;
    }
    if (!pd_i2c_hid_descr(node, &descr)) {
        kprintf("PDACPIPlatform: %s has no HID descriptor address\n", path);
        goto out;
    }
    if (uacpi_namespace_node_find(UACPI_NULL, conn.controller, &ctrl) != UACPI_STATUS_OK || ctrl == UACPI_NULL) {
        kprintf("PDACPIPlatform: %s controller %s not found\n", path, conn.controller);
        goto out;
    }

    {
        IOService *bus = pd_i2c_controller(ctrl, conn.controller);
        if (bus == NULL)
            goto out;

        // a device tree child of the controller, which PDI2CController::publishDevices turns into a nub
        IORegistryEntry *child = new IORegistryEntry;
        if (child && child->init()) {
            static const char compat[] = "hid-over-i2c";
            child->setName("hid", gIODTPlane);
            pd_set_data(child, "compatible", compat, sizeof(compat));
            pd_set_u32(child, "reg", conn.addr);
            pd_set_u32(child, "hid-descr-addr", descr);
            pd_set_u32(child, "clock-frequency", conn.speed);
            child->setProperty("acpi-path", path);
            child->attachToParent(bus, gIODTPlane);
            kprintf("PDACPIPlatform: %s HID over I2C at 0x%02x on %s, descriptor at 0x%llx, %u Hz\n",
                    path, conn.addr, conn.controller, descr, conn.speed);
        }
        OSSafeReleaseNULL(child);
    }
out:
    uacpi_free_absolute_path(path);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

// controllers are registered after every device is attached, so a driver sees all its children at start
void pd_acpi_publish_i2c_hid(IOService *expert)
{
    sExpertNub = expert;
    sNewControllers = OSArray::withCapacity(2);
    uacpi_find_devices("PNP0C50", pd_found_i2c_hid, UACPI_NULL);
    uacpi_find_devices("ACPI0C50", pd_found_i2c_hid, UACPI_NULL);

    for (unsigned i = 0; sNewControllers && i < sNewControllers->getCount(); i++) {
        IOService *dev = OSDynamicCast(IOService, sNewControllers->getObject(i));
        if (dev)
            dev->registerService();
    }
    OSSafeReleaseNULL(sNewControllers);
}
