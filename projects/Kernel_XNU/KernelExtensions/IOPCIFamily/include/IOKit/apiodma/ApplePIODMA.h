//
//  ApplePIODMA.h
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/24/20.
//

#ifndef _IOKIT_ApplePIODMA_H
#define _IOKIT_ApplePIODMA_H

#include <IOKit/IOLocks.h>
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMapper.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/apiodma/ApplePIODMADefinitions.h>
#include <IOKit/apiodma/ApplePIODMARequestPool.h>
#include <IOKit/apiodma/ApplePIODMARequest.h>

enum
{
    kApplePIODMAMemoryIndexPIODMARegisters = 0
};

enum tApplePIODMAState
{
    kApplePIODMAStateDisabled = 0,
    kApplePIODMAStateDisabledPending,
    kApplePIODMAStateEnabled
};

class ApplePIODMARequest;

class ApplePIODMA : public IOService
{
    OSDeclareAbstractStructors(ApplePIODMA)

public:
#pragma mark IOService overrides
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;

#pragma mark Session Management
    /*!
     * @brief       Enables the PIODMA engine to be ready to service requests
     * @result      kIOReturnSuccess if there were no errors.
     */
    virtual IOReturn enable();

    /*!
     * @brief       Disables the PIODMA engine
     */
    virtual void disable();

#pragma mark DMA access

    /*!
     * @brief       Copy memory from host memory to device memory
     * @discussion  This method will copy in contiguous memory from host memory to device memory. This method may block and should not be used in a primary
     *                      interrupt context.
     * @param       sourceBase                 Memory descriptor for the base address of the host memory to be copied from
     * @param       sourceOffset             Offset into sourceBase to start the transfer from
     * @param       destinationBase      Memory descriptor for the destination address of the device to be copied to
     * @param       destinationOffset Offset into destinationBase of the device to be copied to
     * @param       size			    Amount of bytes to copy
     * @result      kIOReturnSuccess if there were no errors. kIOReturnNoPower will be returned if enable has not been called yet.
     */
    virtual IOReturn hostToDevice(IOMemoryDescriptor* sourceBase,
                                  IOByteCount         sourceOffset,
                                  IOMemoryDescriptor* destinationBase,
                                  IOByteCount         destinationOffset,
                                  IOByteCount         size);

    virtual IOReturn hostToDevice(const void*         source,
                                  IOMemoryDescriptor* destinationBase,
                                  IOByteCount         destinationOffset,
                                  IOByteCount         size);

    /*!
     * @brief       Copy memory from device memory to host memory
     * @discussion  This method will copy in contiguous memory from device memory to host memory. This method may block and should not be used in a primary
     *                      interrupt context.
     * @param       sourceBase                 Memory descriptor for the base address of the device memory to be copied from
     * @param       sourceOffset             Offset into sourceBase to start the transfer from
     * @param       destinationBase      Memory descriptor for the destination address of the host memory to be copied to
     * @param       destinationOffset Offset into destinationBase of the host memory to be copied to
     * @param       size			    Amount of bytes to copy
     * @result      kIOReturnSuccess if there were no errors. kIOReturnNoPower will be returned if enable has not been called yet.
     */
    virtual IOReturn deviceToHost(IOMemoryDescriptor* sourceBase,
                                  IOByteCount         sourceOffset,
                                  IOMemoryDescriptor* destinationBase,
                                  IOByteCount         destinationOffset,
                                  IOByteCount         size);

    virtual IOReturn deviceToHost(IOMemoryDescriptor* sourceBase,
                                  IOByteCount         sourceOffset,
                                  void*               destination,
                                  IOByteCount         size);

    /*!
     * @brief       Copy memory from device memory to device memory
     * @discussion  This method will copy in contiguous memory from device memory to device memory. This method may block and should not be used in a primary
     *                      interrupt context.
     * @param       sourceBase                 Memory descriptor for the base address of the device memory to be copied from
     * @param       sourceOffset             Offset into sourceBase to start the transfer from
     * @param       destinationBase      Memory descriptor for the destination address of the device memory to be copied to
     * @param       destinationOffset Offset into destinationBase of the device memory to be copied to
     * @param       size			    Amount of bytes to copy
     * @result      kIOReturnSuccess if there were no errors. kIOReturnNoPower will be returned if enable has not been called yet.
     */
    virtual IOReturn deviceToDevice(IOMemoryDescriptor* sourceBase,
                                    IOByteCount         sourceOffset,
                                    IOMemoryDescriptor* destinationBase,
                                    IOByteCount         destinationOffset,
                                    IOByteCount         size);

    /*!
     * @brief       Copy memory from host memory to host memory
     * @discussion  This method will copy in contiguous memory from host memory to host memory. Similar to a memcp. This method may block and should not be used in a primary
     *                      interrupt context.
     * @param       sourceBase                 Memory descriptor for the base address of the host memory to be copied from
     * @param       sourceOffset             Offset into sourceBase to start the transfer from
     * @param       destinationBase      Memory descriptor for the destination address of the host memory to be copied to
     * @param       destinationOffset Offset into destinationBase of the host memory to be copied to
     * @param       size			    Amount of bytes to copy
     * @result      kIOReturnSuccess if there were no errors. kIOReturnNoPower will be returned if enable has not been called yet.
     */
    virtual IOReturn hostToHost(IOMemoryDescriptor* sourceBase,
                                IOByteCount         sourceOffset,
                                IOMemoryDescriptor* destinationBase,
                                IOByteCount         destinationOffset,
                                IOByteCount         size);

    virtual IOReturn hostToHost(const void* source,
                                void*       destination,
                                IOByteCount size);

protected:
    void                            executeRequest(ApplePIODMARequest* request);
    virtual IOReturn                executeRequestGated(ApplePIODMARequest* request);
    virtual ApplePIODMARequestPool* allocateRequestPool()                                        = 0;
    virtual void                    interruptOccurred(IOInterruptEventSource* source, int count) = 0;
    virtual IOReturn                enableGated();
    virtual IOReturn                disableGated();

    tApplePIODMAState       _state;
    IORWLock*               _stateLock;
    uint32_t                _debugLoggingMask;
    IOWorkLoop*             _workLoop;
    IOCommandGate*          _commandGate;
    IODeviceMemory*         _pioDeviceMemory;
    IOMemoryMap*            _pioDeviceMemoryMap;
    IOMapper*               _memoryMapper;
    IOInterruptEventSource* _interruptEventSource;


    ApplePIODMARequestPool* _requestPool;
    ApplePIODMARequest**    _completionQueue;
    unsigned int            _completionQueueSize;
    unsigned int            _completionTail;
    unsigned int            _completionHead;

};

#endif /* _IOKIT_ApplePIODMA_H */
