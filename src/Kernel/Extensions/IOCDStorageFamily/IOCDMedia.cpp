/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <IOKit/storage/IOCDBlockStorageDriver.h>
#include <IOKit/storage/IOCDMedia.h>

#define	super IOMedia
OSDefineMetaClassAndStructors(IOCDMedia, IOMedia)

class IOStorageSyncerLock
{
protected:

    IOLock * _lock;

public:

    inline IOStorageSyncerLock( )
    {
        _lock = IOLockAlloc( );
    }

    inline ~IOStorageSyncerLock( )
    {
        if ( _lock ) IOLockFree( _lock );
    }

    inline void lock( )
    {
        IOLockLock( _lock );
    }

    inline void unlock( )
    {
        IOLockUnlock( _lock );
    }

    inline void sleep( void * event )
    {
        IOLockSleep( _lock, event, THREAD_UNINT );
    }

    inline void wakeup( void * event )
    {
        IOLockWakeup( _lock, event, false );
    }
};

static IOStorageSyncerLock gIOStorageSyncerLock;

class IOStorageSyncer
{
protected:

    IOReturn _status;
    bool     _wakeup;

public:

    IOStorageSyncer( )
    {
        _wakeup = false;
    }

    IOReturn wait( )
    {
        gIOStorageSyncerLock.lock( );

        while ( _wakeup == false )
        {
            gIOStorageSyncerLock.sleep( this );
        }

        gIOStorageSyncerLock.unlock( );

        return _status;
    }

    void signal( IOReturn status )
    {
        _status = status;

        gIOStorageSyncerLock.lock( );

        _wakeup = true;

        gIOStorageSyncerLock.wakeup( this );

        gIOStorageSyncerLock.unlock( );
    }
};

static void storageCompletion(void *   target,
                              void *   parameter,
                              IOReturn status,
                              UInt64   actualByteCount)
{
    //
    // Internal completion routine for synchronous versions of read and write.
    //

    if (parameter)  *((UInt64 *)parameter) = actualByteCount;
    ((IOStorageSyncer *)target)->signal(status);
}

IOCDBlockStorageDriver * IOCDMedia::getProvider() const
{
    //
    // Obtain this object's provider.  We override the superclass's method to
    // return a more specific subclass of IOService -- IOCDBlockStorageDriver.  
    // This method serves simply as a convenience to subclass developers.
    //

    return (IOCDBlockStorageDriver *) IOService::getProvider();
}

bool IOCDMedia::matchPropertyTable(OSDictionary * table, SInt32 * score)
{
    //
    // Compare the properties in the supplied table to this object's properties.
    //

    // Ask our superclass' opinion.

    if (super::matchPropertyTable(table, score) == false)  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied table.

    return compareProperty(table, kIOCDMediaTOCKey ) &&
           compareProperty(table, kIOCDMediaTypeKey);
}

void IOCDMedia::read(IOService *           /* client */,
                     UInt64                byteStart,
                     IOMemoryDescriptor *  buffer,
                     IOStorageAttributes * attributes,
                     IOStorageCompletion * completion)
{
    //
    // Read data from the storage object at the specified byte offset into the
    // specified buffer, asynchronously.   When the read completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the read.
    //
    // This method will work even when the media is in the terminated state.
    //

    if (isInactive())
    {
        complete(completion, kIOReturnNoMedia);
        return;
    }

    if (_openLevel == kIOStorageAccessNone)    // (instantaneous value, no lock)
    {
        complete(completion, kIOReturnNotOpen);
        return;
    }

    if (_mediaSize == 0 || _preferredBlockSize == 0)
    {
        complete(completion, kIOReturnUnformattedMedia);
        return;
    }

    if (buffer == 0)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    if (buffer->getDirection() != kIODirectionIn)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    if (_mediaSize < byteStart)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    if (_mediaSize - byteStart < buffer->getLength())
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    byteStart += _mediaBase;
    getProvider()->readCD( /* client     */ this,
                           /* byteStart  */ byteStart,
                           /* buffer     */ buffer,
                           /* sectorArea */ (CDSectorArea) 0xF8, // (2352 bytes)
                           /* sectorType */ (CDSectorType) 0x00, // ( all types)
                           /* attributes */ attributes,
                           /* completion */ completion );
}

