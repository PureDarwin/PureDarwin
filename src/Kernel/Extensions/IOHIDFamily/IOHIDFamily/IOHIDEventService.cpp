/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2013 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <AssertMacros.h>
#include <TargetConditionals.h>
#include <stdint.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/IOLib.h>
#include <IOKit/usb/IOUSBHostFamily.h>
#include <IOKit/hid/IOHIDEventServiceTypes.h>

#include "IOHIDKeys.h"
#include "IOHIDSystem.h"
#include "IOHIDEventService.h"
#include "IOHIDInterface.h"
#include "IOHIDPrivateKeys.h"
#include "AppleHIDUsageTables.h"
#include "OSStackRetain.h"
#include <sys/sysctl.h>
#include <kern/debug.h>

#if TARGET_OS_OSX
    #include "IOHIDPointing.h"
    #include "IOHIDKeyboard.h"
    #include "IOHIDConsumer.h"
    #include "IOHIDEvent.h"
#endif /* TARGET_OS_OSX */

#include "IOHIDEventData.h"

#include "IOHIDPrivate.h"
#include "IOHIDFamilyPrivate.h"
#include "IOHIDevicePrivateKeys.h"
#include "ev_private.h"
#include "IOHIDFamilyTrace.h"
#include "IOHIDDebug.h"
#include <stdatomic.h>

#include "IOHIDEventServiceUserClient.h"
#include "IOHIDEventServiceFastPathUserClient.h"

extern "C" int  kern_stack_snapshot_with_reason(char *reason);
extern "C" kern_return_t sysdiagnose_notify_user(uint32_t keycode);

enum {
    kBootProtocolNone   = 0,
    kBootProtocolKeyboard,
    kBootProtocolMouse
};

enum {
    kShimEventProcessor = 0x01
};

#define     kDefaultFixedResolution             (400 << 16)
#define     kDefaultScrollFixedResolution       (9 << 16)

#define     kMaxSystemAbsoluteRangeUnsigned     65535
#define     kMaxSystemAbsoluteRangeSigned       32767
#define     kMaxSystemBarrelPressure            kMaxSystemAbsoluteRangeSigned
#define     kMaxSystemTipPressure               kMaxSystemAbsoluteRangeUnsigned

#define     kDelayedOption                      (1<<31)

#define     NUB_LOCK                            if (_nubLock) IORecursiveLockLock(_nubLock)
#define     NUB_UNLOCK                          if (_nubLock) IORecursiveLockUnlock(_nubLock)


#define     SET_HID_PROPERTIES_EMBEDDED(service)                                \
        service->setProperty(kIOHIDPrimaryUsagePageKey, getPrimaryUsagePage(), 32); \
        service->setProperty(kIOHIDPrimaryUsageKey, getPrimaryUsage(), 32);



#define     SET_HID_PROPERTIES(service)                                     \
    service->setProperty(kIOHIDTransportKey, getTransport());               \
    service->setProperty(kIOHIDLocationIDKey, getLocationID(), 32);         \
    service->setProperty(kIOHIDVendorIDKey, getVendorID(), 32);             \
    service->setProperty(kIOHIDVendorIDSourceKey, getVendorIDSource(), 32); \
    service->setProperty(kIOHIDProductIDKey, getProductID(), 32);           \
    service->setProperty(kIOHIDVersionNumberKey, getVersion(), 32);         \
    service->setProperty(kIOHIDCountryCodeKey, getCountryCode(), 32);       \
    service->setProperty(kIOHIDManufacturerKey, getManufacturer());         \
    service->setProperty(kIOHIDProductKey, getProduct());                   \
    service->setProperty(kIOHIDSerialNumberKey, getSerialNumber());         \
    service->setProperty(kIOHIDDeviceUsagePairsKey, getDeviceUsagePairs()); \
    service->setProperty(kIOHIDReportIntervalKey, getReportInterval(), 32);

#define		_provider							_reserved->provider
#define     _workLoop                           _reserved->workLoop
#define     _deviceUsagePairs                   _reserved->deviceUsagePairs
#define     _commandGate                        _reserved->commandGate
#define     _keyboard                           _reserved->keyboard
#define     _multiAxis                          _reserved->multiAxis
#define     _digitizer                          _reserved->digitizer
#define     _relativePointer                    _reserved->relativePointer
#define     _absolutePointer                    _reserved->absolutePointer
#define     _keyboardShim                       _reserved->keyboardShim
#ifdef POINTING_SHIM_SUPPORT
#define     _pointingShim                       _reserved->pointingShim
#endif
#define     _debugMask                          _reserved->debugMask

#define     _clientDict                         _reserved->clientDict
#define     _eventMemory                        _reserved->eventMemory
#define     _eventMemLock                       _reserved->eventMemLock

#define     kDebuggerTriplePressDelayMS         1000
#define     kDebuggerLongDelayMS                5000
#define     kShutdownDelayForStackshot          4000
#define     kShutdownDelayForPanic              3500
#define     kATVChordDelayMS                    5000
#define     kDelayedStackshotMask               (1 << 31)


#define STACKSHOT_MASK_ATV          0x30        // ATV PlayPause + Volume-)
#define STACKSHOT_MASK_WATCH        0x0C        // Menu (Crown) + Help (Pill)


#if TARGET_OS_WATCH
// Watch
#define DELAYED_STACKSHOT_TIMEOUT   1000
#define DELAYED_STACKSHOT_MASK      (STACKSHOT_MASK_WATCH | kDelayedStackshotMask)
#else
// TV and all other (currently only TV is applicable)
#define DELAYED_STACKSHOT_TIMEOUT   5000
#define DELAYED_STACKSHOT_MASK      (STACKSHOT_MASK_ATV | kDelayedStackshotMask)
#endif


#define dispatch_workloop_sync(b)            \
if (!isInactive() && _commandGate) {         \
    _commandGate->runActionBlock(^IOReturn{  \
        if (isInactive()) {                  \
            return kIOReturnOffline;         \
        };                                   \
        b                                    \
        return kIOReturnSuccess;             \
    });                                      \
}

enum {
    kLegacyShimDisabled,
    kLegacyShimEnabledForSingleUserMode,
    kLegacyShimEnabled
};

//===========================================================================
// IOHIDClientData class
class IOHIDClientData : public OSObject
{
    OSDeclareDefaultStructors(IOHIDClientData)

    IOService * client;
    void *      context;
    void *      action;

public:
    static IOHIDClientData* withClientInfo(IOService *client, void* context, void * action);
    inline IOService *  getClient()     { return client; }
    inline void *       getContext()    { return context; }
    inline void *       getAction()     { return action; }
};

//===========================================================================
// IOHIDEventService class

#define super IOService

OSDefineMetaClassAndAbstractStructors( IOHIDEventService, IOService )
//====================================================================================================
// IOHIDEventService::init
//====================================================================================================
bool IOHIDEventService::init ( OSDictionary * properties )
{
    if (!super::init(properties))
        return false;

    _reserved = IONew(ExpansionData, 1);
    if (!_reserved) {
        return false;
    }
    
    bzero(_reserved, sizeof(ExpansionData));

    _nubLock = IORecursiveLockAlloc();
 
    _clientDictLock = IOLockAlloc();
    _eventMemLock = IOLockAlloc();

    _clientDict = OSDictionary::withCapacity(2);
    if ( _clientDict == 0 )
        return false;

    return true;
}
//====================================================================================================
// IOHIDEventService::start
//====================================================================================================
bool IOHIDEventService::start ( IOService * provider )
{
    UInt32      bootProtocol  = 0;
    OSObject    *obj          = NULL;
    OSNumber    *number       = NULL;
    OSString    *string       = NULL;
    OSBoolean   *boolean      = NULL;
    IOHIDDevice *device       = NULL;

    _provider = provider;
    _provider->retain();
    
    device = OSDynamicCast(IOHIDDevice, provider->getProvider());

    if ( !super::start(provider) )
        return false;

    if ( !handleStart(provider) )
        return false;

    _workLoop = getWorkLoop();
    if ( !_workLoop )
        return false;

    _workLoop->retain();
    
    _keyboard.appleVendorSupported = (getProperty(kIOHIDAppleVendorSupported, gIOServicePlane) == kOSBooleanTrue);

    _multiAxis.timer =
    IOTimerEventSource::timerEventSource(this,
                                         OSMemberFunctionCast(IOTimerEventSource::Action,
                                                              this,
                                                              &IOHIDEventService::multiAxisTimerCallback));
    if (!_multiAxis.timer || (_workLoop->addEventSource(_multiAxis.timer) != kIOReturnSuccess))
        return false;


    _commandGate = IOCommandGate::commandGate(this);
    if (!_commandGate || (_workLoop->addEventSource(_commandGate) != kIOReturnSuccess))
        return false;

    calculateStandardType();

    SET_HID_PROPERTIES(this);
    SET_HID_PROPERTIES_EMBEDDED(this);

    obj = copyProperty("BootProtocol");
    number = OSDynamicCast(OSNumber, obj);
    if (number)
        bootProtocol = number->unsigned32BitValue();
    OSSafeReleaseNULL(obj);
    
    obj = copyProperty(kIOHIDPhysicalDeviceUniqueIDKey);
    string = OSDynamicCast(OSString, obj);
    if (string)
        setProperty(kIOHIDPhysicalDeviceUniqueIDKey, string);
    OSSafeReleaseNULL(obj);
    
    obj = provider->copyProperty(kIOHIDBuiltInKey);
    boolean = OSDynamicCast(OSBoolean, obj);
    if (boolean)
        setProperty(kIOHIDBuiltInKey, boolean);
    OSSafeReleaseNULL(obj);

    obj = provider->copyProperty(kIOHIDProtectedAccessKey);
    boolean = OSDynamicCast(OSBoolean, obj);
    if (boolean)
        setProperty(kIOHIDProtectedAccessKey, boolean);
    OSSafeReleaseNULL(obj);

#if TARGET_OS_OSX
   
    int legacy_shim;
    if (!PE_parse_boot_argn("hid-legacy-shim", &legacy_shim, sizeof (legacy_shim))) {
        legacy_shim = 0;
    }
    
    IOLog("HID: Legacy shim %d\n", kLegacyShimEnabled);
    
    if (legacy_shim) {
        
        _keyboardShim  = kLegacyShimEnabled;
#ifdef POINTING_SHIM_SUPPORT
        _pointingShim  = kLegacyShimEnabled;
#endif
    } else {
 
        boolean_t singleUser = isSingleUser();
        
#ifdef POINTING_SHIM_SUPPORT
        _pointingShim = kLegacyShimDisabled;
#endif
        _keyboardShim = singleUser ? kLegacyShimEnabledForSingleUserMode : kLegacyShimDisabled;
        
    }
    
#endif
    
    parseSupportedElements (getReportElements(), bootProtocol);
    
    if (supportsHeadset(getReportElements())) {
        setProperty(kIOHIDDeviceTypeHintKey, kIOHIDDeviceTypeHeadsetKey);
        if (device) {
            device->setProperty(kIOHIDDeviceTypeHintKey, kIOHIDDeviceTypeHeadsetKey);
        }
    }

    _readyForInputReports = true;
    
    obj = getProperty(kIOHIDRegisterServiceKey);
    if (obj != kOSBooleanFalse) {
        registerService(kIOServiceAsynchronous);
    }
    
    return true;
}

#if TARGET_OS_OSX

//====================================================================================================
// stopAndReleaseShim
//====================================================================================================

static void stopAndReleaseShim ( IOService * service, IOService * provider )
{
    if ( !service )
        return;

    IOService * serviceProvider = service->getProvider();

    if ( serviceProvider == provider )
    {
        service->stop(provider);
        service->detach(provider);
    }
    service->release();
}

#endif /* TARGET_OS_OSX */

//====================================================================================================
// IOHIDEventService::stop
//====================================================================================================
void IOHIDEventService::stop( IOService * provider )
{
    handleStop ( provider );

    if (_multiAxis.timer) {
        _multiAxis.timer->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_multiAxis.timer);

        _multiAxis.timer->release();
        _multiAxis.timer = 0;
    }

#if TARGET_OS_IPHONE

    if ( _keyboard.debug.nmiTimer ) {
        _keyboard.debug.nmiTimer->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.debug.nmiTimer);

        _keyboard.debug.nmiTimer->release();
        _keyboard.debug.nmiTimer = 0;
    }

    if ( _keyboard.debug.stackshotTimer ) {
        _keyboard.debug.stackshotTimer->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.debug.stackshotTimer);

        _keyboard.debug.stackshotTimer->release();
        _keyboard.debug.stackshotTimer = 0;
    }

