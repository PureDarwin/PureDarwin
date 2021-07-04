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
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOAppleLabelScheme.h>
#include <libkern/OSByteOrder.h>
#include <os/overflow.h>

#define super IOFilterScheme
OSDefineMetaClassAndStructors(IOAppleLabelScheme, IOFilterScheme);

//
// Notes
//
// o the on-disk structure's fields are big-endian formatted
// o the al_offset value is relative to the media container
//

#define kIOMediaBaseKey "Base"

bool IOAppleLabelScheme::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    // State our assumptions.

    assert(sizeof(applelabel) == 512);              // (compiler/platform check)

    // Ask our superclass' opinion.

    if (super::init(properties) == false)  return false;

    // Initialize our state.

    _content = 0;

    return true;
}

void IOAppleLabelScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if ( _content )  _content->release();

    super::free();
}

IOService * IOAppleLabelScheme::probe(IOService * provider, SInt32 * score)
{
    //
    // Determine whether the provider media contains an Apple label scheme.
    //

    // State our assumptions.

    assert(OSDynamicCast(IOMedia, provider));

    // Ask superclass' opinion.

    if (super::probe(provider, score) == 0)  return 0;

    // Scan the provider media for an Apple label scheme.

    _content = scan(score);

    return ( _content ) ? this : 0;
}

bool IOAppleLabelScheme::start(IOService * provider)
{
    //
    // Publish the new media object which represents our content.
    //

    // State our assumptions.

    assert(_content);

    // Ask our superclass' opinion.

    if ( super::start(provider) == false )  return false;

    // Attach and register the new media object representing our content.

    _content->attach(this);

    attachMediaObjectToDeviceTree(_content);

    _content->registerService();

    return true;
}

void IOAppleLabelScheme::stop(IOService * provider)
{
    //
    // Clean up after the media object we published before terminating.
    //

    // State our assumptions.

    assert(_content);

    // Detach the media objects we previously attached to the device tree.

    detachMediaObjectFromDeviceTree(_content);

    super::stop(provider);
}

IOMedia * IOAppleLabelScheme::scan(SInt32 * score)
{
    //
    // Scan the provider media for an Apple label scheme.
    //

    IOBufferMemoryDescriptor * buffer         = 0;
    UInt64                     bufferBase     = 0;
    UInt32                     bufferSize     = 0;
    applelabel *               headerMap      = 0;
    UInt64                     labelBase      = 0;
    UInt32                     labelCheck     = 0;
    char *                     labelMap       = 0;
    UInt32                     labelSize      = 0;
    IOMedia *                  media          = getProvider();
    UInt64                     mediaBlockSize = media->getPreferredBlockSize();
    bool                       mediaIsOpen    = false;
    IOMedia *                  newMedia       = 0;
    OSDictionary *             properties     = 0;
    IOReturn                   status         = kIOReturnError;

    // Determine whether this media is formatted.

    if ( media->isFormatted() == false )  goto scanErr;

    // Determine whether this media has an appropriate block size.

    if ( (mediaBlockSize % sizeof(applelabel)) )  goto scanErr;

    // Allocate a buffer large enough to hold one map, rounded to a media block.

    bufferSize = IORound(sizeof(applelabel), mediaBlockSize);
    buffer     = IOBufferMemoryDescriptor::withCapacity(
                                           /* capacity      */ bufferSize,
                                           /* withDirection */ kIODirectionIn );
    if ( buffer == 0 )  goto scanErr;

    // Open the media with read access.

    mediaIsOpen = media->open(this, 0, kIOStorageAccessReader);
    if ( mediaIsOpen == false )  goto scanErr;

    // Read the label header into our buffer.

    status = media->read(this, 0, buffer);
    if ( status != kIOReturnSuccess )  goto scanErr;

    headerMap = (applelabel *) buffer->getBytesNoCopy();

    // Determine whether the label header signature is present.

    if ( OSSwapBigToHostInt16(headerMap->al_magic) != AL_MAGIC )
    {
        goto scanErr;
    }

    // Determine whether the label header version is valid.

    if ( OSSwapBigToHostInt16(headerMap->al_type) != AL_TYPE_DEFAULT )
    {
        goto scanErr;
    }

    // Compute the relative byte position and size of the label.

    labelBase  = OSSwapBigToHostInt64(headerMap->al_offset);
    labelCheck = OSSwapBigToHostInt32(headerMap->al_checksum);
    labelSize  = OSSwapBigToHostInt32(headerMap->al_size);

    if ( labelSize > 131072 )
    {
        goto scanErr;
    }

    // Allocate a buffer large enough to hold one map, rounded to a media block.

	// Ensure that the end of the label, rounded up to the mediaBlockSize,
	// will not overflow the integer type of bufferSize

	if ( os_add3_overflow(labelBase, labelSize, mediaBlockSize, &bufferSize ) != 0)  goto scanErr;

    buffer->release();

    bufferBase = IOTrunc(labelBase, mediaBlockSize);
    bufferSize = IORound(labelBase + labelSize, mediaBlockSize) - bufferBase;
    buffer     = IOBufferMemoryDescriptor::withCapacity(
                                           /* capacity      */ bufferSize,
                                           /* withDirection */ kIODirectionIn );
    if ( buffer == 0 )  goto scanErr;

    // Determine whether the label leaves the confines of the container.

    if ( bufferBase + bufferSize > media->getSize() )  goto scanErr;

    // Read the label into our buffer.

    status = media->read(this, bufferBase, buffer);
    if ( status != kIOReturnSuccess )  goto scanErr;

    labelMap = (char *) buffer->getBytesNoCopy() + (labelBase % mediaBlockSize);

    // Determine whether the label checksum is valid.

    if ( crc32(0, labelMap, labelSize) != labelCheck )
    {
        goto scanErr;
    }

    // Obtain the properties.

	properties = OSDynamicCast(OSDictionary, OSUnserializeXML(labelMap, labelSize));

	if ( properties == 0 )
    {
        goto scanErr;
    }

    // Determine whether the content is corrupt.

    if ( isContentCorrupt(properties) )
    {
        goto scanErr;
    }

    // Determine whether the content is corrupt.

    if ( isContentInvalid(properties) )
    {
        goto scanErr;
    }

    // Create a media object to represent the content.

    newMedia = instantiateMediaObject(properties);

    if ( newMedia == 0 )
    {
        goto scanErr;
    }

    // Release our resources.

    media->close(this);
    buffer->release();
    properties->release();

    return newMedia;

scanErr:

    // Release our resources.

    if ( mediaIsOpen )  media->close(this);
    if ( buffer )  buffer->release();
    if ( properties )  properties->release();

    return 0;
}