void IOCDMedia::write(IOService *           client,
                      UInt64                byteStart,
                      IOMemoryDescriptor *  buffer,
                      IOStorageAttributes * attributes,
                      IOStorageCompletion * completion)
{
    //
    // Write data into the storage object at the specified byte offset from the
    // specified buffer, asynchronously.   When the write completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the write.
    //
    // This method will work even when the media is in the terminated state.
    //

    if (isInactive())
    {
        complete(completion, kIOReturnNoMedia);
        return;
    }

    if (_openLevel == kIOStorageAccessNone)    // (instantaneous value, no lock)
    {
        complete(completion, kIOReturnNotOpen);
        return;
    }

    if (_openLevel == kIOStorageAccessReader)  // (instantaneous value, no lock)
    {
        complete(completion, kIOReturnNotPrivileged);
        return;
    }

    if (_isWritable == 0)
    {
        complete(completion, kIOReturnLockedWrite);
        return;
    }

    if (_mediaSize == 0 || _preferredBlockSize == 0)
    {
        complete(completion, kIOReturnUnformattedMedia);
        return;
    }

    if (buffer == 0)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    if (buffer->getDirection() != kIODirectionOut)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    if (_mediaSize < byteStart)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    if (_mediaSize - byteStart < buffer->getLength())
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    byteStart += _mediaBase;
    getProvider()->writeCD(
                           /* client     */ this,
                           /* byteStart  */ byteStart,
                           /* buffer     */ buffer,
                           /* sectorArea */ (CDSectorArea) 0xF8, // (2352 bytes)
                           /* sectorType */ (CDSectorType) 0x00, // ( all types)
                           /* attributes */ attributes,
                           /* completion */ completion );
}

IOReturn IOCDMedia::readCD(IOService *           client,
                           UInt64                byteStart,
                           IOMemoryDescriptor *  buffer,
                           CDSectorArea          sectorArea,
                           CDSectorType          sectorType,
                           IOStorageAttributes * attributes,
                           UInt64 *              actualByteCount)
{
    //
    // Read data from the CD media object at the specified byte offset into the
    // specified buffer, synchronously.   Special areas of the CD sector can be
    // read via this method, such as the header and subchannel data.   When the
    // read completes, this method will return to the caller.   The actual byte
    // count field is optional.
    //
    // This method will work even when the media is in the terminated state.
    //

    IOStorageCompletion completion;
    IOStorageSyncer     syncer;

    // Fill in the completion information for this request.

    completion.target    = &syncer;
    completion.action    = storageCompletion;
    completion.parameter = actualByteCount;

    // Issue the asynchronous read.

    readCD(client, byteStart, buffer, sectorArea, sectorType, attributes, &completion);

    // Wait for the read to complete.

    return syncer.wait();
}

void IOCDMedia::readCD(IOService *           client,
                       UInt64                byteStart,
                       IOMemoryDescriptor *  buffer,
                       CDSectorArea          sectorArea,
                       CDSectorType          sectorType,
                       IOStorageAttributes * attributes,
                       IOStorageCompletion * completion)
{
    //
    // Read data from the CD media object at the specified byte offset into the
    // specified buffer, asynchronously.  Special areas of the CD sector can be
    // read via this method, such as the header and subchannel data.   When the
    // read completes, the caller will be notified via the specified completion
    // action.
    //
    // The buffer will be retained for the duration of the read.
    //
    // This method will work even when the media is in the terminated state.
    //

    if (isInactive())
    {
        complete(completion, kIOReturnNoMedia);
        return;
    }

    if (_openLevel == kIOStorageAccessNone)    // (instantaneous value, no lock)
    {
        complete(completion, kIOReturnNotOpen);
        return;
    }

    if (_mediaSize == 0 || _preferredBlockSize == 0)
    {
        complete(completion, kIOReturnUnformattedMedia);
        return;
    }

    if (buffer == 0)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    byteStart += _mediaBase;
    getProvider()->readCD( /* client     */ this,
                           /* byteStart  */ byteStart,
                           /* buffer     */ buffer,
                           /* sectorArea */ sectorArea,
                           /* sectorType */ sectorType,
                           /* attributes */ attributes,
                           /* completion */ completion );
}

IOReturn IOCDMedia::readISRC(UInt8 track, CDISRC isrc)
{
    //
    // Read the International Standard Recording Code for the specified track.
    //
    // This method will work even when the media is in the terminated state.
    //

    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->readISRC(track, isrc);
}

IOReturn IOCDMedia::readMCN(CDMCN mcn)
{
    //
    // Read the Media Catalog Number (also known as the Universal Product Code).
    //
    // This method will work even when the media is in the terminated state.
    //
    
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->readMCN(mcn);
}

CDTOC * IOCDMedia::getTOC()
{
    //
    // Get the full Table Of Contents.
    //
    // This method will work even when the media is in the terminated state.
    //

    if (isInactive())
    {
        return 0;
    }

    return getProvider()->getTOC();
}

IOReturn IOCDMedia::getSpeed(UInt16 * kilobytesPerSecond)
{
    //
    // Get the current speed used for data transfers.
    //
    
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->getSpeed(kilobytesPerSecond);
}

IOReturn IOCDMedia::setSpeed(UInt16 kilobytesPerSecond)
{
    //
    // Set the speed to be used for data transfers.
    //
    
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->setSpeed(kilobytesPerSecond);
}

