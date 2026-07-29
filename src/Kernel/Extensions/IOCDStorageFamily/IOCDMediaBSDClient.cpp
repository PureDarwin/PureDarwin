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

#include <sys/errno.h>
#include <sys/proc.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>

#define super IOMediaBSDClient
OSDefineMetaClassAndStructors(IOCDMediaBSDClient, IOMediaBSDClient)

typedef struct
{
    uint64_t      offset;

    uint8_t       sectorArea;
    uint8_t       sectorType;

    uint8_t       reserved0080[6];

    uint32_t      bufferLength;
    user32_addr_t buffer;
} dk_cd_read_32_t;

typedef struct
{
    uint64_t      offset;

    uint8_t       sectorArea;
    uint8_t       sectorType;

    uint8_t       reserved0080[10];

    uint32_t      bufferLength;
    user64_addr_t buffer;
} dk_cd_read_64_t;

typedef struct
{
    uint8_t       format;
    uint8_t       formatAsTime;

    uint8_t       reserved0016[5];

    union
    {
        uint8_t   session;
        uint8_t   track;
    } address;

    uint8_t       reserved0064[2];

    uint16_t      bufferLength;
    user32_addr_t buffer;
} dk_cd_read_toc_32_t;

typedef struct
{
    uint8_t       format;
    uint8_t       formatAsTime;

    uint8_t       reserved0016[5];

    union
    {
        uint8_t   session;
        uint8_t   track;
    } address;

    uint8_t       reserved0064[6];

    uint16_t      bufferLength;
    user64_addr_t buffer;
} dk_cd_read_toc_64_t;

typedef struct
{
    uint8_t       reserved0000[10];

    uint16_t      bufferLength;
    user32_addr_t buffer;
} dk_cd_read_disc_info_32_t;

typedef struct
{
    uint8_t       reserved0000[14];

    uint16_t      bufferLength;
    user64_addr_t buffer;
} dk_cd_read_disc_info_64_t;

typedef struct
{
    uint8_t       reserved0000[4];

    uint32_t      address;
    uint8_t       addressType;

    uint8_t       reserved0072[1];

    uint16_t      bufferLength;
    user32_addr_t buffer;
} dk_cd_read_track_info_32_t;

typedef struct
{
    uint8_t       reserved0000[4];

    uint32_t      address;
    uint8_t       addressType;

    uint8_t       reserved0072[5];

    uint16_t      bufferLength;
    user64_addr_t buffer;
} dk_cd_read_track_info_64_t;

#define DKIOCCDREAD32          _IOWR('d', 96, dk_cd_read_32_t)
#define DKIOCCDREAD64          _IOWR('d', 96, dk_cd_read_64_t)

#define DKIOCCDREADTOC32       _IOWR('d', 100, dk_cd_read_toc_32_t)
#define DKIOCCDREADTOC64       _IOWR('d', 100, dk_cd_read_toc_64_t)

#define DKIOCCDREADDISCINFO32  _IOWR('d', 101, dk_cd_read_disc_info_32_t)
#define DKIOCCDREADDISCINFO64  _IOWR('d', 101, dk_cd_read_disc_info_64_t)
#define DKIOCCDREADTRACKINFO32 _IOWR('d', 102, dk_cd_read_track_info_32_t)
#define DKIOCCDREADTRACKINFO64 _IOWR('d', 102, dk_cd_read_track_info_64_t)

static bool DKIOC_IS_RESERVED(caddr_t data, uint32_t reserved)
{
    UInt32 index;

    for ( index = 0; index < sizeof(reserved) * 8; index++, reserved >>= 1 )
    {
        if ( (reserved & 1) )
        {
            if ( data[index] )  return true;
        }
    }

    return false;
}

static IOMemoryDescriptor * DKIOC_PREPARE_BUFFER( user_addr_t address,
                                                  UInt32      length,
                                                  IODirection direction,
                                                  proc_t      proc )
{
    IOMemoryDescriptor * buffer = 0;

    if ( address && length )
    {
        buffer = IOMemoryDescriptor::withAddressRange(    // (create the buffer)
            /* address */ address,
            /* length  */ length,
            /* options */ direction,
            /* task    */ (proc == kernproc) ? kernel_task : current_task() );
    }

    if ( buffer )
    {
        if ( buffer->prepare() != kIOReturnSuccess )     // (prepare the buffer)
        {
            buffer->release();
            buffer = 0;
        }
    }

    return buffer;
}

static void DKIOC_COMPLETE_BUFFER(IOMemoryDescriptor * buffer)
{
    if ( buffer )
    {
        buffer->complete();                             // (complete the buffer)
        buffer->release();                               // (release the buffer)
    }
}

IOCDMedia * IOCDMediaBSDClient::getProvider() const
{
    //
    // Obtain this object's provider.  We override the superclass's method
    // to return a more specific subclass of IOService -- IOCDMedia.  This
    // method serves simply as a convenience to subclass developers.
    //

    return (IOCDMedia *) IOService::getProvider();
}

