/*
 * Copyright (c) 2006-2014 Apple Inc. All rights reserved.
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

#include <IOKit/storage/IOBDBlockStorageDriver.h>
#include <IOKit/storage/IOBDMedia.h>

#define	super IOMedia
OSDefineMetaClassAndStructors(IOBDMedia, IOMedia)

IOBDBlockStorageDriver * IOBDMedia::getProvider() const
{
    //
    // Obtain this object's provider.   We override the superclass's method to
    // return a more specific subclass of IOService -- IOBDBlockStorageDriver.
    // This method serves simply as a convenience to subclass developers.
    //

    return (IOBDBlockStorageDriver *) IOService::getProvider();
}

bool IOBDMedia::matchPropertyTable(OSDictionary * table, SInt32 * score)
{
    //
    // Compare the properties in the supplied table to this object's properties.
    //

    // Ask our superclass' opinion.

    if (super::matchPropertyTable(table, score) == false)  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied table.

    return compareProperty(table, kIOBDMediaTypeKey);
}

IOReturn IOBDMedia::reportKey( IOMemoryDescriptor * buffer,
                               UInt8                keyClass,
                               UInt32               address,
                               UInt8                grantID,
                               UInt8                format )
{
    return reportKey( /* buffer     */                buffer,
                      /* keyClass   */ (DVDKeyClass)  keyClass,
                      /* address    */                address,
                      /* blockCount */                0,
                      /* grantID    */                grantID,
                      /* format     */ (DVDKeyFormat) format );
}

IOReturn IOBDMedia::reportKey( IOMemoryDescriptor * buffer,
                               UInt8                keyClass,
                               UInt32               address,
                               UInt8                blockCount,
                               UInt8                grantID,
                               UInt8                format )
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->reportKey( /* buffer     */                buffer,
                                     /* keyClass   */ (DVDKeyClass)  keyClass,
                                     /* address    */                address,
                                     /* blockCount */                blockCount,
                                     /* grantID    */                grantID,
                                     /* format     */ (DVDKeyFormat) format );
}

IOReturn IOBDMedia::sendKey( IOMemoryDescriptor * buffer,
                             UInt8                keyClass,
                             UInt8                grantID,
                             UInt8                format )
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->sendKey( /* buffer   */                buffer,
                                   /* keyClass */ (DVDKeyClass)  keyClass,
                                   /* grantID  */                grantID,
                                   /* format   */ (DVDKeyFormat) format );
}

IOReturn IOBDMedia::readStructure( IOMemoryDescriptor * buffer,
                                   UInt8                format,
                                   UInt32               address,
                                   UInt8                layer,
                                   UInt8                grantID )
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    if (buffer == 0)
    {
        return kIOReturnBadArgument;
    }

    return getProvider()->readStructure( /* buffer  */ buffer,
                                         /* format  */ format,
                                         /* address */ address,
                                         /* layer   */ layer,
                                         /* grantID */ grantID );
}

IOReturn IOBDMedia::getSpeed(UInt16 * kilobytesPerSecond)
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->getSpeed(kilobytesPerSecond);
}

IOReturn IOBDMedia::setSpeed(UInt16 kilobytesPerSecond)
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->setSpeed(kilobytesPerSecond);
}

IOReturn IOBDMedia::readDiscInfo( IOMemoryDescriptor * buffer,
                                  UInt8                type,
                                  UInt16 *             actualByteCount )
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

    return getProvider()->readDiscInfo( /* buffer          */ buffer,
                                        /* actualByteCount */ actualByteCount );
}

IOReturn IOBDMedia::readTrackInfo( IOMemoryDescriptor * buffer,
                                   UInt32               address,
                                   UInt8                addressType,
                                   UInt8                open,
                                   UInt16 *             actualByteCount )
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
                                        /* buffer          */ buffer,
                                        /* address         */ address,
                                        /* addressType     */ addressType,
                                        /* actualByteCount */ actualByteCount );
}

IOReturn IOBDMedia::splitTrack(UInt32 address)
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->splitTrack(address);
}

OSMetaClassDefineReservedUsed(IOBDMedia,  0);       /* reportKey */
OSMetaClassDefineReservedUnused(IOBDMedia,  1);
OSMetaClassDefineReservedUnused(IOBDMedia,  2);
OSMetaClassDefineReservedUnused(IOBDMedia,  3);
OSMetaClassDefineReservedUnused(IOBDMedia,  4);
OSMetaClassDefineReservedUnused(IOBDMedia,  5);
OSMetaClassDefineReservedUnused(IOBDMedia,  6);
OSMetaClassDefineReservedUnused(IOBDMedia,  7);
OSMetaClassDefineReservedUnused(IOBDMedia,  8);
OSMetaClassDefineReservedUnused(IOBDMedia,  9);
OSMetaClassDefineReservedUnused(IOBDMedia, 10);
OSMetaClassDefineReservedUnused(IOBDMedia, 11);
OSMetaClassDefineReservedUnused(IOBDMedia, 12);
OSMetaClassDefineReservedUnused(IOBDMedia, 13);
OSMetaClassDefineReservedUnused(IOBDMedia, 14);
OSMetaClassDefineReservedUnused(IOBDMedia, 15);
