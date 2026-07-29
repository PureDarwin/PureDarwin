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
#include <IOKit/storage/IOCDBlockStorageDriver.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOCDBlockStorageDevice.h>
#include <libkern/OSByteOrder.h>

#define	super	IOBlockStorageDriver
OSDefineMetaClassAndStructors(IOCDBlockStorageDriver,IOBlockStorageDriver)

IOCDBlockStorageDevice *
IOCDBlockStorageDriver::getProvider() const
{
    return (IOCDBlockStorageDevice *) IOService::getProvider();
}


/* Accept a new piece of media, doing whatever's necessary to make it
 * show up properly to the system. The arbitration lock is assumed to
 * be held during the call.
 */
IOReturn
IOCDBlockStorageDriver::acceptNewMedia(void)
{
    IOReturn result;
    int i;
    int nentries;
    int nAudioTracks;

    /* First, we cache information about the tracks on the disc: */
    
    result = cacheTocInfo();
    if (result != kIOReturnSuccess) {
        assert(_toc == NULL);
    }

    /* Scan thru the track list, counting up the number of Data and Audio tracks. */
    
    nAudioTracks = 0;

    _minBlockNumberAudio = 0xFFFFFFFF;
    _maxBlockNumberAudio = 0xFFFFFFFF;

    if (_toc) {
        nentries = CDTOCGetDescriptorCount(_toc);

        for (i = 0; i < nentries; i++) {   
            UInt32 lba = CDConvertMSFToClippedLBA(_toc->descriptors[i].p);
            /* tracks 1-99, not leadout or skip intervals */
            if (_toc->descriptors[i].point <= 99 && _toc->descriptors[i].adr == 1) {
                if ((_toc->descriptors[i].control & 0x04)) {
                    /* it's a data track */
                    _maxBlockNumberAudio = min(_maxBlockNumberAudio, lba ? (lba - 1) : 0);
                } else {
                    nAudioTracks++;
                    _minBlockNumberAudio = min(_minBlockNumberAudio, lba);
                }
            /* leadout */
            } else if (_toc->descriptors[i].point == 0xA2 && _toc->descriptors[i].adr == 1) {
                _maxBlockNumber = max(_maxBlockNumber, lba ? (lba - 1) : 0);
                _maxBlockNumberAudio = min(_maxBlockNumberAudio, lba ? (lba - 1) : 0);
            }
        }

        if (_maxBlockNumberAudio < _minBlockNumberAudio) {
            _maxBlockNumberAudio = 0xFFFFFFFF;

            /* find first data track or leadout after the audio tracks */
            for (i = 0; i < nentries; i++) {   
                UInt32 lba = CDConvertMSFToClippedLBA(_toc->descriptors[i].p);
                /* tracks 1-99, not leadout or skip intervals */
                if (_toc->descriptors[i].point <= 99 && _toc->descriptors[i].adr == 1) {
                    if ((_toc->descriptors[i].control & 0x04)) {
                        /* it's a data track */
                        if (lba > _minBlockNumberAudio) {
                            _maxBlockNumberAudio = min(_maxBlockNumberAudio, lba - 1);
                        }
                    }
                /* leadout */
                } else if (_toc->descriptors[i].point == 0xA2 && _toc->descriptors[i].adr == 1) {
                    if (lba > _minBlockNumberAudio) {
                        _maxBlockNumberAudio = min(_maxBlockNumberAudio, lba - 1);
                    }
                }
            }
        }
    }

    /* Obtain disc status: */

    switch (getMediaType()) {
        case kCDMediaTypeR:
        case kCDMediaTypeRW: {
            bool checkIsWritable = false;
            CDDiscInfo discInfo;
            CDTrackInfo trackInfo;

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

            /* Obtain track status: */

            if (checkIsWritable) {
                UInt16 trackLast = discInfo.lastTrackNumberInLastSessionLSB;

                result = reportTrackInfo(trackLast,&trackInfo);
                if (result != kIOReturnSuccess) {
                    break;
                }

                if (discInfo.discStatus == 0x01) { /* is disc incomplete? */
                    _maxBlockNumber = CDConvertMSFToClippedLBA(discInfo.lastPossibleStartTimeOfLeadOut);
                }

                if (trackInfo.packet) { /* is track incremental? */
                    _writeProtected = false;
                    break;
                }

                if (discInfo.discStatus == 0x01) { /* is disc incomplete? */
                    if (trackInfo.blank) { /* is track invisible? */
                        UInt16 trackFirst = discInfo.firstTrackNumberInLastSessionLSB;

                        if (trackFirst < trackLast) {
                            result = reportTrackInfo(trackLast - 1,&trackInfo);
                            if (result != kIOReturnSuccess) {
                                break;
                            }

                            if (trackInfo.packet) { /* is track incremental? */
                                _writeProtected = false;
                                break;
                            }
                        }
                    }
                }
            }

            break;
        }
    }

    /* Instantiate a media object and attach it to ourselves. */

    result = super::acceptNewMedia();
    if (result != kIOReturnSuccess) {
        return(result);			/* give up now */
    }

    return(result);
}

