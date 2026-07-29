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

#include <sys/errno.h>
#include <sys/proc.h>
#include <IOKit/storage/IOBDMediaBSDClient.h>

#define super IOMediaBSDClient
OSDefineMetaClassAndStructors(IOBDMediaBSDClient, IOMediaBSDClient)

typedef struct
{
    uint8_t       format;

    uint8_t       reserved0008[3];

    uint32_t      address;
    uint8_t       grantID;
    uint8_t       layer;

    uint8_t       reserved0080[4];

    uint16_t      bufferLength;
    user32_addr_t buffer;
} dk_bd_read_structure_32_t;

typedef struct
{
    uint8_t       format;

    uint8_t       reserved0008[3];

    uint32_t      address;
    uint8_t       grantID;
    uint8_t       layer;

    uint8_t       reserved0080[4];

    uint16_t      bufferLength;
    user64_addr_t buffer;
} dk_bd_read_structure_64_t;

typedef struct
{
    uint8_t       format;
    uint8_t       keyClass;
    uint8_t       blockCount;

    uint8_t       reserved0024[1];

    uint32_t      address;
    uint8_t       grantID;

    uint8_t       reserved0072[5];

    uint16_t      bufferLength;
    user32_addr_t buffer;
} dk_bd_report_key_32_t;

typedef struct
{
    uint8_t       format;
    uint8_t       keyClass;
    uint8_t       blockCount;

    uint8_t       reserved0024[1];

    uint32_t      address;
    uint8_t       grantID;

    uint8_t       reserved0072[5];

    uint16_t      bufferLength;
    user64_addr_t buffer;
} dk_bd_report_key_64_t;

typedef struct
{
    uint8_t       format;
    uint8_t       keyClass;

    uint8_t       reserved0016[6];

    uint8_t       grantID;

    uint8_t       reserved0072[5];

    uint16_t      bufferLength;
    user32_addr_t buffer;
} dk_bd_send_key_32_t;

typedef struct
{
    uint8_t       format;
    uint8_t       keyClass;

    uint8_t       reserved0016[6];

    uint8_t       grantID;

    uint8_t       reserved0072[5];

    uint16_t      bufferLength;
    user64_addr_t buffer;
} dk_bd_send_key_64_t;

typedef struct
{
    uint8_t       reserved0000[14];

    uint16_t      bufferLength;
    user32_addr_t buffer;
} dk_bd_read_disc_info_32_t;

typedef struct
{
    uint8_t       reserved0000[14];

    uint16_t      bufferLength;
    user64_addr_t buffer;
} dk_bd_read_disc_info_64_t;

typedef struct
{
    uint8_t       reserved0000[4];

    uint32_t      address;
    uint8_t       addressType;

    uint8_t       reserved0072[5];

    uint16_t      bufferLength;
    user32_addr_t buffer;
} dk_bd_read_track_info_32_t;

typedef struct
{
    uint8_t       reserved0000[4];

    uint32_t      address;
    uint8_t       addressType;

    uint8_t       reserved0072[5];

    uint16_t      bufferLength;
    user64_addr_t buffer;
} dk_bd_read_track_info_64_t;

#define DKIOCBDREADSTRUCTURE32   _IOW('d', 160, dk_bd_read_structure_32_t)
#define DKIOCBDREADSTRUCTURE64   _IOW('d', 160, dk_bd_read_structure_64_t)
#define DKIOCBDREPORTKEY32       _IOW('d', 161, dk_bd_report_key_32_t)
#define DKIOCBDREPORTKEY64       _IOW('d', 161, dk_bd_report_key_64_t)
#define DKIOCBDSENDKEY32         _IOW('d', 162, dk_bd_send_key_32_t)
#define DKIOCBDSENDKEY64         _IOW('d', 162, dk_bd_send_key_64_t)

#define DKIOCBDREADDISCINFO32    _IOWR('d', 164, dk_bd_read_disc_info_32_t)
#define DKIOCBDREADDISCINFO64    _IOWR('d', 164, dk_bd_read_disc_info_64_t)
#define DKIOCBDREADTRACKINFO32   _IOWR('d', 165, dk_bd_read_track_info_32_t)
#define DKIOCBDREADTRACKINFO64   _IOWR('d', 165, dk_bd_read_track_info_64_t)

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

IOBDMedia * IOBDMediaBSDClient::getProvider() const
{
    //
    // Obtain this object's provider.  We override the superclass's method
    // to return a more specific subclass of IOService -- IOBDMedia.  This
    // method serves simply as a convenience to subclass developers.
    //

    return (IOBDMedia *) IOService::getProvider();
}

