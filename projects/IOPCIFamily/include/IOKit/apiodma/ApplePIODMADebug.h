//
//  ApplePIODMADebug.h
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/29/20.
//

#ifndef ApplePIODMADebug_h
#define ApplePIODMADebug_h
#include <IOKit/IOTypes.h>
#include <IOKit/IOService.h>
#include <IOKit/IOTimeStamp.h>
#include <IOKit/IOLib.h>
#include <os/log.h>
#include <libkern/sysctl.h>

#ifdef __LP64__
#define kprintfNamespaceFormatString         "%06lu.%06u %s::%s: "
#define kprintfServiceFormatString           "%06lu.%06u %s:@%s %s::%s: "
#define kprintfFilterSafeServiceFormatString "%06lu.%06u Unknown@Unknown: %s::%s: "
#define kprintfObjectFormatString            "%06lu.%06u %s %s::%s: "
#define kprintfFilterSafeObjectFormatString  "%06lu.%06u Unknown: %s::%s: "
#else
#define kprintfNamespaceFormatString         "%06u.%06u %s::%s: "
#define kprintfServiceFormatString           "%06u.%06u %s:@%s %s::%s: "
#define kprintfFilterSafeServiceFormatString "%06u.%06u Unknown@Unknown: %s::%s: "
#define kprintfObjectFormatString            "%06u.%06u %s %s::%s: "
#define kprintfFilterSafeObjectFormatString  "%06u.%06u Unknown: %s::%s: "
#endif

#ifdef __LP64__
#define iologNamespaceFormatString "%06lu.%06u %s::%s: "
#define iologServiceFormatString   "%06lu.%06u %s:@%s %s::%s: "
#define iologObjectFormatString    "%06lu.%06u %s %s::%s: "
#else
#define iologNamespaceFormatString "%06u.%06u %s::%s: "
#define iologServiceFormatString   "%06u.%06u %s:@%s %s::%s: "
#define iologObjectFormatString    "%06u.%06u %s %s::%s: "
#endif

#define os_logNamespaceFormatString  "%s::%s: "
#define os_logServiceFormatString    "%s: %s::%s: "
#define os_logObjectFormatString     "%s: %s::%s: "

#define pioDMADebugServiceWithClass(mask, class, fmt, args...)                                                                         \
    do                                                                                                                                 \
    {                                                                                                                                  \
        if((mask) & _debugLoggingMask)                                                                                                 \
        {                                                                                                                              \
            clock_sec_t  seconds;                                                                                                      \
            clock_usec_t useconds;                                                                                                     \
            clock_get_system_microtime(&seconds, &useconds);                                                                           \
            if(((mask) & _debugLoggingMask) & kApplePIODMADebugLoggingAssert)                                                          \
            {                                                                                                                          \
                panic("((mask) & _debugLoggingMask) & kApplePIODMADebugLoggingAssert)");                                               \
            }                                                                                                                          \
            else if(((mask) & kApplePIODMADebugLoggingKprintf) || (_debugLoggingMask & kApplePIODMADebugLoggingKprintf))               \
            {                                                                                                                          \
                if(_debugLoggingMask & kApplePIODMADebugLoggingKprintf)                                                                \
                {                                                                                                                      \
                    kprintf(kprintfServiceFormatString fmt, seconds, useconds, getName(), getLocation(), #class, __FUNCTION__,##args); \
                }                                                                                                                      \
            }                                                                                                                          \
            else                                                                                                                       \
            {                                                                                                                          \
                IOLog(iologServiceFormatString fmt, seconds, useconds, getName(), getLocation(), #class, __FUNCTION__,##args);         \
            }                                                                                                                          \
        }                                                                                                                              \
    } while(0);

#define pioDMADebugObjectWithClass(mask, class, fmt, args...)                                                                               \
    {                                                                                                                                       \
        if((mask) & _debugLoggingMask & ~(kApplePIODMADebugLoggingGlobalMask))                                                              \
        {                                                                                                                                   \
            clock_sec_t  seconds;                                                                                                           \
            clock_usec_t useconds;                                                                                                          \
            clock_get_system_microtime(&seconds, &useconds);                                                                                \
            if(((mask) & kApplePIODMADebugLoggingKprintf) || (_debugLoggingMask & kApplePIODMADebugLoggingKprintf))                         \
            {                                                                                                                               \
                if(ml_at_interrupt_context() == false)                                                                                      \
                {                                                                                                                           \
                    kprintf(kprintfObjectFormatString fmt, seconds, useconds, getMetaClass()->getClassName(), #class, __FUNCTION__,##args); \
                }                                                                                                                           \
                else                                                                                                                        \
                {                                                                                                                           \
                    kprintf(kprintfFilterSafeObjectFormatString fmt, seconds, useconds, #class, __FUNCTION__,##args);                       \
                }                                                                                                                           \
            }                                                                                                                               \
            else                                                                                                                            \
            {                                                                                                                               \
                if(ml_at_interrupt_context() == false)                                                                                      \
                {                                                                                                                           \
                    IOLog(iologObjectFormatString fmt, seconds, useconds, getMetaClass()->getClassName(), #class, __FUNCTION__,##args);     \
                }                                                                                                                           \
            }                                                                                                                               \
                                                                                                                                            \
            if(((mask) & _debugLoggingMask) & kApplePIODMADebugLoggingAssert)                                                               \
            {                                                                                                                               \
                panic("((mask) & _debugLoggingMask) & kApplePIODMADebugLoggingAssert");                                                     \
            }                                                                                                                               \
        }                                                                                                                                   \
    }

enum tApplePIODMADebugLoggingMask
{
    // Class-specific logging masks
    kApplePIODMADebugLoggingAlways    = (1 << 0),    // 0x00000001
    kApplePIODMADebugLoggingError     = (1 << 1),    // 0x00000002
    kApplePIODMADebugLoggingVerbose   = (1 << 2),    // 0x00000004
    kApplePIODMADebugLoggingInit      = (1 << 3),    // 0x00000008
    kApplePIODMADebugLoggingIO        = (1 << 4),    // 0x00000010
    kApplePIODMADebugLoggingInterrupt = (1 << 5),    // 0x00000020

    // Global logging masks
    kApplePIODMADebugLoggingKprintf    = (1 << 30),  // 0x40000000
    kApplePIODMADebugLoggingAssert     = (1 << 31),  // 0x80000000
    kApplePIODMADebugLoggingGlobalMask = 0xff000000
};


uint32_t applePIODMAgetDebugLoggingMask(const char* bootArg);
uint32_t applePIODMAgetDebugLoggingMaskForMetaClass(const OSMetaClass* metaClass, const OSMetaClass* stopClass);

#endif /* ApplePIODMADebug_h */