bool IOAppleLabelScheme::isContentCorrupt(OSDictionary * properties)
{
    //
    // Ask whether the given content appears to be corrupt.
    //

    return false;
}

bool IOAppleLabelScheme::isContentInvalid(OSDictionary * properties)
{
    //
    // Ask whether the given content appears to be invalid.
    //

    UInt64     contentBase = 0;
    UInt64     contentSize = 0;
    IOMedia *  media       = getProvider();
    OSObject * object      = 0;

    // Compute the relative byte position and size of the new content.

    object = properties->getObject(kIOMediaBaseKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentBase = ((OSNumber *) object)->unsigned64BitValue();
    }

    object = properties->getObject(kIOMediaSizeKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentSize = ((OSNumber *) object)->unsigned64BitValue();
    }

    // Determine whether the content is a placeholder.

    if ( contentSize == 0 )  return true;

    // Determine whether the new content leaves the confines of the container.

    if ( contentBase + contentSize > media->getSize() )  return true;

    return false;
}

IOMedia * IOAppleLabelScheme::instantiateMediaObject(OSDictionary * properties)
{
    //
    // Instantiate a new media object to represent the given content.
    //

    IOMediaAttributeMask contentAttributes = 0;
    UInt64               contentBase       = 0;
    UInt64               contentBlockSize  = 0;
    const char *         contentHint       = 0;
    bool                 contentIsWhole    = false;
    bool                 contentIsWritable = false;
    UInt64               contentSize       = 0;
    IOMedia *            media             = getProvider();
    IOMedia *            newMedia          = 0; 
    OSObject *           object            = 0;

    contentAttributes = media->getAttributes();
    contentBlockSize  = media->getPreferredBlockSize();
    contentIsWhole    = media->isWhole();
    contentIsWritable = media->isWritable();
    contentSize       = media->getSize();

    properties = OSDictionary::withDictionary(properties);
    if ( properties == 0 )  return 0;

    // Obtain the initialization properties.

    object = properties->getObject(kIOMediaBaseKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentBase = ((OSNumber *) object)->unsigned64BitValue();
    }

    properties->removeObject(kIOMediaBaseKey);

    object = properties->getObject(kIOMediaContentKey);

    if ( OSDynamicCast(OSString, object) )
    {
        contentHint = ((OSString *) object)->getCStringNoCopy();
    }

    properties->removeObject(kIOMediaContentKey);

    object = properties->getObject(kIOMediaContentHintKey);

    if ( OSDynamicCast(OSString, object) )
    {
        contentHint = ((OSString *) object)->getCStringNoCopy();
    }

    properties->removeObject(kIOMediaContentHintKey);

    object = properties->getObject(kIOMediaEjectableKey);

    if ( OSDynamicCast(OSBoolean, object) )
    {
        if ( ((OSBoolean *) object)->getValue() )
        {
            contentAttributes |= kIOMediaAttributeEjectableMask;
        }
        else
        {
            contentAttributes &= ~kIOMediaAttributeEjectableMask;
        }
    }

    properties->removeObject(kIOMediaEjectableKey);

    object = properties->getObject(kIOMediaPreferredBlockSizeKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentBlockSize = ((OSNumber *) object)->unsigned64BitValue();
    }

    properties->removeObject(kIOMediaPreferredBlockSizeKey);

    object = properties->getObject(kIOMediaRemovableKey);

    if ( OSDynamicCast(OSBoolean, object) )
    {
        if ( ((OSBoolean *) object)->getValue() )
        {
            contentAttributes |= kIOMediaAttributeRemovableMask;
        }
        else
        {
            contentAttributes &= ~kIOMediaAttributeRemovableMask;
        }
    }

    properties->removeObject(kIOMediaRemovableKey);

    object = properties->getObject(kIOMediaWholeKey);

    if ( OSDynamicCast(OSBoolean, object) )
    {
        contentIsWhole = ((OSBoolean *) object)->getValue();
    }

    properties->removeObject(kIOMediaWholeKey);

    object = properties->getObject(kIOMediaSizeKey);

    if ( OSDynamicCast(OSNumber, object) )
    {
        contentSize = ((OSNumber *) object)->unsigned64BitValue();
    }

    properties->removeObject(kIOMediaSizeKey);

    object = properties->getObject(kIOMediaWritableKey);

    if ( OSDynamicCast(OSBoolean, object) )
    {
        contentIsWritable = ((OSBoolean *) object)->getValue();
    }

    properties->removeObject(kIOMediaWritableKey);

    // Create the new media object.

    newMedia = instantiateDesiredMediaObject(properties);

    if ( newMedia )
    {
        if ( newMedia->init(
                /* base               */ contentBase,
                /* size               */ contentSize,
                /* preferredBlockSize */ contentBlockSize,
                /* attributes         */ contentAttributes,
                /* isWhole            */ contentIsWhole,
                /* isWritable         */ contentIsWritable,
                /* contentHint        */ contentHint ) )
        {
            // Set a location.

            newMedia->setLocation("1");

            // Set the properties.

            newMedia->getPropertyTable()->merge(properties);
        }
        else
        {
            newMedia->release();
            newMedia = 0;
        }
    }

    properties->release();

    return newMedia;
}

