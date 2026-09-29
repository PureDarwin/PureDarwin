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

#include <IOKit/IOLib.h>    // IOMalloc/IOFree
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/hidsystem/IOHIDSystem.h>
#include <IOKit/IOEventSource.h>
#include <IOKit/IOMessage.h>

#include "IOHIDFamilyPrivate.h"
#include <IOKit/hid/IOHIDDevice.h>
#include "IOHIDElementPrivate.h"
#include "IOHIDDescriptorParserPrivate.h"
#include "IOHIDInterface.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDFamilyPrivate.h"
#include "IOHIDLibUserClient.h"
#include "IOHIDFamilyTrace.h"
#include "IOHIDEventSource.h"
#include "OSStackRetain.h"
#include "IOHIDDebug.h"
#include "IOHIDUsageTables.h"
#include "AppleHIDUsageTables.h"
#include "IOHIDDeviceElementContainer.h"

#include <sys/queue.h>
#include <machine/limits.h>
#include <os/overflow.h>
#include <stdatomic.h>

#if TARGET_OS_OSX
#include "IOHIKeyboard.h"
#include "IOHIPointing.h"
#endif

#include <IOKit/hidsystem/IOHIDShared.h>

//@todo we need this to be configrable
#define kHIDClientTimeoutUS     1000000ULL

#define HIDDeviceLogFault(fmt, ...)   HIDLogFault("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDDeviceLogError(fmt, ...)   HIDLogError("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDDeviceLog(fmt, ...)        HIDLog("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDDeviceLogInfo(fmt, ...)    HIDLogInfo("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDDeviceLogDebug(fmt, ...)   HIDLogDebug("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)

//===========================================================================
// IOHIDAsyncReportQueue class

class IOHIDAsyncReportQueue : public IOEventSource
{
    OSDeclareDefaultStructors( IOHIDAsyncReportQueue )

    struct AsyncReportEntry {
        queue_chain_t   chain;

        AbsoluteTime                    timeStamp;
        uint8_t *                       reportData;
        size_t                          reportLength;
        IOHIDReportType                 reportType;
        IOOptionBits                    options;
        UInt32                          completionTimeout;
        IOHIDCompletion                 completion;
    };

    IOLock *        fQueueLock;
    queue_head_t    fQueueHead;

public:
    static IOHIDAsyncReportQueue *withOwner(IOHIDDevice *inOwner);

    virtual bool init(IOHIDDevice *owner);

    virtual bool checkForWork(void) APPLE_KEXT_OVERRIDE;

    virtual IOReturn postReport(AbsoluteTime         timeStamp,
                                IOMemoryDescriptor * report,
                                IOHIDReportType      reportType,
                                IOOptionBits         options,
                                UInt32               completionTimeout,
                                IOHIDCompletion *    completion);
};

OSDefineMetaClassAndStructors( IOHIDAsyncReportQueue, IOEventSource )

//---------------------------------------------------------------------------
IOHIDAsyncReportQueue *IOHIDAsyncReportQueue::withOwner(IOHIDDevice *inOwner)
{
    IOHIDAsyncReportQueue *es = NULL;
    bool result = false;

    es = OSTypeAlloc( IOHIDAsyncReportQueue );
    if (es) {
        result = es->init( inOwner/*, inAction*/ );

        if (!result) {
            es->release();
            es = NULL;
        }

    }

    return es;
}

//---------------------------------------------------------------------------
bool IOHIDAsyncReportQueue::init(IOHIDDevice *owner_I)
{
    queue_init( &fQueueHead );
    fQueueLock = IOLockAlloc();
    return IOEventSource::init(owner_I/*, action*/);
}

//---------------------------------------------------------------------------
bool IOHIDAsyncReportQueue::checkForWork()
{
    bool moreToDo = false;

    IOLockLock(fQueueLock);
    if (!queue_empty(&fQueueHead)) {

        AsyncReportEntry *entry = NULL;
        queue_remove_first(&fQueueHead, entry, AsyncReportEntry *, chain);

        if (entry) {
            IOLockUnlock(fQueueLock);

            IOReturn status;

            IOMemoryDescriptor *md = IOMemoryDescriptor::withAddress(entry->reportData, entry->reportLength, kIODirectionOut);

            if (md) {
                md->prepare();

                status = ((IOHIDDevice *)owner)->handleReportWithTime(entry->timeStamp, md, entry->reportType, entry->options);

                md->complete();

                md->release();

                if (entry->completion.action) {
                    (entry->completion.action)(entry->completion.target, entry->completion.parameter, status, 0);
                }
            }

            IOFree(entry->reportData, entry->reportLength);
            IODelete(entry, AsyncReportEntry, 1);

            IOLockLock(fQueueLock);
        }
    }

    moreToDo = (!queue_empty(&fQueueHead));
    IOLockUnlock(fQueueLock);

    return moreToDo;
}

//---------------------------------------------------------------------------
IOReturn IOHIDAsyncReportQueue::postReport(
                                        AbsoluteTime         timeStamp,
                                        IOMemoryDescriptor * report,
                                        IOHIDReportType      reportType,
                                        IOOptionBits         options,
                                        UInt32               completionTimeout,
                                        IOHIDCompletion *    completion)
{
    AsyncReportEntry *entry;

    entry = IONew(AsyncReportEntry, 1);
    if (!entry)
        return kIOReturnError;

    bzero(entry, sizeof(AsyncReportEntry));

    entry->timeStamp = timeStamp;

    entry->reportLength = report->getLength();
    entry->reportData = (uint8_t *)IOMalloc(entry->reportLength);

    if (entry->reportData) {
        report->readBytes(0, entry->reportData, entry->reportLength);

        entry->reportType = reportType;
        entry->options = options;
        entry->completionTimeout = completionTimeout;

        if (completion)
            entry->completion = *completion;

        IOLockLock(fQueueLock);
        queue_enter(&fQueueHead, entry, AsyncReportEntry *, chain);
        IOLockUnlock(fQueueLock);

        signalWorkAvailable();
    } else {
        IODelete(entry, AsyncReportEntry, 1);
    }

    return kIOReturnSuccess;
}

//===========================================================================
// IOHIDDevice class

#undef  super
#define super IOService

OSDefineMetaClassAndAbstractStructors( IOHIDDevice, IOService )

// RESERVED IOHIDDevice CLASS VARIABLES
// Defined here to avoid conflicts from within header file
#define _clientSet					_reserved->clientSet
#define _seizedClient				_reserved->seizedClient
#define _eventDeadline				_reserved->eventDeadline
#define _performTickle				_reserved->performTickle
#define _performWakeTickle          _reserved->performWakeTickle
#define _interfaceNubs              _reserved->interfaceNubs
#define _hierarchElements           _reserved->hierarchElements
#define _interfaceElementArrays     _reserved->interfaceElementArrays
#define _asyncReportQueue           _reserved->asyncReportQueue
#define _workLoop                   _reserved->workLoop
#define _eventSource                _reserved->eventSource
#define _deviceNotify               _reserved->deviceNotify
#define _elementContainer           _reserved->elementContainer

#define WORKLOOP_LOCK   ((IOHIDEventSource *)_eventSource)->lock()
#define WORKLOOP_UNLOCK ((IOHIDEventSource *)_eventSource)->unlock()

#define kIOHIDEventThreshold	10

#define GetElement(index)  \
    (IOHIDElementPrivate *) _elementArray->getObject((UInt32)index)

#ifndef kIOUserClientCrossEndianKey
#define kIOUserClientCrossEndianKey "IOUserClientCrossEndian"
#endif

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
#endif

// *** GAME DEVICE HACK ***
static SInt32 g3DGameControllerCount = 0;
// *** END GAME DEVICE HACK ***

//---------------------------------------------------------------------------
// Initialize an IOHIDDevice object.

bool IOHIDDevice::init( OSDictionary * dict )
{
    _reserved = IONew( ExpansionData, 1 );

    if (!_reserved)
        return false;

	bzero(_reserved, sizeof(ExpansionData));
    
    // Create an OSSet to store client objects. Initial capacity
    // (which can grow) is set at 2 clients.

    _clientSet = OSSet::withCapacity(2);
    if ( _clientSet == 0 )
        return false;

    return super::init(dict);
}

//---------------------------------------------------------------------------
// Free an IOHIDDevice object after its retain count drops to zero.
// Release all resource.