int IOCDMediaBSDClient::ioctl( dev_t   dev,
                               u_long  cmd,
                               caddr_t data,
                               int     flags,
                               proc_t  proc )
{
    //
    // Process a CD-specific ioctl.
    //

    IOMemoryDescriptor * buffer = 0;
    int                  error  = 0;
    IOReturn             status = kIOReturnSuccess;

    switch ( cmd )
    {
        case DKIOCCDREAD32:                               // (dk_cd_read_32_t *)
        {
            UInt64            actualByteCount = 0;
            dk_cd_read_32_t * request         = (dk_cd_read_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0xFC00) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readCD(
                       /* client          */                this,
                       /* byteStart       */                request->offset,
                       /* buffer          */                buffer,
                       /* sectorArea      */ (CDSectorArea) request->sectorArea,
                       /* sectorType      */ (CDSectorType) request->sectorType,
                       /* attributes      */                NULL,
                       /* actualByteCount */                &actualByteCount );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            request->bufferLength = (UInt32) actualByteCount;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREAD64:                               // (dk_cd_read_64_t *)
        {
            UInt64            actualByteCount = 0;
            dk_cd_read_64_t * request         = (dk_cd_read_64_t *) data;

            if ( proc_is64bit(proc) == 0 )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0xFFC00) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readCD(
                       /* client          */                this,
                       /* byteStart       */                request->offset,
                       /* buffer          */                buffer,
                       /* sectorArea      */ (CDSectorArea) request->sectorArea,
                       /* sectorType      */ (CDSectorType) request->sectorType,
                       /* attributes      */                NULL,
                       /* actualByteCount */                &actualByteCount );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            request->bufferLength = (UInt32) actualByteCount;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADISRC:                           // (dk_cd_read_isrc_t *)
        {
            dk_cd_read_isrc_t * request = (dk_cd_read_isrc_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0xC000) )  { error = EINVAL;  break; }

            status = getProvider()->readISRC(request->track, request->isrc);

        } break;

        case DKIOCCDREADMCN:                             // (dk_cd_read_mcn_t *)
        {
            dk_cd_read_mcn_t * request = (dk_cd_read_mcn_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0xC000) )  { error = EINVAL;  break; }

            status = getProvider()->readMCN(request->mcn);

        } break;

        case DKIOCCDGETSPEED:                                    // (uint16_t *)
        {
            status = getProvider()->getSpeed((uint16_t *)data);

        } break;

        case DKIOCCDSETSPEED:                                    // (uint16_t *)
        {
            status = getProvider()->setSpeed(*(uint16_t *)data);

        } break;

        case DKIOCCDREADTOC32:                        // (dk_cd_read_toc_32_t *)
        {
            dk_cd_read_toc_32_t * request = (dk_cd_read_toc_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x37C) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readTOC(
                       /* buffer          */ buffer,
                       /* format          */ request->format,
                       /* formatAsTime    */ request->formatAsTime,
                       /* address         */ request->address.session,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADTOC64:                        // (dk_cd_read_toc_64_t *)
        {
            dk_cd_read_toc_64_t * request = (dk_cd_read_toc_64_t *) data;

            if ( proc_is64bit(proc) == 0 )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3F7C) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readTOC(
                       /* buffer          */ buffer,
                       /* format          */ request->format,
                       /* formatAsTime    */ request->formatAsTime,
                       /* address         */ request->address.session,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADDISCINFO32:             // (dk_cd_read_disc_info_32_t *)
        {
            dk_cd_read_disc_info_32_t * request;

            request = (dk_cd_read_disc_info_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3FF) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readDiscInfo(
                       /* buffer          */ buffer,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADDISCINFO64:             // (dk_cd_read_disc_info_64_t *)
        {
            dk_cd_read_disc_info_64_t * request;

            request = (dk_cd_read_disc_info_64_t *) data;

            if ( proc_is64bit(proc) == 0 )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3FFF) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readDiscInfo(
                       /* buffer          */ buffer,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADTRACKINFO32:           // (dk_cd_read_track_info_32_t *)
        {
            dk_cd_read_track_info_32_t * request;

            request = (dk_cd_read_track_info_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x20F) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readTrackInfo(
                       /* buffer          */ buffer,
                       /* address         */ request->address,
                       /* addressType     */ request->addressType,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADTRACKINFO64:           // (dk_cd_read_track_info_64_t *)
        {
            dk_cd_read_track_info_64_t * request;

            request = (dk_cd_read_track_info_64_t *) data;

            if ( proc_is64bit(proc) == 0 )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3E0F) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readTrackInfo(
                       /* buffer          */ buffer,
                       /* address         */ request->address,
                       /* addressType     */ request->addressType,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        default:
        {
            //
            // A foreign ioctl was received.  Ask our superclass' opinion.
            //

            error = super::ioctl(dev, cmd, data, flags, proc);

        } break;
    }

    return error ? error : getProvider()->errnoFromReturn(status);
}

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 0);
OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 1);
OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 2);
OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 3);
OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 4);
OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 5);
OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 6);
OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 7);