#else

    NUB_LOCK;

    stopAndReleaseShim ( _keyboardNub, this );
    _keyboardNub = 0;

#ifdef POINTING_SHIM_SUPPORT
    stopAndReleaseShim ( _pointingNub, this );
    _pointingNub = 0;
#endif
    
    stopAndReleaseShim ( _consumerNub, this );
    _consumerNub = 0;


    NUB_UNLOCK;
    
#endif /* TARGET_OS_IPHONE */

    super::stop( provider );
}

//====================================================================================================
// IOHIDEventService::matchPropertyTable
//====================================================================================================
bool IOHIDEventService::matchPropertyTable(OSDictionary * table, SInt32 * score)
{
    RETAIN_ON_STACK(this);
    // Ask our superclass' opinion.
    if (super::matchPropertyTable(table, score) == false)
        return false;

    return MatchPropertyTable(this, table, score);
}

//====================================================================================================
// IOHIDEventService::calculateStandardType
//====================================================================================================
void IOHIDEventService::calculateStandardType()
{
    IOHIDStandardType   result = kIOHIDStandardTypeANSI;
    OSNumber *          number;
    OSObject *          obj;

        obj = copyProperty(kIOHIDStandardTypeKey);
        number = OSDynamicCast(OSNumber, obj);
        if ( number ) {
#if TARGET_OS_IPHONE
            result = number->unsigned32BitValue();
#endif
        }
        else {
            UInt16 productID    = getProductID();
            UInt16 vendorID     = getVendorID();

            if (vendorID == kUSBHostVendorIDAppleComputer) {

                switch (productID) {
                    case kprodUSBCosmoISOKbd:  //Cosmo ISO
                    case kprodUSBAndyISOKbd:  //Andy ISO
                    case kprodQ6ISOKbd:  //Q6 ISO
                    case kprodQ30ISOKbd:  //Q30 ISO
#if TARGET_OS_IPHONE
                        _keyboard.swapISO = true;
#endif /* TARGET_OS_IPHONE */
                        // fall through
                    case kprodFountainISOKbd:  //Fountain ISO
                    case kprodSantaISOKbd:  //Santa ISO
                        result = kIOHIDStandardTypeISO;
                        break;
                    case kprodUSBCosmoJISKbd:  //Cosmo JIS
                    case kprodUSBAndyJISKbd:  //Andy JIS is 0x206
                    case kprodQ6JISKbd:  //Q6 JIS
                    case kprodQ30JISKbd:  //Q30 JIS
                    case kprodFountainJISKbd:  //Fountain JIS
                    case kprodSantaJISKbd:  //Santa JIS
                        result = kIOHIDStandardTypeJIS;
                        break;
                }

                setProperty(kIOHIDStandardTypeKey, result, 32);
            }
        }
    OSSafeReleaseNULL(obj);

#if TARGET_OS_IPHONE
    if ( !_keyboard.swapISO && result == kIOHIDStandardTypeISO ) {
        obj = copyProperty("alt_handler_id");
        number = OSDynamicCast(OSNumber, obj);
        if ( number ) {
            switch (number->unsigned32BitValue()) {
                case kgestUSBCosmoISOKbd:
                case kgestUSBAndyISOKbd:
                case kgestQ6ISOKbd:
                case kgestQ30ISOKbd:
                case kgestM89ISOKbd:
                case kgestUSBGenericISOkd:
                    _keyboard.swapISO = true;
                    break;
            }
        }
        OSSafeReleaseNULL(obj);
    }
#endif /* TARGET_OS_IPHONE */
}

//====================================================================================================
// IOHIDEventService::supportsHeadset
//====================================================================================================
bool IOHIDEventService::supportsHeadset(OSArray *elements)
{
    bool result = false;
    UInt32 count, index;
    bool playPause = false, volumeIncrement = false, volumeDecrement = false;
    bool unsupportedUsage = false;
    
    require(elements, exit);
    
    for (index = 0, count = elements->getCount(); index < count; index++) {
        IOHIDElement *element = NULL;
        
        element = OSDynamicCast(IOHIDElement, elements->getObject(index));
        if (!element) {
            continue;
        }
        
        switch (element->getUsagePage()) {
                // If an element with kHIDPage_Telephony/kHIDUsage_Tfon_Flash exists
                // then we return true.
            case kHIDPage_Telephony:
                if (element->getUsage() == kHIDUsage_Tfon_Flash) {
                    result = true;
                    goto exit;
                }
                break;
            case kHIDPage_Consumer:
                switch (element->getUsage()) {
                    case kHIDUsage_Csmr_PlayOrPause:
                        playPause = true;
                        break;
                    case kHIDUsage_Csmr_VolumeIncrement:
                        volumeIncrement = true;
                        break;
                    case kHIDUsage_Csmr_VolumeDecrement:
                        volumeDecrement = true;
                        break;
                    case kHIDUsage_Csmr_FastForward:
                        unsupportedUsage = true;
                        break;
                    case kHIDUsage_Csmr_Rewind:
                        unsupportedUsage = true;
                        break;
                    case kHIDUsage_Csmr_ScanNextTrack:
                        unsupportedUsage = true;
                        break;
                    case kHIDUsage_Csmr_ScanPreviousTrack:
                        unsupportedUsage = true;
                        break;
                    case kHIDUsage_Csmr_DataOnScreen:
                        unsupportedUsage = true;
                        break;
                }
                break;
            case kHIDPage_GenericDesktop:
                switch (element->getUsage()) {
                    case kHIDUsage_GD_SystemMenuLeft:
                        unsupportedUsage = true;
                        break;
                    case kHIDUsage_GD_SystemMenuRight:
                        unsupportedUsage = true;
                        break;
                    case kHIDUsage_GD_SystemAppMenu:
                        unsupportedUsage = true;
                        break;
                }
        }
    }
    
    // If kHIDPage_Telephony/kHIDUsage_Tfon_Flash element was not present, then
    // device must conform to primaryUsagePage/primaryUsage of consumer/consumer control
    require_quiet(getPrimaryUsagePage() == kHIDPage_Consumer &&
                  getPrimaryUsage() == kHIDUsage_Csmr_ConsumerControl, exit);
    
    // Play/Pause, volume increment and volume decrement elements must be present
    require_quiet(playPause && volumeIncrement && volumeDecrement, exit);
    
    // None of the unsupported elements must be present
    require_quiet(!unsupportedUsage, exit);
    
    result = true;
    
exit:
    return result;
}


static const OSSymbol * propagateProps[] = {kIOHIDPropagatePropertyKeys};

//====================================================================================================
// IOHIDEventService::setSystemProperties
//====================================================================================================
IOReturn IOHIDEventService::setSystemProperties( OSDictionary * properties )
{
    OSNumber *      number      = NULL;

    if ( !properties )
        return kIOReturnBadArgument;
    
    if ( properties->getObject(kIOHIDDeviceParametersKey) == kOSBooleanTrue ) {
        OSObject *obj = copyProperty(kIOHIDEventServicePropertiesKey);
        OSDictionary * eventServiceProperties = OSDynamicCast(OSDictionary, obj);
        if ( eventServiceProperties ) {
            if (eventServiceProperties->setOptions(0, 0) & OSDictionary::kImmutable) {
                OSDictionary * copyEventServiceProperties = (OSDictionary*)eventServiceProperties->copyCollection();
                obj->release();
                eventServiceProperties = copyEventServiceProperties;
            }
        } else {
            OSSafeReleaseNULL(obj);
            eventServiceProperties = OSDictionary::withCapacity(4);
        }

        if ( eventServiceProperties ) {
            eventServiceProperties->merge(properties);
            eventServiceProperties->removeObject(kIOHIDResetKeyboardKey);
            eventServiceProperties->removeObject(kIOHIDResetPointerKey);
            eventServiceProperties->removeObject(kIOHIDDeviceParametersKey);
            setProperty(kIOHIDEventServicePropertiesKey, eventServiceProperties);
            eventServiceProperties->release();
        }
    }
    number = OSDynamicCast(OSNumber, properties->getObject(kIOHIDDebugConfigKey));
    if (number) {
        _debugMask = number->unsigned32BitValue();
    }

    if (_provider && !isInactive()) {
        for (const OSSymbol * prop : propagateProps) {
            OSObject *val = properties->getObject(prop);
            if (val) {
                _provider->setProperty(prop, val);
            }
        }
    }
    
    return kIOReturnSuccess;
}

//====================================================================================================
// IOHIDEventService::setProperties
//====================================================================================================
IOReturn IOHIDEventService::setProperties( OSObject * properties )
{
    OSDictionary *  propertyDict    = OSDynamicCast(OSDictionary, properties);
    IOReturn        ret             = kIOReturnBadArgument;
    if (propertyDict) {
      propertyDict->setObject(kIOHIDDeviceParametersKey, kOSBooleanTrue);
      ret = setSystemProperties( propertyDict );
      propertyDict->removeObject(kIOHIDDeviceParametersKey);
    }
    return ret;
}