void IOHIDDevice::free()
{
    OSSafeReleaseNULL(_elementArray);
    OSSafeReleaseNULL(_hierarchElements);
    OSSafeReleaseNULL(_interfaceElementArrays);
    OSSafeReleaseNULL(_elementContainer);

    if ( _clientSet )
    {
        // Should not have any clients.
        if (_clientSet->getCount()) {
            _clientSet->iterateObjects(^bool(OSObject *object) {
                IOService *client = OSDynamicCast(IOService, object);
                
                if (client) {
                    HIDDeviceLogError("Device never closed by %s: 0x%llx",
                                      client->getName(), client->getRegistryEntryID());
                }
                
                return false;
            });
        }
        OSSafeReleaseNULL(_clientSet);
    }

    OSSafeReleaseNULL(_eventSource);
    OSSafeReleaseNULL(_workLoop);

    if ( _reserved )
    {
        IODelete( _reserved, ExpansionData, 1 );
    }

    return super::free();
}

//---------------------------------------------------------------------------
// Start up the IOHIDDevice.

bool IOHIDDevice::start( IOService * provider )
{
    IOMemoryDescriptor *    reportDescriptor        = NULL;
    IOMemoryMap *           reportDescriptorMap     = NULL;
    OSData *                reportDescriptorData    = NULL;
    OSNumber *              primaryUsagePage        = NULL;
    OSNumber *              primaryUsage            = NULL;
    OSObject *              obj                     = NULL;
    OSObject *              obj2                    = NULL;
    OSDictionary *          matching                = NULL;
    bool                    multipleInterface       = false;
    IOReturn                ret;
    bool                    result                  = false;

    HIDDeviceLogInfo("start");

    require(super::start(provider), exit);
    
    _workLoop = getWorkLoop();
    require_action(_workLoop, exit, HIDDeviceLogError("failed to get a work loop"));
    
    _workLoop->retain();
    
    _eventSource = IOHIDEventSource::HIDEventSource(this, NULL);
    require(_eventSource, exit);
    require_noerr(_workLoop->addEventSource(_eventSource), exit);

    // Call handleStart() before fetching the report descriptor.
    require_action(handleStart(provider), exit, HIDDeviceLogError("handleStart failed"));

    // Fetch report descriptor for the device, and parse it.
    require_noerr(newReportDescriptor(&reportDescriptor), exit);
    require(reportDescriptor, exit);
    
    reportDescriptorMap = reportDescriptor->map();
    require(reportDescriptorMap, exit);
    
    reportDescriptorData = OSData::withBytes((void*)reportDescriptorMap->getVirtualAddress(), (unsigned int)reportDescriptorMap->getSize());
    require(reportDescriptorData, exit);

    setProperty(kIOHIDReportDescriptorKey, reportDescriptorData);

    ret = parseReportDescriptor( reportDescriptor );
    require_noerr_action(ret, exit, HIDDeviceLogError("failed to parse report descriptor"));

    // Enable multiple interfaces if the first top-level collection has usage pair kHIDPage_AppleVendor, kHIDUsage_AppleVendor_MultipleInterfaces
    if (_elementArray->getCount() > 1 &&
        ((IOHIDElement *)_elementArray->getObject(1))->getUsagePage() == kHIDPage_AppleVendor &&
        ((IOHIDElement *)_elementArray->getObject(1))->getUsage() == kHIDUsage_AppleVendor_MultipleInterfaces) {
        setProperty(kIOHIDMultipleInterfaceEnabledKey, kOSBooleanTrue);
    }

    multipleInterface = (getProperty(kIOHIDMultipleInterfaceEnabledKey) == kOSBooleanTrue);

    _hierarchElements = _elementContainer->getFlattenedElements();
    require(_hierarchElements, exit);
    
    _hierarchElements->retain();
    
    if (conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse)) {
        setupResolution();
    }

    if (multipleInterface) {
        HIDDeviceLogDebug("has multiple interface support");
        
        _interfaceElementArrays = _elementContainer->getFlattenedCollections();
        require(_interfaceElementArrays, exit);
        
        _interfaceElementArrays->retain();
    }
    else {
        // Single interface behavior - all elements in single array, including additional top-level collections.
        _interfaceElementArrays = OSArray::withCapacity(1);
        require(_interfaceElementArrays, exit);

        _interfaceElementArrays->setObject(_hierarchElements);
    }
    
    // Once the report descriptors have been parsed, we are ready
    // to handle reports from the device.

    _readyForInputReports = true;

    // Publish properties to the registry before any clients are
    // attached.
    require(publishProperties(provider), exit);

    // *** GAME DEVICE HACK ***
    obj = copyProperty(kIOHIDPrimaryUsagePageKey);
    primaryUsagePage = OSDynamicCast(OSNumber, obj);
    obj2 = copyProperty(kIOHIDPrimaryUsageKey);
    primaryUsage = OSDynamicCast(OSNumber, obj2);

    if ((primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == 0x05)) &&
        (primaryUsage && (primaryUsage->unsigned32BitValue() == 0x01))) {
        OSIncrementAtomic(&g3DGameControllerCount);
    }

    OSSafeReleaseNULL(obj);
    OSSafeReleaseNULL(obj2);
    // *** END GAME DEVICE HACK ***
    
    // create interface
    _interfaceNubs = OSArray::withCapacity(1);
    require(_interfaceNubs, exit);

    for (unsigned int i = 0; i < _interfaceElementArrays->getCount(); i++) {
        IOHIDInterface * interface  = IOHIDInterface::withElements((OSArray *)_interfaceElementArrays->getObject(i));
        IOHIDElement *   root       = (IOHIDElement *)((OSArray *)_interfaceElementArrays->getObject(i))->getObject(0);

        require(interface, exit);

        // If there's more than one interface, set element subset for diagnostics.
        if (_interfaceElementArrays->getCount() > 1) {
            interface->setProperty(kIOHIDElementKey, root);
        }

        _interfaceNubs->setObject(interface);

        OSSafeReleaseNULL(interface);
    }
    
    // set interface properties
    publishProperties(NULL);
    
    if (conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) ||
        conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse) ||
        conformsTo(kHIDPage_Digitizer, kHIDUsage_Dig_TouchPad)) {
        setProperty(kIOHIDRequiresTCCAuthorizationKey, kOSBooleanTrue);
    }

    matching = registryEntryIDMatching(getRegistryEntryID());
    _deviceNotify = addMatchingNotification(gIOFirstMatchNotification,
                                            matching,
                                            IOHIDDevice::_publishDeviceNotificationHandler,
                                            NULL);
    matching->release();
    require(_deviceNotify, exit);
    
    obj = getProperty(kIOHIDRegisterServiceKey);
    if (obj != kOSBooleanFalse) {
        registerService();
    }
    
    result = true;
    
exit:
    if ( !result ) {
        HIDDeviceLogError("start failed");
        stop(provider);
    }
    
    if ( reportDescriptor )
        reportDescriptor->release();
    if ( reportDescriptorData )
        reportDescriptorData->release();
    if ( reportDescriptorMap )
        reportDescriptorMap->release();

    return result;
}

bool IOHIDDevice::_publishDeviceNotificationHandler(void * target __unused,
                                                    void * refCon __unused,
                                                    IOService * newService,
                                                    IONotifier * notifier __unused)
{
    IOHIDDevice *self = OSDynamicCast(IOHIDDevice, newService);

    if (self) {
        self->createInterface();
    }

    return true;
}

//---------------------------------------------------------------------------
// Stop the IOHIDDevice.

void IOHIDDevice::stop(IOService * provider)
{
    HIDDeviceLogInfo("stop");

    // *** GAME DEVICE HACK ***
    OSObject *obj = copyProperty(kIOHIDPrimaryUsagePageKey);
    OSObject *obj2 = copyProperty(kIOHIDPrimaryUsageKey);
    OSNumber *primaryUsagePage =  OSDynamicCast(OSNumber, obj);
    OSNumber *primaryUsage = OSDynamicCast(OSNumber, obj2);

    if ((primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == 0x05)) &&
        (primaryUsage && (primaryUsage->unsigned32BitValue() == 0x01))) {
        OSDecrementAtomic(&g3DGameControllerCount);
    }
    OSSafeReleaseNULL(obj);
    OSSafeReleaseNULL(obj2);
    // *** END GAME DEVICE HACK ***

    if (_deviceNotify) {
        _deviceNotify->remove();
        _deviceNotify = NULL;
    }
    
    handleStop(provider);
    
    if (_workLoop) {
        if (_eventSource) {
            _workLoop->removeEventSource(_eventSource);
        }
    }

    _readyForInputReports = false;

    OSSafeReleaseNULL(_interfaceNubs);

    super::stop(provider);
}


