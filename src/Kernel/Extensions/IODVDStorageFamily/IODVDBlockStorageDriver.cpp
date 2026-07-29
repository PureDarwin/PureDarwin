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
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IODVDBlockStorageDevice.h>
#include <IOKit/storage/IODVDBlockStorageDriver.h>
#include <IOKit/storage/IODVDMedia.h>

#define	super	IOCDBlockStorageDriver
OSDefineMetaClassAndStructors(IODVDBlockStorageDriver,IOCDBlockStorageDriver)

OSCompileAssert(sizeof(DVDDiscInfo) == sizeof(CDDiscInfo));
OSCompileAssert(sizeof(DVDRZoneInfo) == sizeof(CDTrackInfo));

#define reportDiscInfo(x)    reportDiscInfo((CDDiscInfo *)(x))
#define reportRZoneInfo(y,x) reportTrackInfo((y),(CDTrackInfo *)(x))

IODVDBlockStorageDevice *
IODVDBlockStorageDriver::getProvider() const
{
    return (IODVDBlockStorageDevice *) IOService::getProvider();
}

/* Accept a new piece of media, doing whatever's necessary to make it
 * show up properly to the system.
 */
IOReturn
IODVDBlockStorageDriver::acceptNewMedia(void)
{
    IOReturn result;

    if (getMediaType() < kDVDMediaTypeMin || getMediaType() > kDVDMediaTypeMax) {
        return super::acceptNewMedia();
    }

    /* Obtain disc status: */

    switch (getMediaType()) {
        case kDVDMediaTypeR:
        case kDVDMediaTypeRW: {
            bool checkIsWritable = false;
            DVDDiscInfo discInfo;
            DVDRZoneInfo rzoneInfo;

            result = reportDiscInfo(&discInfo);
            if (result != kIOReturnSuccess) {
                break;
            }

            switch (discInfo.discStatus) {
                case 0x01: /* is disc incomplete? */
                    checkIsWritable = true;
                    break;
                case 0x02: /* is disc complete? */
                    checkIsWritable = discInfo.erasable ? true : false;
                    break;
            }

            /* Obtain rzone status: */

            if (checkIsWritable) {
                UInt16 rzoneLast = (discInfo.lastRZoneNumberInLastBorderMSB << 8) |
                                    discInfo.lastRZoneNumberInLastBorderLSB;

                result = reportRZoneInfo(rzoneLast,&rzoneInfo);
                if (result != kIOReturnSuccess) {
                    break;
                }

                if (discInfo.discStatus == 0x01) { /* is disc incomplete? */
                    _maxBlockNumber = max( _maxBlockNumber,
                                           max( OSSwapBigToHostInt32(rzoneInfo.rzoneStartAddress) +
                                                OSSwapBigToHostInt32(rzoneInfo.rzoneSize), 1 ) - 1 );
                }

                if (rzoneInfo.incremental) { /* is rzone incremental? */
                    _writeProtected = false;
                    break;
                }

                if (discInfo.discStatus == 0x01) { /* is disc incomplete? */
                    if (rzoneInfo.blank) { /* is rzone invisible? */
                        UInt16 rzoneFirst = (discInfo.firstRZoneNumberInLastBorderMSB << 8) |
                                             discInfo.firstRZoneNumberInLastBorderLSB;

                        if (rzoneFirst < rzoneLast) {
                            result = reportRZoneInfo(rzoneLast - 1,&rzoneInfo);
                            if (result != kIOReturnSuccess) {
                                break;
                            }

                            if (rzoneInfo.incremental) { /* is rzone incremental? */
                                _writeProtected = false;
                                break;
                            }
                        }
                    }
                }
            }

            break;
        }
        case kDVDMediaTypePlusR:
        case kDVDMediaTypeHDR: {
            DVDDiscInfo discInfo;
            DVDRZoneInfo rzoneInfo;
            IOReturn result;

            result = reportDiscInfo(&discInfo);
            if (result != kIOReturnSuccess) {
                break;
            }

            /* Obtain rzone status: */

            if (discInfo.discStatus == 0x01) { /* is disc incomplete? */
                UInt16 rzoneLast = (discInfo.lastRZoneNumberInLastBorderMSB << 8) |
                                    discInfo.lastRZoneNumberInLastBorderLSB;

                _writeProtected = false;

                result = reportRZoneInfo(rzoneLast,&rzoneInfo);
                if (result != kIOReturnSuccess) {
                    break;
                }

                _maxBlockNumber = max( _maxBlockNumber,
                                       max( OSSwapBigToHostInt32(rzoneInfo.rzoneStartAddress) +
                                            OSSwapBigToHostInt32(rzoneInfo.rzoneSize), 1 ) - 1 );
            }

            break;
        }
    }

    return IOBlockStorageDriver::acceptNewMedia();
}

const char *
IODVDBlockStorageDriver::getDeviceTypeName(void)
{
    return(kIOBlockStorageDeviceTypeDVD);
}