//====================================================================================================
// IOHIDEventService::parseSupportedElements
//====================================================================================================
void IOHIDEventService::parseSupportedElements ( OSArray * elementArray, UInt32 bootProtocol )
{
    UInt32              count               = 0;
    UInt32              index               = 0;
    UInt32              usage               = 0;
    UInt32              usagePage           = 0;
    UInt32              supportedModifiers  = 0;
    UInt32              buttonCount         = 0;
    IOHIDElement *      element             = 0;
    OSArray *           functions           = 0;
    IOFixed             pointingResolution  = 0;
    IOFixed             scrollResolution    = 0;
    bool                pointingDevice      = false;
    bool                keyboardDevice      = false;
    bool                consumerDevice      = false;
    bool                escKeySupported     = false;
    
    switch ( bootProtocol )
    {
        case kBootProtocolMouse:
            pointingDevice = true;
            break;
        case kBootProtocolKeyboard:
            keyboardDevice = true;
            break;
    }

    if ( elementArray )
    {
        count = elementArray->getCount();

        for ( index = 0; index < count; index++ )
        {
            element = OSDynamicCast(IOHIDElement, elementArray->getObject(index));

            if ( !element )
                continue;

            usagePage   = element->getUsagePage();
            usage       = element->getUsage();

            switch ( usagePage )
            {
                case kHIDPage_GenericDesktop:
                    switch ( usage )
                    {
                        case kHIDUsage_GD_Mouse:
                            pointingDevice      = true;
                            break;
                        case kHIDUsage_GD_X:
                            if ( !(pointingResolution = determineResolution(element)) )
                                pointingResolution = kDefaultFixedResolution;
                            break;
                        case kHIDUsage_GD_Z:
                        case kHIDUsage_GD_Wheel:
                            if ( !(scrollResolution = determineResolution(element)) )
                                scrollResolution = kDefaultScrollFixedResolution;
                            break;
                        case kHIDUsage_GD_SystemPowerDown:
                        case kHIDUsage_GD_SystemSleep:
                        case kHIDUsage_GD_SystemWakeUp:
                            consumerDevice      = true;
                            break;
                    }
                    break;

                case kHIDPage_Button:
                    buttonCount ++;
                    break;

                case kHIDPage_KeyboardOrKeypad:
                    keyboardDevice = true;
                    switch ( usage )
                    {
                        case kHIDUsage_KeyboardLeftControl:
                            supportedModifiers |= NX_CONTROLMASK;
                            supportedModifiers |= NX_DEVICELCTLKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftShift:
                            supportedModifiers |= NX_SHIFTMASK;
                            supportedModifiers |= NX_DEVICELSHIFTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftAlt:
                            supportedModifiers |= NX_ALTERNATEMASK;
                            supportedModifiers |= NX_DEVICELALTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftGUI:
                            supportedModifiers |= NX_COMMANDMASK;
                            supportedModifiers |= NX_DEVICELCMDKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightControl:
                            supportedModifiers |= NX_CONTROLMASK;
                            supportedModifiers |= NX_DEVICERCTLKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightShift:
                            supportedModifiers |= NX_SHIFTMASK;
                            supportedModifiers |= NX_DEVICERSHIFTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightAlt:
                            supportedModifiers |= NX_ALTERNATEMASK;
                            supportedModifiers |= NX_DEVICERALTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightGUI:
                            supportedModifiers |= NX_COMMANDMASK;
                            supportedModifiers |= NX_DEVICERCMDKEYMASK;
                            break;
                        case kHIDUsage_KeyboardCapsLock:
                            supportedModifiers |= NX_ALPHASHIFT_STATELESS_MASK;
                            supportedModifiers |= NX_DEVICE_ALPHASHIFT_STATELESS_MASK;
                            break;
                        case kHIDUsage_KeyboardEscape:
                            escKeySupported = true;
                            break;
                    }
                    break;

                case kHIDPage_Consumer:
                    consumerDevice = true;
                    break;
                case kHIDPage_Digitizer:
                    pointingDevice = true;
                    switch ( usage )
                    {
                        case kHIDUsage_Dig_Pen:
                        case kHIDUsage_Dig_LightPen:
                        case kHIDUsage_Dig_TouchScreen:
                            setProperty(kIOHIDDisplayIntegratedKey, true);
                            break;
                        case kHIDUsage_Dig_TipSwitch:
                        case kHIDUsage_Dig_BarrelSwitch:
                        case kHIDUsage_Dig_Eraser:
                            buttonCount ++;
                        default:
                            break;
                    }
                    break;
                case kHIDPage_AppleVendorTopCase:
                    if ((usage == kHIDUsage_AV_TopCase_KeyboardFn) &&
                        (_keyboard.appleVendorSupported))
                    {
                        supportedModifiers |= NX_SECONDARYFNMASK;
                    }
                    break;
            }

            // Cache device functions
            if ((element->getType() == kIOHIDElementTypeCollection) &&
                ((element->getCollectionType() == kIOHIDElementCollectionTypeApplication) ||
                (element->getCollectionType() == kIOHIDElementCollectionTypePhysical)))
            {
                OSNumber * usagePageRef, * usageRef;
                OSDictionary * pairRef;

                if(!functions) functions = OSArray::withCapacity(2);

                pairRef     = OSDictionary::withCapacity(2);
                usageRef    = OSNumber::withNumber(usage, 32);
                usagePageRef= OSNumber::withNumber(usagePage, 32);

                pairRef->setObject(kIOHIDDeviceUsageKey, usageRef);
                pairRef->setObject(kIOHIDDeviceUsagePageKey, usagePageRef);

                UInt32 	pairCount = functions->getCount();
                bool 	found = false;
                for(unsigned i=0; i<pairCount; i++)
                {
                    OSDictionary *tempPair = (OSDictionary *)functions->getObject(i);

                    if ( false != (found = tempPair->isEqualTo(pairRef)) )
                        break;
                }

                if (!found)
                {
                    functions->setObject(functions->getCount(), pairRef);
                }

                pairRef->release();
                usageRef->release();
                usagePageRef->release();
            }
        }
        
        if (_deviceUsagePairs) {
            _deviceUsagePairs->release();
        }
        _deviceUsagePairs = functions;
    }

    if ( pointingDevice )
    {
        if ( pointingResolution )
            setProperty(kIOHIDPointerResolutionKey, pointingResolution, 32);

        if ( scrollResolution )
            setProperty(kIOHIDScrollResolutionKey, scrollResolution, 32);
      
        if (buttonCount) {
            setProperty(kIOHIDPointerButtonCountKey, buttonCount, 32);
        }
    }
    if (keyboardDevice) {
        setProperty(kIOHIDKeyboardSupportedModifiersKey, supportedModifiers, 32);
        setProperty("HIDKeyboardKeysDefined", true);
    }
    
#if TARGET_OS_OSX
    if (keyboardDevice) {
        
        // multitouch should report absence of esc key on specific keyboards
        // we should act only if given key is not published
        if (!getProperty(kIOHIDKeyboardSupportsEscKey)) {
            setProperty(kIOHIDKeyboardSupportsEscKey, escKeySupported ? kOSBooleanTrue : kOSBooleanFalse);
        }
    }
#endif
    
#if TARGET_OS_OSX
    
    NUB_LOCK;
  
#ifdef POINTING_SHIM_SUPPORT
    if ( pointingDevice && _pointingShim != kLegacyShimDisabled) {
        _pointingNub = newPointingShim(buttonCount, pointingResolution, scrollResolution, kShimEventProcessor);
    }
#endif
    if (keyboardDevice && _keyboardShim != kLegacyShimDisabled) {
        _keyboardNub = newKeyboardShim(supportedModifiers, kShimEventProcessor);
    }
    if (consumerDevice && _keyboardShim != kLegacyShimDisabled) {
        _consumerNub = newConsumerShim(kShimEventProcessor);
    }

    NUB_UNLOCK;

#endif
}

#ifdef POINTING_SHIM_SUPPORT
//====================================================================================================
// IOHIDEventService::newPointingShim
//====================================================================================================
IOHIDPointing * IOHIDEventService::newPointingShim (
                            UInt32          buttonCount,
                            IOFixed         pointerResolution,
                            IOFixed         scrollResolution,
                            IOOptionBits    options)
{
#if TARGET_OS_OSX // {
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);
    IOHIDPointing   *pointingNub = IOHIDPointing::Pointing(buttonCount, pointerResolution, scrollResolution, isDispatcher);;
    OSNumber        *value;
    
    require(pointingNub, no_nub);

	SET_HID_PROPERTIES(pointingNub);

    require(pointingNub->attach(this), no_attach);
    require(pointingNub->start(this), no_start);
    value = OSNumber::withNumber(getRegistryEntryID(), 64);
    if (value) {
        pointingNub->setProperty(kIOHIDAltSenderIdKey, value);
        value->release();
    }
    return pointingNub;

no_start:
    pointingNub->detach(this);

no_attach:
    pointingNub->release();
    pointingNub = NULL;

no_nub:

#endif // } TARGET_OS_OSX
    return NULL;
}
#endif

//====================================================================================================
// IOHIDEventService::newKeyboardShim
//====================================================================================================
IOHIDKeyboard * IOHIDEventService::newKeyboardShim (
                                UInt32          supportedModifiers,
                                IOOptionBits    options)
{
#if TARGET_OS_OSX // {
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);
    IOHIDKeyboard   *keyboardNub = IOHIDKeyboard::Keyboard(supportedModifiers, isDispatcher);

    require(keyboardNub, no_nub);

        SET_HID_PROPERTIES(keyboardNub);

    require(keyboardNub->attach(this), no_attach);
    require(keyboardNub->start(this), no_start);
    keyboardNub->setProperty(kIOHIDAltSenderIdKey, OSNumber::withNumber(getRegistryEntryID(), 64));

    return keyboardNub;

no_start:
    keyboardNub->detach(this);

no_attach:
    keyboardNub->release();
    keyboardNub = NULL;

no_nub:
#else
    (void)supportedModifiers;
    (void)options;
#endif // } TARGET_OS_OSX
    return NULL;
}

//====================================================================================================
// IOHIDEventService::newConsumerShim
//====================================================================================================
IOHIDConsumer * IOHIDEventService::newConsumerShim ( IOOptionBits options )
{
#if TARGET_OS_OSX// {
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);
    IOHIDConsumer   *consumerNub = IOHIDConsumer::Consumer(isDispatcher);;

    require(consumerNub, no_nub);

        SET_HID_PROPERTIES(consumerNub);

    require(consumerNub->attach(this), no_attach);
    require(consumerNub->start(this), no_start);
    consumerNub->setProperty(kIOHIDAltSenderIdKey, OSNumber::withNumber(getRegistryEntryID(), 64));

    return consumerNub;

no_start:
    consumerNub->detach(this);

no_attach:
    consumerNub->release();
    consumerNub = NULL;

no_nub:
#else
    (void)options;
#endif // } TARGET_OS_OSX
    return NULL;
}

//====================================================================================================
// IOHIDEventService::determineResolution
//====================================================================================================
IOFixed IOHIDEventService::determineResolution ( IOHIDElement * element )
{
    IOFixed resolution = 0;
    bool supportResolution = true;

#if TARGET_OS_OSX
    if ((element->getFlags() & kIOHIDElementFlagsRelativeMask) != 0) {

        if ( element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_MultiAxisController) )
            supportResolution = false;
    }
    else {
        supportResolution = false;
    }
#endif /* TARGET_OS_OSX */
    
    if ( supportResolution ) {
        if ((element->getPhysicalMin() != element->getLogicalMin()) &&
            (element->getPhysicalMax() != element->getLogicalMax()))
        {
            SInt32 logicalDiff = (element->getLogicalMax() - element->getLogicalMin());
            SInt32 physicalDiff = (element->getPhysicalMax() - element->getPhysicalMin());

            // Since IOFixedDivide truncated fractional part and can't use floating point
            // within the kernel, have to convert equation when using negative exponents:
            // _resolution = ((logMax -logMin) * 10 **(-exp))/(physMax -physMin)

            // Even though unitExponent is stored as SInt32, The real values are only
            // a signed nibble that doesn't expand to the full 32 bits.
            SInt32 resExponent = element->getUnitExponent() & 0x0F;

            if (resExponent < 8)
            {
                for (int i = resExponent; i > 0; i--)
                {
                    physicalDiff *=  10;
                }
            }
            else
            {
                for (int i = 0x10 - resExponent; i > 0; i--)
                {
                    logicalDiff *= 10;
                }
            }
            resolution = (logicalDiff / physicalDiff) << 16;
        }
    }

    return resolution;
}

//====================================================================================================
// IOHIDEventService::free
//====================================================================================================
void IOHIDEventService::free()
{
    IORecursiveLock* tempLock = NULL;

    OSSafeReleaseNULL(_provider);

    if ( _nubLock ) {
        IORecursiveLockLock(_nubLock);
        tempLock = _nubLock;
        _nubLock = NULL;
    }

    if (_commandGate) {
        if ( _workLoop ) {
            _workLoop->removeEventSource(_commandGate);
        }
        _commandGate->release();
        _commandGate = 0;
    }

    if ( _deviceUsagePairs ) {
        _deviceUsagePairs->release();
        _deviceUsagePairs = NULL;
    }
    
    if ( _clientDict ) {
        if (_clientDict->getCount()) {
            _clientDict->iterateObjects(^bool(const OSSymbol *key,
                                              OSObject *object __unused) {
                IOService *client = OSDynamicCast(IOService, key);
                
                if (client) {
                    HIDServiceLogError("Service never closed by %s: 0x%llx",
                                       client->getName(), client->getRegistryEntryID());
                }
                
                return false;
            });
        }
        _clientDict->release();
        _clientDict = NULL;
    }
    
    OSSafeReleaseNULL(_eventMemory);
    
    if (_eventMemLock) {
        IOLockFree(_eventMemLock);
        _eventMemLock = NULL;
    }

#if TARGET_OS_IPHONE
    if (_keyboard.debug.nmiTimer) {
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.debug.nmiTimer);

        _keyboard.debug.nmiTimer->release();
        _keyboard.debug.nmiTimer = 0;
    }

    if (_keyboard.debug.stackshotTimer) {
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.debug.stackshotTimer);

        _keyboard.debug.stackshotTimer->release();
        _keyboard.debug.stackshotTimer = 0;
    }

#endif /* TARGET_OS_IPHONE */

    if ( _workLoop ) {
        // not our workloop. don't stop it.
        _workLoop->release();
        _workLoop = NULL;
    }

    if (_reserved) {
        IODelete(_reserved, ExpansionData, 1);
        _reserved = NULL;
    }

    if (_clientDictLock) {
        IOLockFree(_clientDictLock);
        _clientDictLock = NULL;
    }
    
    if ( tempLock ) {
        IORecursiveLockUnlock(tempLock);
        IORecursiveLockFree(tempLock);
    }

    super::free();
}

//==============================================================================
// IOHIDEventService::handleOpen
//==============================================================================
bool IOHIDEventService::handleOpen(IOService *  client,
                                    IOOptionBits options __unused,
                                    void *       argument)
{
    bool accept = false;
    OSDictionary * clientDict;

    IOLockLock(_clientDictLock);
    do {
        // Was this object already registered as our client?
        if ( _clientDict->getObject((const OSSymbol *)client) ) {
            accept = true;
            break;
        }

        // Copy before mutation.
        clientDict = OSDictionary::withDictionary(_clientDict);
        if ( !clientDict )
            break;

        _clientDict->release();
        _clientDict = clientDict;

        // Add the new client object to our client dict.
        if ( !OSDynamicCast(IOHIDClientData, (OSObject *)argument) ||
                !_clientDict->setObject((const OSSymbol *)client, (IOHIDClientData *)argument))
            break;

        accept = true;
    } while (false);
    IOLockUnlock(_clientDictLock);

    return accept;
}

