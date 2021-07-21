//
//  ApplePIODMARequest.cpp
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/26/20.
//
#include <IOKit/IOMapper.h>
#include <IOKit/apiodma/ApplePIODMADebug.h>
#include <IOKit/apiodma/ApplePIODMARequest.h>

#define super IOCommand
OSDefineMetaClassAndStructors(ApplePIODMARequest, super)
#define debug(mask, fmt, args...) pioDMADebugObjectWithClass(mask, ApplePIODMARequest, fmt,##args)

ApplePIODMARequest * ApplePIODMARequest::withMapper(IOMapper * mapper,
                                                    uint32_t byteAlignment,
                                                    uint8_t numberOfAddressBits,
                                                    uint64_t maxTransferSize,
                                                    uint64_t maxSegmentSize)
{
    ApplePIODMARequest* result = new ApplePIODMARequest;
    if(   result != NULL
       && result->initWithMapper(mapper, byteAlignment, numberOfAddressBits, maxTransferSize, maxSegmentSize) == false)
    {
        OSSafeReleaseNULL(result);
    }

    return result;
}


bool ApplePIODMARequest::initWithMapper(IOMapper* mapper,
                                        uint32_t  byteAlignment,
                                        uint8_t   numberOfAddressBits,
                                        uint64_t  maxTransferSize,
                                        uint64_t  maxSegmentSize)
{
    _debugLoggingMask = applePIODMAgetDebugLoggingMaskForMetaClass(getMetaClass(), super::metaClass);

    if(super::init() == false)
    {
        debug(kApplePIODMADebugLoggingAlways, "failed super::init\n");
        return false;
    }

    _commandSourceDMACommand = IODMACommand::withSpecification(kIODMACommandOutputHost64,
                                                               numberOfAddressBits,
                                                               maxSegmentSize,
                                                               IODMACommand::kMapped,
                                                               maxTransferSize,
                                                               byteAlignment,
                                                               mapper);

    _commandDestinationDMACommand = IODMACommand::withSpecification(kIODMACommandOutputHost64,
                                                                    numberOfAddressBits,
                                                                    maxSegmentSize,
                                                                    IODMACommand::kMapped,
                                                                    maxTransferSize,
                                                                    byteAlignment,
                                                                    mapper);

    _bufferBaseDMACommand = IODMACommand::withSpecification(kIODMACommandOutputHost64,
                                                            numberOfAddressBits,
                                                            maxSegmentSize,
                                                            IODMACommand::kMapped,
                                                            maxTransferSize,
                                                            byteAlignment,
                                                            mapper);

    _targetBaseDMACommand = IODMACommand::withSpecification(kIODMACommandOutputHost64,
                                                            numberOfAddressBits,
                                                            maxSegmentSize,
                                                            IODMACommand::kMapped,
                                                            maxTransferSize,
                                                            byteAlignment,
                                                            mapper);

    // TODO: allow linked lists (command size should be generic packet + linked list header size)
    _commandSourceBuffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
                                                                       kIOMemoryPhysicallyContiguous | kIODirectionOut,
                                                                       sizeof(ApplePIODMAGenericPacket),
                                                                       sizeof(uint32_t),
                                                                       0, 0);

    _commandDestinationBuffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
                                                                            kIOMemoryPhysicallyContiguous | kIODirectionIn,
                                                                            sizeof(ApplePIODMAGenericPacket),
                                                                            sizeof(uint32_t),
                                                                            0, 0);

    _scalarBufferDescriptor = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
                                                                          kIOMemoryPhysicallyContiguous | kIODirectionInOut,
                                                                          sizeof(uint64_t),
                                                                          sizeof(uint32_t),
                                                                          0, 0);

    _scalarTargetDescriptor = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
                                                                          kIOMemoryPhysicallyContiguous | kIODirectionInOut,
                                                                          sizeof(uint64_t),
                                                                          sizeof(uint32_t),
                                                                          0, 0);


    if(   (_commandSourceDMACommand == NULL)
       || (_commandDestinationDMACommand == NULL)
       || (_bufferBaseDMACommand == NULL)
       || (_targetBaseDMACommand == NULL)
       || (_commandSourceBuffer == NULL)
       || (_commandDestinationBuffer == NULL)
       || (_scalarBufferDescriptor == NULL)
       || (_scalarTargetDescriptor == NULL))
    {
        debug(kApplePIODMADebugLoggingAlways, "couldn't allocate DMA commands\n");
        return false;
    }

    bzero(_commandDestinationBuffer->getBytesNoCopy(), sizeof(ApplePIODMAGenericPacket));
    bzero(_commandSourceBuffer->getBytesNoCopy(), sizeof(ApplePIODMAGenericPacket));

    return true;
}

