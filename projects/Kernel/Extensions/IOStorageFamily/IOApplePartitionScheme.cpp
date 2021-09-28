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

#include <IOKit/assert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOApplePartitionScheme.h>
#include <libkern/OSByteOrder.h>

#define super IOPartitionScheme
OSDefineMetaClassAndStructors(IOApplePartitionScheme, IOPartitionScheme);

//
// Notes
//
// o the on-disk structure's fields are big-endian formatted
// o the dpme_pblock_start and dpme_pblocks block values are:
//   o for media without a driver map:
//     o natural block size based
//   o for media with a driver map:
//     o driver map block size based, unless the driver map block size is 2048
//       and a valid partition entry exists at a 512 byte offset into the disk,
//       in which case, assume a 512 byte block size, except for the partition
//       entries that lie on a 2048 byte multiple and are one of the following
//       types: Apple_Patches, Apple_Driver, Apple_Driver43, Apple_Driver43_CD,
//       Apple_Driver_ATA, Apple_Driver_ATAPI; in which case, we assume a 2048
//       byte block size (for the one partition)
// o the dpme_pblock_start block value is relative to the media container
//

bool IOApplePartitionScheme::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    // State our assumptions.

    assert(sizeof(dpme)   == 512);                  // (compiler/platform check)
    assert(sizeof(DDMap)  ==   8);                  // (compiler/platform check)
    assert(sizeof(Block0) == 512);                  // (compiler/platform check)

    // Ask our superclass' opinion.

    if (super::init(properties) == false)  return false;

    // Initialize our state.

    _partitions = 0;

    return true;
}

void IOApplePartitionScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if ( _partitions )  _partitions->release();

    super::free();
}

IOService * IOApplePartitionScheme::probe(IOService * provider, SInt32 * score)
{
    //
    // Determine whether the provider media contains an Apple partition map.
    //

    // State our assumptions.

    assert(OSDynamicCast(IOMedia, provider));

    // Ask superclass' opinion.

    if (super::probe(provider, score) == 0)  return 0;

    // Scan the provider media for an Apple partition map.

    _partitions = scan(score);

    return ( _partitions ) ? this : 0;
}

bool IOApplePartitionScheme::start(IOService * provider)
{
    //
    // Publish the new media objects which represent our partitions.
    //

    IOMedia *    partition;
    OSIterator * partitionIterator;

    // State our assumptions.

    assert(_partitions);

    // Ask our superclass' opinion.

    if ( super::start(provider) == false )  return false;

    // Attach and register the new media objects representing our partitions.

    partitionIterator = OSCollectionIterator::withCollection(_partitions);
    if ( partitionIterator == 0 )  return false;

    while ( (partition = (IOMedia *) partitionIterator->getNextObject()) )
    {
        if ( partition->attach(this) )
        {
            attachMediaObjectToDeviceTree(partition);

            partition->registerService();
        }
    }

    partitionIterator->release();

    // set partition scheme to be valid
    _partitionSchemeState |= kIOPartitionScheme_partition_valid;

    return true;
}

void IOApplePartitionScheme::stop(IOService * provider)
{
    //
    // Clean up after the media objects we published before terminating.
    //

    IOMedia *    partition;
    OSIterator * partitionIterator;

    // State our assumptions.

    assert(_partitions);

    // Detach the media objects we previously attached to the device tree.

    partitionIterator = OSCollectionIterator::withCollection(_partitions);

    if ( partitionIterator )
    {
        while ( (partition = (IOMedia *) partitionIterator->getNextObject()) )
        {
            detachMediaObjectFromDeviceTree(partition);
        }

        partitionIterator->release();
    }

    super::stop(provider);
}

IOReturn IOApplePartitionScheme::requestProbe(IOOptionBits options)
{
    //
    // Request that the provider media be re-scanned for partitions.
    //

    OSSet * partitions    = 0;
    OSSet * partitionsNew;
    SInt32  score         = 0;

    // Scan the provider media for partitions.
    if ( ( _partitionSchemeState & kIOPartitionScheme_partition_valid ) == 0 )
    {
        return kIOReturnError;
    }

    partitionsNew = scan( &score );

    if ( partitionsNew )
    {
        if ( lockForArbitration( false ) )
        {
            partitions = juxtaposeMediaObjects( _partitions, partitionsNew );

            if ( partitions )
            {
                _partitions->release( );

                _partitions = partitions;
            }

            unlockForArbitration( );
        }

        partitionsNew->release( );
    }

    return partitions ? kIOReturnSuccess : kIOReturnError;
}