//==============================================================================
// IOHIDEventService::handleClose
//==============================================================================
void IOHIDEventService::handleClose(IOService * client, IOOptionBits options __unused)
{
    OSDictionary * clientDict;

    IOLockLock(_clientDictLock);

    // Copy before mutation.
    clientDict = OSDictionary::withDictionary(_clientDict);
    require(clientDict, exit);

    _clientDict->release();
    _clientDict = clientDict;

    if ( _clientDict->getObject((const OSSymbol *)client) )
        _clientDict->removeObject((const OSSymbol *)client);

exit:
    IOLockUnlock(_clientDictLock);
}

//==============================================================================
// IOHIDEventService::handleIsOpen
//==============================================================================
bool IOHIDEventService::handleIsOpen(const IOService * client) const
{
    bool ret;

    IOLockLock(_clientDictLock);
    if (client)
        ret = _clientDict->getObject((const OSSymbol *)client) != NULL;
    else
        ret = (_clientDict->getCount() > 0);
    IOLockUnlock(_clientDictLock);

    return ret;
}

//====================================================================================================
// IOHIDEventService::handleStart
//====================================================================================================
bool IOHIDEventService::handleStart( IOService * provider __unused )
{
    return true;
}

//====================================================================================================
// IOHIDEventService::handleStop
//====================================================================================================
void IOHIDEventService::handleStop(  IOService * provider __unused )
{}

//====================================================================================================
// IOHIDEventService::getTransport
//====================================================================================================
OSString * IOHIDEventService::getTransport ()
{
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDTransportKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getManufacturer
//====================================================================================================
OSString * IOHIDEventService::getManufacturer ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDManufacturerKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getProduct
//====================================================================================================
OSString * IOHIDEventService::getProduct ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDProductKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getSerialNumber
//====================================================================================================
OSString * IOHIDEventService::getSerialNumber ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDSerialNumberKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getLocationID
//====================================================================================================
UInt32 IOHIDEventService::getLocationID ()
{
	UInt32 value = 0;

	if ( _provider ) {
        OSObject *obj = _provider->copyProperty(kIOHIDLocationIDKey);
        OSNumber * number = OSDynamicCast(OSNumber, obj);
		if ( number )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(obj);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVendorID
//====================================================================================================
UInt32 IOHIDEventService::getVendorID ()
{
	UInt32 value = 0;

	if ( _provider ) {
        OSObject *obj = _provider->copyProperty(kIOHIDVendorIDKey);
        OSNumber * number = OSDynamicCast(OSNumber, obj);
		if ( number )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(obj);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVendorIDSource
//====================================================================================================
UInt32 IOHIDEventService::getVendorIDSource ()
{
	UInt32 value = 0;

	if ( _provider ) {
        OSObject *obj = _provider->copyProperty(kIOHIDVendorIDSourceKey);
        OSNumber * number = OSDynamicCast(OSNumber, obj);
		if ( number )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(obj);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getProductID
//====================================================================================================
UInt32 IOHIDEventService::getProductID ()
{
	UInt32 value = 0;

	if ( _provider ) {
        OSObject *obj = _provider->copyProperty(kIOHIDProductIDKey);
        OSNumber * number = OSDynamicCast(OSNumber, obj);
		if ( number )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(obj);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVersion
//====================================================================================================
UInt32 IOHIDEventService::getVersion ()
{
	UInt32 value = 0;

	if ( _provider ) {
        OSObject *obj = _provider->copyProperty(kIOHIDVersionNumberKey);
        OSNumber * number = OSDynamicCast(OSNumber, obj);
		if ( number )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(obj);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getCountryCode
//====================================================================================================
UInt32 IOHIDEventService::getCountryCode ()
{
	UInt32 value = 0;

	if ( _provider ) {
        OSObject *obj = _provider->copyProperty(kIOHIDCountryCodeKey);
        OSNumber * number = OSDynamicCast(OSNumber, obj);
		if ( number )
			value = number->unsigned32BitValue();
        OSSafeReleaseNULL(obj);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getReportElements
//====================================================================================================
OSArray * IOHIDEventService::getReportElements()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::setElementValue
//====================================================================================================
IOReturn IOHIDEventService::setElementValue (
                                UInt32                      usagePage __unused,
                                UInt32                      usage __unused,
                                UInt32                      value __unused )
{
    return kIOReturnUnsupported;
}

//====================================================================================================
// IOHIDEventService::getElementValue
//====================================================================================================
UInt32 IOHIDEventService::getElementValue (
                                UInt32                      usagePage __unused,
                                UInt32                      usage __unused )
{
    return 0;
}


#if TARGET_OS_IPHONE
//==============================================================================
// IOHIDEventService::debuggerTimerCallback
//==============================================================================
void IOHIDEventService::debuggerTimerCallback(IOTimerEventSource *sender __unused)
{
    if ( _keyboard.debug.mask && _keyboard.debug.mask == _keyboard.debug.startMask && _keyboard.debug.mask == _keyboard.debug.nmiHoldMask) {
        triggerDebugger();
    }
}

//==============================================================================
// IOHIDEventService::triggerDebugger
//==============================================================================
void IOHIDEventService::triggerDebugger()
{
    PE_enter_debugger("NMI");
}
#endif /* TARGET_OS_IPHONE */

#if TARGET_OS_IPHONE
//==============================================================================
// IOHIDEventService::stackshotTimerCallback
//==============================================================================
void IOHIDEventService::stackshotTimerCallback(IOTimerEventSource *sender __unused)
{
    if ( _keyboard.debug.mask && _keyboard.debug.mask == _keyboard.debug.startMask ) {
        _keyboard.debug.stackshotHeld = 1;
    }
}

#endif /* TARGET_OS_IPHONE */

//==============================================================================
// IOHIDEventService::multiAxisTimerCallback
//==============================================================================
void IOHIDEventService::multiAxisTimerCallback(IOTimerEventSource *sender __unused)
{
    AbsoluteTime timestamp;

    clock_get_uptime(&timestamp);
    dispatchMultiAxisPointerEvent(timestamp, _multiAxis.buttonState, _multiAxis.x, _multiAxis.y, _multiAxis.z, _multiAxis.rX, _multiAxis.rY, _multiAxis.rZ, _multiAxis.options | kIOHIDEventOptionIsRepeat);
}

#if TARGET_OS_OSX

KeyValueMask IOHIDEventService::keyMonitorTable[] = {
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftControl ),     kKeyMaskCtrl},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightControl),     kKeyMaskCtrl},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftShift   ),     kKeyMaskShift},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightShift  ),     kKeyMaskShift},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftAlt     ),     kKeyMaskAlt},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightAlt    ),     kKeyMaskAlt},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftGUI     ),     kKeyMaskCommand},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightGUI    ),     kKeyMaskCommand},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardPeriod      ),     kKeyMaskPeriod},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeypadPeriod        ),     kKeyMaskPeriod},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardComma       ),     kKeyMaskComma},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeypadComma         ),     kKeyMaskComma},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardSlash       ),     kKeyMaskSlash},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeypadSlash         ),     kKeyMaskSlash},
    {Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardEscape      ),     kKeyMaskEsc}
};

DebugKeyAction IOHIDEventService::debugKeyActionTable[] = {
  { (kKeyMaskCtrl | kKeyMaskShift | kKeyMaskAlt | kKeyMaskCommand | kKeyMaskPeriod),
    IOHIDEventService::debugActionSysdiagnose,
    (void*)kHIDUsage_KeyboardPeriod
  },
  { (kKeyMaskCtrl | kKeyMaskShift | kKeyMaskAlt | kKeyMaskCommand | kKeyMaskComma),
    IOHIDEventService::debugActionSysdiagnose,
    (void*)kHIDUsage_KeyboardComma
  },
  { (kKeyMaskCtrl | kKeyMaskShift | kKeyMaskAlt | kKeyMaskCommand | kKeyMaskSlash),
    IOHIDEventService::debugActionSysdiagnose,
    (void*)kHIDUsage_KeyboardSlash
  },
  { kKeyMaskCtrl | kKeyMaskShift | kKeyMaskAlt | kKeyMaskCommand | kKeyMaskEsc,
    debugActionNMI,
    NULL
  },
};

//====================================================================================================
// IOHIDEventService::debugActionNMI
//====================================================================================================

void IOHIDEventService::debugActionNMI (IOHIDEventService *self __unused, void * parameter __unused) {
  PE_enter_debugger("HID: USB Programmer Key");
}

//====================================================================================================
// IOHIDEventService::debugActionSysdiagnose
//====================================================================================================
void IOHIDEventService::debugActionSysdiagnose (IOHIDEventService *self __unused, void * parameter) {
  uint32_t  keyCode = (uint32_t)(uintptr_t)parameter;
  kern_stack_snapshot_with_reason((char *)"HID: Stackshot triggered using keycombo");
  sysdiagnose_notify_user(keyCode);
  HIDLog("HID: Posted stackshot event 0x%08x", keyCode);
}

#endif

//====================================================================================================
// IOHIDEventService::dispatchKeyboardEvent
//====================================================================================================
void IOHIDEventService::dispatchKeyboardEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value,
                                IOOptionBits                options)
{
    if ( ! _readyForInputReports )
        return;
    IOHIDEvent * event = NULL;
#if TARGET_OS_IPHONE // {
    UInt32 debugMask = 0;
    if ( !_keyboard.debug.nmiHoldMask ) {
        OSData * nmi_mask = OSDynamicCast(OSData, getProperty("button-nmi_mask", gIOServicePlane));
        if ( nmi_mask) {
            _keyboard.debug.nmiHoldMask = *(UInt32 *) nmi_mask->getBytesNoCopy();
            _keyboard.debug.nmiDelay = kDebuggerLongDelayMS;
        } else {
#if TARGET_OS_TV // Apple TV NMI keychord: FAV (List button) + PlayPause
            _keyboard.debug.nmiHoldMask = 0x50;
            _keyboard.debug.nmiDelay = kATVChordDelayMS;
#elif TARGET_OS_WATCH // WatchOS NMI keychord: Hold Menu (crown) & triple-press Help (pill)
            _keyboard.debug.nmiHoldMask = 0x4;
            _keyboard.debug.nmiTriplePressMask = 0x8;
            _keyboard.debug.nmiDelay = kDebuggerTriplePressDelayMS;
#else
            _keyboard.debug.nmiHoldMask = 0x1;
            _keyboard.debug.nmiTriplePressMask = 0x2;
            _keyboard.debug.nmiDelay = kDebuggerTriplePressDelayMS;
#endif // TARGET_OS_TV
        }
    }
    
  
    switch (usagePage) {
        case kHIDPage_KeyboardOrKeypad:
            if ( _keyboard.swapISO ) {

                switch ( usage ) {
                    case kHIDUsage_KeyboardGraveAccentAndTilde:
                        usage = kHIDUsage_KeyboardNonUSBackslash;
                        break;
                    case kHIDUsage_KeyboardNonUSBackslash:
                        usage = kHIDUsage_KeyboardGraveAccentAndTilde;
                        break;
                }
            }
            break;
        case kHIDPage_Consumer:
            switch (usage) {
                case kHIDUsage_Csmr_Power:
                    debugMask = 0x1;
                    break;
                case kHIDUsage_Csmr_VolumeDecrement:
                    debugMask = 0x20;
                    break;
                case kHIDUsage_Csmr_VolumeIncrement:
                    debugMask = 0x2;
                    break;
                case kHIDUsage_Csmr_Menu:
                    debugMask = 0x4;
                    break;
                case kHIDUsage_Csmr_Help:
                    debugMask = 0x8;
                    break;
                case kHIDUsage_Csmr_PlayOrPause:
                    debugMask = 0x10;
                    break;
                case kHIDUsage_Csmr_DataOnScreen:
                    debugMask = 0x40;
                    break;
            };
            break;
        case kHIDPage_Telephony:
            switch (usage) {
                case kHIDUsage_Tfon_Hold:
                    debugMask = 0x1;
                    break;
            };
            break;
    };

    if ( value )
        _keyboard.debug.mask |= debugMask;
    else
        _keyboard.debug.mask &= ~debugMask;


    // If triple press mask is not set, use hold button NMI trigger.
    if ( !_keyboard.debug.nmiTriplePressMask ) {
        if ( _keyboard.debug.mask == _keyboard.debug.nmiHoldMask ) {
            if ( !_keyboard.debug.nmiTimer ) {
                _keyboard.debug.nmiTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::debuggerTimerCallback));
                if (_keyboard.debug.nmiTimer) {
                    if ((_workLoop->addEventSource(_keyboard.debug.nmiTimer) != kIOReturnSuccess)) {
                        _keyboard.debug.nmiTimer->release();
                        _keyboard.debug.nmiTimer = NULL;
                    }
                }
            }
            if ( _keyboard.debug.nmiTimer ) {
                _keyboard.debug.nmiTimer->setTimeoutMS( _keyboard.debug.nmiDelay );
                _keyboard.debug.startMask = _keyboard.debug.mask;
            }
        }
    }
    // If triple press mask is set, use hold + triple press button NMI trigger.
    else {
        // Proceed if only hold mask and triple press mask are pressed.
        if ( _keyboard.debug.mask  == (_keyboard.debug.nmiHoldMask | _keyboard.debug.nmiTriplePressMask) ) {
            AbsoluteTime abs;
            UInt64 timestamp;
            
            clock_get_uptime(&abs);
            absolutetime_to_nanoseconds(abs, &timestamp);
            timestamp /= kMillisecondScale;
            
            if ( timestamp > _keyboard.debug.nmiStartTime + _keyboard.debug.nmiDelay ) {
                _keyboard.debug.nmiPressCount = 0;
            }
            
            switch (_keyboard.debug.nmiPressCount++) {
                case 0:
                    _keyboard.debug.nmiStartTime = timestamp;
                    break;
                case 2:
                    triggerDebugger();
                    break;
                default:
                    break;
            }
        }
        // Reset if hold mask is release or a button in neither mask is pressed.
        else if ( (_keyboard.debug.mask & ~_keyboard.debug.nmiTriplePressMask) != _keyboard.debug.nmiHoldMask){
            _keyboard.debug.nmiPressCount = 0;
        }
    }
    
    // stackshot keychord check
    if(_keyboard.debug.mask == STACKSHOT_MASK_WATCH ||
       _keyboard.debug.mask == STACKSHOT_MASK_ATV) {
        if ( !_keyboard.debug.stackshotTimer ) {
            _keyboard.debug.stackshotTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::stackshotTimerCallback));
            if ( _keyboard.debug.stackshotTimer ) {
                if ((_workLoop->addEventSource(_keyboard.debug.stackshotTimer) != kIOReturnSuccess)) {
                    _keyboard.debug.stackshotTimer->release();
                    _keyboard.debug.stackshotTimer = NULL;
                }
            }
        }
        if ( _keyboard.debug.stackshotTimer ) {
            _keyboard.debug.stackshotTimer->setTimeoutMS(DELAYED_STACKSHOT_TIMEOUT);
            _keyboard.debug.startMask = _keyboard.debug.mask;
        }
        handle_stackshot_keychord(_keyboard.debug.mask);
    }
    if ( _keyboard.debug.mask == 0x0 ) {
        // Always reset flag on release.
        if ( _keyboard.debug.stackshotHeld ) {
            handle_stackshot_keychord(DELAYED_STACKSHOT_MASK);
        }
		if ( _keyboard.debug.stackshotTimer ) {
			_keyboard.debug.stackshotTimer->cancelTimeout();
		}
        _keyboard.debug.stackshotHeld = 0;
    }

    event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, value!=0, options);
    if ( event ) {
      dispatchEvent(event);
      event->release();
    }

