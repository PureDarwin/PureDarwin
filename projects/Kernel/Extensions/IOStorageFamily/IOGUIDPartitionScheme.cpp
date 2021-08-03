/*
 * Copyright (c) 1998-2016 Apple Inc. All rights reserved.
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
#include <IOKit/storage/IOFDiskPartitionScheme.h>
#include <IOKit/storage/IOGUIDPartitionScheme.h>
#include <libkern/OSByteOrder.h>
#include <sys/utfconv.h>
#include <IOKit/storage/IOBlockStorageDevice.h>

#define super IOPartitionScheme
OSDefineMetaClassAndStructors(IOGUIDPartitionScheme, IOPartitionScheme);

#define UCS_LITTLE_ENDIAN 0x00000001

static size_t ucs2_to_utf8( const uint16_t * ucs2str,
                            size_t           ucs2strsiz,
                            char *           utf8str,
                            size_t           utf8strsiz,
                            uint32_t         flags )
{
    size_t ucs2strlen;
    size_t utf8strlen;

    for ( ucs2strlen = 0; ucs2strlen < ucs2strsiz; ucs2strlen++ )
    {
        if ( ucs2str[ucs2strlen] == 0 )  break;
    }

    utf8_encodestr( ucs2str,
                    ucs2strlen * sizeof(uint16_t),
                    (uint8_t *) utf8str,
                    &utf8strlen,
                    utf8strsiz,
                    '/',
#ifdef __BIG_ENDIAN__
                    (flags & UCS_LITTLE_ENDIAN) ? UTF_REVERSE_ENDIAN : 0 );
#else /* !__BIG_ENDIAN__ */
                    (flags & UCS_LITTLE_ENDIAN) ? 0 : UTF_REVERSE_ENDIAN );
#endif /* !__BIG_ENDIAN__ */

    return utf8strlen;
}

static void uuid_unswap(uuid_t uu)
{
    uint8_t tmp;

    tmp = uu[0];  uu[0] = uu[3];  uu[3] = tmp;
    tmp = uu[2];  uu[2] = uu[1];  uu[1] = tmp;
    tmp = uu[4];  uu[4] = uu[5];  uu[5] = tmp;
    tmp = uu[6];  uu[6] = uu[7];  uu[7] = tmp;
}

bool IOGUIDPartitionScheme::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    // Ask our superclass' opinion.

    if ( super::init(properties) == false )  return false;

    // Initialize our state.

    _partitions = 0;

    return true;
}

void IOGUIDPartitionScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if ( _partitions )  _partitions->release();

    super::free();
}

void IOGUIDPartitionScheme::handleClose(IOService * client, IOOptionBits options)
{
    super::handleClose(client, options);

    // if the client has been already removed from the partition table
    // Now is the time to terminate the IOMedia object:
    OSObject* obj = client->getProperty(kIOMediaLiveKey);
    if (obj && OSDynamicCast(OSBoolean, obj))
    {
        // if kIOMediaLiveKey is 0 and kIOMediaPartitionIDKey is removed
        // then it means that this partition has been removed from partition table.
        if (0 == ((OSBoolean *) obj)->getValue() && NULL == client->getProperty(kIOMediaPartitionIDKey))
        {
            client->terminate();
            detachMediaObjectFromDeviceTree(OSDynamicCast(IOMedia, client));
        }
    }
}

IOService * IOGUIDPartitionScheme::probe(IOService * provider, SInt32 * score)
{
    //
    // Determine whether the provider media contains a GUID partition map.
    //

    // State our assumptions.

    assert(OSDynamicCast(IOMedia, provider));

    // Ask our superclass' opinion.

    if ( super::probe(provider, score) == 0 )  return 0;

    // Scan the provider media for a GUID partition map.

    _partitions = scan(score);

    return ( _partitions ) ? this : 0;
}

bool IOGUIDPartitionScheme::start(IOService * provider)
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

void IOGUIDPartitionScheme::stop(IOService * provider)
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

IOReturn IOGUIDPartitionScheme::requestProbe(IOOptionBits options)
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

