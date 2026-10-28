// PureDarwin ColorSync: display identity so far
#ifndef COLORSYNC_COLORSYNC_H
#define COLORSYNC_COLORSYNC_H

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

CF_EXTERN_C_BEGIN

CF_EXPORT CFUUIDRef CGDisplayCreateUUIDFromDisplayID(uint32_t displayID);
CF_EXPORT uint32_t CGDisplayGetDisplayIDFromUUID(CFUUIDRef uuid);

CF_EXTERN_C_END

#endif