void ApplePIODMARequest::free()
{
    OSSafeReleaseNULL(_commandSourceDMACommand);
    OSSafeReleaseNULL(_commandDestinationDMACommand);
    OSSafeReleaseNULL(_commandSourceBuffer);
    OSSafeReleaseNULL(_commandDestinationBuffer);
    super::free();
}

tApplePIODMAGenericPacketDeviceType ApplePIODMARequest::deviceType(tApplePIODMARequestBufferType sourceType)
{
    tApplePIODMAGenericPacketDeviceType result = kApplePIODMAGenericPacketDeviceTypeMemory;
    switch(sourceType)
    {
        case kApplePIODMARequestBufferTypeHostMemory:
        {
            result = kApplePIODMAGenericPacketDeviceTypeMemory;
            break;
        }
        case kApplePIODMARequestBufferTypeDeviceMMIOSpace:
        {
            result = kApplePIODMAGenericPacketDeviceTypePIO;
            break;
        }
        case kApplePIODMARequestBufferTypeDeviceConfigurationSpace:
        {
            result = kApplePIODMAGenericPacketDeviceTypePIO;
            break;
        }
        case kApplePIODMARequestBufferTypeDevicePIOSpace:
        {
            result = kApplePIODMAGenericPacketDeviceTypePIO;
            break;
        }
        default:
        {
            break;
        }
    }

    return result;
}

