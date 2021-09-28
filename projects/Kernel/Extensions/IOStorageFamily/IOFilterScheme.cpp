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

#include <IOKit/storage/IOFilterScheme.h>

#define super IOStorage
OSDefineMetaClassAndStructors(IOFilterScheme, IOStorage)

IOMedia * IOFilterScheme::getProvider() const
{
    //
    // Obtain this object's provider.  We override the superclass's method
    // to return a more specific subclass of OSObject -- an IOMedia.  This
    // method serves simply as a convenience to subclass developers.
    //

    return (IOMedia *) IOService::getProvider();
}

bool IOFilterScheme::handleOpen(IOService *  client,
                                IOOptionBits options,
                                void *       argument)
{
    //
    // The handleOpen method grants or denies permission to access this object
    // to an interested client.  The argument is an IOStorageAccess value that
    // specifies the level of access desired -- reader or reader-writer.
    //
    // This method can be invoked to upgrade or downgrade the access level for
    // an existing client as well.  The previous access level will prevail for
    // upgrades that fail, of course.   A downgrade should never fail.  If the
    // new access level should be the same as the old for a given client, this
    // method will do nothing and return success.  In all cases, one, singular
    // close-per-client is expected for all opens-per-client received.
    //
    // This implementation replaces the IOService definition of handleOpen().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we make our decision, change our state, and return from this method.
    //

    return getProvider()->open(this, options, (IOStorageAccess) (uintptr_t) argument);
}

bool IOFilterScheme::handleIsOpen(const IOService * client) const
{
    //
    // The handleIsOpen method determines whether the specified client, or any
    // client if none is specificed, presently has an open on this object.
    //
    // This implementation replaces the IOService definition of handleIsOpen().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we return from this method.
    //

    return getProvider()->isOpen(this);
}

void IOFilterScheme::handleClose(IOService * client, IOOptionBits options)
{
    //
    // The handleClose method closes the client's access to this object.
    //
    // This implementation replaces the IOService definition of handleClose().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we change our state and return from this method.
    //

    getProvider()->close(this, options);
}

void IOFilterScheme::read(IOService *           client,
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
    // For simple filter schemes, the default behavior is to simply pass the
    // read through to the provider media.  More complex filter schemes such
    // as RAID will need to do extra processing here.
    //

    getProvider( )->read( this, byteStart, buffer, attributes, completion );
}

void IOFilterScheme::write(IOService *           client,
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
    // For simple filter schemes, the default behavior is to simply pass the
    // write through to the provider media. More complex filter schemes such
    // as RAID will need to do extra processing here.
    //

    getProvider( )->write( this, byteStart, buffer, attributes, completion );
}

IOReturn IOFilterScheme::synchronize(IOService *                 client,
                                     UInt64                      byteStart,
                                     UInt64                      byteCount,
                                     IOStorageSynchronizeOptions options)
{
    //
    // Flush the cached data in the storage object, if any.
    //

#if TARGET_OS_OSX
    if ( _respondsTo_synchronizeCache )
    {
        if ( options == _kIOStorageSynchronizeOption_super__synchronizeCache )
        {
            options = 0;
        }
        else
        {
            return IOStorage::synchronize( client, byteStart, byteCount, options );
        }
    }
#endif /* TARGET_OS_OSX */

    return getProvider( )->synchronize( this, byteStart, byteCount, options );
}

IOReturn IOFilterScheme::unmap(IOService *           client,
                               IOStorageExtent *     extents,
                               UInt32                extentsCount,
                               IOStorageUnmapOptions options)
{
    //
    // Delete unused data from the storage object at the specified byte offsets.
    //

    return getProvider( )->unmap( this, extents, extentsCount, options );
}

IOReturn
IOFilterScheme::getProvisionStatus(IOService *                          client,
                                   UInt64                               byteStart,
                                   UInt64                               byteCount,
                                   UInt32 *                             extentsCount,
                                   IOStorageProvisionExtent *           extents,
                                   IOStorageGetProvisionStatusOptions   options)
{
    return getProvider( )->getProvisionStatus( this, byteStart, byteCount, extentsCount, extents, options );
}