#endif

#if TARGET_OS_OSX
    for (unsigned int index = 0 ; index < (sizeof(_keyboard.pressedKeys)/sizeof(_keyboard.pressedKeys[0])); index++) {
        if (value) {
            if (!_keyboard.pressedKeys[index].isValid()) {
                _keyboard.pressedKeys[index] = Key (usagePage, usage);
                break;
            }
        } else {
            if (_keyboard.pressedKeys[index].usage() == usage && _keyboard.pressedKeys[index].usagePage() == usagePage) {
                _keyboard.pressedKeys[index] = Key (0, 0);
                break;
            }
        }
    }
    
    _keyboard.pressedKeysMask = 0;
    
    for (unsigned int index = 0 ; index < (sizeof(_keyboard.pressedKeys)/sizeof(_keyboard.pressedKeys[0])); index++) {
        uint32_t maskForKey = 0;
        if (_keyboard.pressedKeys[index].isValid()) {
            maskForKey = kKeyMaskUnknown;
            for (unsigned int i = 0 ; i < (sizeof(keyMonitorTable)/sizeof(keyMonitorTable[0])); i++) {
                if (keyMonitorTable[i].key == _keyboard.pressedKeys[index]) {
                    maskForKey = keyMonitorTable[i].mask;
                    break;
                }
            }
        }
        _keyboard.pressedKeysMask |= maskForKey;
    }
    
    uint32_t debugMask = (_keyboard.pressedKeysMask & kKeyMaskUnknown) ? 0 : _keyboard.pressedKeysMask;

    if (debugMask && value != 0) {
      for (unsigned int index = 0 ; index < (sizeof(debugKeyActionTable)/sizeof(debugKeyActionTable[0])); index++) {
        if (debugKeyActionTable[index].mask == debugMask) {
          HIDLogError ("HID: taking action for debug key mask %x", debugMask);
          debugKeyActionTable[index].action(this, debugKeyActionTable[index].parameter);
          return;
        }
      }
    }
  
    
    event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, (value != 0), options);
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
    
    if (_keyboardShim != kLegacyShimDisabled) {

        NUB_LOCK;
    
        IOHID_DEBUG(kIOHIDDebugCode_DispatchKeyboard, usagePage, usage, value, options);
        
        if (usagePage == kHIDPage_KeyboardOrKeypad ||
           (_keyboard.appleVendorSupported && (usagePage == kHIDPage_AppleVendorKeyboard || (usagePage == kHIDPage_AppleVendorTopCase && usage == kHIDUsage_AV_TopCase_KeyboardFn)))) {
            if ( !_keyboardNub ) {
                _keyboardNub = newKeyboardShim();
            }
            if ( _keyboardNub ) {
                _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, (value != 0), options);
            }
        }

        if (usagePage == kHIDPage_Consumer) {
            if ( !_consumerNub )
                _consumerNub = newConsumerShim();
            
            if ( _consumerNub ) {
                _consumerNub->dispatchConsumerEvent(_keyboardNub, timeStamp, usagePage, usage, value, options);
            }
        }
        
        NUB_UNLOCK;
    }
#endif /* TARGET_OS_OSX */ // }

}

//====================================================================================================
// IOHIDEventService::dispatchRelativePointerEvent
//====================================================================================================
void IOHIDEventService::dispatchRelativePointerEventWithFixed(
                                AbsoluteTime                timeStamp,
                                IOFixed                     dx,
                                IOFixed                     dy,
                                UInt32                      buttonState,
                                IOOptionBits                options)
{

    IOHID_DEBUG(kIOHIDDebugCode_DispatchRelativePointer, dx, dy, buttonState, options);

    if ( ! _readyForInputReports )
        return;

    if ( !dx && !dy && buttonState == _relativePointer.buttonState )
        return;

    
    IOHIDEvent *event = IOHIDEvent::relativePointerEventWithFixed(timeStamp, dx, dy, 0, buttonState, _relativePointer.buttonState);

    if ( event ) {
        dispatchEvent(event);
        event->release();
    }


#ifdef POINTING_SHIM_SUPPORT
    if (_pointingShim != kLegacyShimDisabled) {
        
        NUB_LOCK;
        
        if (!_pointingNub ) {
            _pointingNub = newPointingShim();
        }
        if (_pointingNub) {
            _pointingNub->dispatchRelativePointerEvent(timeStamp, dx, dy, buttonState, options);
        }
      
        NUB_UNLOCK;
    }
#endif
    
    _relativePointer.buttonState = buttonState;

}

//====================================================================================================
// IOHIDEventService::dispatchRelativePointerEvent
//====================================================================================================
void IOHIDEventService::dispatchRelativePointerEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      dx,
                                SInt32                      dy,
                                UInt32                      buttonState,
                                IOOptionBits                options)
{
    dispatchRelativePointerEventWithFixed (timeStamp, dx << 16, dy << 16, buttonState, options);
}

static IOFixed __ScaleToFixed(int32_t value, int32_t min, int32_t max)
{
    int32_t range = max - min;
    int32_t offset = value - min;

    return IOFixedDivide(offset<<16, range<<16);
}

//====================================================================================================
// IOHIDEventService::dispatchAbsolutePointerEvent
//====================================================================================================
void IOHIDEventService::dispatchAbsolutePointerEvent(
                                                     AbsoluteTime                timeStamp,
                                                     SInt32                      x,
                                                     SInt32                      y,
                                                     IOGBounds *                 bounds,
                                                     UInt32                      buttonState,
                                                     bool                        inRange __unused,
                                                     SInt32                      tipPressure __unused,
                                                     SInt32                      tipPressureMin __unused,
                                                     SInt32                      tipPressureMax __unused,
                                                     IOOptionBits                options)
{
#if TARGET_OS_IPHONE

    dispatchDigitizerEvent(timeStamp, 0, kDigitizerTransducerTypeStylus, inRange, buttonState, __ScaleToFixed(x, bounds->minx, bounds->maxx), __ScaleToFixed(y, bounds->miny, bounds->maxy), __ScaleToFixed(tipPressure, tipPressureMin, tipPressureMax));
    
    (void)options;
#else
    IOHID_DEBUG(kIOHIDDebugCode_DispatchAbsolutePointer, x, y, buttonState, options);

    // Built-in like the virtual digitizer Apple's VMs expose, whose events WindowServer places on screen
    IOHIDEvent *event = IOHIDEvent::absolutePointerEvent(timeStamp, __ScaleToFixed(x, bounds->minx, bounds->maxx), __ScaleToFixed(y, bounds->miny, bounds->maxy), 0, buttonState, _absolutePointer.buttonState, kIOHIDEventOptionIsBuiltIn);
    
    if (event) {
        dispatchEvent(event);
        OSSafeReleaseNULL(event);
    }

#ifdef POINTING_SHIM_SUPPORT
    if (_pointingShim != kLegacyShimDisabled) {
        
        NUB_LOCK;

        if ( !_pointingNub )
            _pointingNub = newPointingShim();

        NUB_UNLOCK;
    }
#endif
  
    _absolutePointer.buttonState = buttonState;

#endif /* TARGET_OS_IPHONE */

}


//====================================================================================================
// IOHIDEventService::dispatchScrollWheelEvent
//====================================================================================================
void IOHIDEventService::dispatchScrollWheelEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      deltaAxis1,
                                SInt32                      deltaAxis2,
                                SInt32                      deltaAxis3,
                                IOOptionBits                options __unused)
{
    dispatchScrollWheelEventWithFixed (timeStamp, deltaAxis1 << 16, deltaAxis2 << 16, deltaAxis3 << 16);
}

//====================================================================================================
// IOHIDEventService::dispatchTabletPointEvent
//====================================================================================================
void IOHIDEventService::dispatchTabletPointerEvent(
                                AbsoluteTime                timeStamp __unused,
                                UInt32                      transducerID __unused,
                                SInt32                      x,
                                SInt32                      y,
                                SInt32                      z __unused,
                                IOGBounds *                 bounds __unused,
                                UInt32                      buttonState,
                                SInt32                      tipPressure __unused,
                                SInt32                      tipPressureMin __unused,
                                SInt32                      tipPressureMax __unused,
                                SInt32                      barrelPressure __unused,
                                SInt32                      barrelPressureMin __unused,
                                SInt32                      barrelPressureMax __unused,
                                SInt32                      tiltX __unused,
                                SInt32                      tiltY __unused,
                                UInt32                      twist __unused,
                                IOOptionBits                options)
{
    IOHID_DEBUG(kIOHIDDebugCode_DispatchTabletPointer, x, y, buttonState, options);


    if ( ! _readyForInputReports )
        return;

#ifdef POINTING_SHIM_SUPPORT
  
    if (_pointingShim != kLegacyShimDisabled) {
        NUB_LOCK;

        if ( !_pointingNub )
            _pointingNub = newPointingShim();

//        NXEventData         tabletData = {};
//
//        ScalePressure(&tipPressure, tipPressureMin, tipPressureMax, 0, kMaxSystemTipPressure);
//        ScalePressure(&barrelPressure, barrelPressureMin, barrelPressureMax, -kMaxSystemBarrelPressure, kMaxSystemBarrelPressure);
//
//        IOGPoint newLoc;
//
//        newLoc.x = x;
//        newLoc.y = y;
//
//        //IOHIDSystem::scaleLocationToCurrentScreen(&newLoc, bounds);
//
//        tabletData.tablet.x                    = newLoc.x;
//        tabletData.tablet.y                    = newLoc.y;
//        tabletData.tablet.z        f            = z;
//        tabletData.tablet.buttons              = buttonState;
//        tabletData.tablet.pressure             = tipPressure;
//        tabletData.tablet.tilt.x               = tiltX;
//        tabletData.tablet.tilt.y               = tiltY;
//        tabletData.tablet.rotation             = twist;
//        tabletData.tablet.tangentialPressure   = barrelPressure;
//        tabletData.tablet.deviceID             = _digitizer.deviceID;
//
//        _pointingNub->dispatchTabletEvent(&tabletData, timeStamp);

        NUB_UNLOCK;
    }
#endif
}