// TODO: support BAR selection and greater than 36bit addresses
// TODO: support multiple packets in 1 command (linked list / multi-segments)
IOReturn ApplePIODMARequest::prepareGenericPacket(IOMemoryDescriptor*                 bufferBase,
                                                  IOByteCount                         bufferOffset,
                                                  IOMemoryDescriptor*                 targetBase,
                                                  IOByteCount                         targetOffset,
                                                  tApplePIODMAGenericPacketDeviceType bufferType,
                                                  tApplePIODMAGenericPacketDeviceType targetType,
                                                  IOByteCount                         transferSize,
                                                  uint8_t                             commandType,
                                                  uint8_t                             bufferBaseAddressSelect,
                                                  uint8_t                             targetBaseAddressSelect)
{
    IOReturn result = kIOReturnSuccess;

    IOPhysicalAddress bufferPhysicalAddress = bufferOffset;
    IOPhysicalAddress targetPhysicalAddress = targetOffset;

    if(bufferBase != NULL)
    {
        _bufferBase = bufferBase;
        _bufferBase->retain();
    }

    if(targetBase != NULL)
    {
        _targetBase = targetBase;
        _targetBase->retain();
    }

    if(   (result == kIOReturnSuccess)
       && (_bufferBase != NULL))
    {
        result = _bufferBaseDMACommand->setMemoryDescriptor(_bufferBase);
        IODMACommand::Segment64 segment;

        if(result == kIOReturnSuccess)
        {
            bzero(&segment, sizeof(IODMACommand::Segment64));
            uint32_t numSegments = 1;
            result = _bufferBaseDMACommand->genIOVMSegments(&bufferOffset, &segment, &numSegments);
        }

        if(result == kIOReturnSuccess)
        {
            bufferPhysicalAddress = segment.fIOVMAddr;
        }
        else
        {
            debug(kApplePIODMADebugLoggingAlways, "couldn't generate physical address for buffer, result = 0x%x\n", result);
        }
    }

    if(   (result == kIOReturnSuccess)
       && (_targetBase != NULL))
    {
        result = _targetBaseDMACommand->setMemoryDescriptor(_targetBase);
        IODMACommand::Segment64 segment;

        if(result == kIOReturnSuccess)
        {
            bzero(&segment, sizeof(IODMACommand::Segment64));
            uint32_t numSegments = 1;
            result = _targetBaseDMACommand->genIOVMSegments(&targetOffset, &segment, &numSegments);
        }

        if(result == kIOReturnSuccess)
        {
            targetPhysicalAddress = segment.fIOVMAddr;
        }
        else
        {
            debug(kApplePIODMADebugLoggingAlways, "couldn't generate physical address for target, result = 0x%x\n", result);
        }
    }

    // TODO: allow linked lists
    _commandSize  = sizeof(ApplePIODMAGenericPacket) / sizeof(uint32_t);
    _commandType  = commandType;
    _transferSize = transferSize;
    if(result == kIOReturnSuccess)
    {
        ApplePIODMAGenericPacket* genericPacket = reinterpret_cast<ApplePIODMAGenericPacket*>(_commandSourceBuffer->getBytesNoCopy());
        genericPacket->word0 = ((bufferPhysicalAddress << kApplePIODMAGenericPacketWord0BufferAddrOffsetPhase) & kApplePIODMAGenericPacketWord0BufferAddrOffset)
                               | ((targetBaseAddressSelect << kApplePIODMAGenericPacketWord0TargetBARSelPhase) & kApplePIODMAGenericPacketWord0TargetBARSel)
                               | ((bufferBaseAddressSelect << kApplePIODMAGenericPacketWord0BufferBarSelPhase) & kApplePIODMAGenericPacketWord0BufferBarSel)
                               | ((bufferType << kApplePIODMAGenericPacketWord0TargetDeviceTypePhase) & kApplePIODMAGenericPacketWord0TargetDeviceType)
                               | ((targetType << kApplePIODMAGenericPacketWord0BufferDeviceTypePhase) & kApplePIODMAGenericPacketWord0BufferDeviceType)
                               | ((kApplePIODMAGenericPacketWord0TargetBurstTypeIncrementingValue << kApplePIODMAGenericPacketWord0TargetBurstTypePhase) & kApplePIODMAGenericPacketWord0TargetBurstType)
                               | ((kApplePIODMAGenericPacketWord0TargetBurstTypeIncrementingValue << kApplePIODMAGenericPacketWord0BufferBurstTypePhase) & kApplePIODMAGenericPacketWord0BufferBurstType)
                               | ((kApplePIODMAGenericPacketWord0ExtendedTypeValue << kApplePIODMAGenericPacketWord0ExtendedTypePhase) & kApplePIODMAGenericPacketWord0ExtendedType)
                               | ((kApplePIODMAGenericPacketWord0TypeValue << kApplePIODMAGenericPacketWord0TypePhase) & kApplePIODMAGenericPacketWord0Type);


        genericPacket->word1 = ((targetPhysicalAddress << kApplePIODMAGenericPacketWord1TargetAddrOffsetPhase) & kApplePIODMAGenericPacketWord1TargetAddrOffset)
                               | (((bufferPhysicalAddress >> kApplePIODMAGenericPacketWord0BufferAddrOffsetWidth) << kApplePIODMAGenericPacketWord1BufferAddrOffsetPhase) & kApplePIODMAGenericPacketWord1BufferAddrOffset);
        genericPacket->word2 = (((targetPhysicalAddress  >> kApplePIODMAGenericPacketWord1TargetAddrOffsetWidth) << kApplePIODMAGenericPacketWord2TargetAddrOffsetPhase) & kApplePIODMAGenericPacketWord2TargetAddrOffset);
        genericPacket->word3 = ((_transferSize << kApplePIODMAGenericPacketWord3TransferSizePhase) & kApplePIODMAGenericPacketWord3TransferSize);

        result = _commandSourceDMACommand->setMemoryDescriptor(_commandSourceBuffer);

        if(result != kIOReturnSuccess)
        {
            debug(kApplePIODMADebugLoggingAlways, "couldn't set memory descriptor for command source buffer, result = 0x%x\n", result);
        }

        debug(kApplePIODMADebugLoggingIO, "generic packet word0 = 0x%x, word1 = 0x%x, word2 = 0x%x, word3 = 0x%x, buffer = 0x%llx, target = 0x%llx\n",
              genericPacket->word0,
              genericPacket->word1,
              genericPacket->word2,
              genericPacket->word3,
              bufferPhysicalAddress,
              targetPhysicalAddress);
    }

    if(result == kIOReturnSuccess)
    {
        result = _commandDestinationDMACommand->setMemoryDescriptor(_commandDestinationBuffer);

        if(result != kIOReturnSuccess)
        {
            debug(kApplePIODMADebugLoggingAlways, "couldn't set memory descriptor for command destination buffer, result = 0x%x\n", result);
        }
    }

    if(result != kIOReturnSuccess)
    {
        complete();
    }

    return result;
}

