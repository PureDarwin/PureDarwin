//
//  ApplePIODMARequestPool.cpp
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/26/20.
//
#include <IOKit/IOReturn.h>
#include <IOKit/apiodma/ApplePIODMADebug.h>
#include <IOKit/apiodma/ApplePIODMARequest.h>
#include <IOKit/apiodma/ApplePIODMARequestPool.h>


#define super IOCommandPool
OSDefineMetaClassAndStructors(ApplePIODMARequestPool, IOCommandPool)

#define debug(mask, fmt, args...) pioDMADebugObjectWithClass(mask, ApplePIODMARequestPool, fmt,##args)

ApplePIODMARequestPool * ApplePIODMARequestPool::withWorkLoop(IOWorkLoop * workLoop,
                                                              IOMapper *   mapper,
                                                              uint32_t maxOutstandingCommands,
                                                              uint32_t byteAlignment,
                                                              uint8_t numberOfAddressBits,
                                                              uint64_t maxTransferSize,
                                                              uint64_t maxSegmentSize)
{
    ApplePIODMARequestPool* result = new ApplePIODMARequestPool;
    if(   result != NULL
       && result->initWithWorkLoop(workLoop,
                                   mapper,
                                   maxOutstandingCommands,
                                   byteAlignment,
                                   numberOfAddressBits,
                                   maxTransferSize,
                                   maxSegmentSize) == false)
    {
        OSSafeReleaseNULL(result);
    }

    return result;
}

#pragma mark IOCommandPool overrides
bool ApplePIODMARequestPool::initWithWorkLoop(IOWorkLoop* workLoop,
                                              IOMapper*   mapper,
                                              uint32_t    maxOutstandingCommands,
                                              uint32_t    byteAlignment,
                                              uint8_t     numberOfAddressBits,
                                              uint64_t    maxTransferSize,
                                              uint64_t    maxSegmentSize)
{
    if(super::initWithWorkLoop(workLoop) == false)
    {
        return false;
    }

    _debugLoggingMask = applePIODMAgetDebugLoggingMaskForMetaClass(getMetaClass(), super::metaClass);

    _maxOutstandingCommands = maxOutstandingCommands;
    _byteAlignment          = byteAlignment;
    _numberOfAddressBits    = numberOfAddressBits;
    _maxTransferSize        = maxTransferSize;
    _maxSegmentSize         = maxSegmentSize;
    _maxOutstandingCommands = maxOutstandingCommands;

    _workLoop = workLoop;
    _workLoop->retain();

    if(mapper != NULL)
    {
        _memoryMapper = mapper;
        _memoryMapper->retain();
    }

    // allocate the commands during initialization
    for(unsigned int i = 0; i < maximumCommandsSupported(); i++)
    {
        IOCommand* command = allocateCommand();
        returnCommand(command);
    }


    return true;
}

void ApplePIODMARequestPool::free()
{
    OSSafeReleaseNULL(_workLoop);
    OSSafeReleaseNULL(_memoryMapper);
    super::free();
}

#pragma mark Pool Management
IOCommand* ApplePIODMARequestPool::allocateCommand()
{
    return ApplePIODMARequest::withMapper(_memoryMapper,
                                          _byteAlignment,
                                          _numberOfAddressBits,
                                          _maxTransferSize,
                                          _maxSegmentSize);
}

unsigned int ApplePIODMARequestPool::maximumCommandsSupported()
{
    return _maxOutstandingCommands;
}