bool IOHIDDevice::matchPropertyTable(OSDictionary * table, SInt32 * score)
{
    bool    match       = true;
    RETAIN_ON_STACK(this);

    // Ask our superclass' opinion.
    if (super::matchPropertyTable(table, score) == false)
        return false;

    match = MatchPropertyTable(this, table, score);

    // *** HACK ***
    // RY: For games that are accidentaly matching on the keys
    // PrimaryUsage = 0x01
    // PrimaryUsagePage = 0x05
    // If there no devices present that contain these values,
    // then return true.
    if (!match && (g3DGameControllerCount <= 0) && table) {
        OSNumber *primaryUsage = OSDynamicCast(OSNumber, table->getObject(kIOHIDPrimaryUsageKey));
        OSNumber *primaryUsagePage = OSDynamicCast(OSNumber, table->getObject(kIOHIDPrimaryUsagePageKey));

        if ((primaryUsage && (primaryUsage->unsigned32BitValue() == 0x01)) &&
            (primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == 0x05))) {
            match = true;
            HIDDeviceLogError("IOHIDManager: It appears that an application is attempting to locate an invalid device.  A workaround is in currently in place, but will be removed after version 10.2");
        }
    }
    // *** END HACK ***

    return match;
}



//---------------------------------------------------------------------------
// Fetch and publish HID properties to the registry.

bool IOHIDDevice::publishProperties(IOService * provider __unused)
{
#define SET_PROP_FROM_VALUE(key, value) \
    do {                                \
        OSObject *prop = value;         \
        if (prop) {                     \
            if (service) {              \
                service->setProperty(key, prop); \
            }                           \
            prop->release();            \
        }                               \
    } while (0)

    OSArray * services = OSArray::withCapacity(1);
    if (!services)
        return false;

    if (_interfaceNubs) {
        services->merge(_interfaceNubs);
    }
    services->setObject(this);

    for (unsigned int i = 0; i < services->getCount(); i++) {
        OSArray *       deviceUsagePairs = NULL;
        OSDictionary *  primaryUsagePair;
        OSNumber *      primaryUsage;
        OSNumber *      primaryUsagePage;
        IOService *     service = OSDynamicCast(IOService, services->getObject(i));
        if (!service)
            continue;

        // Need interface index to set the right properties for this interface.
        // Relies on interfaces being first in the service array.
        if (OSDynamicCast(IOHIDInterface, service)) {
            deviceUsagePairs = newDeviceUsagePairs((OSArray *)_interfaceElementArrays->getObject(i), 0);
        }
        else {
            deviceUsagePairs = newDeviceUsagePairs();
        }

        if (!deviceUsagePairs)
            continue;

        primaryUsagePair = (OSDictionary *)deviceUsagePairs->getObject(0);
        if (!primaryUsagePair) {
            OSSafeReleaseNULL(deviceUsagePairs);
            continue;
        }

        primaryUsage = (OSNumber *)primaryUsagePair->getObject(kIOHIDDeviceUsageKey);
        primaryUsagePage = (OSNumber *)primaryUsagePair->getObject(kIOHIDDeviceUsagePageKey);
        if (!primaryUsage || !primaryUsagePage) {
            OSSafeReleaseNULL(deviceUsagePairs);
            continue;
        }

        primaryUsage->retain();
        primaryUsagePage->retain();

        SET_PROP_FROM_VALUE(    kIOHIDTransportKey,         newTransportString()        );
        SET_PROP_FROM_VALUE(    kIOHIDVendorIDKey,          newVendorIDNumber()         );
        SET_PROP_FROM_VALUE(    kIOHIDVendorIDSourceKey,    newVendorIDSourceNumber()   );
        SET_PROP_FROM_VALUE(    kIOHIDProductIDKey,         newProductIDNumber()        );
        SET_PROP_FROM_VALUE(    kIOHIDVersionNumberKey,     newVersionNumber()          );
        SET_PROP_FROM_VALUE(    kIOHIDManufacturerKey,      newManufacturerString()     );
        SET_PROP_FROM_VALUE(    kIOHIDProductKey,           newProductString()          );
        SET_PROP_FROM_VALUE(    kIOHIDLocationIDKey,        newLocationIDNumber()       );
        SET_PROP_FROM_VALUE(    kIOHIDCountryCodeKey,       newCountryCodeNumber()      );
        SET_PROP_FROM_VALUE(    kIOHIDSerialNumberKey,      newSerialNumberString()     );
        SET_PROP_FROM_VALUE(    kIOHIDReportIntervalKey,    newReportIntervalNumber()   );
        SET_PROP_FROM_VALUE(    kIOHIDPrimaryUsageKey,      primaryUsage                );
        SET_PROP_FROM_VALUE(    kIOHIDPrimaryUsagePageKey,  primaryUsagePage            );
        SET_PROP_FROM_VALUE(    kIOHIDDeviceUsagePairsKey,  deviceUsagePairs            );
        SET_PROP_FROM_VALUE(    kIOHIDMaxInputReportSizeKey,    copyProperty(kIOHIDMaxInputReportSizeKey));
        SET_PROP_FROM_VALUE(    kIOHIDMaxOutputReportSizeKey,   copyProperty(kIOHIDMaxOutputReportSizeKey));
        SET_PROP_FROM_VALUE(    kIOHIDMaxFeatureReportSizeKey,  copyProperty(kIOHIDMaxFeatureReportSizeKey));
        SET_PROP_FROM_VALUE(    kIOHIDRelaySupportKey,          copyProperty(kIOHIDRelaySupportKey, gIOServicePlane));
        SET_PROP_FROM_VALUE(    kIOHIDReportDescriptorKey,      copyProperty(kIOHIDReportDescriptorKey));
        SET_PROP_FROM_VALUE(    kIOHIDProtectedAccessKey,       copyProperty(kIOHIDProtectedAccessKey));

        if ( getProvider() )
        {

            SET_PROP_FROM_VALUE("BootProtocol", getProvider()->copyProperty("bInterfaceProtocol"));
            SET_PROP_FROM_VALUE("HIDDefaultBehavior", copyProperty("HIDDefaultBehavior"));
            SET_PROP_FROM_VALUE(kIOHIDAuthenticatedDeviceKey, copyProperty(kIOHIDAuthenticatedDeviceKey));
        }
    }

    OSSafeReleaseNULL(services);

    return true;
}

//---------------------------------------------------------------------------
// Derived from start() and stop().

bool IOHIDDevice::handleStart(IOService * provider __unused)
{
    return true;
}

void IOHIDDevice::handleStop(IOService * provider __unused)
{
}

static inline bool ShouldPostDisplayActivityTickles(IOService *device, OSSet * clientSet, bool isSeized)
{
    OSObject *obj = device->copyProperty(kIOHIDPrimaryUsagePageKey);
    OSNumber *primaryUsagePage = OSDynamicCast(OSNumber, obj);

    if (!clientSet->getCount() || !primaryUsagePage ||
        (primaryUsagePage->unsigned32BitValue() != kHIDPage_GenericDesktop)) {
        OSSafeReleaseNULL(obj);
        return false;
    }
    OSSafeReleaseNULL(obj);

    // We have clients and this device is generic desktop.
    // Probe the client list to make sure that we are not
    // openned by an IOHIDEventService.  If so, there is
    // no reason to tickle the display, as the HID System
    // already does this.
    OSCollectionIterator *  iterator;
    OSObject *              object;
    bool                    returnValue = true;

    if ( !isSeized && (iterator = OSCollectionIterator::withCollection(clientSet)) )
    {
        bool done = false;
        while (!done) {
            iterator->reset();
            while (!done && (NULL != (object = iterator->getNextObject()))) {
                if ( object->metaCast("IOHIDEventService"))
                {
                    returnValue = false;
                    done = true;
                }
            }
            if (iterator->isValid()) {
                done = true;
            }
        }
        iterator->release();
    }
    return returnValue;
}

static inline bool ShouldPostDisplayActivityTicklesForWakeDevice(
    IOService *device, OSSet * clientSet, bool isSeized)
{
    bool        returnValue = false;
    OSObject *obj = device->copyProperty(kIOHIDPrimaryUsagePageKey);
    OSNumber *  primaryUsagePage = OSDynamicCast(OSNumber, obj);
    if (primaryUsagePage == NULL) {
        goto exit;
    }

    if (primaryUsagePage->unsigned32BitValue() == kHIDPage_Consumer) {
        returnValue = true;
        goto exit;
    }

    if (!clientSet->getCount() || (primaryUsagePage->unsigned32BitValue() != kHIDPage_GenericDesktop)) {
        goto exit;
    }
    

    // We have clients and this device is generic desktop.
    // Probe the client list to make sure that we are
    // openned by an IOHIDEventService.

    OSCollectionIterator *  iterator;
    OSObject *              object;
    

    if ( !isSeized && (iterator = OSCollectionIterator::withCollection(clientSet)) )
    {
        bool done = false;
        while (!done) {
            iterator->reset();
            while (!done && (NULL != (object = iterator->getNextObject()))) {
                if (object->metaCast("IOHIDEventService"))
                {
                    returnValue = true;
                    done = true;
                }
            }
            if (iterator->isValid()) {
                done = true;
            }
        }
        iterator->release();
    }
exit:
    OSSafeReleaseNULL(obj);
    return returnValue;
}