IOReturn
IOCDBlockStorageDriver::cacheTocInfo(void)
{
    IOBufferMemoryDescriptor *buffer;
    IOReturn result;
    CDTOC *toc;
    UInt16 tocSize;

    assert(sizeof(CDTOC) == 4);		/* (compiler/platform check) */
    assert(sizeof(CDTOCDescriptor) == 11);		/* (compiler/platform check) */
    
    assert(_toc == NULL);

    /* Read the TOC header: */

    buffer = IOBufferMemoryDescriptor::withCapacity(sizeof(CDTOC),kIODirectionIn);
    if (buffer == NULL) {
        return(kIOReturnNoMemory);
    }

    result = getProvider()->readTOC(buffer);
    if (result != kIOReturnSuccess) {
        buffer->release();
        return(result);
    }

    toc = (CDTOC *) buffer->getBytesNoCopy();
    tocSize = OSSwapBigToHostInt16(toc->length) + sizeof(toc->length);

    buffer->release();

    /* Reject the TOC if its size is too small: */

    if (tocSize <= sizeof(CDTOC)) {
        return(kIOReturnNotFound);
    }

    /* Read the TOC in full: */

    buffer = IOBufferMemoryDescriptor::withCapacity(tocSize,kIODirectionIn);
    if (buffer == NULL) {
        return(kIOReturnNoMemory);
    }

    result = getProvider()->readTOC(buffer);
    if (result != kIOReturnSuccess) {
        buffer->release();
        return(result);
    }
    
    toc = (CDTOC *) IOMalloc(tocSize);
    if (toc == NULL) {
        buffer->release();
        return(kIOReturnNoMemory);
    }

    if (buffer->readBytes(0,toc,tocSize) != tocSize) {
        buffer->release();
        IOFree(toc,tocSize);
        return(kIOReturnNoMemory);
    }

    _toc = toc;
    _tocSize = tocSize;

    buffer->release();

    return(result);
}

/* Decommission all nubs. The arbitration lock is assumed to
 * be held during the call.
 */
IOReturn
IOCDBlockStorageDriver::decommissionMedia(bool forcible)
{
    IOReturn result;

    result = super::decommissionMedia(forcible);

    if (result == kIOReturnSuccess) {
        if (_toc) {
            IOFree(_toc,_tocSize);
            _toc = NULL;
            _tocSize = 0;
        }

        _minBlockNumberAudio = 0;
        _maxBlockNumberAudio = 0;
    }

    return(result);
}

/* We should check with other clients using the other nubs before we allow
 * the client of the IOCDMedia to eject the media.
 */
IOReturn
IOCDBlockStorageDriver::ejectMedia(void)
{
    /* For now, we don't check with the other clients. */
    
    return(super::ejectMedia());
}