int IOBDMediaBSDClient::ioctl( dev_t   dev,
                               u_long  cmd,
                               caddr_t data,
                               int     flags,
                               proc_t  proc )
{
    //
    // Process a BD-specific ioctl.
    //

    IOMemoryDescriptor * buffer = 0;
    int                  error  = 0;
    IOReturn             status = kIOReturnSuccess;

    switch ( cmd )
    {
        case DKIOCBDREADSTRUCTURE32:            // (dk_bd_read_structure_32_t *)
        {
            dk_bd_read_structure_32_t * request;

            request = (dk_bd_read_structure_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3C0E) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ CAST_USER_ADDR_T(request->buffer),
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readStructure(
                       /* buffer    */ buffer,
                       /* format    */ request->format,
                       /* address   */ request->address,
                       /* layer     */ request->layer,
                       /* grantID   */ request->grantID );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDREADSTRUCTURE64:            // (dk_bd_read_structure_64_t *)
        {
            dk_bd_read_structure_64_t * request;

            request = (dk_bd_read_structure_64_t *) data;

            if ( proc_is64bit(proc) == 0 )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3C0E) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readStructure(
                       /* buffer    */ buffer,
                       /* format    */ request->format,
                       /* address   */ request->address,
                       /* layer     */ request->layer,
                       /* grantID   */ request->grantID );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDREPORTKEY32:                    // (dk_bd_report_key_32_t *)
        {
            dk_bd_report_key_32_t * request = (dk_bd_report_key_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3E08) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ CAST_USER_ADDR_T(request->buffer),
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->reportKey(
                       /* buffer     */ buffer,
                       /* keyClass   */ request->keyClass,
                       /* address    */ request->address,
                       /* blockCount */ request->blockCount,
                       /* grantID    */ request->grantID,
                       /* format     */ request->format );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDREPORTKEY64:                    // (dk_bd_report_key_64_t *)
        {
            dk_bd_report_key_64_t * request = (dk_bd_report_key_64_t *) data;

            if ( proc_is64bit(proc) == 0 )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3E08) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->reportKey(
                       /* buffer     */ buffer,
                       /* keyClass   */ request->keyClass,
                       /* address    */ request->address,
                       /* blockCount */ request->blockCount,
                       /* grantID    */ request->grantID,
                       /* format     */ request->format );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDSENDKEY32:                        // (dk_bd_send_key_32_t *)
        {
            dk_bd_send_key_32_t * request = (dk_bd_send_key_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3EFC) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ CAST_USER_ADDR_T(request->buffer),
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionOut,
                       /* proc      */ proc );

            status = getProvider()->sendKey(
                       /* buffer    */ buffer,
                       /* keyClass  */ request->keyClass,
                       /* grantID   */ request->grantID,
                       /* format    */ request->format );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDSENDKEY64:                        // (dk_bd_send_key_64_t *)
        {
            dk_bd_send_key_64_t * request = (dk_bd_send_key_64_t *) data;

            if ( proc_is64bit(proc) == 0 )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3EFC) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionOut,
                       /* proc      */ proc );

            status = getProvider()->sendKey(
                       /* buffer    */ buffer,
                       /* keyClass  */ request->keyClass,
                       /* grantID   */ request->grantID,
                       /* format    */ request->format );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDGETSPEED:                                    // (uint16_t *)
        {
            status = getProvider()->getSpeed((uint16_t *)data);

        } break;

        case DKIOCBDSETSPEED:                                    // (uint16_t *)
        {
            status = getProvider()->setSpeed(*(uint16_t *)data);

        } break;

        case DKIOCBDREADDISCINFO32:             // (dk_bd_read_disc_info_32_t *)
        {
            dk_bd_read_disc_info_32_t * request;

            request = (dk_bd_read_disc_info_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3FFF) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ CAST_USER_ADDR_T(request->buffer),
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readDiscInfo(
                       /* buffer          */ buffer,
                       /* type            */ 0,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDREADDISCINFO64:             // (dk_bd_read_disc_info_64_t *)
        {
            dk_bd_read_disc_info_64_t * request;

            request = (dk_bd_read_disc_info_64_t *) data;

            if ( proc_is64bit(proc) == 0 )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3FFF) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readDiscInfo(
                       /* buffer          */ buffer,
                       /* type            */ 0,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDREADTRACKINFO32:           // (dk_bd_read_track_info_32_t *)
        {
            dk_bd_read_track_info_32_t * request;

            request = (dk_bd_read_track_info_32_t *) data;

            if ( proc_is64bit(proc) )  { error = ENOTTY;  break; }

            if ( DKIOC_IS_RESERVED(data, 0x3E0F) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ CAST_USER_ADDR_T(request->buffer),
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn,
                       /* proc      */ proc );

            status = getProvider()->readTrackInfo(
                       /* buffer          */ buffer,
                       /* address         */ request->address,
                       /* addressType     */ request->addressType,
                       /* open            */ 0,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDREADTRACKINFO64:           // (dk_bd_read_track_info_64_t *)
        {
            dk_bd_read_track_info_64_t * request;

            request = (dk_bd_read_track_info_64_t *) data;

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
                       /* open            */ 0,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCBDSPLITTRACK:                        // splitTrack(uint32_t *)
        {
            status = getProvider()->splitTrack(*(uint32_t *)data);

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

OSMetaClassDefineReservedUnused(IOBDMediaBSDClient, 0);
OSMetaClassDefineReservedUnused(IOBDMediaBSDClient, 1);
OSMetaClassDefineReservedUnused(IOBDMediaBSDClient, 2);
OSMetaClassDefineReservedUnused(IOBDMediaBSDClient, 3);
OSMetaClassDefineReservedUnused(IOBDMediaBSDClient, 4);
OSMetaClassDefineReservedUnused(IOBDMediaBSDClient, 5);
OSMetaClassDefineReservedUnused(IOBDMediaBSDClient, 6);
OSMetaClassDefineReservedUnused(IOBDMediaBSDClient, 7);