IOReturn ApplePIODMARequest::complete()
{
    IOReturn result = kIOReturnSuccess;

    // clear out command to be re-used in pool
    _bufferBaseDMACommand->clearMemoryDescriptor();
    _targetBaseDMACommand->clearMemoryDescriptor();
    OSSafeReleaseNULL(_bufferBase);
    OSSafeReleaseNULL(_targetBase);
    _commandSourceDMACommand->clearMemoryDescriptor();
    _commandDestinationDMACommand->clearMemoryDescriptor();
    bzero(_commandSourceBuffer->getBytesNoCopy(), sizeof(ApplePIODMAGenericPacket));
    bzero(_commandDestinationBuffer->getBytesNoCopy(), sizeof(ApplePIODMAGenericPacket));

    if(_scalarBuffer != NULL)
    {
        bcopy(_scalarBufferDescriptor->getBytesNoCopy(), _scalarBuffer, _transferSize);
        _scalarBuffer = NULL;
    }

    if(_scalarTarget != NULL)
    {
        bcopy(_scalarTargetDescriptor->getBytesNoCopy(), _scalarTarget, _transferSize);
        _scalarTarget = NULL;
    }

    _errorAddress = 0;
    _errorStatus  = 0;
    _commandTag   = 0;
    _commandSize  = 0;
    _commandType  = 0;
    _transferSize = 0;
    return result;
}

IOReturn ApplePIODMARequest::prepareGenericPacket(void*                               buffer,
                                                  IOMemoryDescriptor*                 targetBase,
                                                  IOByteCount                         targetOffset,
                                                  tApplePIODMAGenericPacketDeviceType bufferType,
                                                  tApplePIODMAGenericPacketDeviceType targetType,
                                                  IOByteCount                         transferSize,
                                                  uint8_t                             commandType,
                                                  uint8_t                             bufferBaseAddressSelect,
                                                  uint8_t                             targetBaseAddressSelect)
{
    if(   (transferSize > _scalarBufferDescriptor->getLength())
       || (buffer == NULL))
    {
        debug(kApplePIODMADebugLoggingAlways, "rejecting scalar request of size %llu\n", transferSize);
        return kIOReturnBadArgument;
    }

    _scalarBuffer = buffer;
    bcopy(buffer, _scalarBufferDescriptor->getBytesNoCopy(), transferSize);

    return prepareGenericPacket(_scalarBufferDescriptor,
                                0,
                                targetBase,
                                targetOffset,
                                bufferType,
                                targetType,
                                transferSize,
                                commandType);
}