void
IOCDBlockStorageDriver::executeRequest(UInt64 byteStart,
                                       IOMemoryDescriptor *buffer,
                                       IOStorageAttributes *attributes,
                                       IOStorageCompletion *completion,
                                       IOBlockStorageDriver::Context *context)
{
    UInt32 block;
    UInt32 nblks;
    IOReturn result;

    if (!_mediaObject) {		/* no media? you lose */
        complete(completion, kIOReturnNoMedia,0);
        return;
    }

    /* We know that we are never called with a request too large,
     * nor one that is misaligned with a block.
     */
    assert((byteStart           % context->block.size) == 0);
    assert((buffer->getLength() % context->block.size) == 0);
    
    block = byteStart           / context->block.size;
    nblks = buffer->getLength() / context->block.size;

/* Now the protocol-specific provider implements the actual
     * start of the data transfer: */

    if (context->block.type == kBlockTypeCD) {
        /* Some drives have firmware that performs better at audio reads when
         * the block type is specifically set to CDDA, rather than "unknown";
         * this is a temporary measure until all clients cut over to the CDDA
         * based APIs; this measure should not be depended upon in the future.
         */
        if (context->block.typeSub[1] == kCDSectorTypeUnknown &&
            block                     >= _minBlockNumberAudio &&
            block + nblks - 1         <= _maxBlockNumberAudio) {
            context->block.typeSub[1] = kCDSectorTypeCDDA;

            if (context->block.typeSub[0] & 0xF8) {
                context->block.typeSub[0] &= ~(0xF8);
                context->block.typeSub[0] |= kCDSectorAreaUser;
            }
        }

        if (buffer->getDirection() == kIODirectionIn) {
            result = getProvider()->doAsyncReadCD(buffer,block,nblks,
                                   (CDSectorArea)context->block.typeSub[0],
                                   (CDSectorType)context->block.typeSub[1],
                                   completion ? *completion : (IOStorageCompletion) { 0 });
        } else {
            complete(completion,kIOReturnUnsupported);
            return;
        }
    } else {
        result = getProvider()->doAsyncReadWrite(buffer,block,nblks,attributes,completion);
    }

    if (result != kIOReturnSuccess) {		/* it failed to start */
        complete(completion,result);
        return;
    }
}

void
IOCDBlockStorageDriver::free(void)
{
    if (_expansionData) {
        IODelete(_expansionData, ExpansionData, 1);
    }

    super::free();
}

const char *
IOCDBlockStorageDriver::getDeviceTypeName(void)
{
    return(kIOBlockStorageDeviceTypeCDROM);
}

UInt64
IOCDBlockStorageDriver::getMediaBlockSize(CDSectorArea area,CDSectorType type)
{
    UInt64 blockSize = 0;

    const SInt16 areaSize[kCDSectorTypeCount][8] =
    {                  /* 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 */
       /* Unknown    */ {   96,  294,   16,  280, 2048,    4,    8,   12 },
       /* CDDA       */ {   96,  294,   16,    0, 2352,    0,    0,    0 },
       /* Mode1      */ {   96,  294,   16,  288, 2048,    4,    0,   12 },
       /* Mode2      */ {   96,  294,   16,    0, 2336,    4,    0,   12 },
       /* Mode2Form1 */ {   96,  294,   16,  280, 2048,    4,    8,   12 },
       /* Mode2Form2 */ {   96,  294,   16,    0, 2328,    4,    8,   12 },
    };

    if ( type >= kCDSectorTypeCount )  return 0;

    for ( UInt32 index = 0; index < 8; index++ )
    {
        if ( ((area >> index) & 0x01) )
        {
            if ( areaSize[type][index] == -1 )  return 0;
            blockSize += areaSize[type][index];
        }
    }

    return blockSize;
}

UInt32
IOCDBlockStorageDriver::getMediaType(void)
{
    return(_mediaType);
}

CDTOC *
IOCDBlockStorageDriver::getTOC(void)
{
    return(_toc);
}

bool
IOCDBlockStorageDriver::init(OSDictionary * properties)
{
    if (super::init(properties) == false) {
        return false;
    }

    _expansionData = IONew(ExpansionData, 1);

    if (_expansionData == NULL) {
        return false;
    }

    _minBlockNumberAudio = 0;
    _maxBlockNumberAudio = 0;
    _toc = NULL;
    _tocSize = 0;

    return(true);
}

IOMedia *
IOCDBlockStorageDriver::instantiateDesiredMediaObject(void)
{
    return(new IOCDMedia);
}

