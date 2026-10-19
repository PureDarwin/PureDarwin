// IOGraphicsLib.h - the user side of IOGraphics, registry keys and types for framebuffers and displays
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// the user side of IOGraphics: the registry keys and types for framebuffers and displays.
// PD's displays are the compositor's outputs, so the framebuffer client calls are not offered
#ifndef _IOKIT_IOGRAPHICSLIB_H
#define _IOKIT_IOGRAPHICSLIB_H

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>

__BEGIN_DECLS

enum {
    kIODisplayMatchingInfo = 0x00000100,
    kIODisplayOnlyPreferredName = 0x00000200,
    kIODisplayNoProductName = 0x00000400,
};

// the display's registry properties, the EDID and names among them
CFDictionaryRef IODisplayCreateInfoDictionary(io_service_t framebuffer, IOOptionBits options);
CFDictionaryRef IOCreateDisplayInfoDictionary(io_service_t framebuffer, IOOptionBits options);

__END_DECLS

#endif