OSSet * IOApplePartitionScheme::scan(SInt32 * score)
{
    //
    // Scan the provider media for an Apple partition map.  Returns the set
    // of media objects representing each of the partitions (the retain for
    // the set is passed to the caller), or null should no partition map be
    // found.  The default probe score can be adjusted up or down, based on
    // the confidence of the scan.
    //

    IOBufferMemoryDescriptor * buffer         = 0;
    UInt32                     bufferReadAt   = 0;
    IOByteCount                bufferSize     = 0;
    UInt32                     dpmeBlockSize  = 0;
    UInt32                     dpmeCount      = 0;
    UInt32                     dpmeID         = 0;
    dpme *                     dpmeMap        = 0;
    UInt32                     dpmeMaxCount   = 0;
    bool                       dpmeOldSchool  = false;
    Block0 *                   driverMap      = 0;
    IOMedia *                  media          = getProvider();
    UInt64                     mediaBlockSize = media->getPreferredBlockSize();
    bool                       mediaIsOpen    = false;
    OSSet *                    partitions     = 0;
    IOReturn                   status         = kIOReturnError;

    // Determine whether this media is formatted.

    if ( media->isFormatted() == false )  goto scanErr;

    // Determine whether this media has an appropriate block size.

    if ( (mediaBlockSize % sizeof(dpme)) )  goto scanErr;

    // Allocate a buffer large enough to hold one map, rounded to a media block.

    bufferSize = IORound(max(sizeof(Block0), sizeof(dpme)), mediaBlockSize);
    buffer     = IOBufferMemoryDescriptor::withCapacity(
                                           /* capacity      */ bufferSize,
                                           /* withDirection */ kIODirectionIn );
    if ( buffer == 0 )  goto scanErr;

    // Allocate a set to hold the set of media objects representing partitions.

    partitions = OSSet::withCapacity(8);
    if ( partitions == 0 )  goto scanErr;

    // Open the media with read access.

    mediaIsOpen = open(this, 0, kIOStorageAccessReader);
    if ( mediaIsOpen == false )  goto scanErr;

    // Read the driver map into our buffer.

    bufferReadAt = 0;

    status = media->read(this, bufferReadAt, buffer);
    if ( status != kIOReturnSuccess )  goto scanErr;

    driverMap = (Block0 *) buffer->getBytesNoCopy();

    // Determine the official block size to use to scan the partition entries.

    dpmeBlockSize = (UInt32) mediaBlockSize;             // (natural block size)

    if ( OSSwapBigToHostInt16(driverMap->sbSig) == BLOCK0_SIGNATURE )
    {
        dpmeBlockSize = OSSwapBigToHostInt16(driverMap->sbBlkSize);

        // Increase the probe score when a driver map is detected, since we are
        // more confident in the match when it is present.  This will eliminate
        // conflicts with FDisk when it shares the same block as the driver map.

        *score += 2000;
    }

    // Determine whether we have an old school partition map, where there is
    // a partition entry at a 512 byte offset into the disk, even though the
    // driver map block size is 2048.

    if ( dpmeBlockSize == 2048 )
    {
        if ( bufferSize >= sizeof(Block0) + sizeof(dpme) )       // (in buffer?)
        {
            dpmeMap = (dpme *) (driverMap + 1);
        }
        else                                                  // (not in buffer)
        {
            // Read the partition entry at byte offset 512 into our buffer.

            bufferReadAt = sizeof(dpme);

            status = media->read(this, bufferReadAt, buffer);
            if ( status != kIOReturnSuccess )  goto scanErr;

            dpmeMap = (dpme *) buffer->getBytesNoCopy();
        }

        // Determine whether the partition entry signature is present.

        if ( OSSwapBigToHostInt16(dpmeMap->dpme_signature) == DPME_SIGNATURE )
        {
            dpmeBlockSize = sizeof(dpme);             // (old school block size)
            dpmeOldSchool = true;
        }
    }

    // Scan the media for Apple partition entries.

    for ( dpmeID = 1, dpmeCount = 1; dpmeID <= dpmeCount; dpmeID++ ) 
    {
        UInt32 partitionBlockSize = dpmeBlockSize;

        // Determine whether we've exhausted the current buffer of entries.

        if ( dpmeID * dpmeBlockSize + sizeof(dpme) > bufferReadAt + bufferSize )
        {
            // Read the next partition entry into our buffer.

            bufferReadAt = dpmeID * dpmeBlockSize;

            status = media->read(this, bufferReadAt, buffer);
            if ( status != kIOReturnSuccess )  goto scanErr;
        }

        dpmeMap = (dpme *) ( ((UInt8 *) buffer->getBytesNoCopy()) +
                             (dpmeID * dpmeBlockSize) - bufferReadAt );

        // Determine whether the partition entry signature is present.

        if ( OSSwapBigToHostInt16(dpmeMap->dpme_signature) != DPME_SIGNATURE )
        {
            goto scanErr;
        }

        // Obtain an accurate number of entries in the partition map.

        if ( !strncmp(dpmeMap->dpme_type, "Apple_partition_map", sizeof(dpmeMap->dpme_type)) ||
             !strncmp(dpmeMap->dpme_type, "Apple_Partition_Map", sizeof(dpmeMap->dpme_type)) ||
             !strncmp(dpmeMap->dpme_type, "Apple_patition_map",  sizeof(dpmeMap->dpme_type)) )
        {
            dpmeCount    = OSSwapBigToHostInt32(dpmeMap->dpme_map_entries);
            dpmeMaxCount = OSSwapBigToHostInt32(dpmeMap->dpme_pblocks);
        }
        else if ( dpmeCount == 1 )
        {
            dpmeCount = OSSwapBigToHostInt32(dpmeMap->dpme_map_entries);
        }

        // Obtain an accurate block size for an old school partition map.

        if ( dpmeOldSchool && (dpmeID % 4) == 0 )
        {
            if ( !strncmp(dpmeMap->dpme_type, "Apple_Driver",       sizeof(dpmeMap->dpme_type)) ||
                 !strncmp(dpmeMap->dpme_type, "Apple_Driver43",     sizeof(dpmeMap->dpme_type)) ||
                 !strncmp(dpmeMap->dpme_type, "Apple_Driver43_CD",  sizeof(dpmeMap->dpme_type)) ||
                 !strncmp(dpmeMap->dpme_type, "Apple_Driver_ATA",   sizeof(dpmeMap->dpme_type)) ||
                 !strncmp(dpmeMap->dpme_type, "Apple_Driver_ATAPI", sizeof(dpmeMap->dpme_type)) ||
                 !strncmp(dpmeMap->dpme_type, "Apple_Patches",      sizeof(dpmeMap->dpme_type)) )
            {
                partitionBlockSize = 2048;
            }
        }

        // Determine whether the partition is corrupt (fatal).

        if ( isPartitionCorrupt(
                                 /* partition          */ dpmeMap,
                                 /* partitionID        */ dpmeID,
                                 /* partitionBlockSize */ partitionBlockSize ) )
        {
            goto scanErr;
        }

        // Determine whether the partition is invalid (skipped).

        if ( isPartitionInvalid(
                                 /* partition          */ dpmeMap,
                                 /* partitionID        */ dpmeID,
                                 /* partitionBlockSize */ partitionBlockSize ) )
        {
            continue;
        }

        // Create a media object to represent this partition.

        IOMedia * newMedia = instantiateMediaObject(
                                 /* partition          */ dpmeMap,
                                 /* partitionID        */ dpmeID,
                                 /* partitionBlockSize */ partitionBlockSize );

        if ( newMedia )
        {
            partitions->setObject(newMedia);
            newMedia->release();
        }
    }

    // Determine whether we ever came accross an Apple_partition_map partition.

    if ( dpmeMaxCount == 0 )  goto scanErr;

    // Release our resources.

    close(this);
    buffer->release();

    return partitions;

scanErr:

    // Release our resources.

    if ( mediaIsOpen )  close(this);
    if ( partitions )  partitions->release();
    if ( buffer )  buffer->release();

    return 0;
}

