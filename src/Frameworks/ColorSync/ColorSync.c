// the display UUIDs ColorSync hands out, stable for a monitor because they come from its EDID identity
#include <ColorSync/ColorSync.h>
#include <string.h>

static CFUUIDBytes displayBytes(uint32_t display)
{
    uint32_t vendor = CGDisplayVendorNumber(display), model = CGDisplayModelNumber(display);
    uint32_t serial = CGDisplaySerialNumber(display), unit = CGDisplayUnitNumber(display);
    CFUUIDBytes b = {
        vendor >> 24, vendor >> 16, vendor >> 8, vendor,
        model >> 24, model >> 16, (model >> 8 & 0x0f) | 0x40, model,
        (serial >> 24 & 0x3f) | 0x80, serial >> 16, serial >> 8, serial,
        unit >> 24, unit >> 16, unit >> 8, unit,
    };

    return b;
}

CFUUIDRef CGDisplayCreateUUIDFromDisplayID(uint32_t displayID)
{
    return CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, displayBytes(displayID));
}

uint32_t CGDisplayGetDisplayIDFromUUID(CFUUIDRef uuid)
{
    CGDirectDisplayID displays[16];
    uint32_t count = 0;
    CFUUIDBytes want;

    if (!uuid || CGGetOnlineDisplayList(16, displays, &count) != kCGErrorSuccess)
        return 0;
    want = CFUUIDGetUUIDBytes(uuid);
    for (uint32_t i = 0; i < count; i++) {
        CFUUIDBytes have = displayBytes(displays[i]);

        if (memcmp(&have, &want, sizeof(want)) == 0)
            return displays[i];
    }
    return 0;
}
