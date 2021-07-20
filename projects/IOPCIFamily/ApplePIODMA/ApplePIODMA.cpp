//
//  ApplePIODMA.c
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/25/20.
//

#include <IOKit/apiodma/ApplePIODMADebug.h>
#include <IOKit/apiodma/ApplePIODMA.h>
#include <IOKit/apiodma/ApplePIODMARequest.h>

#define super IOService
OSDefineMetaClassAndAbstractStructors(ApplePIODMA, super)

#define debug(mask, fmt, args...) pioDMADebugServiceWithClass(mask, ApplePIODMA, fmt,##args)

bool ApplePIODMA::start(IOService* provider)
{
    _debugLoggingMask = applePIODMAgetDebugLoggingMaskForMetaClass(getMetaClass(), super::metaClass);

    OSData* piodmaIDData = OSDynamicCast(OSData, getProperty(kApplePIODMAID, gIOServicePlane));

    if(piodmaIDData == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "unable to get APIODMA ID\n");
        return false;
    }

    setProperty(kApplePIODMAID, piodmaIDData);
    uint32_t piodmaID           = *reinterpret_cast<const uint32_t*>(piodmaIDData->getBytesNoCopy());
    char     locationString[32] = { 0 };

    snprintf(locationString, sizeof(locationString), "%x", piodmaID);
    setLocation(locationString);

    if(super::start(provider) != true)
    {
        debug(kApplePIODMADebugLoggingAlways, "failed start\n");
        return false;
    }

    _workLoop = IOWorkLoop::workLoop();
    if(_workLoop == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't create workLoop\n");
        stop(provider);
        return false;
    }

    _commandGate = IOCommandGate::commandGate(this);
    if(_commandGate == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't create commandGate\n");
        stop(provider);
        return false;
    }

    if(_workLoop->addEventSource(_commandGate) != kIOReturnSuccess)
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't add event source to workLoop\n");
        stop(provider);
        return false;
    }

    _pioDeviceMemory = provider->getDeviceMemoryWithIndex(kApplePIODMAMemoryIndexPIODMARegisters);
    if(_pioDeviceMemory == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "Couldn't get device memory\n");
        stop(provider);
        return false;
    }
    _pioDeviceMemory->retain();

    _pioDeviceMemoryMap = _pioDeviceMemory->map();
    if(_pioDeviceMemoryMap == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't map memory\n");
        stop(provider);
        return false;
    }

    _memoryMapper = IOMapper::copyMapperForDevice(provider);
    if(_memoryMapper == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't copy mapper\n");
        stop(provider);
        return false;
    }

    _requestPool = allocateRequestPool();
    if(_requestPool == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't create request pool\n");
        stop(provider);
        return false;
    }
    _completionQueueSize = _requestPool->maximumCommandsSupported();

    _completionQueue = reinterpret_cast<ApplePIODMARequest**>(IOMallocZero(_completionQueueSize * sizeof(ApplePIODMARequest*)));
    if(_completionQueue == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't create completionqueue\n");
        stop(provider);
        return false;
    }

    _interruptEventSource = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventSource::Action, this, &ApplePIODMA::interruptOccurred), provider);
    if(_interruptEventSource == NULL)
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't create interrupt event source\n");
        stop(provider);
        return false;
    }

    _workLoop->addEventSource(_interruptEventSource);

    _stateLock = IORWLockAlloc();
    if(_stateLock == NULL)
    {
        return false;
    }

    return true;
}

void ApplePIODMA::stop(IOService* provider)
{
    disable();

    if(_interruptEventSource != NULL)
    {
        _interruptEventSource->disable();
        if(_workLoop != NULL)
        {
            _workLoop->removeEventSource(_interruptEventSource);
        }
        OSSafeReleaseNULL(_interruptEventSource);
    }

    if(_commandGate != NULL)
    {
        _workLoop->removeEventSource(_commandGate);
    }
    OSSafeReleaseNULL(_commandGate);
    OSSafeReleaseNULL(_workLoop);
    OSSafeReleaseNULL(_pioDeviceMemoryMap);
    OSSafeReleaseNULL(_pioDeviceMemory);
    OSSafeReleaseNULL(_memoryMapper);
    OSSafeReleaseNULL(_requestPool);


    if(_completionQueue != NULL)
    {
        IOFree(_completionQueue, _completionQueueSize * sizeof(ApplePIODMARequest*));
        _completionQueue = NULL;
    }

    if(_stateLock != NULL)
    {
        IORWLockFree(_stateLock);
        _stateLock = NULL;
    }

    super::stop(provider);
}