//====================================================================================================
// IOHIDEventService::dispatchTabletProximityEvent
//====================================================================================================
void IOHIDEventService::dispatchTabletProximityEvent(
                                AbsoluteTime                timeStamp __unused,
                                UInt32                      transducerID,
                                bool                        inRange __unused,
                                bool                        invert __unused,
                                UInt32                      vendorTransducerUniqueID,
                                UInt32                      vendorTransducerSerialNumber,
                                IOOptionBits                options)
{

    IOHID_DEBUG(kIOHIDDebugCode_DispatchTabletProx, transducerID, vendorTransducerUniqueID, vendorTransducerSerialNumber, options);

    if ( ! _readyForInputReports )
        return;

#ifdef POINTING_SHIM_SUPPORT
  
    if (_pointingShim != kLegacyShimDisabled) {
        NUB_LOCK;

        if ( !_pointingNub )
            _pointingNub = newPointingShim();

//        NXEventData tabletData      = {};
//        UInt32      capabilityMask  = NX_TABLET_CAPABILITY_DEVICEIDMASK | NX_TABLET_CAPABILITY_ABSXMASK | NX_TABLET_CAPABILITY_ABSYMASK;
//        
//        if ( _digitizer.deviceID == 0 )
//            _digitizer.deviceID = IOHIDPointing::generateDeviceID();
//        
//        if ( options & kDigitizerCapabilityButtons )
//            capabilityMask |= NX_TABLET_CAPABILITY_BUTTONSMASK;
//        if ( options & kDigitizerCapabilityPressure )
//            capabilityMask |= NX_TABLET_CAPABILITY_PRESSUREMASK;
//        if ( options & kDigitizerCapabilityTangentialPressure )
//            capabilityMask |= NX_TABLET_CAPABILITY_TANGENTIALPRESSUREMASK;
//        if ( options & kDigitizerCapabilityZ )
//            capabilityMask |= NX_TABLET_CAPABILITY_ABSZMASK;
//        if ( options & kDigitizerCapabilityTiltX )
//            capabilityMask |= NX_TABLET_CAPABILITY_TILTXMASK;
//        if ( options & kDigitizerCapabilityTiltY )
//            capabilityMask |= NX_TABLET_CAPABILITY_TILTYMASK;
//        if ( options & kDigitizerCapabilityTwist )
//            capabilityMask |= NX_TABLET_CAPABILITY_ROTATIONMASK;
//
//        tabletData.proximity.vendorID               = getVendorID();
//        tabletData.proximity.tabletID               = getProductID();
//        tabletData.proximity.pointerID              = transducerID;
//        tabletData.proximity.deviceID               = _digitizer.deviceID;
//        tabletData.proximity.vendorPointerType      = NX_TABLET_POINTER_PEN;
//        tabletData.proximity.pointerSerialNumber    = vendorTransducerSerialNumber;
//        tabletData.proximity.uniqueID               = vendorTransducerUniqueID;
//        tabletData.proximity.capabilityMask         = capabilityMask;
//        tabletData.proximity.enterProximity         = inRange;
//        tabletData.proximity.pointerType            = invert ? NX_TABLET_POINTER_ERASER : NX_TABLET_POINTER_PEN;
//
//        _pointingNub->dispatchProximityEvent(&tabletData, timeStamp);

        NUB_UNLOCK;
    }
#endif
}

bool IOHIDEventService::readyForReports()
{
    return _readyForInputReports;
}

//==============================================================================
// IOHIDEventService::getDeviceUsagePairs
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  0);
OSArray * IOHIDEventService::getDeviceUsagePairs()
{
    //RY: Correctly deal with kIOHIDDeviceUsagePairsKey
    OSObject *obj = _provider->copyProperty(kIOHIDDeviceUsagePairsKey);
    OSArray * providerUsagePairs = _provider ? OSDynamicCast(OSArray, obj) : NULL;

    if ( providerUsagePairs && ( providerUsagePairs != _deviceUsagePairs ) ) {
        setProperty(kIOHIDDeviceUsagePairsKey, providerUsagePairs);
        if ( _deviceUsagePairs )
            _deviceUsagePairs->release();

        _deviceUsagePairs = providerUsagePairs;
        _deviceUsagePairs->retain();
    } else if ( !_deviceUsagePairs ) {
        _deviceUsagePairs = OSArray::withCapacity(2);

        if ( _deviceUsagePairs ) {
            OSDictionary * pair = OSDictionary::withCapacity(2);

            if ( pair ) {
                OSNumber * number;

                number = OSNumber::withNumber(getPrimaryUsagePage(), 32);
                if ( number ) {
                    pair->setObject(kIOHIDDeviceUsagePageKey, number);
                    number->release();
                }

                number = OSNumber::withNumber(getPrimaryUsage(), 32);
                if ( number ) {
                    pair->setObject(kIOHIDDeviceUsageKey, number);
                    number->release();
                }

                _deviceUsagePairs->setObject(pair);
                pair->release();
            }
        }
    }
    OSSafeReleaseNULL(obj);
    return _deviceUsagePairs;
}

//==============================================================================
// IOHIDEventService::getReportInterval
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  1);
UInt32 IOHIDEventService::getReportInterval()
{
    UInt32 interval = 8000; // default to 8 milliseconds
    OSObject *object = copyProperty(kIOHIDReportIntervalKey, gIOServicePlane, kIORegistryIterateRecursively | kIORegistryIterateParents);
    OSNumber *num = OSDynamicCast(OSNumber, object);
    
    if ( num )
        interval = num->unsigned32BitValue();
    OSSafeReleaseNULL(object);

    return interval;
}

#define kCenteredPointerMaxRelativeValue 8
#define GET_RELATIVE_VALUE_FROM_CENTERED(centered,relative) \
    relative = (centered * kCenteredPointerMaxRelativeValue) >> 16;\

OSMetaClassDefineReservedUsed(IOHIDEventService,  2);
//==============================================================================
// IOHIDEventService::dispatchMultiAxisPointerEvent
//==============================================================================
void IOHIDEventService::dispatchMultiAxisPointerEvent(
                                                    AbsoluteTime               timeStamp,
                                                    UInt32                     buttonState,
                                                    IOFixed                    x,
                                                    IOFixed                    y,
                                                    IOFixed                    z,
                                                    IOFixed                    rX,
                                                    IOFixed                    rY,
                                                    IOFixed                    rZ,
                                                    IOOptionBits               options)
{

    bool    validAxis       = false;
    bool    validRelative   = false;
    bool    validScroll     = false;
    bool    isZButton       = false;
    UInt32  interval        = 0;

    if ( ! _readyForInputReports )
        return;

    validRelative   = ( options & kMultiAxisOptionRotationForTranslation ) ? rX || rY || _multiAxis.rX || _multiAxis.rY : x || y || _multiAxis.x || _multiAxis.y;
    validScroll     = rZ || _multiAxis.rZ;

    validAxis       = x || y || z || rX || rY || rZ || _multiAxis.x || _multiAxis.y || _multiAxis.z || _multiAxis.rX || _multiAxis.rY || _multiAxis.rZ;

    if ( options & kMultiAxisOptionZForScroll ) {
        validScroll |= z || _multiAxis.z;
    }
    // if z greater than .75 make it a button
    else if ( z > 0xc000 ){
        isZButton = true;
        buttonState |= 1;
    }

    validRelative |= buttonState != _multiAxis.buttonState;

    if ( validAxis || validRelative || validScroll ) {

        SInt32 dx = 0;
        SInt32 dy = 0;
        SInt32 sx = 0;
        SInt32 sy = 0;

        if ( !isZButton && (options & kMultiAxisOptionRotationForTranslation) ) {
            GET_RELATIVE_VALUE_FROM_CENTERED(-rY, dx);
            GET_RELATIVE_VALUE_FROM_CENTERED(rX, dy);
        } else {
            GET_RELATIVE_VALUE_FROM_CENTERED(x, dx);
            GET_RELATIVE_VALUE_FROM_CENTERED(y, dy);
        }

        GET_RELATIVE_VALUE_FROM_CENTERED(rZ, sy);

        if ( options & kMultiAxisOptionZForScroll )
            GET_RELATIVE_VALUE_FROM_CENTERED(z, sx);

#if TARGET_OS_IPHONE
        IOHIDEvent * subEvent = IOHIDEvent::multiAxisPointerEvent(timeStamp, x, y, z, rX, rY, rZ, buttonState, _multiAxis.buttonState, options);
        if ( subEvent ) {

            IOHIDEvent * event;

            if ( validRelative || (!validRelative && !validScroll) ) {
                event = IOHIDEvent::relativePointerEvent(timeStamp, dx, dy, 0, buttonState);
                if ( event ) {

                    if ( subEvent ) {
                        event->appendChild(subEvent);
                    }

                    dispatchEvent(event);
                    event->release();
                }
            }

            if ( validScroll ) {
                event = IOHIDEvent::scrollEvent(timeStamp, sx, sy, 0);
                if ( event ) {

                    if ( subEvent ) {
                        event->appendChild(subEvent);
                    }

                    dispatchEvent(event);
                    event->release();
                }
            }

            subEvent->release();
        }
#else
        dispatchRelativePointerEvent(timeStamp, dx, dy, buttonState, options);
        dispatchScrollWheelEvent(timeStamp, sy, sx, 0, options);
#endif

        if ( (options & kIOHIDEventOptionIsRepeat) == 0 ) {
            _multiAxis.timer->cancelTimeout();
            if ( validAxis )
                interval = getReportInterval() + getReportInterval()/2;
        } else if ( validAxis ) {
            interval = getReportInterval();
        }

        if ( interval )
            _multiAxis.timer->setTimeoutUS(interval);

    }

    _multiAxis.x            = x;
    _multiAxis.y            = y;
    _multiAxis.z            = z;
    _multiAxis.rX           = rX;
    _multiAxis.rY           = rY;
    _multiAxis.rZ           = rZ;
    _multiAxis.buttonState  = buttonState;
    _multiAxis.options      = (options & ~kIOHIDEventOptionIsRepeat);
}

//==============================================================================
// IOHIDEventService::dispatchDigitizerEventWithOrientation
//==============================================================================
void IOHIDEventService::dispatchDigitizerEventWithOrientation(
                                AbsoluteTime                    timeStamp,
                                UInt32                          transducerID,
                                DigitizerTransducerType         type __unused,
                                bool                            inRange,
                                UInt32                          buttonState,
                                IOFixed                         x,
                                IOFixed                         y,
                                IOFixed                         z,
                                IOFixed                         tipPressure,
                                IOFixed                         auxPressure,
                                IOFixed                         twist,
                                DigitizerOrientationType        orientationType,
                                IOFixed *                       orientationParams,
                                UInt32                          orientationParamCount,
                                IOOptionBits                    options)
{
    IOHID_DEBUG(kIOHIDDebugCode_DispatchDigitizer, x, y, buttonState, options);

    IOFixed params[5]   = {};
    bool    touch       = false;

    if ( ! _readyForInputReports )
        return;

    if ( !inRange ) {
        buttonState = 0;
        tipPressure = 0;
    }

    if ( orientationParams ) {
        orientationParamCount = min(5, orientationParamCount);

        bcopy(orientationParams, params, sizeof(IOFixed) * orientationParamCount);
    }

#if TARGET_OS_IPHONE
    IOHIDEvent *    collectionEvent = NULL;
    IOHIDEvent *    childEvent      = NULL;
    SInt32          eventMask       = 0;   // what's changed
    UInt32          eventOptions    = 0;

    if ( options & kDigitizerInvert )
        eventOptions |= kIOHIDTransducerInvert;

    childEvent = IOHIDEvent::digitizerEvent(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, eventOptions);
    require(childEvent, exit);

    buttonState |= (tipPressure>>16) & 1;

    if ( tipPressure )
        touch |= 1;
    else
        touch |= buttonState & 1;

    childEvent->setIntegerValue(kIOHIDEventFieldDigitizerTouch, touch);
    if (touch != _digitizer.touch) {
        eventMask |= kIOHIDDigitizerEventTouch;
    }

    if (inRange != _digitizer.range) {
        eventMask |= kIOHIDDigitizerEventRange;

        if ( inRange ) {
            _digitizer.x = x;
            _digitizer.y = y;
            eventMask |= kIOHIDDigitizerEventIdentity;
        }
    }

    if (inRange && ( (_digitizer.x != x) || (_digitizer.y != y) || (_digitizer.z != z) ) ) {
        eventMask |= kIOHIDDigitizerEventPosition;
    }


    childEvent->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, eventMask);

    collectionEvent = IOHIDEvent::digitizerEvent(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, eventOptions);
    require(collectionEvent, exit);

    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerCollection, TRUE);
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerRange, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerRange));
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerEventMask));
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerTouch, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerTouch));

    collectionEvent->appendChild(childEvent);

    dispatchEvent(collectionEvent);

