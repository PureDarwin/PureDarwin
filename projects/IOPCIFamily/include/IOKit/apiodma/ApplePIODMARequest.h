//
//  ApplePIODMARequest.h
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/26/20.
//

#ifndef ApplePIODMARequest_H
#define ApplePIODMARequest_H
#include <IOKit/IOCommand.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOMapper.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include <IOKit/apiodma/ApplePIODMADefinitions.h>
#include <IOKit/apiodma/ApplePIODMARequestPool.h>


enum tApplePIODMARequestBufferType
{
    kApplePIODMARequestBufferTypeHostMemory = 0,
    kApplePIODMARequestBufferTypeDeviceMMIOSpace,
    kApplePIODMARequestBufferTypeDeviceConfigurationSpace,
    kApplePIODMARequestBufferTypeDevicePIOSpace,
    kApplePIODMARequestBufferTypeCount
};

class ApplePIODMARequest : public IOCommand
{
    OSDeclareDefaultStructors(ApplePIODMARequest);

public:
    virtual void free() override;

#pragma Session Management

public:
    static ApplePIODMARequest* withMapper(IOMapper* mapper,
                                          uint32_t  byteAlignment,
                                          uint8_t   numberOfAddressBits,
                                          uint64_t  maxTransferSize,
                                          uint64_t  maxSegmentSize);

    virtual IOReturn prepareGenericPacket(IOMemoryDescriptor*                 bufferBase,
                                          IOByteCount                         bufferOffset,
                                          IOMemoryDescriptor*                 targetBase,
                                          IOByteCount                         targetOffset,
                                          tApplePIODMAGenericPacketDeviceType bufferType,
                                          tApplePIODMAGenericPacketDeviceType targetType,
                                          IOByteCount                         transferSize,
                                          uint8_t                             commandType,
                                          uint8_t                             bufferBaseAddressSelect = 0,
                                          uint8_t                             targetBaseAddressSelect = 0);

    virtual IOReturn prepareGenericPacket(void*                               buffer,
                                          IOMemoryDescriptor*                 targetBase,
                                          IOByteCount                         targetOffset,
                                          tApplePIODMAGenericPacketDeviceType bufferType,
                                          tApplePIODMAGenericPacketDeviceType targetType,
                                          IOByteCount                         transferSize,
                                          uint8_t                             commandType,
                                          uint8_t                             bufferBaseAddressSelect = 0,
                                          uint8_t                             targetBaseAddressSelect = 0);

    virtual IOReturn prepareGenericPacket(IOMemoryDescriptor*                 bufferBase,
                                          IOByteCount                         bufferOffset,
                                          void*                               target,
                                          tApplePIODMAGenericPacketDeviceType bufferType,
                                          tApplePIODMAGenericPacketDeviceType targetType,
                                          IOByteCount                         transferSize,
                                          uint8_t                             commandType,
                                          uint8_t                             bufferBaseAddressSelect = 0,
                                          uint8_t                             targetBaseAddressSelect = 0);

    virtual IOReturn prepareGenericPacket(void*                               buffer,
                                          void*                               target,
                                          tApplePIODMAGenericPacketDeviceType bufferType,
                                          tApplePIODMAGenericPacketDeviceType targetType,
                                          IOByteCount                         transferSize,
                                          uint8_t                             commandType,
                                          uint8_t                             bufferBaseAddressSelect = 0,
                                          uint8_t                             targetBaseAddressSelect = 0);

    virtual IOReturn complete();

    virtual void              setCommandTag(uint8_t commandTag);
    virtual uint8_t           commandTag();
    virtual uint32_t          commandSize();
    virtual uint32_t          errorStatus();
    virtual void              setErrorStatus(uint32_t errorStatus);
    virtual IOPhysicalAddress errorAddress();
    virtual void              setErrorAddress(IOPhysicalAddress errorAddress);
    virtual uint8_t           commandType();
    virtual IOPhysicalAddress commandSource();
    virtual IOPhysicalAddress commandDestination();

protected:
    virtual bool initWithMapper(IOMapper* mapper,
                                uint32_t  byteAlignment,
                                uint8_t   numberOfAddressBits,
                                uint64_t  maxTransferSize,
                                uint64_t  maxSegmentSize);

    virtual tApplePIODMAGenericPacketDeviceType deviceType(tApplePIODMARequestBufferType sourceType);

    IODMACommand*       _bufferBaseDMACommand;
    IODMACommand*       _targetBaseDMACommand;
    IOMemoryDescriptor* _bufferBase;
    IOMemoryDescriptor* _targetBase;


    IODMACommand*             _commandSourceDMACommand;
    IODMACommand*             _commandDestinationDMACommand;
    IOBufferMemoryDescriptor* _commandSourceBuffer;
    IOBufferMemoryDescriptor* _commandDestinationBuffer;
    IOBufferMemoryDescriptor* _scalarBufferDescriptor;
    IOBufferMemoryDescriptor* _scalarTargetDescriptor;
    void*                     _scalarBuffer;
    void*                     _scalarTarget;

    uint32_t          _errorStatus;
    IOPhysicalAddress _errorAddress;

    uint32_t _debugLoggingMask;

    uint32_t    _commandSize;
    IOByteCount _transferSize;
    uint8_t     _commandTag;
    uint8_t     _commandType;

};

#endif /* ApplePIODMARequest_H */
