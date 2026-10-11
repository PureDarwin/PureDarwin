/*
 * IO80211Controller.cpp - the base for 802.11 drivers.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include "IO80211Controller.h"
#include "IO80211Interface.h"
#include "pd80211_glue.h"
#include <IOKit/IOLib.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/network/IOOutputQueue.h>
#include <sys/errno.h>

#define super IOEthernetController
OSDefineMetaClassAndAbstractStructors(IO80211Controller, IOEthernetController)

bool IO80211Controller::init(OSDictionary *properties)
{
    if (!super::init(properties))
        return false;
    pd80211_init();
    fIC = NULL;
    fInterface = NULL;
    fEnabled = false;
    return true;
}

void IO80211Controller::free()
{
    OSSafeReleaseNULL(fInterface);
    super::free();
}

// one automatic medium, the link speed is whatever the radio negotiated
bool IO80211Controller::publishMedia()
{
    OSDictionary *media = OSDictionary::withCapacity(1);
    IONetworkMedium *automatic = IONetworkMedium::medium(kIOMediumIEEE80211Auto, 0);
    bool ok = media && automatic && IONetworkMedium::addMedium(media, automatic) && publishMediumDictionary(media);

    OSSafeReleaseNULL(automatic);
    OSSafeReleaseNULL(media);
    return ok;
}

bool IO80211Controller::attach80211(struct ieee80211com *ic)
{
    fIC = ic;
    pd80211_hook(ic, this, &IO80211Controller::inputFrame, &IO80211Controller::linkChanged);
    if (!publishMedia())
        IOLog("IO80211Controller: no medium published\n");
    if (!attachInterface((IONetworkInterface **)&fInterface, true))
        return false;
    setLinkStatus(kIONetworkLinkValid, NULL);
    return true;
}

void IO80211Controller::detach80211()
{
    if (fEnabled && fIC)
        pd80211_set_up(fIC, 0);
    fEnabled = false;
    if (fInterface)
        detachInterface(fInterface, true);
    fIC = NULL;
}

IONetworkInterface *IO80211Controller::createInterface()
{
    IO80211Interface *netif = new IO80211Interface;

    if (netif && !netif->init(this)) {
        netif->release();
        return NULL;
    }
    return netif;
}

void IO80211Controller::inputFrame(void *owner, mbuf_t m)
{
    IO80211Controller *self = (IO80211Controller *)owner;

    if (self->fInterface == NULL) {
        mbuf_freem(m);
        return;
    }
    self->fInterface->inputPacket(m, (UInt32)mbuf_pkthdr_len(m), IONetworkInterface::kInputOptionQueuePacket);
    self->fInterface->flushInputQueue();
}

void IO80211Controller::linkChanged(void *owner, int up)
{
    IO80211Controller *self = (IO80211Controller *)owner;

    IOLog("IO80211Controller: link %s\n", up ? "up" : "down");
    self->setLinkStatus(up ? kIONetworkLinkValid | kIONetworkLinkActive : kIONetworkLinkValid, NULL);
}

IOReturn IO80211Controller::enable(IONetworkInterface *interface)
{
    if (fIC == NULL)
        return kIOReturnNotReady;
    if (!fEnabled && pd80211_set_up(fIC, 1) != 0)
        return kIOReturnIOError;
    fEnabled = true;
    return kIOReturnSuccess;
}

IOReturn IO80211Controller::disable(IONetworkInterface *interface)
{
    if (fEnabled && fIC)
        pd80211_set_up(fIC, 0);
    fEnabled = false;
    setLinkStatus(kIONetworkLinkValid, NULL);
    return kIOReturnSuccess;
}

UInt32 IO80211Controller::outputPacket(mbuf_t m, void *param)
{
    if (fIC == NULL) {
        mbuf_freem(m);
        return kIOReturnOutputDropped;
    }
    return pd80211_output(fIC, m) == 0 ? kIOReturnOutputSuccess : kIOReturnOutputDropped;
}

IOReturn IO80211Controller::getHardwareAddress(IOEthernetAddress *addr)
{
    if (fIC == NULL)
        return kIOReturnNotReady;
    memcpy(addr->bytes, pd80211_macaddr(fIC), kIOEthernetAddressSize);
    return kIOReturnSuccess;
}

IOReturn IO80211Controller::setPromiscuousMode(bool active)
{
    return kIOReturnSuccess;
}

IOReturn IO80211Controller::setMulticastMode(bool active)
{
    return kIOReturnSuccess;
}

IOReturn IO80211Controller::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
    return kIOReturnSuccess;
}

const OSString *IO80211Controller::newVendorString() const
{
    return OSString::withCString("PureDarwin");
}

const OSString *IO80211Controller::newModelString() const
{
    return OSString::withCString("802.11");
}

// each request checks its buffer is big enough, a get reports the size it filled
#define NEED(type) do { if (*len < sizeof(type)) return EINVAL; *len = sizeof(type); } while (0)

SInt32 IO80211Controller::apple80211Request(bool set, int type, int value, void *data, uint32_t *len)
{
    if (fIC == NULL)
        return ENXIO;

    if (set) {
        switch (type) {
        case APPLE80211_IOC_SCAN_REQ:
            if (*len < sizeof(struct apple80211_scan_data))
                return pd80211_scan(fIC, NULL);
            return pd80211_scan(fIC, (struct apple80211_scan_data *)data);
        case APPLE80211_IOC_ASSOCIATE:
            NEED(struct apple80211_assoc_data);
            return pd80211_associate(fIC, (struct apple80211_assoc_data *)data);
        case APPLE80211_IOC_DISASSOCIATE:
            return pd80211_disassociate(fIC);
        case APPLE80211_IOC_POWER:
            NEED(struct apple80211_power_data);
            return pd80211_set_up(fIC, ((struct apple80211_power_data *)data)->power_state != 0);
        case APPLE80211_IOC_PHY_MODE:
            NEED(struct apple80211_phymode_data);
            return pd80211_set_band(fIC, ((struct apple80211_phymode_data *)data)->phy_mode);
        default:
            return EOPNOTSUPP;
        }
    }

    switch (type) {
    case APPLE80211_IOC_STATE: {
        NEED(struct apple80211_state_data);
        struct apple80211_state_data *out = (struct apple80211_state_data *)data;
        out->version = APPLE80211_VERSION;
        out->state = pd80211_state(fIC);
        return 0;
    }
    case APPLE80211_IOC_SSID:
        NEED(struct apple80211_ssid_data);
        return pd80211_ssid(fIC, (struct apple80211_ssid_data *)data);
    case APPLE80211_IOC_BSSID:
        NEED(struct apple80211_bssid_data);
        return pd80211_bssid(fIC, (struct apple80211_bssid_data *)data);
    case APPLE80211_IOC_CHANNEL:
        NEED(struct apple80211_channel);
        return pd80211_channel(fIC, (struct apple80211_channel *)data);
    case APPLE80211_IOC_RSSI:
        NEED(struct apple80211_rssi_data);
        return pd80211_rssi(fIC, (struct apple80211_rssi_data *)data);
    case APPLE80211_IOC_PHY_MODE:
        NEED(struct apple80211_phymode_data);
        return pd80211_phymode(fIC, (struct apple80211_phymode_data *)data);
    case APPLE80211_IOC_CARD_CAPABILITIES:
        NEED(struct apple80211_capability_data);
        return pd80211_capabilities(fIC, (struct apple80211_capability_data *)data);
    case APPLE80211_IOC_POWER: {
        NEED(struct apple80211_power_data);
        struct apple80211_power_data *out = (struct apple80211_power_data *)data;
        out->version = APPLE80211_VERSION;
        out->power_state = pd80211_is_up(fIC);
        return 0;
    }
    case APPLE80211_IOC_HW_ADDR: {
        NEED(struct apple80211_hwaddr_data);
        struct apple80211_hwaddr_data *out = (struct apple80211_hwaddr_data *)data;
        out->version = APPLE80211_VERSION;
        memcpy(out->addr, pd80211_macaddr(fIC), APPLE80211_ADDR_LEN);
        return 0;
    }
    case APPLE80211_IOC_SCAN_RESULT:
        NEED(struct apple80211_scan_result);
        return pd80211_scan_result(fIC, value, (struct apple80211_scan_result *)data);
    default:
        return EOPNOTSUPP;
    }
}