exit:
    if ( collectionEvent )
        collectionEvent->release();

    if ( childEvent )
        childEvent->release();
    
    (void) orientationType;
#else
    bool invert = options & kDigitizerInvert;

    // Entering proximity
    if ( inRange && inRange != _digitizer.range ) {
        dispatchTabletProximityEvent(timeStamp, transducerID, inRange, invert, options);
    }

    if ( inRange ) {
        Bounds  bounds = {0, kMaxSystemAbsoluteRangeSigned, 0, kMaxSystemAbsoluteRangeSigned};

        SInt32 scaledX      = (SInt32)((SInt64)x * kMaxSystemAbsoluteRangeSigned) >> 16;
        SInt32 scaledY      = (SInt32)((SInt64)y * kMaxSystemAbsoluteRangeSigned) >> 16;
        SInt32 scaledZ      = (SInt32)((SInt64)z * kMaxSystemAbsoluteRangeSigned) >> 16;
        SInt32 scaledTP     = (SInt32)((SInt64)tipPressure * EV_MAXPRESSURE) >> 16;
        SInt32 scaledBP     = (SInt32)((SInt64)auxPressure * EV_MAXPRESSURE) >> 16;
        SInt32 scaledTiltX  = (SInt32)(((SInt64)params[0] * kMaxSystemAbsoluteRangeSigned)/90) >> 16;
        SInt32 scaledTiltY  = (SInt32)(((SInt64)params[1] * kMaxSystemAbsoluteRangeSigned)/90) >> 16;

        if ( orientationType != kDigitizerOrientationTypeTilt )
            bzero(params, sizeof(params));

        dispatchTabletPointerEvent(timeStamp, transducerID, scaledX, scaledY, scaledZ, &bounds, buttonState, scaledTP, 0, EV_MAXPRESSURE, scaledBP, 0, EV_MAXPRESSURE, scaledTiltX, scaledTiltY, twist>>10 /*10:6 fixed*/);

        dispatchAbsolutePointerEvent(timeStamp, scaledX, scaledY, &bounds, buttonState, inRange, scaledTP, 0, EV_MAXPRESSURE);
    }

    if ( !inRange && inRange != _digitizer.range ) {
        dispatchTabletProximityEvent(timeStamp, transducerID, inRange, invert, options);
    }



#endif /* TARGET_OS_IPHONE */

    _digitizer.range        = inRange;
    _digitizer.x            = x;
    _digitizer.y            = y;
    _digitizer.z            = z;
    _digitizer.touch        = touch;

}

//==============================================================================
// IOHIDEventService::dispatchDigitizerEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  3);
void IOHIDEventService::dispatchDigitizerEvent(
                                               AbsoluteTime                    timeStamp,
                                               UInt32                          transducerID,
                                               DigitizerTransducerType         type,
                                               bool                            inRange,
                                               UInt32                          buttonState,
                                               IOFixed                         x,
                                               IOFixed                         y,
                                               IOFixed                         z,
                                               IOFixed                         tipPressure,
                                               IOFixed                         auxPressure,
                                               IOFixed                         twist,
                                               IOOptionBits                    options )
{
    dispatchDigitizerEventWithOrientation(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, kDigitizerOrientationTypeTilt, NULL, 0, options);
}

//==============================================================================
// IOHIDEventService::dispatchDigitizerEventWithTiltOrientation
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  4);
void IOHIDEventService::dispatchDigitizerEventWithTiltOrientation(
                                                                  AbsoluteTime                    timeStamp,
                                                                  UInt32                          transducerID,
                                                                  DigitizerTransducerType         type,
                                                                  bool                            inRange,
                                                                  UInt32                          buttonState,
                                                                  IOFixed                         x,
                                                                  IOFixed                         y,
                                                                  IOFixed                         z,
                                                                  IOFixed                         tipPressure,
                                                                  IOFixed                         auxPressure,
                                                                  IOFixed                         twist,
                                                                  IOFixed                         tiltX,
                                                                  IOFixed                         tiltY,
                                                                  IOOptionBits                    options)
{
    IOFixed params[] = {tiltX, tiltY};

    dispatchDigitizerEventWithOrientation(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, kDigitizerOrientationTypeTilt, params, sizeof(params)/sizeof(IOFixed), options);
}


//==============================================================================
// IOHIDEventService::dispatchDigitizerEventWithPolarOrientation
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  5);
void IOHIDEventService::dispatchDigitizerEventWithPolarOrientation(
                                                                   AbsoluteTime                    timeStamp,
                                                                   UInt32                          transducerID,
                                                                   DigitizerTransducerType         type,
                                                                   bool                            inRange,
                                                                   UInt32                          buttonState,
                                                                   IOFixed                         x,
                                                                   IOFixed                         y,
                                                                   IOFixed                         z,
                                                                   IOFixed                         tipPressure,
                                                                   IOFixed                         auxPressure,
                                                                   IOFixed                         twist,
                                                                   IOFixed                         altitude,
                                                                   IOFixed                         azimuth,
                                                                   IOOptionBits                    options)
{
    IOFixed params[] = {altitude, azimuth};

    dispatchDigitizerEventWithOrientation(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, kDigitizerOrientationTypePolar, params, sizeof(params)/sizeof(IOFixed), options);
}


//==============================================================================
// IOHIDEventService::dispatchUnicodeEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  6);
void IOHIDEventService::dispatchUnicodeEvent(AbsoluteTime timeStamp, UInt8 * payload, UInt32 length, UnicodeEncodingType encoding, IOFixed quality, IOOptionBits options)
{
#if TARGET_OS_IPHONE
    IOHIDEvent * event = IOHIDEvent::unicodeEvent(timeStamp, payload, length, encoding, quality, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
#else
#pragma unused(timeStamp, payload, length, encoding, quality, options)
#endif
}


//==============================================================================
// IOHIDEventService::dispatchStandardGameControllerEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  12);
void IOHIDEventService::dispatchStandardGameControllerEvent(
                                                            AbsoluteTime                    timeStamp,
                                                            IOFixed                         dpadUp,
                                                            IOFixed                         dpadDown,
                                                            IOFixed                         dpadLeft,
                                                            IOFixed                         dpadRight,
                                                            IOFixed                         faceX,
                                                            IOFixed                         faceY,
                                                            IOFixed                         faceA,
                                                            IOFixed                         faceB,
                                                            IOFixed                         shoulderL,
                                                            IOFixed                         shoulderR,
                                                            IOOptionBits                    options)
{
    IOHIDEvent * event = IOHIDEvent::standardGameControllerEvent(timeStamp, dpadUp, dpadDown, dpadLeft, dpadRight, faceX, faceY, faceA, faceB, shoulderL, shoulderR, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
}

//==============================================================================
// IOHIDEventService::dispatchExtendedGameControllerEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  13);
void IOHIDEventService::dispatchExtendedGameControllerEvent(
                                                            AbsoluteTime                    timeStamp,
                                                            IOFixed                         dpadUp,
                                                            IOFixed                         dpadDown,
                                                            IOFixed                         dpadLeft,
                                                            IOFixed                         dpadRight,
                                                            IOFixed                         faceX,
                                                            IOFixed                         faceY,
                                                            IOFixed                         faceA,
                                                            IOFixed                         faceB,
                                                            IOFixed                         shoulderL1,
                                                            IOFixed                         shoulderR1,
                                                            IOFixed                         shoulderL2,
                                                            IOFixed                         shoulderR2,
                                                            IOFixed                         joystickX,
                                                            IOFixed                         joystickY,
                                                            IOFixed                         joystickZ,
                                                            IOFixed                         joystickRz,
                                                            IOOptionBits                    options)
{
    IOHIDEvent * event = IOHIDEvent::extendedGameControllerEvent(timeStamp, dpadUp, dpadDown, dpadLeft, dpadRight, faceX, faceY, faceA, faceB, shoulderL1, shoulderR1, shoulderL2, shoulderR2, joystickX, joystickY, joystickZ, joystickRz, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
}


OSMetaClassDefineReservedUsed(IOHIDEventService, 14);
void IOHIDEventService::dispatchBiometricEvent(
                                               AbsoluteTime                 timeStamp,
                                               IOFixed                      level,
                                               IOHIDBiometricEventType      eventType,
                                               IOOptionBits                 options)
{
    IOHIDEvent *event = IOHIDEvent::biometricEvent(timeStamp, level, eventType, options);
    
    if (event) {
        dispatchEvent(event);
        event->release();
    }
}


static const OSSymbol * openedKey = OSSymbol::withCStringNoCopy(kIOHIDDeviceOpenedByEventSystemKey);

void IOHIDEventService::close(IOService *forClient, IOOptionBits options)
{
    
    if (options & kIOHIDOpenedByEventSystem) {
        if (_provider && !isInactive()) {
            _provider->setProperty(kIOHIDDeviceOpenedByEventSystemKey, kOSBooleanFalse);
        }
    }
    super::close(forClient, options);
}

OSDefineMetaClassAndStructors(IOHIDClientData, OSObject)

IOHIDClientData * IOHIDClientData::withClientInfo(IOService *client, void* context, void * action)
{
    IOHIDClientData * data = new IOHIDClientData;

    if (!data) { }
    else if (data->init()) {
        data->client  = client;
        data->context = context;
        data->action  = action;
    } else {
        data->release();
        data = NULL;
    }

    return data;
}

//==============================================================================
// IOHIDEventService::open
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  7);
bool IOHIDEventService::open(   IOService *                 client,
                                IOOptionBits                options,
                                void *                      context,
                                Action                      action)
{
    IOHIDClientData * clientData = IOHIDClientData::withClientInfo(client, context, (void*)action);
    bool ret = false;

    if ( clientData ) {
        if (super::open(client, options, clientData)) {
            ret = true;
        }
        clientData->release();
    }

#if TARGET_OS_OSX

    if (_keyboardShim == kLegacyShimEnabledForSingleUserMode) {

        NUB_LOCK;

        _keyboardShim = kLegacyShimDisabled;

        if (_keyboardNub) {
            stopAndReleaseShim ( _keyboardNub, this );
            _keyboardNub = NULL;
        }

        if (_consumerNub) {
            stopAndReleaseShim ( _consumerNub, this );
            _consumerNub = NULL;
        }

        NUB_UNLOCK;
    }

#endif

    return ret;
}

//==============================================================================
// IOHIDEventService::dispatchEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  7);
void IOHIDEventService::dispatchEvent(IOHIDEvent * event, IOOptionBits options)
{
    OSDictionary *          clientDict;
    OSCollectionIterator *  iterator;
    IOHIDClientData *       clientData;
    OSObject *              clientKey;
    IOService *             client;
    void *                  context;
    Action                  action;
    uint64_t                currentTime;
    event->setSenderID(getRegistryEntryID());
    
#if TARGET_OS_OSX
    if (event->getType() == kIOHIDEventTypeKeyboard &&
        event->getIntegerValue(kIOHIDEventFieldKeyboardDown)) {
        _sleepDisplayTickle();
    }
#endif
    
    clock_get_uptime(&currentTime);
    
    IOHID_DEBUG(kIOHIDDebugCode_DispatchHIDEvent, event->getTimeStamp(), currentTime, options, getRegistryEntryID());
    
    if (_debugMask & kIOHIDDebugPerfEvent) {
        IOHIDEventPerfData data = {.driverDispatchTime = currentTime, 0, 0, 0};
        IOHIDEvent *perfEvent = IOHIDEvent::vendorDefinedEvent (
                                            currentTime,
                                            kHIDPage_AppleVendor,
                                            kHIDUsage_AppleVendor_Perf,
                                            0,
                                            (UInt8*)&data,
                                            sizeof(data),
                                            0
                                            );
        if (perfEvent) {
           event->appendChild(perfEvent);
           perfEvent->release();
        }
    }

    IOLockLock(_clientDictLock);

    _clientDict->retain();
    clientDict = _clientDict;

    IOLockUnlock(_clientDictLock);
    
    if ( !(iterator = OSCollectionIterator::withCollection(clientDict)) )
        return;

    while ((clientKey = iterator->getNextObject())) {

        clientData = OSDynamicCast(IOHIDClientData, clientDict->getObject((const OSSymbol *)clientKey));

        if ( !clientData )
            continue;

        client  = clientData->getClient();
        context = clientData->getContext();
        action  = (Action)clientData->getAction();

        if ( action )
            (*action)(client, this, context, event, options);
    }

    iterator->release();
    clientDict->release();
}

//==============================================================================
// IOHIDEventService::getPrimaryUsagePage
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  7);
UInt32 IOHIDEventService::getPrimaryUsagePage ()
{
    UInt32		primaryUsagePage = 0;
    OSArray *	deviceUsagePairs = getDeviceUsagePairs();

    if ( deviceUsagePairs && deviceUsagePairs->getCount() ) {
        OSDictionary * pair = OSDynamicCast(OSDictionary, deviceUsagePairs->getObject(0));

        if ( pair ) {
            OSNumber * number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsagePageKey));

            if ( number )
                primaryUsagePage = number->unsigned32BitValue();
        }
    }

    return primaryUsagePage;
}

