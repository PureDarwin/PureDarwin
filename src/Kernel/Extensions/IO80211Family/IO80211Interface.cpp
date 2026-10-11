/*
 * IO80211Interface.cpp - the en interface of an 802.11 controller.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include "IO80211Interface.h"
#include "IO80211Controller.h"
#include <IOKit/IOLib.h>
#include <sys/errno.h>
#include <sys/systm.h>

#define super IOEthernetInterface
OSDefineMetaClassAndStructors(IO80211Interface, IOEthernetInterface)

// the largest buffer a request may carry
#define kMaxRequestData 4096

SInt32 IO80211Interface::performCommand(IONetworkController *controller, unsigned long cmd, void *arg0, void *arg1)
{
    if (cmd != SIOCSA80211 && cmd != SIOCGA80211)
        return super::performCommand(controller, cmd, arg0, arg1);

    IO80211Controller *wifi = OSDynamicCast(IO80211Controller, controller);
    struct apple80211req *req = (struct apple80211req *)arg1;
    bool set = cmd == SIOCSA80211;

    if (wifi == NULL || req == NULL)
        return EINVAL;
    if (req->req_len > kMaxRequestData)
        return EINVAL;

    uint32_t size = req->req_len, len = size;
    void *buf = size ? IOMallocZero(size) : NULL;
    SInt32 error;

    if (len && buf == NULL)
        return ENOMEM;
    if (len && (error = copyin((user_addr_t)(uintptr_t)req->req_data, buf, len)) != 0) {
        IOFree(buf, size);
        return error;
    }
    error = wifi->apple80211Request(set, req->req_type, req->req_val, buf, &len);
    if (error == 0 && !set && len && len <= size)
        error = copyout(buf, (user_addr_t)(uintptr_t)req->req_data, len);
    if (error == 0 && !set)
        req->req_len = len;
    if (buf)
        IOFree(buf, size);
    return error;
}