IOMedia *
IODVDBlockStorageDriver::instantiateDesiredMediaObject(void)
{
    if (getMediaType() < kDVDMediaTypeMin || getMediaType() > kDVDMediaTypeMax) {
        return super::instantiateDesiredMediaObject();
    }

    return(new IODVDMedia);
}

IOMedia *
IODVDBlockStorageDriver::instantiateMediaObject(UInt64 base,UInt64 byteSize,
                                        UInt32 blockSize,char *mediaName)
{
    IOMedia *media = NULL;

    if (getMediaType() < kDVDMediaTypeMin || getMediaType() > kDVDMediaTypeMax) {
        return super::instantiateMediaObject(base,byteSize,blockSize,mediaName);
    }

    media = IOBlockStorageDriver::instantiateMediaObject(
                                             base,byteSize,blockSize,mediaName);

    if (media) {
        const char *description = NULL;
        const char *picture = NULL;

        switch (getMediaType()) {
            case kDVDMediaTypeROM:
                description = kIODVDMediaTypeROM;
                picture = "DVD.icns";
                break;
            case kDVDMediaTypeRAM:
                description = kIODVDMediaTypeRAM;
                picture = "DVD-RAM.icns";
                break;
            case kDVDMediaTypeR:
                description = kIODVDMediaTypeR;
                picture = "DVD-R.icns";
                break;
            case kDVDMediaTypeRW:
                description = kIODVDMediaTypeRW;
                picture = "DVD-RW.icns";
                break;
            case kDVDMediaTypePlusR:
                description = kIODVDMediaTypePlusR;
                picture = "DVD+R.icns";
                break;
            case kDVDMediaTypePlusRW:
                description = kIODVDMediaTypePlusRW;
                picture = "DVD+RW.icns";
                break;
            case kDVDMediaTypeHDROM:
                description = kIODVDMediaTypeHDROM;
                picture = "DVD.icns";
                break;
            case kDVDMediaTypeHDRAM:
                description = kIODVDMediaTypeHDRAM;
                picture = "DVD-RAM.icns";
                break;
            case kDVDMediaTypeHDR:
                description = kIODVDMediaTypeHDR;
                picture = "DVD-R.icns";
                break;
            case kDVDMediaTypeHDRW:
                description = kIODVDMediaTypeHDRW;
                picture = "DVD-RW.icns";
                break;
        }

        if (description) {
            media->setProperty(kIODVDMediaTypeKey, description);
        }

        if (picture) {
            OSDictionary *dictionary = OSDictionary::withCapacity(2);
            OSString *identifier = OSString::withCString("com.apple.iokit.IODVDStorageFamily");
            OSString *resourceFile = OSString::withCString(picture);

            if (dictionary && identifier && resourceFile) {
                dictionary->setObject("CFBundleIdentifier", identifier);
                dictionary->setObject("IOBundleResourceFile", resourceFile);
            }

            media->setProperty(kIOMediaIconKey, dictionary);

            if (resourceFile) {
                resourceFile->release();
            }
            if (identifier) {
                identifier->release();
            }
            if (dictionary) {
                dictionary->release();
            }
        }
    }

    return media;
}

IOReturn
IODVDBlockStorageDriver::reportKey(IOMemoryDescriptor *buffer,const DVDKeyClass keyClass,
                                        const UInt32 lba,const UInt8 agid,const DVDKeyFormat keyFormat)
{
    return(reportKey(buffer,keyClass,lba,0,agid,keyFormat));
}

IOReturn
IODVDBlockStorageDriver::reportKey(IOMemoryDescriptor *buffer,const DVDKeyClass keyClass,
                                        const UInt32 lba,const UInt8 blockCount,
                                        const UInt8 agid,const DVDKeyFormat keyFormat)
{
    return(getProvider()->reportKey(buffer,keyClass,lba,blockCount,agid,keyFormat));
}

IOReturn
IODVDBlockStorageDriver::sendKey(IOMemoryDescriptor *buffer,const DVDKeyClass keyClass,
                                        const UInt8 agid,const DVDKeyFormat keyFormat)
{
    return(getProvider()->sendKey(buffer,keyClass,agid,keyFormat));
}

IOReturn
IODVDBlockStorageDriver::readStructure(IOMemoryDescriptor *buffer,const DVDStructureFormat format,
                                        const UInt32 address,const UInt8 layer,const UInt8 agid)
{
    return(getProvider()->readDVDStructure(buffer,format,address,layer,agid));
}

OSMetaClassDefineReservedUsed(IODVDBlockStorageDriver,  0);     /* reportKey */
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  1);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  2);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  3);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  4);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  5);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  6);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  7);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  8);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver,  9);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 10);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 11);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 12);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 13);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 14);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 15);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 16);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 17);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 18);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 19);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 20);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 21);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 22);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 23);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 24);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 25);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 26);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 27);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 28);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 29);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 30);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDriver, 31);