#pragma mark DMA access
IOReturn ApplePIODMA::hostToDevice(IOMemoryDescriptor* sourceBase,
                                   IOByteCount         sourceOffset,
                                   IOMemoryDescriptor* destinationBase,
                                   IOByteCount         destinationOffset,
                                   IOByteCount         size)
{
    return kIOReturnUnsupported;
}

IOReturn ApplePIODMA::hostToDevice(const void*         source,
                                   IOMemoryDescriptor* destinationBase,
                                   IOByteCount         destinationOffset,
                                   IOByteCount         size)
{
    return kIOReturnUnsupported;
}

IOReturn ApplePIODMA::deviceToHost(IOMemoryDescriptor* sourceBase,
                                   IOByteCount         sourceOffset,
                                   IOMemoryDescriptor* destinationBase,
                                   IOByteCount         destinationOffset,
                                   IOByteCount         size)
{
    return kIOReturnUnsupported;
}

IOReturn ApplePIODMA::deviceToHost(IOMemoryDescriptor* sourceBase,
                                   IOByteCount         sourceOffset,
                                   void*               destination,
                                   IOByteCount         size)
{
    return kIOReturnUnsupported;
}

IOReturn ApplePIODMA::deviceToDevice(IOMemoryDescriptor* sourceBase,
                                     IOByteCount         sourceOffset,
                                     IOMemoryDescriptor* destinationBase,
                                     IOByteCount         destinationOffset,
                                     IOByteCount         size)
{
    return kIOReturnUnsupported;
}

IOReturn ApplePIODMA::hostToHost(IOMemoryDescriptor* sourceBase,
                                 IOByteCount         sourceOffset,
                                 IOMemoryDescriptor* destinationBase,
                                 IOByteCount         destinationOffset,
                                 IOByteCount         size)
{
    return kIOReturnUnsupported;
}

IOReturn ApplePIODMA::hostToHost(const void* source,
                                 void*       destination,
                                 IOByteCount size)
{
    return kIOReturnUnsupported;
}

IOReturn ApplePIODMA::enable()
{
    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &ApplePIODMA::enableGated));
}

IOReturn ApplePIODMA::enableGated()
{
    _interruptEventSource->enable();

    IORWLockWrite(_stateLock);
    _state = kApplePIODMAStateEnabled;
    IORWLockUnlock(_stateLock);
    debug(kApplePIODMADebugLoggingInit, "\n");

    return kIOReturnSuccess;
}

void ApplePIODMA::disable()
{
    _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &ApplePIODMA::disableGated));
}

IOReturn ApplePIODMA::disableGated()
{
    IORWLockWrite(_stateLock);
    _state = kApplePIODMAStateDisabled;
    IORWLockUnlock(_stateLock);
    debug(kApplePIODMADebugLoggingInit, "\n");

    _interruptEventSource->disable();
    return kIOReturnSuccess;
}

void ApplePIODMA::executeRequest(ApplePIODMARequest* request)
{
    _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &ApplePIODMA::executeRequestGated),
                            reinterpret_cast<void*>(request));
}

IOReturn ApplePIODMA::executeRequestGated(ApplePIODMARequest* request)
{
    uint8_t commandTag = request->commandTag();
    debug(kApplePIODMADebugLoggingVerbose, "enqueuing command Tag 0x%x at index %u\n", commandTag, _completionTail);

    _completionQueue[_completionTail] = request;

    _completionTail = (_completionTail + 1) % _completionQueueSize;

    IOReturn result = _commandGate->commandSleep(request, THREAD_UNINT);
    if(result != THREAD_AWAKENED)
    {
        debug(kApplePIODMADebugLoggingAlways,
              "request tag 0x%x thread did not wake normally, result = 0x%x\n",
              commandTag,
              result);
    }

    return kIOReturnSuccess;
}