OSSet * IOGUIDPartitionScheme::scan(SInt32 * score)
{
    //
    // Scan the provider media for a GUID partition map.    Returns the set
    // of media objects representing each of the partitions (the retain for
    // the set is passed to the caller), or null should no partition map be
    // found.  The default probe score can be adjusted up or down, based on
    // the confidence of the scan.
    //

    IOBufferMemoryDescriptor * buffer         = 0;
    IOByteCount                bufferSize     = 0;
    UInt32                     fdiskID        = 0;
    disk_blk0 *                fdiskMap       = 0;
    UInt64                     gptBlock       = 0;
    UInt32                     gptCheck       = 0;
    UInt32                     gptCount       = 0;
    UInt32                     gptID          = 0;
    gpt_ent *                  gptMap         = 0;
    UInt32                     gptSize        = 0;
    UInt32                     headerCheck    = 0;
    gpt_hdr *                  headerMap      = 0;
    UInt32                     headerSize     = 0;
    IOMedia *                  media          = getProvider();
    UInt64                     mediaBlockSize = media->getPreferredBlockSize();
    bool                       mediaIsOpen    = false;
    OSSet *                    partitions     = 0;
    IOReturn                   status         = kIOReturnError;

    // Determine whether this media is formatted.

    if ( media->isFormatted() == false )  goto scanErr;

    // Determine whether this media has an appropriate block size.

    if ( (mediaBlockSize % sizeof(disk_blk0)) )  goto scanErr;

    // Allocate a buffer large enough to hold one map, rounded to a media block.

    bufferSize = IORound(sizeof(disk_blk0), mediaBlockSize);
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

    // Read the protective map into our buffer.

    status = media->read(this, 0, buffer);
    if ( status != kIOReturnSuccess )  goto scanErr;

    fdiskMap = (disk_blk0 *) buffer->getBytesNoCopy();

    // Determine whether the protective map signature is present.

    if ( OSSwapLittleToHostInt16(fdiskMap->signature) != DISK_SIGNATURE )
    {
         goto scanErr;
    }

    // Scan for valid partition entries in the protective map.

    for ( unsigned index = 0; index < DISK_NPART; index++ )
    {
        if ( fdiskMap->parts[index].systid )
        {
            if ( fdiskMap->parts[index].systid == 0xEE )
            {
                if ( fdiskID )  goto scanErr;

                fdiskID = index + 1;
            }
        }
    }

    if ( fdiskID == 0 )  goto scanErr;

    // Read the partition header into our buffer.

    status = media->read(this, mediaBlockSize, buffer);
    if ( status != kIOReturnSuccess )  goto scanErr;

    headerMap = (gpt_hdr *) buffer->getBytesNoCopy();

    // Determine whether the partition header signature is present.

    if ( memcmp(headerMap->hdr_sig, GPT_HDR_SIG, strlen(GPT_HDR_SIG)) )
    {
        goto scanErr;
    }

    // Determine whether the partition header size is valid.

    headerCheck = OSSwapLittleToHostInt32(headerMap->hdr_crc_self);
    headerSize  = OSSwapLittleToHostInt32(headerMap->hdr_size);

    if ( headerSize < offsetof(gpt_hdr, padding) )
    {
        goto scanErr;
    }

    if ( headerSize > mediaBlockSize )
    {
        goto scanErr;
    }

    // Determine whether the partition header checksum is valid.

    headerMap->hdr_crc_self = 0;

    if ( crc32(0, headerMap, headerSize) != headerCheck )
    {
        goto scanErr;
    }

    // Determine whether the partition entry size is valid.

    gptCheck = OSSwapLittleToHostInt32(headerMap->hdr_crc_table);
    gptSize  = OSSwapLittleToHostInt32(headerMap->hdr_entsz);

    if ( gptSize < sizeof(gpt_ent) )
    {
        goto scanErr;
    }

    if ( gptSize > UINT16_MAX )
    {
        goto scanErr;
    }

    // Determine whether the partition entry count is valid.

    gptBlock = OSSwapLittleToHostInt64(headerMap->hdr_lba_table);
    gptCount = OSSwapLittleToHostInt32(headerMap->hdr_entries);

    if ( gptCount > UINT16_MAX )
    {
        goto scanErr;
    }

    // publish the GPT disk GUID as an OSString
    {
        uuid_string_t uuid;
        uuid_unswap( headerMap->hdr_uuid );
        uuid_unparse( headerMap->hdr_uuid, uuid );
        setProperty( kIOGUIDPartitionSchemeUUIDKey, uuid );
    }

    // Allocate a buffer large enough to hold one map, rounded to a media block.

    buffer->release();
    buffer = 0;

    // In case gptCount * gptSize + mediaBlockSize exceed UInt32, the IORound will
    // return 0.
    bufferSize = IORound(gptCount * gptSize, mediaBlockSize);
    if ( bufferSize == 0 )  goto scanErr;

    buffer     = IOBufferMemoryDescriptor::withCapacity(
                                           /* capacity      */ bufferSize,
                                           /* withDirection */ kIODirectionIn );
    if ( buffer == 0 )  goto scanErr;

    // Read the partition header into our buffer.

    status = media->read(this, gptBlock * mediaBlockSize, buffer);
    if ( status != kIOReturnSuccess )  goto scanErr;

    gptMap = (gpt_ent *) buffer->getBytesNoCopy();

    // Determine whether the partition entry checksum is valid.

    if ( crc32(0, gptMap, gptCount * gptSize) != gptCheck )
    {
        goto scanErr;
    }

    // Scan for valid partition entries in the partition map.

    for ( gptID = 1; gptID <= gptCount; gptID++ )
    {
        gptMap = (gpt_ent *) ( ((UInt8 *) buffer->getBytesNoCopy()) +
                               (gptID * gptSize) - gptSize );

        uuid_unswap( gptMap->ent_type );
        uuid_unswap( gptMap->ent_uuid );
 
        if ( isPartitionUsed( gptMap ) )
        {
            // Determine whether the partition is corrupt (fatal).

            if ( isPartitionCorrupt( gptMap, gptID ) )
            {
                goto scanErr;
            }

            // Determine whether the partition is invalid (skipped).

            if ( isPartitionInvalid( gptMap, gptID ) )
            {
                continue;
            }

            // Create a media object to represent this partition.

            IOMedia * newMedia = instantiateMediaObject( gptMap, gptID );

            if ( newMedia )
            {
                partitions->setObject(newMedia);
                newMedia->release();
            }
        }
    }

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

bool IOGUIDPartitionScheme::isPartitionUsed(gpt_ent * partition)
{
    //
    // Ask whether the given partition is used.
    //

    return uuid_is_null(partition->ent_type) ? false : true;
}

bool IOGUIDPartitionScheme::isPartitionCorrupt( gpt_ent * /* partition   */ ,
                                                UInt32    /* partitionID */ )
{
    //
    // Ask whether the given partition appears to be corrupt. A partition that
    // is corrupt will cause the failure of the GUID partition map recognition
    // altogether.
    //

    return false;
}

bool IOGUIDPartitionScheme::isPartitionInvalid( gpt_ent * partition,
                                                UInt32    partitionID )
{
    //
    // Ask whether the given partition appears to be invalid.  A partition that
    // is invalid will cause it to be skipped in the scan, but will not cause a
    // failure of the GUID partition map recognition.
    //

    IOMedia * media          = getProvider();
    UInt64    mediaBlockSize = media->getPreferredBlockSize();
    UInt64    partitionBase  = 0;
    UInt64    partitionSize  = 0;

    // Compute the relative byte position and size of the new partition.

    partitionBase  = OSSwapLittleToHostInt64(partition->ent_lba_start);
    partitionSize  = OSSwapLittleToHostInt64(partition->ent_lba_end);
    partitionBase *= mediaBlockSize;
    partitionSize *= mediaBlockSize;

    // Determine whether the partition is a placeholder.

    if ( partitionBase == partitionSize )  return true;

    // Compute the relative byte position and size of the new partition.

    partitionSize -= partitionBase - mediaBlockSize;

    // Determine whether the new partition leaves the confines of the container.

    if ( partitionBase + partitionSize > media->getSize() )  return true;

    return false;
}

IOMedia * IOGUIDPartitionScheme::instantiateMediaObject( gpt_ent * partition,
                                                         UInt32    partitionID )
{
    //
    // Instantiate a new media object to represent the given partition.
    //

    IOMedia *     media          = getProvider();
    UInt64        mediaBlockSize = media->getPreferredBlockSize();
    UInt64        partitionBase  = 0;
    uuid_string_t partitionHint;
    char          partitionName[36 * 3 + 1];
    UInt64        partitionSize  = 0;

    ucs2_to_utf8( partition->ent_name,
                  sizeof(partition->ent_name),
                  partitionName,
                  sizeof(partitionName),
                  UCS_LITTLE_ENDIAN );

    uuid_unparse( partition->ent_type,
                  partitionHint );

    // Compute the relative byte position and size of the new partition.

    partitionBase  = OSSwapLittleToHostInt64(partition->ent_lba_start);
    partitionSize  = OSSwapLittleToHostInt64(partition->ent_lba_end);
    partitionBase *= mediaBlockSize;
    partitionSize *= mediaBlockSize;
    partitionSize -= partitionBase - mediaBlockSize;

    // Create the new media object.

    IOMedia * newMedia = instantiateDesiredMediaObject(
                                   /* partition   */ partition,
                                   /* partitionID */ partitionID );

    if ( newMedia )
    {
         if ( newMedia->init(
                /* base               */ partitionBase,
                /* size               */ partitionSize,
                /* preferredBlockSize */ mediaBlockSize,
                /* attributes         */ media->getAttributes(),
                /* isWhole            */ false,
                /* isWritable         */ media->isWritable(),
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

            // Set the "Universal Unique ID" key for this partition.

            uuid_string_t uuid;
            uuid_unparse(partition->ent_uuid, uuid);
            newMedia->setProperty(kIOMediaUUIDKey, uuid);

            UInt64 gptAttributes = OSSwapLittleToHostInt64( partition->ent_attr );
            newMedia->setProperty(kIOMediaGPTPartitionAttributesKey, gptAttributes, 64);
        }
        else
        {
            newMedia->release();
            newMedia = 0;
        }
    }

    return newMedia;
}

IOMedia * IOGUIDPartitionScheme::instantiateDesiredMediaObject(
                                                         gpt_ent * partition,
                                                         UInt32    partitionID )
{
    //
    // Allocate a new media object (called from instantiateMediaObject).
    //

    return new IOMedia;
}

IOReturn IOGUIDPartitionScheme::message(UInt32      type,
                                        IOService * provider,
                                        void *      argument)
{
    //
    // Generic entry point for calls from the provider.  A return value of
    // kIOReturnSuccess indicates that the message was received, and where
    // applicable, that it was successful.
    //

    switch (type)
    {
        case kIOMessageMediaParametersHaveChanged:
        {
            OSIterator * partitionIterator;

            partitionIterator = OSCollectionIterator::withCollection(_partitions);

            if ( partitionIterator )
            {
                IOMedia *    media          = getProvider();
                IOMedia *    partition;

                while ( (partition = (IOMedia *) partitionIterator->getNextObject()) )
                {

                    lockForArbitration();

                    partition->init( partition->getBase(),
                                     partition->getSize(),
                                     media->getPreferredBlockSize(),
                                     media->getAttributes(),
                                     partition->isWhole(),
                                     media->isWritable(),
                                     partition->getContentHint() );

                    unlockForArbitration();
                }

                partitionIterator->release();
            }
            return kIOReturnSuccess;
        }
        default:
        {
            return super::message(type, provider, argument);
        }
    }
}

OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  0);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  1);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  2);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  3);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  4);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  5);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  6);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  7);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  8);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme,  9);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme, 10);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme, 11);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme, 12);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme, 13);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme, 14);
OSMetaClassDefineReservedUnused(IOGUIDPartitionScheme, 15);
