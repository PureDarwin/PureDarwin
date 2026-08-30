#ifndef _APPLEPS2MOUSE_H
#define _APPLEPS2MOUSE_H

#include <IOKit/IOService.h>
#include "ApplePS2MouseDevice.h"

class ApplePS2Mouse : public IOService
{
    OSDeclareDefaultStructors(ApplePS2Mouse);

private:
    ApplePS2MouseDevice * _device;
    bool                  _interruptInstalled;
    UInt8                 _mouseIndex;
    UInt8                 _packet[4];
    UInt8                 _packetByte;     /* bytes of _packet filled so far */
    UInt8                 _packetLength;   /* 3, or 4 once the wheel is on */
    UInt8                 _lastButtons;

    bool  sendCommand(UInt8 command);
    bool  setSampleRate(UInt8 rate);
    UInt8 identify();
    bool  enableWheel();
    void  setMouseEnable(bool enable);
    void  handlePacketByte(UInt8 data);
    void  dispatchPacket();

public:
    virtual bool init(OSDictionary * dict) APPLE_KEXT_OVERRIDE;
    virtual ApplePS2Mouse * probe(IOService * provider, SInt32 * score) APPLE_KEXT_OVERRIDE;

    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService * provider) APPLE_KEXT_OVERRIDE;

    /* interrupt path (called by controller with each raw byte) */
    static void interruptOccurred(void * target, UInt8 data);
};

#endif /* _APPLEPS2MOUSE_H */
