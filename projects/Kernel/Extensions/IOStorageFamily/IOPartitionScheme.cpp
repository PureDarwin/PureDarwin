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

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMessage.h>
#include <IOKit/storage/IOPartitionScheme.h>

#define super IOStorage
OSDefineMetaClassAndStructors(IOPartitionScheme, IOStorage)

static SInt32 partitionComparison( const OSMetaClassBase * object1,
                                   const OSMetaClassBase * object2,
                                   void *                  context )
{
    //
    // Internal comparison routine for sorting partitions in an ordered set.
    //

    UInt64 base1 = ( ( IOMedia * ) object1 )->getBase( );
    UInt64 base2 = ( ( IOMedia * ) object2 )->getBase( );

    if ( base1 < base2 ) return  1;
    if ( base1 > base2 ) return -1;

    return 0;
}

IOMedia * IOPartitionScheme::getProvider() const
{
    //
    // Obtain this object's provider.  We override the superclass's method
    // to return a more specific subclass of OSObject -- an IOMedia.  This
    // method serves simply as a convenience to subclass developers.
    //

    return (IOMedia *) IOService::getProvider();
}

bool IOPartitionScheme::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    if (super::init(properties) == false)  return false;

    _openLevel         = kIOStorageAccessNone;
    _openReaders       = OSSet::withCapacity(16);
    _openReaderWriters = OSSet::withCapacity(16);

    if (_openReaders == 0 || _openReaderWriters == 0) return false;

    return true;
}

void IOPartitionScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if (_openReaders)        _openReaders->release();
    if (_openReaderWriters)  _openReaderWriters->release();

    super::free();
}

bool IOPartitionScheme::handleOpen(IOService *  client,
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

    IOStorageAccess access  = (IOStorageAccess) (uintptr_t) argument;
    IOStorageAccess level;

    assert(client);
    assert( access == kIOStorageAccessReader       ||
            access == kIOStorageAccessReaderWriter );

    //
    // A partition scheme multiplexes the opens it receives from several clients
    // and sends one open to the level below that satisfies the highest level of
    // access.
    //

    unsigned writers = _openReaderWriters->getCount();

    if (_openReaderWriters->containsObject(client))  writers--;
    if (access == kIOStorageAccessReaderWriter)  writers++;

    level = (writers) ? kIOStorageAccessReaderWriter : kIOStorageAccessReader;

    //
    // Determine whether the levels below us accept this open or not (we avoid
    // the open if the required access is the access we already hold).
    //

    if ( _openLevel != level )
    {
        IOStorage * provider;

        provider = OSDynamicCast( IOStorage, getProvider( ) );

        if ( provider )
        {
            bool success;

            level = ( level | kIOStorageAccessSharedLock );

            success = provider->open( this, options, level );

            level = ( level & kIOStorageAccessReaderWriter );

            if ( success == false )
            {
                return false;
            }
        }
    }

    //
    // Process the open.
    //

    if (access == kIOStorageAccessReader)
    {
        _openReaders->setObject(client);

        _openReaderWriters->removeObject(client);           // (for a downgrade)
    }
    else // (access == kIOStorageAccessReaderWriter)
    {
        _openReaderWriters->setObject(client);

        _openReaders->removeObject(client);                  // (for an upgrade)
    }

    _openLevel = level;

    return true;
}

bool IOPartitionScheme::handleIsOpen(const IOService * client) const
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

    if (client == 0)  return (_openLevel != kIOStorageAccessNone);

    return ( _openReaderWriters->containsObject(client) ||
             _openReaders->containsObject(client)       );
}

void IOPartitionScheme::handleClose(IOService * client, IOOptionBits options)
{
    //
    // The handleClose method closes the client's access to this object.
    //
    // This implementation replaces the IOService definition of handleClose().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we change our state and return from this method.
    //

    assert(client);

    //
    // Process the close.
    //

    if (_openReaderWriters->containsObject(client))  // (is it a reader-writer?)
    {
        _openReaderWriters->removeObject(client);
    }
    else if (_openReaders->containsObject(client))  // (is the client a reader?)
    {
        _openReaders->removeObject(client);
    }
    else                                      // (is the client is an imposter?)
    {
        assert(0);
        return;
    }

    //
    // Reevaluate the open we have on the level below us.  If no opens remain,
    // we close, or if no reader-writer remains, but readers do, we downgrade.
    //

    IOStorageAccess level;

    if (_openReaderWriters->getCount())  level = kIOStorageAccessReaderWriter;
    else if (_openReaders->getCount())   level = kIOStorageAccessReader;
    else                                 level = kIOStorageAccessNone;

    if ( _openLevel != level )
    {
        IOStorage * provider;

        provider = OSDynamicCast( IOStorage, getProvider( ) );

        if ( provider )
        {
            if ( level == kIOStorageAccessNone )
            {
                provider->close( this, options );
            }
            else
            {
                bool success;

                level = ( level | kIOStorageAccessSharedLock );

                success = provider->open( this, 0, level );

                level = ( level & kIOStorageAccessReaderWriter );

                assert( success );
            }
         }

         _openLevel = level;
    }
}