//---------------------------------------------------------------------------
// Handle a client open on the interface.

bool IOHIDDevice::handleOpen(IOService      *client,
                             IOOptionBits   options,
                             void           *argument __unused)
{
    bool  		accept = false;

    HIDDeviceLogDebug("open by %s 0x%llx (0x%x)", client->getName(), client->getRegistryEntryID(), (unsigned int)options);

    do {
        if ( _seizedClient)
            break;

        // Was this object already registered as our client?

        if ( _clientSet->containsObject(client) )
        {
            HIDDeviceLog("multiple opens from client 0x%llx", client->getRegistryEntryID());
            accept = true;
            break;
        }

        // Add the new client object to our client set.

        if ( _clientSet->setObject(client) == false )
            break;

        //  opportunistic workaround for <rdar://problem/59381437> CrashTracer: Regression : hidd at com.apple.iokit.IOHIDLib: 0xd705
        //  @todo remove once correct interface for seize and tranzaction implemented
        if ((options & kIOServiceSeize) && (conformsTo (kHIDPage_Sensor, kHIDUsage_Snsr_Light_AmbientLight) == false))
        {
            messageClients( kIOMessageServiceIsRequestingClose, (void*)(intptr_t)options);

            _seizedClient = client;

#if TARGET_OS_OSX
            IOHIKeyboard * keyboard = OSDynamicCast(IOHIKeyboard, getProvider());
            IOHIPointing * pointing = OSDynamicCast(IOHIPointing, getProvider());
            if ( keyboard )
                keyboard->IOHIKeyboard::message(kIOHIDSystemDeviceSeizeRequestMessage, this, (void *)true);
            else if ( pointing )
                pointing->IOHIPointing::message(kIOHIDSystemDeviceSeizeRequestMessage, this, (void *)true);
#endif
        }
        accept = true;
    }
    while (false);

    _performTickle = ShouldPostDisplayActivityTickles(this, _clientSet, _seizedClient);
    _performWakeTickle = ShouldPostDisplayActivityTicklesForWakeDevice(this, _clientSet, _seizedClient);

    return accept;
}

//---------------------------------------------------------------------------
// Handle a client close on the interface.

void IOHIDDevice::handleClose(IOService * client, IOOptionBits options __unused)
{
    // Remove the object from the client OSSet.
    HIDDeviceLogDebug("close by %s 0x%llx (0x%x)", client->getName(), client->getRegistryEntryID(), (unsigned int)options);
    
    if ( _clientSet->containsObject(client) )
    {
        // Remove the client from our OSSet.
        _clientSet->removeObject(client);

        if (client == _seizedClient)
        {
            _seizedClient = 0;

#if TARGET_OS_OSX
            IOHIKeyboard * keyboard = OSDynamicCast(IOHIKeyboard, getProvider());
            IOHIPointing * pointing = OSDynamicCast(IOHIPointing, getProvider());
            if ( keyboard )
                keyboard->IOHIKeyboard::message(kIOHIDSystemDeviceSeizeRequestMessage, this, (void *)false);
            else if ( pointing )
                pointing->IOHIPointing::message(kIOHIDSystemDeviceSeizeRequestMessage, this, (void *)false);
#endif
        }

        _performTickle = ShouldPostDisplayActivityTickles(this, _clientSet, _seizedClient);
        _performWakeTickle = ShouldPostDisplayActivityTicklesForWakeDevice(this, _clientSet, _seizedClient);
    }
}

//---------------------------------------------------------------------------
// Query whether a client has an open on the interface.

bool IOHIDDevice::handleIsOpen(const IOService * client) const
{
    if (client)
        return _clientSet->containsObject(client);
    else
        return (_clientSet->getCount() > 0);
}


//---------------------------------------------------------------------------
// Create a new user client.

IOReturn IOHIDDevice::newUserClient( task_t          owningTask,
                                     void *          security_id,
                                     UInt32          type,
                                     OSDictionary *  properties,
                                     IOUserClient ** handler )
{
    HIDDeviceLogDebug("new user client");

    // RY: This is really skanky.  Apparently there are some subclasses out there
    // that want all the benefits of IOHIDDevice w/o supporting the default HID
    // User Client.  I know!  Shocking!  Anyway, passing a type known only to the
    // default hid clients to ensure that at least connect to our correct client.
    if ( type == kIOHIDLibUserClientConnectManager ) {
        if ( isInactive() ) {
            HIDDeviceLogInfo("inactive, cannot create user client");
            *handler = NULL;
            return kIOReturnNotReady;
        }

        if ( properties ) {
            properties->setObject( kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue );
        }

      
        return newUserClientInternal (owningTask, security_id, properties, handler );
    }

    return super::newUserClient( owningTask, security_id, type, properties, handler );
}

IOReturn IOHIDDevice::newUserClientInternal( task_t          owningTask,
                                          void *          security_id,
                                          OSDictionary *  properties,
                                          IOUserClient ** handler )
{
    IOUserClient *  client = new IOHIDLibUserClient;
    IOReturn        ret = kIOReturnSuccess;

    do {
        if ( !client->initWithTask( owningTask, security_id, kIOHIDLibUserClientConnectManager, properties ) ) {
            client->release();
            ret = kIOReturnBadArgument;
            break;
        }

        if ( !client->attach( this ) ) {
            client->release();
            ret = kIOReturnUnsupported;
            break;
        }

        if ( !client->start( this ) ) {
            client->detach( this );
            client->release();
            ret = kIOReturnUnsupported;
            break;
        }

        *handler = client;

    } while (false);

    if (ret != kIOReturnSuccess) {
        HIDDeviceLogError("failed to create user client: 0x%x", (unsigned int)ret);
    }

    return ret;
}

//---------------------------------------------------------------------------
// Handle provider messages.

IOReturn IOHIDDevice::message( UInt32 type, IOService * provider, void * argument )
{
    bool providerIsInterface = _interfaceNubs ? (_interfaceNubs->getNextIndexOfObject(provider, 0) != -1) : false;

    HIDDeviceLogDebug("message: 0x%x from: 0x%llx %d", (unsigned int)type, (provider ? provider->getRegistryEntryID() : 0), _performWakeTickle);

    if ((kIOMessageDeviceSignaledWakeup == type) && _performWakeTickle)
    {
        IOHIDSystemActivityTickle(NX_HARDWARE_TICKLE, this); // not a real event. tickle is not maskable.
        return kIOReturnSuccess;
    }
    if (kIOHIDMessageRelayServiceInterfaceActive == type && providerIsInterface) {
        bool msgArg = (argument == kOSBooleanTrue);
        setProperty(kIOHIDRelayServiceInterfaceActiveKey, msgArg ? kOSBooleanTrue : kOSBooleanFalse);
        messageClients(type, (void *)(uintptr_t)msgArg);
        return kIOReturnSuccess;
    }
    return super::message(type, provider, argument);
}

//---------------------------------------------------------------------------
// Handle properties.

static const OSSymbol * msgProps[] = {kIOHIDDeviceMessagePropertyUpdateKeys};

bool IOHIDDevice::setProperty( const OSSymbol * key, OSObject * object)
{
    bool ret;

    // Update the property table before notifying clients of property changes.
    ret = super::setProperty(key, object);
    require(ret, exit);

    for (const OSSymbol * prop : msgProps) {
        if (key->isEqualTo(prop)) {
            messageClients(kIOMessageServicePropertyChange);
        }
    }

exit:
    return ret;
}

//---------------------------------------------------------------------------
// Default implementation of the HID property 'getter' functions.

OSString * IOHIDDevice::newTransportString() const
{
    return 0;
}

OSString * IOHIDDevice::newManufacturerString() const
{
    return 0;
}

OSString * IOHIDDevice::newProductString() const
{
    return 0;
}

OSNumber * IOHIDDevice::newVendorIDNumber() const
{
    return 0;
}

OSNumber * IOHIDDevice::newProductIDNumber() const
{
    return 0;
}

OSNumber * IOHIDDevice::newVersionNumber() const
{
    return 0;
}

OSNumber * IOHIDDevice::newSerialNumber() const
{
    return 0;
}