bool IOApplePartitionScheme::isPartitionCorrupt( dpme * partition,
                                                 UInt32 partitionID,
                                                 UInt32 partitionBlockSize )
{
    //
    // Ask whether the given partition appears to be corrupt.  A partition that
    // is corrupt will cause the failure of the Apple partition map recognition
    // altogether.
    //

    if ( !strncmp(partition->dpme_type, "CD_ROM_Mode_1", sizeof(partition->dpme_type)) )  return true;

    return false;
}

bool IOApplePartitionScheme::isPartitionInvalid( dpme * partition,
                                                 UInt32 partitionID,
                                                 UInt32 partitionBlockSize )
{
    //
    // Ask whether the given partition appears to be invalid.  A partition that
    // is invalid will cause it to be skipped in the scan, but will not cause a
    // failure of the Apple partition map recognition.
    //

    IOMedia * media         = getProvider();
    UInt64    partitionBase = 0;
    UInt64    partitionSize = 0;

    // Compute the relative byte position and size of the new partition.

    partitionBase  = OSSwapBigToHostInt32(partition->dpme_pblock_start);
    partitionSize  = OSSwapBigToHostInt32(partition->dpme_pblocks);
    partitionBase *= partitionBlockSize;
    partitionSize *= partitionBlockSize;

    // Determine whether the partition is a placeholder.

    if ( partitionSize == 0 )  return true;

    // Determine whether the partition starts at (or past) the end-of-media.

    if ( partitionBase >= media->getSize() )  return true;

    return false;
}

