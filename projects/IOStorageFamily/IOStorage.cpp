/*
 * Copyright (c) 1998-2015 Apple Inc. All rights reserved.
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

#define IOLOCKS_INLINE

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>
#include <IOKit/IOLocks.h>

#define super IOService
OSDefineMetaClassAndAbstractStructors(IOStorage, IOService)

#if TARGET_OS_OSX
#define kIOStorageSynchronizeOptionsUnsupported ( ( IOStorage::ExpansionData * ) 1 )

#if defined(__x86_64__) || defined(__i386__)
extern "C" void _ZN9IOStorage16synchronizeCacheEP9IOService( IOStorage *, IOService * );
extern "C" void _ZN9IOStorage11synchronizeEP9IOServiceyyj( IOStorage *, IOService *, UInt64, UInt64, IOStorageSynchronizeOptions );

#define storageSynchronizeOptions( storage ) ( ( OSMemberFunctionCast( void *, storage, &IOStorage::synchronizeCache ) == _ZN9IOStorage16synchronizeCacheEP9IOService ) && \
                                               ( OSMemberFunctionCast( void *, storage, &IOStorage::synchronize      ) != _ZN9IOStorage11synchronizeEP9IOServiceyyj   ) )

#else

#define storageSynchronizeOptions( storage ) (1)

#endif
#endif /* TARGET_OS_OSX */

class IOStorageSyncerLock
{
protected:

    IOSimpleLock _lock;

public:

    inline IOStorageSyncerLock( )
    {
        IOSimpleLockInit( &_lock );
    }

    inline ~IOStorageSyncerLock( )
    {
        IOSimpleLockDestroy( &_lock );
    }

    inline void lock( )
    {
        IOSimpleLockLock( &_lock );
    }

    inline void unlock( )
    {
        IOSimpleLockUnlock( &_lock );
    }

    inline void sleep( void * event )
    {
        wait_result_t ret = assert_wait( ( event_t ) event, false );
        
        unlock( );
        
        if( ret == THREAD_WAITING )
        {
            thread_block( THREAD_CONTINUE_NULL );
        }
        
        lock( );
    }

    inline void wakeup( void * event )
    {
        thread_wakeup( event );
    }
};

class IOStorageSyncer
{
protected:

    IOReturn            _status;
    bool                _wakeup;
    IOStorageSyncerLock _syncerLock;

public:

    IOStorageSyncer( )
    {
        _wakeup = false;
    }

    IOReturn wait( )
    {
        _syncerLock.lock( );

        while ( _wakeup == false )
        {
            _syncerLock.sleep( ( void * )this );
        }

        _syncerLock.unlock( );

        return _status;
    }

