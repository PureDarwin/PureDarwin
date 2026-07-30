/*
 * Copyright (c) 1998-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IONETWORKUSERCLIENT_H
#define _IONETWORKUSERCLIENT_H

// IONetworkUserClient type ID.
//
#define kIONetworkUserClientTypeID   0xff000001
#define kIONUCType                   0xff000001  // FIXME

// IONetworkUserClient call structure definitions.
//
enum {
        kIONUCResetNetworkDataIndex          = 0,
        kIONUCWriteNetworkDataIndex          = 1,
        kIONUCReadNetworkDataIndex           = 2,
        kIONUCGetNetworkDataCapacityIndex    = 3,
        kIONUCGetNetworkDataHandleIndex      = 4,
        kIONUCLastIndex
};

#ifdef KERNEL

#include <IOKit/IOUserClient.h>

class IONetworkInterface;

/*! @class IONetworkUserClient
    @abstract An IOUserClient created by an IONetworkInterface to
    manage user space requests. */

class IONetworkUserClient : public IOUserClient
{
    OSDeclareDefaultStructors( IONetworkUserClient )

protected:
    IONetworkInterface *    _owner;
    task_t                  _task;
    OSArray *               _handleArray;
    IOLock *                _handleLock;

/*! @function
    @abstract Free the IONetworkUserClient object. */

    virtual void free(void) APPLE_KEXT_OVERRIDE;

public:

/*! @function withTask
    @abstract Factory method that performs allocation and initialization
    of an IONetworkUserClient object.
    @param owningTask See IOUserClient.
    @result An IONetworkUserClient on success, 0 otherwise. */

    static IONetworkUserClient * withTask(task_t owningTask);

/*! @function start
    @abstract Start the IONetworkUserClient.
    @discussion Open the provider, must be an IONetworkInterface object,
    and initialize the IOExternalMethod array.
    @result true on success, false otherwise. */

    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;

/*! @function clientClose
    @abstract Handle a client close.
    @discussion Close and detach from our owner (provider).
    @result kIOReturnSuccess. */

    virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;

/*! @function clientDied
    @abstract Handle client death.
    @discussion Close and detach from our owner (provider).
    @result kIOReturnSuccess. */

    virtual IOReturn clientDied(void) APPLE_KEXT_OVERRIDE;

    virtual IOReturn externalMethod(
                        uint32_t selector,
                        IOExternalMethodArguments * arguments,
                        IOExternalMethodDispatch * dispatch = 0,
                        OSObject * target = 0,
                        void * reference = 0 ) APPLE_KEXT_OVERRIDE;

protected:

/*! @function resetNetworkData
    @abstract Fill the data buffer in an IONetworkData object with zeroes.
    @param dataHandle The handle associated with an IONetworkData object.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid, or an error from IONetworkData::reset(). */

    IOReturn resetNetworkData(uint32_t  dataHandle);

/*! @function writeNetworkData
    @abstract Write to the data buffer in an IONetworkData object with
    data from a source buffer provided by the caller.
    @param dataHandle The handle associated with an IONetworkData object.
    @param srcBuffer The source buffer provided by the caller.
    @param srcBufferSize The size of the source buffer.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid, or an error from IONetworkData::write(). */

    IOReturn writeNetworkData(uint32_t  dataHandle,
                              void *    srcBuffer,
                              uint32_t  srcBufferSize);

/*! @function readNetworkData
    @abstract Read the data buffer in an IONetworkData object and copy
    this data to a destination buffer provided by the caller.
    @param dataHandle The handle associated with an IONetworkData object.
    @param dstBuffer The destination buffer provided by the caller.
    @param dstBufferSize Pointer to an integer that the caller must
    initialize to hold the size of the destination buffer. This method
    will overwrite it with the actual number of bytes written.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid, or an error from IONetworkData::read(). */

    IOReturn readNetworkData(uint32_t   dataHandle,
                             void *     dstBuffer,
                             uint32_t * dstBufferSize);

/*! @function getNetworkDataCapacity
    @abstract Get the capacity of an IONetworkData object, described
    by the size of its data buffer.
    @param dataHandle The handle of an IONetworkData object.
    @param capacity A pointer to the capacity value returned by this
    method.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid. */

    IOReturn getNetworkDataCapacity(uint32_t   dataHandle,
                                    uint64_t * capacity);

/*! @function getNetworkDataHandle
    @abstract Return an opaque handle to a provider's IONetworkData object.
    @discussion Called to obtain an unique handle that maps to an IONetworkData
    object. This handle can be later passed to other methods defined in this
    class to refer to the same object.
    @param name A C string with the name of the IONetworkData object.
    @param dataHandle If an IONetworkData object with the given name is found,
    then a handle is written to this address.
    @param nameSize The size of the name string, including the final
    terminating null character.
    @param handleSizeP The size of the buffer allocated by the caller
    to store the handle. This should be 4 bytes.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid, or kIOReturnNoMemory if unable to allocate memory. */

    IOReturn getNetworkDataHandle(const char * name,
                                  uint32_t *   dataHandle,
                                  uint32_t     nameSize,
                                  uint32_t *   handleSizeP);

/*! @function setProperties
    @abstract Handle a request to set properties from non-kernel clients.
    This call is propagated to our provider.
    @param properties An OSObject subclass that describes one or more
    properties.
    @result The return value from the invocation of this method in our
    provider is returned. */

    virtual IOReturn setProperties(OSObject * properties) APPLE_KEXT_OVERRIDE;

/*! @function getService
    @abstract Get the IOService which is the provider of this user client.
    @result Returns the IONetworkInterface that created the user client. */

    virtual IOService * getService() APPLE_KEXT_OVERRIDE;
};

#endif /* KERNEL */

#endif /* !_IONETWORKUSERCLIENT_H */