OSNumber * IOHIDDevice::newPrimaryUsageNumber() const
{
    IOHIDElement *  collection;
    OSNumber *      number = NULL;

    collection = OSDynamicCast(IOHIDElement, _hierarchElements->getObject(0));
    require(collection, exit);

    number =  OSNumber::withNumber(collection->getUsage(), 32);

exit:
    return number;
}

OSNumber * IOHIDDevice::newPrimaryUsagePageNumber() const
{
    IOHIDElement *  collection;
    OSNumber *      number = NULL;

    collection = OSDynamicCast(IOHIDElement, _hierarchElements->getObject(0));
    require(collection, exit);

    number =  OSNumber::withNumber(collection->getUsagePage(), 32);

exit:
    return number;
}

//---------------------------------------------------------------------------
// Handle input reports (USB Interrupt In pipe) from the device.

IOReturn IOHIDDevice::handleReport( IOMemoryDescriptor * report,
                                    IOHIDReportType      reportType,
                                    IOOptionBits         options )
{
    AbsoluteTime   currentTime;
    IOReturn       status;

    clock_get_uptime( &currentTime );
    
    if (!_readyForInputReports)
        return kIOReturnOffline;

    WORKLOOP_LOCK;

	status = handleReportWithTime( currentTime, report, reportType, options );
    
    WORKLOOP_UNLOCK;
    
    return status;
    
}

//---------------------------------------------------------------------------
// Get a report from the device.

IOReturn IOHIDDevice::getReport( IOMemoryDescriptor * report,
                                 IOHIDReportType      reportType,
                                 IOOptionBits         options )
{
    IOReturn kr = kIOReturnSuccess;
    
    kr = getReport(report, reportType, options, 0, 0);
    
    if (gIOHIDFamilyDtraceDebug()) {
        
        IOBufferMemoryDescriptor *bmd = OSDynamicCast(IOBufferMemoryDescriptor, report);
        
        hid_trace(kHIDTraceGetReport, (uintptr_t)getRegistryEntryID(), (uintptr_t)(options & 0xff), (uintptr_t)report->getLength(), bmd ? (uintptr_t)bmd->getBytesNoCopy() : 0, (uintptr_t)mach_absolute_time());
    }
    
    return kr;
}

//---------------------------------------------------------------------------
// Send a report to the device.

IOReturn IOHIDDevice::setReport( IOMemoryDescriptor * report,
                                 IOHIDReportType      reportType,
                                 IOOptionBits         options)
{
    if (gIOHIDFamilyDtraceDebug()) {
        
        IOBufferMemoryDescriptor *bmd = OSDynamicCast(IOBufferMemoryDescriptor, report);
        
        hid_trace(kHIDTraceSetReport, (uintptr_t)getRegistryEntryID(), (uintptr_t)(options & 0xff), (uintptr_t)report->getLength(), bmd ? (uintptr_t)bmd->getBytesNoCopy() : 0, (uintptr_t)mach_absolute_time());
    }
    
    return setReport(report, reportType, options, 0, 0);
}

//---------------------------------------------------------------------------
// Parse a report descriptor, and update the property table with
// the IOHIDElementPrivate hierarchy discovered.

IOReturn IOHIDDevice::parseReportDescriptor( IOMemoryDescriptor * report,
                                             IOOptionBits         options __unused)
{
    void *reportData = NULL;
    IOByteCount reportLength;
    IOReturn ret = kIOReturnError;
    IOHIDElementPrivate *root = NULL;
    
    reportLength = report->getLength();
    require_action(reportLength, exit, ret = kIOReturnBadArgument);
    
    reportData = IOMalloc(reportLength);
    require_action(reportData, exit, ret = kIOReturnNoMemory);
    
    report->readBytes(0, reportData, reportLength);
    
    _elementContainer = IOHIDDeviceElementContainer::withDescriptor(reportData,
                                                                    reportLength,
                                                                    this);
    require(_elementContainer, exit);
    
    _elementArray = _elementContainer->getElements();
    require(_elementArray, exit);
    
    _elementArray->retain();
    
    _maxInputReportSize = _elementContainer->getMaxInputReportSize();
    _maxOutputReportSize = _elementContainer->getMaxOutputReportSize();
    _maxFeatureReportSize = _elementContainer->getMaxFeatureReportSize();
    _dataElementIndex = _elementContainer->getDataElementIndex();
    _reportCount = _elementContainer->getReportCount();
    
    setProperty(kIOHIDMaxInputReportSizeKey, _maxInputReportSize, 32);
    setProperty(kIOHIDMaxOutputReportSizeKey, _maxOutputReportSize, 32);
    setProperty(kIOHIDMaxFeatureReportSizeKey, _maxFeatureReportSize, 32);
    
    root = OSDynamicCast(IOHIDElementPrivate, _elementArray->getObject(0));
    if (root) {
        setProperty(kIOHIDElementKey, root->getChildElements());
    }
    
    setProperty(kIOHIDInputReportElementsKey,
                _elementContainer->getInputReportElements());
    
    ret = kIOReturnSuccess;
    
exit:
    if (reportData && reportLength) {
        IOFree(reportData, reportLength);
    }
    
    if (ret != kIOReturnSuccess) {
        HIDLogError("0x%llx: parse report descriptor failed: 0x%x", getRegistryEntryID(), ret);
    }
    
    return ret;
}

