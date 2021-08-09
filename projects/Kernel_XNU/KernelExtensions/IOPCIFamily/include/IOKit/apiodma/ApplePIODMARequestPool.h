//
//  ApplePIODMARequestPool.h
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/26/20.
//

#ifndef ApplePIODMARequestPool_h
#define ApplePIODMARequestPool_h
#include <IOKit/IOCommand.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/apiodma/ApplePIODMADefinitions.h>

class ApplePIODMARequestPool :  public IOCommandPool
{
    OSDeclareDefaultStructors(ApplePIODMARequestPool);

public:
public:
	static ApplePIODMARequestPool*	withWorkLoop(IOWorkLoop* workLoop,
                                                 IOMapper*   mapper,
                                                 uint32_t    maxOutstandingCommands,
                                                 uint32_t    byteAlignment,
                                                 uint8_t     numberOfAddressBits,
                                                 uint64_t    maxTransferSize,
                                                 uint64_t    maxSegmentSize);

    virtual void free() override;
    unsigned int maximumCommandsSupported();

protected:
    virtual bool initWithWorkLoop(IOWorkLoop* workLoop,
                                  IOMapper*   mapper,
                                  uint32_t    maxOutstandingCommands,
                                  uint32_t    byteAlignment,
                                  uint8_t     numberOfAddressBits,
                                  uint64_t    maxTransferSize,
                                  uint64_t    maxSegmentSize);

#pragma mark Pool Management

    virtual IOCommand* allocateCommand();

    uint32_t    _debugLoggingMask;
    IOWorkLoop* _workLoop;
    IOMapper*   _memoryMapper;
    uint32_t    _maxOutstandingCommands;
    uint32_t    _byteAlignment;
    uint8_t     _numberOfAddressBits;
    uint64_t    _maxTransferSize;
    uint64_t    _maxSegmentSize;
};

#endif /* ApplePIODMARequestPool_h */