IOMedia *
IOCDBlockStorageDriver::instantiateMediaObject(UInt64 base,UInt64 byteSize,
                                        UInt32 blockSize,char *mediaName)
{
    IOMedia *media;

    if (blockSize) {
        byteSize /= blockSize;
        byteSize *= kBlockSizeCD;
        blockSize = kBlockSizeCD;
    }

    media = super::instantiateMediaObject(base,byteSize,blockSize,mediaName);

    if (media) {
        const char *description = NULL;
        const char *picture = NULL;

        switch (getMediaType()) {
            case kCDMediaTypeROM:
                description = kIOCDMediaTypeROM;
                picture = "CD.icns";
                break;
            case kCDMediaTypeR:
                description = kIOCDMediaTypeR;
                picture = "CD-R.icns";
                break;
            case kCDMediaTypeRW:
                description = kIOCDMediaTypeRW;
                picture = "CD-RW.icns";
                break;
        }

        if (description) {
            media->setProperty(kIOCDMediaTypeKey, description);
        }

        if (picture) {
            OSDictionary *dictionary = OSDictionary::withCapacity(2);
            OSString *identifier = OSString::withCString("com.apple.iokit.IOCDStorageFamily");
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

        if (_toc) {
            media->setProperty(kIOCDMediaTOCKey,(void*)_toc,_tocSize);
        }
    }

    return media;
}

void
IOCDBlockStorageDriver::readCD(IOService *client,
                               UInt64 byteStart,
                               IOMemoryDescriptor *buffer,
                               CDSectorArea sectorArea,
                               CDSectorType sectorType,
                               IOStorageAttributes *attributes,
                               IOStorageCompletion *completion)
{
    assert(buffer->getDirection() == kIODirectionIn);

    prepareRequest(byteStart, buffer, sectorArea, sectorType, attributes, completion);
}

IOReturn
IOCDBlockStorageDriver::reportDiscInfo(CDDiscInfo *discInfo)
{
    IOMemoryDescriptor *buffer;
    IOReturn result;
    UInt16 discInfoSize;

    bzero(discInfo,sizeof(CDDiscInfo));

    /* Read the Disc Information in full: */

    buffer = IOMemoryDescriptor::withAddress(discInfo,sizeof(CDDiscInfo),kIODirectionIn);
    if (buffer == NULL) {
        return(kIOReturnNoMemory);
    }

    result = getProvider()->readDiscInfo(buffer,&discInfoSize);
    if (result != kIOReturnSuccess) {
        buffer->release();
        return(result);
    }

    buffer->release();

    /* Reject the Disc Information if its size is too small: */

    if (discInfoSize < sizeof(CDDiscInfo)) {
        return(kIOReturnNotFound);
    }

    discInfoSize = OSSwapBigToHostInt16(discInfo->dataLength) + sizeof(discInfo->dataLength);

    if (discInfoSize < sizeof(CDDiscInfo)) {
        return(kIOReturnNotFound);
    }

    return(result);
}

IOReturn
IOCDBlockStorageDriver::reportTrackInfo(UInt16 track,CDTrackInfo *trackInfo)
{
    IOMemoryDescriptor *buffer;
    IOReturn result;
    UInt16 trackInfoSize;

    bzero(trackInfo,sizeof(CDTrackInfo));

    /* Read the Track Information in full: */

    buffer = IOMemoryDescriptor::withAddress(trackInfo,sizeof(CDTrackInfo),kIODirectionIn);
    if (buffer == NULL) {
        return(kIOReturnNoMemory);
    }

    result = getProvider()->readTrackInfo(buffer,track,kCDTrackInfoAddressTypeTrackNumber,&trackInfoSize);
    if (result != kIOReturnSuccess) {
        buffer->release();
        return(result);
    }

    buffer->release();

    /* Reject the Track Information if its size is too small: */

    if (trackInfoSize < offsetof(CDTrackInfo, lastRecordedAddress)) {
        return(kIOReturnNotFound);
    }

    trackInfoSize = OSSwapBigToHostInt16(trackInfo->dataLength) + sizeof(trackInfo->dataLength);

    if (trackInfoSize < offsetof(CDTrackInfo, lastRecordedAddress)) {
        return(kIOReturnNotFound);
    }

    return(result);
}

void
IOCDBlockStorageDriver::prepareRequest(UInt64 byteStart,
                                       IOMemoryDescriptor *buffer,
                                       CDSectorArea sectorArea,
                                       CDSectorType sectorType,
                                       IOStorageAttributes *attributes,
                                       IOStorageCompletion *completion)
{
    IOStorageCompletion completionOut; 
    Context *           context;

    // Determine whether an undefined sector area was specified.

    if ((sectorArea & 0xFF) == 0x00 || (sectorArea & 0x05) == 0x05)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    // Determine whether an undefined sector type was specified.

    if (sectorType >= kCDSectorTypeCount)
    {
        complete(completion, kIOReturnBadArgument);
        return;
    }

    // For a transfer that involves an unknown sector type, the sector area must
    // not describe a vague sector size (and hence, a vague transfer size).  The
    // SYNC, HEADER, SUBHEADER, USER and AUXILIARY sector areas differ in length
    // from one sector type to the next.  All together, however, the same sector
    // areas describe a consistent sector size from one sector type to the next,
    // hence we permit either all the above sector areas to be specified at once
    // or none to be specified at all.  The other sector areas are consistent in
    // size from one sector type to the next, hence need not be considered here.

    if (sectorType == kCDSectorTypeUnknown)
    {
        if ((sectorArea & 0xF8) != 0x00 && (sectorArea & 0xF8) != 0xF8)
        {
            complete(completion, kIOReturnBadArgument);
            return;
        }
    }

    // Allocate a context structure to hold some of our state.

    context = allocateContext();

    if (context == 0)
    {
        complete(completion, kIOReturnNoMemory);
        return;
    }
    
    // Fill in the context structure with some of our state.

    if ( ( sectorArea == kCDSectorAreaUser       )  &&
         ( sectorType == kCDSectorTypeMode1      ||
           sectorType == kCDSectorTypeMode2Form1 )  )
    {
        context->block.size       = getMediaBlockSize();
        context->block.type       = kBlockTypeStandard;
    }
    else
    {
        context->block.size       = getMediaBlockSize(sectorArea, sectorType);
        context->block.type       = kBlockTypeCD;
        context->block.typeSub[0] = sectorArea;
        context->block.typeSub[1] = sectorType;    
    }

    context->request.byteStart  = byteStart;
    context->request.buffer     = buffer;
    context->request.buffer->retain();

    if (attributes)  context->request.attributes = *attributes;
    if (completion)  context->request.completion = *completion;

    clock_get_uptime(&context->timeStart);

    retain();
    completionOut.target    = this;
    completionOut.action    = prepareRequestCompletion;
    completionOut.parameter = context;

    // Deblock the transfer.

    deblockRequest(byteStart, buffer, attributes, &completionOut, context);
}

IOReturn
IOCDBlockStorageDriver::readISRC(UInt8 track,CDISRC isrc)
{
    return(getProvider()->readISRC(track,isrc));
}

IOReturn
IOCDBlockStorageDriver::readMCN(CDMCN mcn)
{
    return(getProvider()->readMCN(mcn));
}

IOReturn
IOCDBlockStorageDriver::recordMediaParameters(void)
{
    IOReturn result;

    result = super::recordMediaParameters();
    if (result != kIOReturnSuccess) {
        return(result);
    }

    _mediaType = getProvider()->getMediaType();

    return(kIOReturnSuccess);
}

IOReturn
IOCDBlockStorageDriver::getSpeed(UInt16 * kilobytesPerSecond)
{
    return(getProvider()->getSpeed(kilobytesPerSecond));
}

IOReturn
IOCDBlockStorageDriver::setSpeed(UInt16 kilobytesPerSecond)
{
    return(getProvider()->setSpeed(kilobytesPerSecond));
}

IOReturn
IOCDBlockStorageDriver::readTOC(IOMemoryDescriptor *buffer,CDTOCFormat format,
                                UInt8 formatAsTime,UInt8 trackOrSessionNumber,
                                UInt16 *actualByteCount)
{
    return(getProvider()->readTOC(buffer,format,formatAsTime,trackOrSessionNumber,actualByteCount));
}

IOReturn
IOCDBlockStorageDriver::readDiscInfo(IOMemoryDescriptor *buffer,
                                     UInt16 *actualByteCount)
{
    return(getProvider()->readDiscInfo(buffer,actualByteCount));
}

IOReturn
IOCDBlockStorageDriver::readTrackInfo(IOMemoryDescriptor *buffer,UInt32 address,
                                      CDTrackInfoAddressType addressType,
                                      UInt16 *actualByteCount)
{
    return(getProvider()->readTrackInfo(buffer,address,addressType,actualByteCount));
}

void
IOCDBlockStorageDriver::writeCD(IOService *client,
                                UInt64 byteStart,
                                IOMemoryDescriptor *buffer,
                                CDSectorArea sectorArea,
                                CDSectorType sectorType,
                                IOStorageAttributes *attributes,
                                IOStorageCompletion *completion)
{
    assert(buffer->getDirection() == kIODirectionOut);

    prepareRequest(byteStart, buffer, sectorArea, sectorType, attributes, completion);
}

OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  0);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  1);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  2);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  3);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  4);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  5);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  6);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  7);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  8);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver,  9);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 10);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 11);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 12);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 13);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 14);
OSMetaClassDefineReservedUnused(IOCDBlockStorageDriver, 15);