IOMedia * IOAppleLabelScheme::instantiateDesiredMediaObject(
                                                     OSDictionary * properties )
{
    //
    // Allocate a new media object (called from instantiateMediaObject).
    //

    return new IOMedia;
}

bool IOAppleLabelScheme::attachMediaObjectToDeviceTree( IOMedia * media )
{
    //
    // Attach the given media object to the device tree plane.
    //

    IORegistryEntry * child;

    if ( (child = getParentEntry(gIOServicePlane)) )
    {
        IORegistryEntry * parent;

        if ( (parent = child->getParentEntry(gIODTPlane)) )
        {
            const char * location = child->getLocation(gIODTPlane);
            const char * name     = child->getName(gIODTPlane);

            if ( media->attachToParent(parent, gIODTPlane) )
            {
                media->setLocation(location, gIODTPlane);
                media->setName(name, gIODTPlane);

                child->detachFromParent(parent, gIODTPlane);

                return true;
            }
        }
    }

    return false;
}

void IOAppleLabelScheme::detachMediaObjectFromDeviceTree( IOMedia * media )
{
    //
    // Detach the given media object from the device tree plane.
    //

    IORegistryEntry * child;

    if ( (child = getParentEntry(gIOServicePlane)) )
    {
        IORegistryEntry * parent;

        if ( (parent = media->getParentEntry(gIODTPlane)) )
        {
            const char * location = media->getLocation(gIODTPlane);
            const char * name     = media->getName(gIODTPlane);

            if ( child->attachToParent(parent, gIODTPlane) )
            {
                child->setLocation(location, gIODTPlane);
                child->setName(name, gIODTPlane);
            }

            media->detachFromParent(parent, gIODTPlane);
        }
    }
}

OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  0);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  1);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  2);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  3);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  4);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  5);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  6);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  7);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  8);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme,  9);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 10);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 11);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 12);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 13);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 14);
OSMetaClassDefineReservedUnused(IOAppleLabelScheme, 15);