IOReturn IOCDMedia::readTOC(IOMemoryDescriptor * buffer,
                            CDTOCFormat          format,
                            UInt8                formatAsTime,
                            UInt8                trackOrSessionNumber,
                            UInt16 *             actualByteCount)
{
    if (isInactive())
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnNoMedia;
    }

    if (buffer == 0)
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnBadArgument;
    }

    return getProvider()->readTOC(
                                /* buffer               */ buffer,
                                /* format               */ format,
                                /* formatAsTime         */ formatAsTime,
                                /* trackOrSessionNumber */ trackOrSessionNumber,
                                /* actualByteCount      */ actualByteCount );
}

IOReturn IOCDMedia::readDiscInfo(IOMemoryDescriptor * buffer,
                                 UInt16 *             actualByteCount)
{
    if (isInactive())
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnNoMedia;
    }

    if (buffer == 0)
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnBadArgument;
    }

    return getProvider()->readDiscInfo(
                                /* buffer               */ buffer,
                                /* actualByteCount      */ actualByteCount );
}

IOReturn IOCDMedia::readTrackInfo(IOMemoryDescriptor *   buffer,
                                  UInt32                 address,
                                  CDTrackInfoAddressType addressType,
                                  UInt16 *               actualByteCount)
{
    if (isInactive())
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnNoMedia;
    }

    if (buffer == 0)
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnBadArgument;
    }

    return getProvider()->readTrackInfo(
                                /* buffer               */ buffer,
                                /* address              */ address,
                                /* addressType          */ addressType,
                                /* actualByteCount      */ actualByteCount );
}

void IOCDMedia::writeCD(IOService *           client,
                        UInt64                byteStart,
                        IOMemoryDescriptor *  buffer,
                        CDSectorArea          sectorArea,
                        CDSectorType          sectorType,
                        IOStorageAttributes * attributes,
                        IOStorageCompletion * completion)
{
    //
    // Write data into the CD media object at the specified byte offset from the
    // specified buffer, asynchronously.    When the write completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the write.
    //
    // This method will work even when the media is in the terminated state.
    //

    if (isInactive())
    {
        complete(completion, kIOReturnNoMedia);
        return;
    }

    if (_openLevel == kIOStorageAccessNone)    // (instantaneous value, no lock)
    {
        complete(completion, kIOReturnNotOpen);
        return;
    }

    if (_openLevel == kIOStorageAccessReader)  // (instantaneous value, no lock)
    {
        complete(completion, kIOReturnNotPrivileged);
        return;
    }

    if (_isWritable == 0)
    {
        complete(completion, kIOReturnLockedWrite);
        return;
    }

    if (_mediaSize == 0 || _preferredBlockSize == 0)
    {
        complete(completion, kIOReturnUnformattedMedia);
        return;
    }

    if (buffer == 0)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    byteStart += _mediaBase;
    getProvider()->writeCD( /* client     */ this,
                            /* byteStart  */ byteStart,
                            /* buffer     */ buffer,
                            /* sectorArea */ sectorArea,
                            /* sectorType */ sectorType,
                            /* attributes */ attributes,
                            /* completion */ completion );
}

IOReturn IOCDMedia::writeCD(IOService *           client,
                            UInt64                byteStart,
                            IOMemoryDescriptor *  buffer,
                            CDSectorArea          sectorArea,
                            CDSectorType          sectorType,
                            IOStorageAttributes * attributes,
                            UInt64 *              actualByteCount)
{
    //
    // Write data into the CD media object at the specified byte offset from the
    // specified buffer, synchronously.    When the write completes, this method
    // will return to the caller.  The actual byte count field is optional.
    //
    // This method will work even when the media is in the terminated state.
    //

    IOStorageCompletion completion;
    IOStorageSyncer     syncer;

    // Fill in the completion information for this request.

    completion.target    = &syncer;
    completion.action    = storageCompletion;
    completion.parameter = actualByteCount;

    // Issue the asynchronous write.

    writeCD(client, byteStart, buffer, sectorArea, sectorType, attributes, &completion);

    // Wait for the write to complete.

    return syncer.wait();
}

OSMetaClassDefineReservedUnused(IOCDMedia,  0);
OSMetaClassDefineReservedUnused(IOCDMedia,  1);
OSMetaClassDefineReservedUnused(IOCDMedia,  2);
OSMetaClassDefineReservedUnused(IOCDMedia,  3);
OSMetaClassDefineReservedUnused(IOCDMedia,  4);
OSMetaClassDefineReservedUnused(IOCDMedia,  5);
OSMetaClassDefineReservedUnused(IOCDMedia,  6);
OSMetaClassDefineReservedUnused(IOCDMedia,  7);
OSMetaClassDefineReservedUnused(IOCDMedia,  8);
OSMetaClassDefineReservedUnused(IOCDMedia,  9);
OSMetaClassDefineReservedUnused(IOCDMedia, 10);
OSMetaClassDefineReservedUnused(IOCDMedia, 11);
OSMetaClassDefineReservedUnused(IOCDMedia, 12);
OSMetaClassDefineReservedUnused(IOCDMedia, 13);
OSMetaClassDefineReservedUnused(IOCDMedia, 14);
OSMetaClassDefineReservedUnused(IOCDMedia, 15);