void IOPartitionScheme::read(IOService *           client,
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
    // For simple partition schemes, the default behavior is to simply pass the
    // read through to the provider media.  More complex partition schemes such
    // as RAID will need to do extra processing here.
    //

    getProvider( )->read( this, byteStart, buffer, attributes, completion );
}

void IOPartitionScheme::write(IOService *           client,
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
    // For simple partition schemes, the default behavior is to simply pass the
    // write through to the provider media. More complex partition schemes such
    // as RAID will need to do extra processing here.
    //

    getProvider( )->write( this, byteStart, buffer, attributes, completion );
}

IOReturn IOPartitionScheme::synchronize(IOService *                 client,
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

IOReturn IOPartitionScheme::unmap(IOService *           client,
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
IOPartitionScheme::getProvisionStatus(IOService *                           client,
                                      UInt64                                byteStart,
                                      UInt64                                byteCount,
                                      UInt32 *                              extentsCount,
                                      IOStorageProvisionExtent *            extents,
                                      IOStorageGetProvisionStatusOptions    options)
{
    return getProvider( )->getProvisionStatus( this, byteStart, byteCount, extentsCount, extents, options );
}

bool IOPartitionScheme::lockPhysicalExtents(IOService * client)
{
    //
    // Lock the contents of the storage object against relocation temporarily,
    // for the purpose of getting physical extents.
    //

    return getProvider( )->lockPhysicalExtents( this );
}

IOStorage * IOPartitionScheme::copyPhysicalExtent(IOService * client,
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

void IOPartitionScheme::unlockPhysicalExtents(IOService * client)
{
    //
    // Unlock the contents of the storage object for relocation again.  This
    // call must balance a successful call to lockPhysicalExtents().
    //

    getProvider( )->unlockPhysicalExtents( this );
}

IOReturn IOPartitionScheme::setPriority(IOService *       client,
                                        IOStorageExtent * extents,
                                        UInt32            extentsCount,
                                        IOStoragePriority priority)
{
    //
    // Reprioritize read or write requests at the specified byte offsets.
    //

    return getProvider( )->setPriority( this, extents, extentsCount, priority );
}

bool IOPartitionScheme::attachMediaObjectToDeviceTree(IOMedia * media)
{
    //
    // Attach the given media object to the device tree plane.
    //

    IORegistryEntry * parent = this;

    while ( (parent = parent->getParentEntry(gIOServicePlane)) )
    {
        if ( parent->inPlane(gIODTPlane) )
        {
            char         location[ 32 ];
            const char * locationOfParent = parent->getLocation(gIODTPlane);
            const char * nameOfParent     = parent->getName(gIODTPlane);

            if ( locationOfParent == 0 )  break;

            if ( OSDynamicCast(IOMedia, parent) == 0 )  break;

            parent = parent->getParentEntry(gIODTPlane);

            if ( parent == 0 )  break;

            if ( media->attachToParent(parent, gIODTPlane) == false )  break;

            strlcpy(location, locationOfParent, sizeof(location));
            if ( strchr(location, ':') )  *(strchr(location, ':') + 1) = 0;
            strlcat(location, media->getLocation(), sizeof(location) - strlen(location));
            media->setLocation(location, gIODTPlane);
            media->setName(nameOfParent, gIODTPlane);

            return true;
        }
    }

    return false;
}

void IOPartitionScheme::detachMediaObjectFromDeviceTree(IOMedia * media)
{
    //
    // Detach the given media object from the device tree plane.
    //

    IORegistryEntry * parent;

    if ( (parent = media->getParentEntry(gIODTPlane)) )
    {
        media->detachFromParent(parent, gIODTPlane);
    }
}

OSSet * IOPartitionScheme::juxtaposeMediaObjects(OSSet * partitionsOld,
                                                 OSSet * partitionsNew)
{
    //
    // Updates a set of existing partitions, represented by partitionsOld,
    // with possible updates from a rescan of the disk, represented by
    // partitionsNew.  It returns a new set of partitions with the results,
    // removing partitions from partitionsOld where applicable, adding
    // partitions from partitionsNew where applicable, and folding in property
    // changes to partitions from partitionsNew into partitionsOld where
    // applicable.
    //

    OSIterator *   iterator    = 0;
    OSIterator *   iterator1   = 0;
    OSIterator *   iterator2   = 0;
    OSSymbol *     key;
    OSSet *        keys        = 0;
    IOMedia *      partition;
    IOMedia *      partition1;
    IOMedia *      partition2;
    OSSet *        partitions  = 0;
    OSOrderedSet * partitions1 = 0;
    OSOrderedSet * partitions2 = 0;
    UInt32         partitionID = 0;
    OSDictionary * properties;

    // Allocate a set to hold the set of media objects representing partitions.

    partitions = OSSet::withCapacity( partitionsNew->getCapacity( ) );
    if ( partitions == 0 ) goto juxtaposeErr;

    // Prepare the reference set of partitions.

    partitions1 = OSOrderedSet::withCapacity( partitionsOld->getCapacity( ), partitionComparison, 0 );
    if ( partitions1 == 0 ) goto juxtaposeErr;

    iterator1 = OSCollectionIterator::withCollection( partitionsOld );
    if ( iterator1 == 0 ) goto juxtaposeErr;

    while ( ( partition1 = ( IOMedia * ) iterator1->getNextObject( ) ) )
    {
        partitionID = max( partitionID, ( UInt32 ) strtoul( partition1->getLocation( ), NULL, 10 ) );

        partitions1->setObject( partition1 );
    }

    iterator1->release( );
    iterator1 = 0;

    // Prepare the comparison set of partitions.

    partitions2 = OSOrderedSet::withCapacity( partitionsNew->getCapacity( ), partitionComparison, 0 );
    if ( partitions2 == 0 ) goto juxtaposeErr;

    iterator2 = OSCollectionIterator::withCollection( partitionsNew );
    if ( iterator2 == 0 ) goto juxtaposeErr;

    while ( ( partition2 = ( IOMedia * ) iterator2->getNextObject( ) ) )
    {
        partitionID = max( partitionID, ( UInt32 ) strtoul( partition2->getLocation( ), NULL, 10 ) );

        partitions2->setObject( partition2 );
    }

    iterator2->release( );
    iterator2 = 0;

    // Juxtapose the partitions.

    iterator1 = OSCollectionIterator::withCollection( partitions1 );
    if ( iterator1 == 0 ) goto juxtaposeErr;

    iterator2 = OSCollectionIterator::withCollection( partitions2 );
    if ( iterator2 == 0 ) goto juxtaposeErr;

    partition1 = ( IOMedia * ) iterator1->getNextObject( );
    partition2 = ( IOMedia * ) iterator2->getNextObject( );

    while ( partition1 || partition2 )
    {
        UInt64 base1;
        UInt64 base2;

        base1 = partition1 ? partition1->getBase( ) : UINT64_MAX;
        base2 = partition2 ? partition2->getBase( ) : UINT64_MAX;

        if ( base1 > base2 )
        {
            // A partition was added.

            partition2->setProperty( kIOMediaLiveKey, true );

            iterator = OSCollectionIterator::withCollection( partitions1 );
            if ( iterator == 0 ) goto juxtaposeErr;

            while ( ( partition = ( IOMedia * ) iterator->getNextObject( ) ) )
            {
                if ( strcmp( partition->getLocation( ), partition2->getLocation( ) ) == 0 )
                {
                    // Set a location value for this partition.

                    char location[ 12 ];

                    partitionID++;

                    snprintf( location, sizeof( location ), "%d", ( int ) partitionID );

                    partition2->setLocation( location );

                    partition2->setProperty( kIOMediaLiveKey, false );

                    break;
                }
            }

            iterator->release( );
            iterator = 0;

            if ( partition2->attach( this ) )
            {
                attachMediaObjectToDeviceTree( partition2 );

                partition2->registerService( kIOServiceAsynchronous );
            }

            partitions->setObject( partition2 );

            partition2 = ( IOMedia * ) iterator2->getNextObject( );
        }
        else if ( base1 < base2 )
        {
            // A partition was removed.

            partition1->setProperty( kIOMediaLiveKey, false );

            if ( handleIsOpen( partition1 ) == false )
            {
                partition1->terminate( );

                detachMediaObjectFromDeviceTree( partition1 );
            }
            else
            {
                partition1->removeProperty( kIOMediaPartitionIDKey );

                partitions->setObject( partition1 );
            }

            partition1 = ( IOMedia * ) iterator1->getNextObject( );
        }
        else
        {
            // A partition was matched.

            bool edit;
            bool move;

            edit = false;
            move = false;

            keys = OSSet::withCapacity( 1 );
            if ( keys == 0 ) goto juxtaposeErr;

            properties = partition2->getPropertyTable( );

            // Determine which properties were updated.

            if ( partition1->getBase( )               != partition2->getBase( )               ||
                 partition1->getSize( )               != partition2->getSize( )               ||
                 partition1->getPreferredBlockSize( ) != partition2->getPreferredBlockSize( ) ||
                 partition1->getAttributes( )         != partition2->getAttributes( )         ||
                 partition1->isWhole( )               != partition2->isWhole( )               ||
                 partition1->isWritable( )            != partition2->isWritable( )            ||
                 strcmp( partition1->getContentHint( ), partition2->getContentHint( ) )       )
            {
                edit = true;
            }

            if ( strcmp( partition1->getName( ),     partition2->getName( )     ) ||
                 strcmp( partition1->getLocation( ), partition2->getLocation( ) ) )
            {
                move = true;
            }

            iterator = OSCollectionIterator::withCollection( properties );
            if ( iterator == 0 ) goto juxtaposeErr;

            while ( ( key = ( OSSymbol * ) iterator->getNextObject( ) ) )
            {
                OSObject * value1;
                OSObject * value2;

                if ( key->isEqualTo( kIOMediaContentHintKey        ) ||
                     key->isEqualTo( kIOMediaEjectableKey          ) ||
                     key->isEqualTo( kIOMediaPreferredBlockSizeKey ) ||
                     key->isEqualTo( kIOMediaRemovableKey          ) ||
                     key->isEqualTo( kIOMediaSizeKey               ) ||
                     key->isEqualTo( kIOMediaWholeKey              ) ||
                     key->isEqualTo( kIOMediaWritableKey           ) )
                {
                    continue;
                }

                if ( key->isEqualTo( kIOMediaContentKey ) ||
                     key->isEqualTo( kIOMediaLeafKey    ) ||
                     key->isEqualTo( kIOMediaLiveKey    ) ||
                     key->isEqualTo( kIOMediaOpenKey    ) )
                {
                    continue;
                }

                value1 = partition1->getProperty( key );
                value2 = partition2->getProperty( key );

                if ( value1 == 0 || value1->isEqualTo( value2 ) == false )
                {
                    keys->setObject( key );
                }
            }

            iterator->release( );
            iterator = 0;

            // A partition was updated.

            partition1->setProperty( kIOMediaLiveKey, ( move == false ) );

            if ( edit )
            {
                partition1->init( partition2->getBase( ),
                                  partition2->getSize( ),
                                  partition2->getPreferredBlockSize( ),
                                  partition2->getAttributes( ),
                                  partition2->isWhole( ),
                                  partition2->isWritable( ),
                                  partition2->getContentHint( ) );
            }

            if ( keys->getCount( ) )
            {
                iterator = OSCollectionIterator::withCollection( keys );
                if ( iterator == 0 ) goto juxtaposeErr;

                while ( ( key = ( OSSymbol * ) iterator->getNextObject( ) ) )
                {
                    partition1->setProperty( key, partition2->getProperty( key ) );
                }

                iterator->release( );
                iterator = 0;
            }

            if ( edit || keys->getCount( ) )
            {
                partition1->messageClients( kIOMessageServicePropertyChange );

                partition1->registerService( kIOServiceAsynchronous );
            }

            keys->release( );
            keys = 0;

            partitions->setObject( partition1 );

            partition1 = ( IOMedia * ) iterator1->getNextObject( );
            partition2 = ( IOMedia * ) iterator2->getNextObject( );
        }
    }

    // Release our resources.

    iterator1->release( );
    iterator2->release( );
    partitions1->release( );
    partitions2->release( );

    return partitions;

juxtaposeErr:

    // Release our resources.

    if ( iterator    ) iterator->release( );
    if ( iterator1   ) iterator1->release( );
    if ( iterator2   ) iterator2->release( );
    if ( keys        ) keys->release( );
    if ( partitions  ) partitions->release( );
    if ( partitions1 ) partitions1->release( );
    if ( partitions2 ) partitions2->release( );

    return 0;
}

OSMetaClassDefineReservedUnused(IOPartitionScheme,  0);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  1);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  2);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  3);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  4);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  5);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  6);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  7);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  8);
OSMetaClassDefineReservedUnused(IOPartitionScheme,  9);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 10);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 11);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 12);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 13);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 14);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 15);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 16);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 17);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 18);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 19);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 20);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 21);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 22);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 23);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 24);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 25);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 26);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 27);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 28);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 29);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 30);
OSMetaClassDefineReservedUnused(IOPartitionScheme, 31);

#if TARGET_OS_OSX
extern "C" void _ZN17IOPartitionScheme16synchronizeCacheEP9IOService( IOPartitionScheme * scheme, IOService * client )
{
    scheme->synchronize( client, 0, 0 );
}
#endif /* TARGET_OS_OSX */