IOReturn IOHIDDevice::createElementHierarchy(HIDPreparsedDataRef parseData __unused)
{
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Fetch the all the possible functions of the device

static OSDictionary * CreateDeviceUsagePairFromElement(IOHIDElementPrivate * element)
{
    OSDictionary *	pair		= 0;
    OSNumber *		usage 		= 0;
    OSNumber *		usagePage 	= 0;
    OSNumber *		type 		= 0;

	pair		= OSDictionary::withCapacity(2);
	usage		= OSNumber::withNumber(element->getUsage(), 32);
	usagePage	= OSNumber::withNumber(element->getUsagePage(), 32);
	type		= OSNumber::withNumber(element->getCollectionType(), 32);

	pair->setObject(kIOHIDDeviceUsageKey, usage);
	pair->setObject(kIOHIDDeviceUsagePageKey, usagePage);
	//pair->setObject(kIOHIDElementCollectionTypeKey, type);

	usage->release();
	usagePage->release();
	type->release();

	return pair;
 }

OSArray * IOHIDDevice::newDeviceUsagePairs()
{
    // starts at one to avoid the virtual collection
    return newDeviceUsagePairs(_elementArray, 1);
}

OSArray * IOHIDDevice::newDeviceUsagePairs(OSArray * elements, UInt32 start)
{
    IOHIDElementPrivate *   element     = NULL;
    OSArray *               functions   = NULL;
    OSDictionary *          pair        = NULL;

    if ( !elements || elements->getCount() <= start )
        return NULL;

    functions = OSArray::withCapacity(2);
    if ( !functions )
        return NULL;

    for (unsigned i=start; i<elements->getCount(); i++)
    {
        element = (IOHIDElementPrivate *)elements->getObject(i);

        if ((element->getType() == kIOHIDElementTypeCollection) &&
            ((element->getCollectionType() == kIOHIDElementCollectionTypeApplication) ||
             (element->getCollectionType() == kIOHIDElementCollectionTypePhysical)))
        {
            pair = CreateDeviceUsagePairFromElement(element);

            UInt32     pairCount = functions->getCount();
            bool     found = false;
            for(unsigned j=0; j<pairCount; j++)
            {
                OSDictionary *tempPair = (OSDictionary *)functions->getObject(j);
                found = tempPair->isEqualTo(pair);
                if (found)
                    break;
            }

            if (!found)
            {
                functions->setObject(functions->getCount(), pair);
            }

            pair->release();
        }
    }

    if ( ! functions->getCount() ) {
        pair = CreateDeviceUsagePairFromElement((IOHIDElementPrivate *)elements->getObject(start));
        functions->setObject(pair);
        pair->release();
    }

    return functions;
}

void IOHIDDevice::setupResolution()
{
    for (unsigned int i = 0; i < _hierarchElements->getCount(); i++) {
        IOHIDElementPrivate *element = NULL;
        SInt32 logicalDiff = 0;
        SInt32 physicalDiff = 0;
        SInt32 exponent = 0;
        IOFixed resolution = 0;
        
        element = (IOHIDElementPrivate *)_hierarchElements->getObject(i);
        
        if (element->getUsagePage() != kHIDPage_GenericDesktop ||
            element->getUsage() != kHIDUsage_GD_X) {
            continue;
        }
        
        if (element->getPhysicalMin() == element->getLogicalMin() ||
            element->getPhysicalMax() == element->getLogicalMax()) {
            continue;
        }
        
        logicalDiff = element->getLogicalMax() - element->getLogicalMin();
        physicalDiff = element->getPhysicalMax() - element->getPhysicalMin();
        exponent = element->getUnitExponent() & 0x0F;
        
        if (exponent < 8) {
            for (unsigned int j = exponent; j > 0; j--) {
                physicalDiff *= 10;
            }
        } else {
            for (unsigned int j = 0x10 - exponent; j > 0; j--) {
                logicalDiff *= 10;
            }
        }
        
        resolution = (logicalDiff / physicalDiff) << 16;
        setProperty(kIOHIDPointerResolutionKey, resolution, 32);
        break;
    }
}

bool IOHIDDevice::getReportCountAndSizes(HIDPreparsedDataRef parseData __unused)
{
    return true;
}

bool IOHIDDevice::setReportSize( UInt8           reportID __unused,
                                 IOHIDReportType reportType __unused,
                                 UInt32          numberOfBits  __unused)
{
    return true;
}

bool
IOHIDDevice::createCollectionElements( HIDPreparsedDataRef parseData __unused,
                                       OSArray             *array __unused,
                                       UInt32              maxCount __unused)
{
    return true;
}

bool IOHIDDevice::linkToParent( const OSArray   *array __unused,
                                UInt32          parentIndex __unused,
                                UInt32          childIndex __unused)
{
    return true;
}

bool IOHIDDevice::createButtonElements( HIDPreparsedDataRef parseData __unused,
                                        OSArray *           array __unused,
                                        UInt32              hidReportType __unused,
                                        IOHIDElementType    elementType __unused,
                                        UInt32              maxCount __unused)
{
    return true;
}

bool IOHIDDevice::createValueElements( HIDPreparsedDataRef parseData __unused,
                                       OSArray *           array __unused,
                                       UInt32              hidReportType __unused,
                                       IOHIDElementType    elementType __unused,
                                       UInt32              maxCount __unused)
{
    return true;
}

bool IOHIDDevice::createReportHandlerElements( HIDPreparsedDataRef parseData __unused)
{
    return true;
}

bool IOHIDDevice::registerElement( IOHIDElementPrivate * element __unused,
                                   IOHIDElementCookie * cookie __unused)
{
    return true;
}

IOBufferMemoryDescriptor * IOHIDDevice::createMemoryForElementValues()
{
    return NULL;
}

IOMemoryDescriptor * IOHIDDevice::getMemoryWithCurrentElementValues() const
{
    return _elementContainer->getElementValuesDescriptor();
}

//---------------------------------------------------------------------------
// Start delivering events from the given element to the specified
// event queue.

IOReturn IOHIDDevice::startEventDelivery( IOHIDEventQueue *  queue,
                                          IOHIDElementCookie cookie,
                                          IOOptionBits       options __unused)
{
    IOHIDElementPrivate * element;
    UInt32         elementIndex = (UInt32) cookie;
    IOReturn       ret = kIOReturnBadArgument;

    if ( ( queue == 0 ) || ( elementIndex < _dataElementIndex ) )
        return kIOReturnBadArgument;

    WORKLOOP_LOCK;

	do {
        if (( element = GetElement(elementIndex) ) == 0)
            break;

        ret = element->addEventQueue( queue ) ?
              kIOReturnSuccess : kIOReturnNoMemory;
    }
    while ( false );

    WORKLOOP_UNLOCK;

    if (ret != kIOReturnSuccess) {
        HIDDeviceLogError("failed to start event delivery: 0x%x", (unsigned int)ret);
    }

    return ret;
}

//---------------------------------------------------------------------------
// Stop delivering events from the given element to the specified
// event queue.

IOReturn IOHIDDevice::stopEventDelivery( IOHIDEventQueue *  queue,
                                         IOHIDElementCookie cookie )
{
    IOHIDElementPrivate * element;
    UInt32         elementIndex = (UInt32) cookie;
    bool           removed      = false;

    // If the cookie provided was zero, then loop and remove the queue
    // from all elements.

    if ( elementIndex == 0 )
        elementIndex = _dataElementIndex;
	else if ( (queue == 0 ) || ( elementIndex < _dataElementIndex ) )
        return kIOReturnBadArgument;

    WORKLOOP_LOCK;

	do {
        if (( element = GetElement(elementIndex++) ) == 0)
            break;

        removed = element->removeEventQueue( queue ) || removed;
    }
    while ( cookie == 0 );

    WORKLOOP_UNLOCK;

    if (!removed) {
        HIDDeviceLogError("failed to stop event delivery");
    }

    return removed ? kIOReturnSuccess : kIOReturnNotFound;
}

//---------------------------------------------------------------------------
// Check whether events from the given element will be delivered to
// the specified event queue.

IOReturn IOHIDDevice::checkEventDelivery( IOHIDEventQueue *  queue,
                                          IOHIDElementCookie cookie,
                                          bool *             started )
{
    IOHIDElementPrivate * element = GetElement( cookie );

    if ( !queue || !element || !started )
        return kIOReturnBadArgument;

    WORKLOOP_LOCK;

    *started = element->hasEventQueue( queue );

    WORKLOOP_UNLOCK;

    return kIOReturnSuccess;
}

#define SetCookiesTransactionState(element, cookies, count, state, index, offset) \
    for (index = offset; index < count; index++) { 			\
        element = GetElement(cookies[index]); 				\
        if (element == NULL) 						\
            continue; 							\
        element->setTransactionState (state);				\
    }

//---------------------------------------------------------------------------
// Update the value of the given element, by getting a report from
// the device.  Assume that the cookieCount > 0

OSMetaClassDefineReservedUsed(IOHIDDevice,  0);
IOReturn IOHIDDevice::updateElementValues(IOHIDElementCookie *cookies, UInt32 cookieCount) {
    IOBufferMemoryDescriptor *	report = NULL;
    IOHIDElementPrivate *	element = NULL;
    IOHIDReportType		    reportType;
    UInt32			        maxReportLength;
    UInt8			        reportID;
    UInt32			        index;
    IOReturn			    ret = kIOReturnError;
    UInt8                   reportMap [UINT8_MAX + 1];

    maxReportLength = max(_maxOutputReportSize, max(_maxFeatureReportSize, _maxInputReportSize));

    // Allocate a mem descriptor with the maxReportLength.
    // This way, we only have to allocate one mem discriptor
    report = IOBufferMemoryDescriptor::withCapacity(maxReportLength, kIODirectionIn);

    if (report == NULL) {
        return kIOReturnNoMemory;
    }
    
    WORKLOOP_LOCK;

    SetCookiesTransactionState(element, cookies, cookieCount, kIOHIDTransactionStatePending, index, 0);
    
    if (cookieCount > 1) {
        memset(reportMap, 0, sizeof(reportMap));
    }
    // Iterate though all the elements in the
    // transaction.  Generate reports if needed.
    for (index = 0; index < cookieCount; index++) {
        
        element = GetElement(cookies[index]);

        if (element == NULL) {
            continue;
        }
        
        if (element->getTransactionState() != kIOHIDTransactionStatePending) {
            continue;
        }

        if (!element->getReportType(&reportType)) {
            continue;
        }
        
        reportID = element->getReportID();

        if (cookieCount > 1) {
            if (reportMap[reportID] & (1 << reportType)) {
                continue;
            }
            reportMap[reportID] |= (1 << reportType);
        }
        
        // We must set report to max size if transaction have
        // elements of different report sizes
        
        report->setLength(maxReportLength);

        // calling down into our subclass, so lets unlock
        WORKLOOP_UNLOCK;
        
        report->prepare();
        ret = getReport(report, reportType, reportID);

        WORKLOOP_LOCK;

        if (ret == kIOReturnSuccess) {
            // If we have a valid report, go ahead and process it.
            ret = handleReport(report, reportType, kIOHIDReportOptionNotInterrupt);
        }

        report->complete();

        if (ret != kIOReturnSuccess) {
            break;
        }
    }

    // release the report
    report->release();

    // If needed, set the transaction state for the
    // remaining elements to idle.
    SetCookiesTransactionState(element, cookies, cookieCount, kIOHIDTransactionStateIdle, index, 0);
    
    WORKLOOP_UNLOCK;

    if (ret != kIOReturnSuccess) {
        HIDDeviceLogError("failed to update element values");
    }

    return ret;
}

//---------------------------------------------------------------------------
// Post the value of the given element, by sending a report to
// the device.  Assume that the cookieCount > 0
OSMetaClassDefineReservedUsed(IOHIDDevice,  1);
IOReturn IOHIDDevice::postElementValues(IOHIDElementCookie * cookies, UInt32 cookieCount)
{
    IOBufferMemoryDescriptor    *report = NULL;
    IOHIDElementPrivate         *cookieElement = NULL;
    UInt8                       *reportData = NULL;
    IOByteCount                 maxReportLength = 0;
    IOHIDReportType             reportType;
    UInt8                       reportID = 0;
    UInt32                      index;
    IOReturn                    ret = kIOReturnError;
    UInt8                       reportMap [UINT8_MAX + 1];


    // Return an error if no cookies are being set
    if (cookieCount == 0) {
        return ret;
    }

    // Get the max report size
    maxReportLength = max(_maxOutputReportSize, max(_maxFeatureReportSize, _maxInputReportSize));

    // Allocate a buffer mem descriptor with the maxReportLength.
    // This way, we only have to allocate one mem buffer.
    report = IOBufferMemoryDescriptor::withCapacity(maxReportLength, kIODirectionOut);

    if (report == NULL) {
        return kIOReturnNoMemory;
    }
    
    if (cookieCount > 1) {
        memset(reportMap, 0, sizeof(reportMap));
    }

    WORKLOOP_LOCK;

    // Set the transaction state on the specified cookies
    SetCookiesTransactionState(cookieElement, cookies, cookieCount, kIOHIDTransactionStatePending, index, 0);

    // Obtain the buffer
    reportData = (UInt8 *)report->getBytesNoCopy();

    // Iterate though all the elements in the
    // transaction.  Generate reports if needed.
    for (index = 0; index < cookieCount; index ++) {

        cookieElement = GetElement(cookies[index]);

        if (cookieElement == NULL) {
            continue;
        }

        // Continue on to the next element if
        // we've already processed this one
        if (cookieElement->getTransactionState() != kIOHIDTransactionStatePending) {
            continue;
        }

        if (!cookieElement->getReportType(&reportType) || (reportType != kIOHIDReportTypeOutput && reportType != kIOHIDReportTypeFeature)) {
            continue;
        }
      
        reportID = cookieElement->getReportID();

        if (cookieCount > 1) {
            if (reportMap[reportID] & (1 << reportType)) {
                continue;
            }
            reportMap[reportID] |= (1 << reportType);
        }
        
        _elementContainer->createReport(reportType, reportID, report);

        // If there are multiple reports, append
        // the reportID to the first byte
        if ( _reportCount > 1 ) {
            reportData[0] = reportID;
        }
        
        WORKLOOP_UNLOCK;
        
        report->prepare();
        ret = setReport( report, reportType, reportID);
        report->complete();
        
        WORKLOOP_LOCK;

        if ( ret != kIOReturnSuccess ) {
            break;
        }
    }

    // If needed, set the transaction state for the
    // remaining elements to idle.
    SetCookiesTransactionState(cookieElement, cookies, cookieCount, kIOHIDTransactionStateIdle, index, 0);

    WORKLOOP_UNLOCK;

    if (report) {
        report->release();
    }

    if (ret != kIOReturnSuccess) {
        HIDDeviceLogError("failed to post element values");
    }

    return ret;
}

OSMetaClassDefineReservedUsed(IOHIDDevice,  2);
OSString * IOHIDDevice::newSerialNumberString() const
{
	OSString * string = 0;
	OSNumber * number = newSerialNumber();

	if ( number )
	{
		char	str[11];
		snprintf(str, sizeof (str), "%d", number->unsigned32BitValue());
		string = OSString::withCString(str);
		number->release();
	}

    return string;
}

OSMetaClassDefineReservedUsed(IOHIDDevice,  3);
OSNumber * IOHIDDevice::newLocationIDNumber() const
{
    return 0;
}


//---------------------------------------------------------------------------
// Get an async report from the device.

OSMetaClassDefineReservedUsed(IOHIDDevice,  4);
IOReturn IOHIDDevice::getReport(IOMemoryDescriptor  *report __unused,
                                IOHIDReportType     reportType __unused,
                                IOOptionBits        options __unused,
                                UInt32              completionTimeout __unused,
                                IOHIDCompletion     *completion __unused)
{
    
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Send an async report to the device.

OSMetaClassDefineReservedUsed(IOHIDDevice,  5);
IOReturn IOHIDDevice::setReport(IOMemoryDescriptor  *report __unused,
                                IOHIDReportType     reportType __unused,
                                IOOptionBits        options __unused,
                                UInt32              completionTimeout __unused,
                                IOHIDCompletion     *completion __unused)
{
    
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Return the vendor id source

OSMetaClassDefineReservedUsed(IOHIDDevice,  6);
OSNumber * IOHIDDevice::newVendorIDSourceNumber() const
{
    return 0;
}

//---------------------------------------------------------------------------
// Return the country code

OSMetaClassDefineReservedUsed(IOHIDDevice,  7);
OSNumber * IOHIDDevice::newCountryCodeNumber() const
{
    return 0;
}

//---------------------------------------------------------------------------
// Handle input reports (USB Interrupt In pipe) from the device.

OSMetaClassDefineReservedUsed(IOHIDDevice,  8);
IOReturn IOHIDDevice::handleReportWithTime(
    AbsoluteTime         timeStamp,
    IOMemoryDescriptor * report,
    IOHIDReportType      reportType,
    IOOptionBits         options)
{
    IOBufferMemoryDescriptor *  bufferDescriptor    = NULL;
    void *                      reportData          = NULL;
    IOByteCount                 reportLength        = 0;
    IOReturn                    ret                 = kIOReturnNotReady;
    bool                        changed             = false;
    bool                        shouldTickle        = false;
    UInt8                       reportID            = 0;

    IOHID_DEBUG(kIOHIDDebugCode_HandleReport, getRegistryEntryID(), __OSAbsoluteTime(timeStamp), reportType, options);

    if ((reportType == kIOHIDReportTypeInput) && !_readyForInputReports)
        return kIOReturnOffline;

    // Get a pointer to the data in the descriptor.
    if ( !report )
        return kIOReturnBadArgument;

    if ( ((unsigned int)reportType) >= kIOHIDReportTypeCount )
        return kIOReturnBadArgument;

    reportLength = report->getLength();
    if ( !reportLength )
        return kIOReturnBadArgument;

    if ( (bufferDescriptor = OSDynamicCast(IOBufferMemoryDescriptor, report)) ) {
        reportData = bufferDescriptor->getBytesNoCopy();
        if ( !reportData )
            return kIOReturnNoMemory;
    } else {
        reportData = IOMalloc(reportLength);
        if ( !reportData )
            return kIOReturnNoMemory;
        report->prepare();
        report->readBytes( 0, reportData, reportLength );
        report->complete();
    }
    
    if (gIOHIDFamilyDtraceDebug()) {
        
        IOBufferMemoryDescriptor *bmd = OSDynamicCast(IOBufferMemoryDescriptor, report);
        
        hid_trace(kHIDTraceHandleReport, (uintptr_t)getRegistryEntryID(), (uintptr_t)(options & 0xff), (uintptr_t)report->getLength(), bmd ? (uintptr_t)bmd->getBytesNoCopy() : 0, (uintptr_t)mach_absolute_time());
    }
    
    
    WORKLOOP_LOCK;

    if ( _readyForInputReports ) {
        // The first byte in the report, may be the report ID.
        // XXX - Do we need to advance the start of the report data?

        reportID = ( _reportCount > 1 ) ? *((UInt8 *) reportData) : 0;
        
        changed = _elementContainer->processReport(reportType,
                                                   reportID,
                                                   reportData,
                                                   (UInt32)reportLength,
                                                   timeStamp,
                                                   &shouldTickle,
                                                   options);
        
        ret = kIOReturnSuccess;
    }

    if (_interfaceNubs && ( reportType == kIOHIDReportTypeInput ) &&
        (( options & kIOHIDReportOptionNotInterrupt ) == 0 ) && !_seizedClient) {
        // In single interface case, the interface handles all reports - including
        // reports with undefined reportID, to maintain compatibility with noncompliant
        // internal devices.
        if (_interfaceNubs->getCount() == 1) {
            IOHIDInterface * interface = (IOHIDInterface *)_interfaceNubs->getObject(0);

            interface->handleReport(timeStamp, report, reportType, reportID, options);
        }
        else {
            // Iterate through attached interfaces. Have an interface handle this report
            // iff the interface's elements contain the relevant reportID.
            for (unsigned int i = 0; i < _interfaceNubs->getCount(); i++) {
                IOHIDInterface *    interface       = (IOHIDInterface *)_interfaceNubs->getObject(i);
                OSArray *           topLevelArray   = (OSArray *)_interfaceElementArrays->getObject(i);

                for (unsigned int j = 0; j < topLevelArray->getCount(); j++) {
                    IOHIDElementPrivate * element = (IOHIDElementPrivate *)topLevelArray->getObject(j);

                    if (reportID == 0 || element->getReportID() == reportID) {
                        interface->handleReport(timeStamp, report, reportType, reportID, options);
                        break;
                    }
                }
            }
        }
    }

    WORKLOOP_UNLOCK;

    if ( !bufferDescriptor && reportData ) {
        // Release the buffer
        IOFree(reportData, reportLength);
    }

    // RY: If this is a non-system HID device, post a null hid
    // event to prevent the system from sleeping.
    if (changed && shouldTickle && _performTickle
            && (CMP_ABSOLUTETIME(&timeStamp, &_eventDeadline) > 0))
    {
        AbsoluteTime ts;

        nanoseconds_to_absolutetime(kIOHIDEventThreshold, &ts);

        _eventDeadline = ts;

        ADD_ABSOLUTETIME(&_eventDeadline, &timeStamp);

        IOHIDSystemActivityTickle(NX_NULLEVENT, this);
    }

    if (ret != kIOReturnSuccess) {
        HIDDeviceLogError("failed to handle report");
    }

    return ret;
}

//---------------------------------------------------------------------------
// Return the polling interval

OSMetaClassDefineReservedUsed(IOHIDDevice, 9);
OSNumber * IOHIDDevice::newReportIntervalNumber() const
{
    UInt32 interval = 8000; // default to 8 milliseconds
    OSObject *obj = copyProperty(kIOHIDReportIntervalKey, gIOServicePlane, kIORegistryIterateRecursively | kIORegistryIterateParents);
    OSNumber *number = OSDynamicCast(OSNumber, obj);
    if ( number )
        interval = number->unsigned32BitValue();
    OSSafeReleaseNULL(obj);

    return OSNumber::withNumber(interval, 32);
}

//---------------------------------------------------------------------------
// Asynchronously handle input reports

OSMetaClassDefineReservedUsed(IOHIDDevice, 10);
IOReturn IOHIDDevice::handleReportWithTimeAsync(
                                      AbsoluteTime         timeStamp,
                                      IOMemoryDescriptor * report,
                                      IOHIDReportType      reportType,
                                      IOOptionBits         options,
                                      UInt32               completionTimeout,
                                      IOHIDCompletion *    completion)
{
    IOReturn result = kIOReturnError;
    
    WORKLOOP_LOCK;

    if (!_asyncReportQueue) {
        _asyncReportQueue = IOHIDAsyncReportQueue::withOwner(this);

        if (_asyncReportQueue) {
            /*status =*/ getWorkLoop()->addEventSource ( _asyncReportQueue );
        }
    }

    if (_asyncReportQueue) {
        result = _asyncReportQueue->postReport(timeStamp, report, reportType, options, completionTimeout, completion);
    }

    WORKLOOP_UNLOCK;

    return result;
}

OSMetaClassDefineReservedUsed(IOHIDDevice, 12);
bool IOHIDDevice::createInterface(IOOptionBits options __unused)
{
    bool result = false;

    HIDDeviceLogDebug("creating interfaces");

    for (unsigned int i = 0; i < _interfaceNubs->getCount(); i++) {
        IOHIDInterface * interface = (IOHIDInterface *)_interfaceNubs->getObject(i);

        result = interface->attach(this);
        require(result, exit);

        result = interface->start(this);
        require_action(result, exit, interface->detach(this));
    }
    
    result = true;
    
exit:
    if (!result) {
        HIDDeviceLogError("Failed to create attach/start nub.\n");
    }

    return result;
}

OSMetaClassDefineReservedUsed(IOHIDDevice, 13);
void IOHIDDevice::destroyInterface(IOOptionBits options __unused)
{
    HIDDeviceLogDebug("destroying interfaces");

    for (unsigned int i = 0; i < _interfaceNubs->getCount(); i++) {
        IOHIDInterface * interface = (OSDynamicCast(IOHIDInterface, _interfaceNubs->getObject(i)));

        //@todo interface needs to be removed
        if (interface) {
            interface->terminate();
        }
    }
}

OSMetaClassDefineReservedUsed(IOHIDDevice, 14);
bool IOHIDDevice::conformsTo(UInt32 usagePage, UInt32 usage)
{
    bool result = false;
    OSArray *usagePairs = NULL;
    
    usagePairs = newDeviceUsagePairs();
    require(usagePairs && usagePairs->getCount(), exit);
    
    for (unsigned int i = 0; i < usagePairs->getCount(); i++) {
        OSDictionary *pairs = NULL;
        OSNumber *up = NULL, *u = NULL;
        UInt32 usagePageNum = 0, usageNum = 0;
        
        pairs = OSDynamicCast(OSDictionary, usagePairs->getObject(i));
        
        if (!pairs) {
            continue;
        }
        
        up = OSDynamicCast(OSNumber, pairs->getObject(kIOHIDDeviceUsagePageKey));
        if (!up) {
            continue;
        }
        
        usagePageNum = up->unsigned32BitValue();
        
        u = OSDynamicCast(OSNumber, pairs->getObject(kIOHIDDeviceUsageKey));
        if (!u) {
            continue;
        }
        
        usageNum = u->unsigned32BitValue();
        
        if (usagePage == usagePageNum && usage == usageNum) {
            result = true;
            break;
        }
    }
    
exit:
    OSSafeReleaseNULL(usagePairs);
    
    return result;
}

OSMetaClassDefineReservedUsed(IOHIDDevice, 15);
void IOHIDDevice::completeReport(OSAction * action __unused,
                                 IOReturn status __unused,
                                 uint32_t actualByteCount __unused)
{
    
}

OSMetaClassDefineReservedUnused(IOHIDDevice, 16);
OSMetaClassDefineReservedUnused(IOHIDDevice, 17);
OSMetaClassDefineReservedUnused(IOHIDDevice, 18);
OSMetaClassDefineReservedUnused(IOHIDDevice, 19);
OSMetaClassDefineReservedUnused(IOHIDDevice, 20);
OSMetaClassDefineReservedUnused(IOHIDDevice, 21);
OSMetaClassDefineReservedUnused(IOHIDDevice, 22);
OSMetaClassDefineReservedUnused(IOHIDDevice, 23);
OSMetaClassDefineReservedUnused(IOHIDDevice, 24);
OSMetaClassDefineReservedUnused(IOHIDDevice, 25);
OSMetaClassDefineReservedUnused(IOHIDDevice, 26);
OSMetaClassDefineReservedUnused(IOHIDDevice, 27);
OSMetaClassDefineReservedUnused(IOHIDDevice, 28);
OSMetaClassDefineReservedUnused(IOHIDDevice, 29);
OSMetaClassDefineReservedUnused(IOHIDDevice, 30);
OSMetaClassDefineReservedUnused(IOHIDDevice, 31);
OSMetaClassDefineReservedUnused(IOHIDDevice, 32);
OSMetaClassDefineReservedUnused(IOHIDDevice, 33);
OSMetaClassDefineReservedUnused(IOHIDDevice, 34);
OSMetaClassDefineReservedUnused(IOHIDDevice, 35);
OSMetaClassDefineReservedUnused(IOHIDDevice, 36);
OSMetaClassDefineReservedUnused(IOHIDDevice, 37);
OSMetaClassDefineReservedUnused(IOHIDDevice, 38);
OSMetaClassDefineReservedUnused(IOHIDDevice, 39);
OSMetaClassDefineReservedUnused(IOHIDDevice, 40);

#pragma clang diagnostic ignored "-Wunused-parameter"

// DriverKit dispatch: needs the IIG-generated argument types, which PureDarwin does not build
#if PD_HID_DRIVERKIT
#include <IOKit/IOUserServer.h>

kern_return_t
IMPL(IOHIDDevice, _Start)
{
    setProperty(kIOHIDDKStartKey, kOSBooleanTrue);
    
    bool ret = start(provider);
    
    return ret ? kIOReturnSuccess : kIOReturnError;
}


void
IMPL(IOHIDDevice, _SetProperty)
{
    
}


void
IMPL(IOHIDDevice, _ProcessReport)
{

}

kern_return_t
IMPL(IOHIDDevice, _HandleReport)
{
    IOBufferMemoryDescriptor *bmd = OSDynamicCast(IOBufferMemoryDescriptor, report);
    
    if (bmd) {
        bmd->setLength(reportLength);
    }
    
    this->handleReportWithTime (timestamp, report, reportType, options);
    
    if (bmd) {
        bmd->setLength(bmd->getCapacity());
    }
    
    return kIOReturnSuccess;
}

void
IMPL(IOHIDDevice, _CompleteReport)
{
    this->completeReport(action, status, actualByteCount);
}


#endif /* PD_HID_DRIVERKIT */