    void signal( IOReturn status )
    {
        _status = status;

        _syncerLock.lock( );

        _wakeup = true;

        _syncerLock.unlock( );
        
        _syncerLock.wakeup( ( void * )this );
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

#if TARGET_OS_OSX
bool IOStorage::attach(IOService * provider)
{
    if ( super::attach( provider ) == false )
    {
        return false;
    }

    if ( storageSynchronizeOptions( this ) == false )
    {
        _respondsTo_synchronizeCache = kIOStorageSynchronizeOptionsUnsupported;
    }

    if ( _respondsTo_synchronizeCache )
    {
        OSDictionary * features;

        features = OSDynamicCast( OSDictionary, getProperty( kIOStorageFeaturesKey, gIOServicePlane ) );

        if ( features )
        {
            features = OSDictionary::withDictionary( features );

            if ( features )
            {
                features->removeObject( kIOStorageFeatureBarrier );

                setProperty( kIOStorageFeaturesKey, features );

                features->release( );
            }
        }
    }

    return true;
}
#endif /* TARGET_OS_OSX */

void IOStorage::complete(IOStorageCompletion * completion,
                         IOReturn              status,
                         UInt64                actualByteCount)
{
    //
    // Invokes the specified completion action of the read/write request.  If
    // the completion action is unspecified, no action is taken.  This method
    // serves simply as a convenience to storage subclass developers.
    //

    if ( completion && completion->action )
    {
        ( completion->action )( completion->target, completion->parameter, status, actualByteCount );
    }
}

bool IOStorage::open(IOService *     client,
                     IOOptionBits    options,
                     IOStorageAccess access)
{
    //
    // Ask the storage object for permission to access its contents; the method
    // is equivalent to IOService::open(), but with the correct parameter types.
    //

    return super::open(client, options, (void *) (uintptr_t) access);
}

IOReturn IOStorage::read(IOService *           client,
                         UInt64                byteStart,
                         IOMemoryDescriptor *  buffer,
                         IOStorageAttributes * attributes,
                         UInt64 *              actualByteCount)
{
    //
    // Read data from the storage object at the specified byte offset into the
    // specified buffer, synchronously.   When the read completes, this method
    // will return to the caller.  The actual byte count field is optional.
    //

    IOStorageCompletion	completion;
    IOStorageSyncer     syncer;

    // Fill in the completion information for this request.

    completion.target    = &syncer;
    completion.action    = storageCompletion;
    completion.parameter = actualByteCount;

    // Issue the asynchronous read.

    read(client, byteStart, buffer, attributes, &completion);

    // Wait for the read to complete.

    return syncer.wait();
}

IOReturn IOStorage::write(IOService *           client,
                          UInt64                byteStart,
                          IOMemoryDescriptor *  buffer,
                          IOStorageAttributes * attributes,
                          UInt64 *              actualByteCount)
{
    //
    // Write data into the storage object at the specified byte offset from the
    // specified buffer, synchronously.   When the write completes, this method
    // will return to the caller.  The actual byte count field is optional.
    //

    IOStorageCompletion completion;
    IOStorageSyncer     syncer;

    // Fill in the completion information for this request.

    completion.target    = &syncer;
    completion.action    = storageCompletion;
    completion.parameter = actualByteCount;

    // Issue the asynchronous write.

    write(client, byteStart, buffer, attributes, &completion);

    // Wait for the write to complete.

    return syncer.wait();
}

#if TARGET_OS_OSX
IOReturn IOStorage::discard(IOService * client,
                            UInt64      byteStart,
                            UInt64      byteCount)
{
    //
    // Delete unused data from the storage object at the specified byte offset.
    //

    return kIOReturnUnsupported;
}

IOReturn IOStorage::unmap(IOService *           client,
                          IOStorageExtent *     extents,
                          UInt32                extentsCount,
                          IOStorageUnmapOptions options)
{
    //
    // Delete unused data from the storage object at the specified byte offsets.
    //

    return kIOReturnUnsupported;
}
#endif /* TARGET_OS_OSX */

IOReturn
IOStorage::getProvisionStatus(IOService *                       client,
                             UInt64                             byteStart,
                             UInt64                             byteCount,
                             UInt32 *                           extentsCount,
                             IOStorageProvisionExtent *         extents,
                             IOStorageGetProvisionStatusOptions options)
{
    return kIOReturnUnsupported;
}

bool IOStorage::lockPhysicalExtents(IOService * client)
{
    //
    // Lock the contents of the storage object against relocation temporarily,
    // for the purpose of getting physical extents.
    //

    return false;
}

IOStorage * IOStorage::copyPhysicalExtent(IOService * client,
                                          UInt64 *    byteStart,
                                          UInt64 *    byteCount)
{
    //
    // Convert the specified byte offset into a physical byte offset, relative
    // to a physical storage object.  This call should only be made within the
    // context of lockPhysicalExtents().
    //

    return NULL;
}

void IOStorage::unlockPhysicalExtents(IOService * client)
{
    //
    // Unlock the contents of the storage object for relocation again.  This
    // call must balance a successful call to lockPhysicalExtents().
    //

    return;
}

IOReturn IOStorage::setPriority(IOService *       client,
                                IOStorageExtent * extents,
                                UInt32            extentsCount,
                                IOStoragePriority priority)

{
    //
    // Reprioritize read or write requests at the specified byte offsets.
    //

    return kIOReturnUnsupported;
}

#if TARGET_OS_OSX
IOReturn IOStorage::synchronizeCache(IOService * client)
{
    //
    // Flush the cached data in the storage object, if any.
    //

    if ( _respondsTo_synchronizeCache )
    {
        return synchronize( client, 0, 0, _kIOStorageSynchronizeOption_super__synchronizeCache );
    }
    else
    {
        return synchronize( client, 0, 0 );
    }
}

IOReturn IOStorage::synchronize(IOService *                 client,
                                UInt64                      byteStart,
                                UInt64                      byteCount,
                                IOStorageSynchronizeOptions options)
{
    //
    // Flush the cached data in the storage object, if any.
    //

    /* default the barrier synchronize to full flush */
    return synchronizeCache( client );
}
#endif /* TARGET_OS_OSX */

OSMetaClassDefineReservedUsed(IOStorage,  0);
OSMetaClassDefineReservedUsed(IOStorage,  1);
OSMetaClassDefineReservedUsed(IOStorage,  2);
OSMetaClassDefineReservedUsed(IOStorage,  3);
OSMetaClassDefineReservedUsed(IOStorage,  4);
OSMetaClassDefineReservedUsed(IOStorage,  5);
OSMetaClassDefineReservedUsed(IOStorage,  6);
OSMetaClassDefineReservedUnused(IOStorage,  7);
OSMetaClassDefineReservedUnused(IOStorage,  8);
OSMetaClassDefineReservedUnused(IOStorage,  9);
OSMetaClassDefineReservedUnused(IOStorage, 10);
OSMetaClassDefineReservedUnused(IOStorage, 11);
OSMetaClassDefineReservedUnused(IOStorage, 12);
OSMetaClassDefineReservedUnused(IOStorage, 13);
OSMetaClassDefineReservedUnused(IOStorage, 14);
OSMetaClassDefineReservedUnused(IOStorage, 15);