IOMedia * IOApplePartitionScheme::instantiateMediaObject(
                                                     dpme * partition,
                                                     UInt32 partitionID,
                                                     UInt32 partitionBlockSize )
{
    //
    // Instantiate a new media object to represent the given partition.
    //

    IOMedia * media               = getProvider();
    UInt64    mediaBlockSize      = media->getPreferredBlockSize();
    UInt64    partitionBase       = 0;
    char      partitionHint[DPISTRLEN + 1];
    bool      partitionIsWritable = media->isWritable();
    char      partitionName[DPISTRLEN + 1];
    UInt64    partitionSize       = 0;

    strncpy(partitionHint, partition->dpme_type, DPISTRLEN);
    strncpy(partitionName, partition->dpme_name, DPISTRLEN);

    partitionHint[DPISTRLEN] = 0;
    partitionName[DPISTRLEN] = 0;

    // Compute the relative byte position and size of the new partition.

    partitionBase  = OSSwapBigToHostInt32(partition->dpme_pblock_start);
    partitionSize  = OSSwapBigToHostInt32(partition->dpme_pblocks);
    partitionBase *= partitionBlockSize;
    partitionSize *= partitionBlockSize;

    // Clip the size of the new partition if it extends past the end-of-media.

    if ( partitionBase + partitionSize > media->getSize() )
    {
        partitionSize = media->getSize() - partitionBase;
    }

    // Determine whether the new partition type is Apple_Free, which we choose
    // not to publish because it is an internal concept to the partition map.

    if ( !strncmp(partition->dpme_type, "Apple_Free", sizeof(partition->dpme_type)) )  return 0;

    // Determine whether the new partition is read-only.
    //
    // Note that we treat the misspelt Apple_patition_map entries as equivalent
    // to Apple_partition_map entries due to the messed up CDs noted in 2513960.

    if ( !strncmp(partition->dpme_type, "Apple_partition_map", sizeof(partition->dpme_type)) ||
         !strncmp(partition->dpme_type, "Apple_Partition_Map", sizeof(partition->dpme_type)) ||
         !strncmp(partition->dpme_type, "Apple_patition_map",  sizeof(partition->dpme_type)) ||
         ( OSSwapBigToHostInt32(partition->dpme_flags) &
           ( DPME_FLAGS_WRITABLE | DPME_FLAGS_VALID )  ) == DPME_FLAGS_VALID )
    {
        partitionIsWritable = false;
    }

    // Create the new media object.

    IOMedia * newMedia = instantiateDesiredMediaObject(
                                 /* partition          */ partition,
                                 /* partitionID        */ partitionID,
                                 /* partitionBlockSize */ partitionBlockSize );

    if ( newMedia )
    {
        if ( newMedia->init(
                /* base               */ partitionBase,
                /* size               */ partitionSize,
                /* preferredBlockSize */ mediaBlockSize,
                /* attributes         */ media->getAttributes(),
                /* isWhole            */ false,
                /* isWritable         */ partitionIsWritable,
                /* contentHint        */ partitionHint ) )
        {
            // Set a name for this partition.

            char name[24];
            snprintf(name, sizeof(name), "Untitled %d", (int) partitionID);
            newMedia->setName(partitionName[0] ? partitionName : name);

            // Set a location value (the partition number) for this partition.

            char location[12];
            snprintf(location, sizeof(location), "%d", (int) partitionID);
            newMedia->setLocation(location);

            // Set the "Base" key for this partition.

            newMedia->setProperty(kIOMediaBaseKey, partitionBase, 64);

            // Set the "Partition ID" key for this partition.

            newMedia->setProperty(kIOMediaPartitionIDKey, partitionID, 32);
        }
        else
        {
            newMedia->release();
            newMedia = 0;
        }
    }

    return newMedia;
}

IOMedia * IOApplePartitionScheme::instantiateDesiredMediaObject(
                                                     dpme * partition,
                                                     UInt32 partitionID,
                                                     UInt32 partitionBlockSize )
{
    //
    // Allocate a new media object (called from instantiateMediaObject).
    //

    return new IOMedia;
}

OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  0);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  1);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  2);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  3);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  4);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  5);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  6);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  7);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  8);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme,  9);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme, 10);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme, 11);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme, 12);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme, 13);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme, 14);
OSMetaClassDefineReservedUnused(IOApplePartitionScheme, 15);
