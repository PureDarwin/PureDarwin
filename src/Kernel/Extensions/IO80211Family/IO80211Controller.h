/*
 * IO80211Controller.h - the base for 802.11 drivers. A driver brings its radio
 * up on net80211 and hands the ieee80211com over, the controller turns it into
 * an Ethernet-looking interface and answers the apple80211 requests.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#ifndef _IO80211CONTROLLER_H
#define _IO80211CONTROLLER_H

#include <IOKit/network/IOEthernetController.h>
#include "apple80211_ioctl.h"

struct ieee80211com;
class IO80211Interface;

class IO80211Controller : public IOEthernetController
{
    OSDeclareAbstractStructors(IO80211Controller)

public:
    virtual bool init(OSDictionary *properties) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;

    // the driver calls this once its radio is attached to net80211
    bool attach80211(struct ieee80211com *ic);
    void detach80211();
    struct ieee80211com *getIEEE80211Com() const { return fIC; }

    virtual IOReturn enable(IONetworkInterface *interface) APPLE_KEXT_OVERRIDE;
    virtual IOReturn disable(IONetworkInterface *interface) APPLE_KEXT_OVERRIDE;
    virtual UInt32 outputPacket(mbuf_t m, void *param) APPLE_KEXT_OVERRIDE;
    virtual IOReturn getHardwareAddress(IOEthernetAddress *addr) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setPromiscuousMode(bool active) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setMulticastMode(bool active) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setMulticastList(IOEthernetAddress *addrs, UInt32 count) APPLE_KEXT_OVERRIDE;
    virtual IONetworkInterface *createInterface() APPLE_KEXT_OVERRIDE;
    virtual const OSString *newVendorString() const APPLE_KEXT_OVERRIDE;
    virtual const OSString *newModelString() const APPLE_KEXT_OVERRIDE;

    // SIOCSA80211 and SIOCGA80211 land here, data is the request's buffer in the kernel
    virtual SInt32 apple80211Request(bool set, int type, int value, void *data, uint32_t *len);

protected:
    struct ieee80211com *fIC;
    IO80211Interface *fInterface;
    bool fEnabled;

private:
    static void inputFrame(void *owner, mbuf_t m);
    static void linkChanged(void *owner, int up);
    bool publishMedia();
};

#endif