//==============================================================================
// IOHIDEventService::getPrimaryUsage
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  10);
UInt32 IOHIDEventService::getPrimaryUsage ()
{
    UInt32		primaryUsage		= 0;
    OSArray *	deviceUsagePairs	= getDeviceUsagePairs();

    if ( deviceUsagePairs && deviceUsagePairs->getCount() ) {
        OSDictionary * pair = OSDynamicCast(OSDictionary, deviceUsagePairs->getObject(0));

        if ( pair ) {
            OSNumber * number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsageKey));

            if ( number )
                primaryUsage = number->unsigned32BitValue();
        }
    }

    return primaryUsage;
}

//==============================================================================
// IOHIDEventService::copyEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  11);
IOHIDEvent * IOHIDEventService::copyEvent(
                                IOHIDEventType              type __unused,
                                IOHIDEvent *                matching __unused,
                                IOOptionBits                options __unused)
{
    return NULL;
}

//==============================================================================
// IOService::newUserClient
//==============================================================================

IOReturn  IOHIDEventService::newUserClient (
                                            task_t owningTask,
                                            void * securityID,
                                            UInt32 type,
                                            OSDictionary * properties,
                                            IOUserClient ** handler )
{
    IOUserClient *client = NULL;
    
    if (type == kIOHIDEventServiceUserClientType) {
        
        client = OSTypeAlloc(IOHIDEventServiceUserClient);
    
    } else if (type == kIOHIDEventServiceFastPathUserClientType) {
    
        client = OSTypeAlloc(IOHIDEventServiceFastPathUserClient);
    
    } else {
    
        return super::newUserClient(owningTask, securityID, type, properties, handler);
    
    }
    
    if (client) {
 
        if (!client->initWithTask(owningTask, securityID, type)) {
            client->release();
            return kIOReturnBadArgument;
        }
        
        if ( !client->attach(this) ) {
            client->release();
            return kIOReturnUnsupported;
        }
        
        if ( !client->start(this) ) {
            client->detach(this);
            client->release();
            return kIOReturnUnsupported;
        }
        
        *handler = client;
        return kIOReturnSuccess;
    }
    return kIOReturnNoMemory;
}

//==============================================================================
// IOHIDEventService::message
//==============================================================================

IOReturn IOHIDEventService::message( UInt32 type, IOService * provider,  void * argument)
{

    IOReturn result;
    if (type == kIOMessageServiceIsRequestingClose) {
        messageClients(type, argument);
    }
    result = super::message(type, provider, argument);
    return result;
}

//==============================================================================
// IOHIDEventService::copyEventForClient
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  15);
IOHIDEvent * IOHIDEventService::copyEventForClient (
                                          OSObject *                  copySpec __unused,
                                          IOOptionBits                options __unused,
                                          void *                      clientContext __unused)
{
    return NULL;
}

//==============================================================================
// IOHIDEventService::copyPropertyForClient
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService, 16);
OSObject * IOHIDEventService::copyPropertyForClient (const char * aKey __unused, void * clientContext __unused) const {
    return NULL;
}


//==============================================================================
// IOHIDEventService::setPropertiesForClient
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService, 17);
IOReturn  IOHIDEventService::setPropertiesForClient (OSObject * properties __unused, void * clientContext __unused) {
    return kIOReturnUnsupported;
}

//==============================================================================
// IOHIDEventService::openForClient
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService, 18);
bool  IOHIDEventService::openForClient (IOService * client __unused, IOOptionBits options __unused, OSDictionary *property __unused, void ** clientContext __unused) {
    return false;
}

//==============================================================================
// IOHIDEventService::closeForClient
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService, 19);
void IOHIDEventService::closeForClient(IOService *client __unused, void *context __unused, IOOptionBits options __unused) {
    
}

//==============================================================================
// IOHIDEventService::dispatchExtendedGameControllerEventWithThumbstickButtons
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService, 20);
void IOHIDEventService::dispatchExtendedGameControllerEventWithThumbstickButtons(
                                                                                AbsoluteTime                    timeStamp,
                                                                                IOFixed                         dpadUp,
                                                                                IOFixed                         dpadDown,
                                                                                IOFixed                         dpadLeft,
                                                                                IOFixed                         dpadRight,
                                                                                IOFixed                         faceX,
                                                                                IOFixed                         faceY,
                                                                                IOFixed                         faceA,
                                                                                IOFixed                         faceB,
                                                                                IOFixed                         shoulderL1,
                                                                                IOFixed                         shoulderR1,
                                                                                IOFixed                         shoulderL2,
                                                                                IOFixed                         shoulderR2,
                                                                                IOFixed                         joystickX,
                                                                                IOFixed                         joystickY,
                                                                                IOFixed                         joystickZ,
                                                                                IOFixed                         joystickRz,
                                                                                boolean_t                       thumbstickButtonLeft,
                                                                                boolean_t                       thumbstickButtonRight,
                                                                                IOOptionBits                    options)
{
    IOHIDEvent * event = IOHIDEvent::extendedGameControllerEvent(timeStamp, dpadUp, dpadDown, dpadLeft, dpadRight, faceX, faceY, faceA, faceB, shoulderL1, shoulderR1, shoulderL2, shoulderR2, joystickX, joystickY, joystickZ, joystickRz, options);
    
    if (event) {
        event->setIntegerValue(kIOHIDEventFieldGameControllerThumbstickButtonRight, thumbstickButtonRight);
        event->setIntegerValue(kIOHIDEventFieldGameControllerThumbstickButtonLeft, thumbstickButtonLeft);
        dispatchEvent(event);
        event->release();
    }
}

//==============================================================================
// IOHIDEventService::copyMatchingEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService, 21);
IOHIDEvent *IOHIDEventService::copyMatchingEvent(OSDictionary *matching __unused)
{
    return NULL;
}

OSMetaClassDefineReservedUsed(IOHIDEventService, 22);
//====================================================================================================
// IOHIDEventService::dispatchScrollWheelEvent
//====================================================================================================
void IOHIDEventService::dispatchScrollWheelEventWithFixed(
                                AbsoluteTime                timeStamp,
                                IOFixed                     deltaAxis1,
                                IOFixed                     deltaAxis2,
                                IOFixed                     deltaAxis3,
                                IOOptionBits                options)
{
    uint8_t momentumOrPhase = (options & kHIDDispatchOptionPhaseAny) >> 4 | ((options & kHIDDispatchOptionScrollMomentumAny) << (4 - 1));
    IOHID_DEBUG(kIOHIDDebugCode_DispatchScroll, deltaAxis1, deltaAxis2, deltaAxis3, options);

    if ( ! _readyForInputReports )
        return;
    
    if ( !deltaAxis1 && !deltaAxis2 && !deltaAxis3 && !momentumOrPhase )
        return;

    IOHIDEvent *event = IOHIDEvent::scrollEventWithFixed(timeStamp, deltaAxis2, deltaAxis1, deltaAxis3); //yxz should be xyz
    if ( event ) {
        if (momentumOrPhase) {
          event->setPhase (momentumOrPhase);
        }
        dispatchEvent(event);
        event->release();
    }

#ifdef POINTING_SHIM_SUPPORT
    if (_pointingShim != kLegacyShimDisabled) {
        NUB_LOCK;

        if ( !_pointingNub )
            _pointingNub = newPointingShim();
        if (_pointingNub)
            _pointingNub->dispatchScrollWheelEvent(timeStamp, deltaAxis1 >> 16, deltaAxis2 >> 16, deltaAxis3 >> 16, options);

        NUB_UNLOCK;
    }
#endif
}


OSMetaClassDefineReservedUnused(IOHIDEventService, 23);
OSMetaClassDefineReservedUnused(IOHIDEventService, 24);
OSMetaClassDefineReservedUnused(IOHIDEventService, 25);
OSMetaClassDefineReservedUnused(IOHIDEventService, 26);
OSMetaClassDefineReservedUnused(IOHIDEventService, 27);
OSMetaClassDefineReservedUnused(IOHIDEventService, 28);
OSMetaClassDefineReservedUnused(IOHIDEventService, 29);
OSMetaClassDefineReservedUnused(IOHIDEventService, 30);
OSMetaClassDefineReservedUnused(IOHIDEventService, 31);


#pragma clang diagnostic ignored "-Wunused-parameter"

// DriverKit dispatch: needs the IIG-generated argument types, which PureDarwin does not build
#if PD_HID_DRIVERKIT
#include <IOKit/IOUserServer.h>

kern_return_t
IMPL(IOHIDEventService, _Start)
{
    setProperty(kIOHIDDKStartKey, kOSBooleanTrue);
    
    bool ret = start (provider);
        
    return ret ? kIOReturnSuccess : kIOReturnError;
}

kern_return_t
IMPL(IOHIDEventService, _DispatchKeyboardEvent)
{
    if (!repeat) {
        options |= kIOHIDKeyboardEventOptionsNoKeyRepeat;
    }
    
    dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
    
    return kIOReturnSuccess;
}

kern_return_t
IMPL(IOHIDEventService, _DispatchRelativePointerEvent)
{
    if (!accelerate) {
        options |= kIOHIDPointerEventOptionsNoAcceleration;
    }
    
    dispatchRelativePointerEventWithFixed(timeStamp, dx, dy, buttonState, options);
    
    return kIOReturnSuccess;
}

kern_return_t
IMPL(IOHIDEventService, _DispatchAbsolutePointerEvent)
{
    if (!accelerate) {
        options |= kIOHIDPointerEventOptionsNoAcceleration;
    }
    
    IOHIDEvent *event = IOHIDEvent::absolutePointerEvent(timeStamp, x, y, 0, buttonState, 0, options);
    
    if (event) {
        dispatchEvent(event);
        event->release();
    }
    
    return kIOReturnSuccess;
}

kern_return_t
IMPL(IOHIDEventService, _DispatchRelativeScrollWheelEvent)
{
    if (!accelerate) {
        options |= kIOHIDScrollEventOptionsNoAcceleration;
    }
    
    dispatchScrollWheelEventWithFixed(timeStamp, dx, dy, dz, options);
    
    return kIOReturnSuccess;
}

void
IMPL(IOHIDEventService, SetLED)
{
    return;
}

kern_return_t
IMPL(IOHIDEventService, SetEventMemory)
{
    IOLockLock(_eventMemLock);
    OSSafeReleaseNULL(_eventMemory);
    if (memory) {
        _eventMemory = memory;
        _eventMemory->retain();
    }
    IOLockUnlock(_eventMemLock);
    
    return kIOReturnSuccess;
}

kern_return_t
IMPL(IOHIDEventService, EventAvailable)
{
    IOHIDEvent *event = NULL;
    IOReturn ret = kIOReturnError;
    
    IOLockLock(_eventMemLock);
    
    if (_eventMemory && length > _eventMemory->getLength()) {
        ret = kIOReturnBadArgument;
        IOLockUnlock(_eventMemLock);
        return ret;
    }
    
    
    if (_eventMemory) {
        event = IOHIDEvent::withBytes(_eventMemory->getBytesNoCopy(), length);
    }
    IOLockUnlock(_eventMemLock);
    
    if (event && !isInactive()) {
        dispatchEvent(event);
        event->release();
        ret = kIOReturnSuccess;
    } else {
        HIDServiceLogError("Failed to create event");
    }
    return ret;
}
#endif /* PD_HID_DRIVERKIT */