bool IOFilterScheme::lockPhysicalExtents(IOService * client)
{
    //
    // Lock the contents of the storage object against relocation temporarily,
    // for the purpose of getting physical extents.
    //

    return getProvider( )->lockPhysicalExtents( this );
}

IOStorage * IOFilterScheme::copyPhysicalExtent(IOService * client,
                                               UInt64 *    byteStart,
                                               UInt64 *    byteCount)
{
    //
    // Convert the specified byte offset into a physical byte offset, relative
    // to a physical storage object.  This call should only be made within the
    // context of lockPhysicalExtents().
    //

    return getProvider( )->copyPhysicalExtent( this, byteStart, byteCount );
}

void IOFilterScheme::unlockPhysicalExtents(IOService * client)
{
    //
    // Unlock the contents of the storage object for relocation again.  This
    // call must balance a successful call to lockPhysicalExtents().
    //

    getProvider( )->unlockPhysicalExtents( this );
}

IOReturn IOFilterScheme::setPriority(IOService *       client,
                                     IOStorageExtent * extents,
                                     UInt32            extentsCount,
                                     IOStoragePriority priority)
{
    //
    // Reprioritize read or write requests at the specified byte offsets.
    //

    return getProvider( )->setPriority( this, extents, extentsCount, priority );
}

OSMetaClassDefineReservedUnused(IOFilterScheme,  0);
OSMetaClassDefineReservedUnused(IOFilterScheme,  1);
OSMetaClassDefineReservedUnused(IOFilterScheme,  2);
OSMetaClassDefineReservedUnused(IOFilterScheme,  3);
OSMetaClassDefineReservedUnused(IOFilterScheme,  4);
OSMetaClassDefineReservedUnused(IOFilterScheme,  5);
OSMetaClassDefineReservedUnused(IOFilterScheme,  6);
OSMetaClassDefineReservedUnused(IOFilterScheme,  7);
OSMetaClassDefineReservedUnused(IOFilterScheme,  8);
OSMetaClassDefineReservedUnused(IOFilterScheme,  9);
OSMetaClassDefineReservedUnused(IOFilterScheme, 10);
OSMetaClassDefineReservedUnused(IOFilterScheme, 11);
OSMetaClassDefineReservedUnused(IOFilterScheme, 12);
OSMetaClassDefineReservedUnused(IOFilterScheme, 13);
OSMetaClassDefineReservedUnused(IOFilterScheme, 14);
OSMetaClassDefineReservedUnused(IOFilterScheme, 15);
OSMetaClassDefineReservedUnused(IOFilterScheme, 16);
OSMetaClassDefineReservedUnused(IOFilterScheme, 17);
OSMetaClassDefineReservedUnused(IOFilterScheme, 18);
OSMetaClassDefineReservedUnused(IOFilterScheme, 19);
OSMetaClassDefineReservedUnused(IOFilterScheme, 20);
OSMetaClassDefineReservedUnused(IOFilterScheme, 21);
OSMetaClassDefineReservedUnused(IOFilterScheme, 22);
OSMetaClassDefineReservedUnused(IOFilterScheme, 23);
OSMetaClassDefineReservedUnused(IOFilterScheme, 24);
OSMetaClassDefineReservedUnused(IOFilterScheme, 25);
OSMetaClassDefineReservedUnused(IOFilterScheme, 26);
OSMetaClassDefineReservedUnused(IOFilterScheme, 27);
OSMetaClassDefineReservedUnused(IOFilterScheme, 28);
OSMetaClassDefineReservedUnused(IOFilterScheme, 29);
OSMetaClassDefineReservedUnused(IOFilterScheme, 30);
OSMetaClassDefineReservedUnused(IOFilterScheme, 31);

#if TARGET_OS_OSX
extern "C" void _ZN14IOFilterScheme16synchronizeCacheEP9IOService( IOFilterScheme * scheme, IOService * client )
{
    scheme->synchronize( client, 0, 0 );
}
#endif /* TARGET_OS_OSX */