IOReturn ApplePIODMARequest::prepareGenericPacket(IOMemoryDescriptor*                 bufferBase,
                                                  IOByteCount                         bufferOffset,
                                                  void*                               target,
                                                  tApplePIODMAGenericPacketDeviceType bufferType,
                                                  tApplePIODMAGenericPacketDeviceType targetType,
                                                  IOByteCount                         transferSize,
                                                  uint8_t                             commandType,
                                                  uint8_t                             bufferBaseAddressSelect,
                                                  uint8_t                             targetBaseAddressSelect)
{
    if(   (transferSize > _scalarTargetDescriptor->getLength())
       || (target == NULL))
    {
        debug(kApplePIODMADebugLoggingAlways, "rejecting scalar request of size %llu\n", transferSize);
        return kIOReturnBadArgument;
    }

    _scalarTarget = target;
    bcopy(target, _scalarTargetDescriptor->getBytesNoCopy(), transferSize);

    return prepareGenericPacket(bufferBase,
                                bufferOffset,
                                _scalarTargetDescriptor,
                                0,
                                bufferType,
                                targetType,
                                transferSize,
                                commandType);
}

IOReturn ApplePIODMARequest::prepareGenericPacket(void*                               buffer,
                                                  void*                               target,
                                                  tApplePIODMAGenericPacketDeviceType bufferType,
                                                  tApplePIODMAGenericPacketDeviceType targetType,
                                                  IOByteCount                         transferSize,
                                                  uint8_t                             commandType,
                                                  uint8_t                             bufferBaseAddressSelect,
                                                  uint8_t                             targetBaseAddressSelect)
{
    if(   (transferSize > _scalarBufferDescriptor->getLength())
       || (transferSize > _scalarTargetDescriptor->getLength())
       || (buffer == NULL)
       || (target == NULL))
    {
        debug(kApplePIODMADebugLoggingAlways, "rejecting scalar request of size %llu\n", transferSize);
        return kIOReturnBadArgument;
    }

    _scalarBuffer = buffer;
    bcopy(buffer, _scalarBufferDescriptor->getBytesNoCopy(), transferSize);

    _scalarTarget = target;
    bcopy(target, _scalarTargetDescriptor->getBytesNoCopy(), transferSize);

    return prepareGenericPacket(_scalarBufferDescriptor,
                                0,
                                _scalarTargetDescriptor,
                                0,
                                bufferType,
                                targetType,
                                transferSize,
                                commandType);
}

void ApplePIODMARequest::setCommandTag(uint8_t commandTag)
{
    _commandTag = commandTag;
}

uint8_t ApplePIODMARequest::commandTag()
{
    return _commandTag;
}

uint32_t ApplePIODMARequest::commandSize()
{
    return _commandSize;
}

uint32_t ApplePIODMARequest::errorStatus()
{
    return _errorStatus;
}

void ApplePIODMARequest::setErrorStatus(uint32_t errorStatus)
{
    _errorStatus = errorStatus;
}

IOPhysicalAddress ApplePIODMARequest::errorAddress()
{
    return _errorAddress;
}

void ApplePIODMARequest::setErrorAddress(IOPhysicalAddress errorAddress)
{
    _errorAddress = errorAddress;
}

uint8_t ApplePIODMARequest::commandType()
{
    return _commandType;
}

IOPhysicalAddress ApplePIODMARequest::commandSource()
{
    // the buffers must be physically contiguous unless we use the linked list method
    IODMACommand::Segment64 segment;
    bzero(&segment, sizeof(IODMACommand::Segment64));
    uint32_t numSegments = 1;
    uint64_t offset      = 0;
    _commandSourceDMACommand->genIOVMSegments(&offset, &segment, &numSegments);

    return segment.fIOVMAddr;
}

IOPhysicalAddress ApplePIODMARequest::commandDestination()
{
    // the buffers must be physically contiguous unless we use the linked list method
    IODMACommand::Segment64 segment;
    bzero(&segment, sizeof(IODMACommand::Segment64));
    uint32_t numSegments = 1;
    uint64_t offset      = 0;
    _commandDestinationDMACommand->genIOVMSegments(&offset, &segment, &numSegments);

    return segment.fIOVMAddr;
}
