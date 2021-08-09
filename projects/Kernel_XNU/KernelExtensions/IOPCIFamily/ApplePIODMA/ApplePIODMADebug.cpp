//
//  ApplePIODMADebug.cpp
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/29/20.
//

#include <IOKit/apiodma/ApplePIODMADebug.h>

uint32_t applePIODMAgetDebugLoggingMask(const char* bootArg)
{
    uint32_t localDebugMask = 0;
    PE_parse_boot_argn(bootArg, &localDebugMask, sizeof(localDebugMask));

    // Include special masks that apply to all boot-args
    localDebugMask |= kApplePIODMADebugLoggingAlways;

    return localDebugMask;
}

uint32_t applePIODMAgetDebugLoggingMaskForMetaClass(const OSMetaClass* metaClass, const OSMetaClass* stopClass)
{
    uint32_t result       = 0;
    char     bootArg[256] = { 0 };

    while(   metaClass != NULL
          && metaClass != stopClass)
    {
        snprintf(bootArg, 256, "%s-debug", metaClass->getClassName());

        result |= applePIODMAgetDebugLoggingMask(bootArg);

        metaClass = metaClass->getSuperClass();
    }

    return result;
}
