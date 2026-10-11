/*
 * IO80211Interface.h - the en interface of an 802.11 controller. It is an
 * Ethernet interface that also answers SIOCSA80211 and SIOCGA80211.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#ifndef _IO80211INTERFACE_H
#define _IO80211INTERFACE_H

#include <IOKit/network/IOEthernetInterface.h>

class IO80211Interface : public IOEthernetInterface
{
    OSDeclareDefaultStructors(IO80211Interface)

public:
    virtual SInt32 performCommand(IONetworkController *controller, unsigned long cmd,
                                  void *arg0, void *arg1) APPLE_KEXT_OVERRIDE;
};

#endif
