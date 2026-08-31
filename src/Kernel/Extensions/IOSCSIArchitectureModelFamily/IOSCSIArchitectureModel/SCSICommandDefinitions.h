/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_SCSI_COMMAND_DEFINITIONS_H_
#define _IOKIT_SCSI_COMMAND_DEFINITIONS_H_


#if KERNEL
#include <IOKit/IOTypes.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif


#pragma mark About this file
/* This file contains all the definitions for types and constants that are
 * used by the command set classes for building CDBs.  The field type
 * definitions are used for the parameters passed to a method that builds and
 * sends any SCSI defined command to clearly identify the type of value
 * expected for a parameter.
 * The command methods will then use the appropriate mask to verify that the
 * value passed into a parameter is of the specified type.
 * Currently only types and masks are defined for 4 bytes and smaller fields.
 * If a command is defined that uses a larger field, these should be expanded
 * to include those sizes.
 */ 

#pragma mark Field Type Definitions
/* These are the type definitions used for the parameters of methods that
 * build and send Command Descriptor Blocks.
 */

/* 1 Byte or smaller fields. */
typedef UInt8	SCSICmdField1Bit;
typedef UInt8	SCSICmdField2Bit;
typedef UInt8	SCSICmdField3Bit;
typedef UInt8	SCSICmdField4Bit;
typedef UInt8	SCSICmdField5Bit;
typedef UInt8	SCSICmdField6Bit;
typedef UInt8	SCSICmdField7Bit;
typedef UInt8	SCSICmdField1Byte;

/* 2 Bytes or smaller fields. */
typedef UInt16	SCSICmdField9Bit;
typedef UInt16	SCSICmdField10Bit;
typedef UInt16	SCSICmdField11Bit;
typedef UInt16	SCSICmdField12Bit;
typedef UInt16	SCSICmdField13Bit;
typedef UInt16	SCSICmdField14Bit;
typedef UInt16	SCSICmdField15Bit;
typedef UInt16	SCSICmdField2Byte;

/* 3 Bytes or smaller fields. */
typedef UInt32	SCSICmdField17Bit;
typedef UInt32	SCSICmdField18Bit;
typedef UInt32	SCSICmdField19Bit;
typedef UInt32	SCSICmdField20Bit;
typedef UInt32	SCSICmdField21Bit;
typedef UInt32	SCSICmdField22Bit;
typedef UInt32	SCSICmdField23Bit;
typedef UInt32	SCSICmdField3Byte;

/* 4 Bytes or smaller fields. */
typedef UInt32	SCSICmdField25Bit;
typedef UInt32	SCSICmdField26Bit;
typedef UInt32	SCSICmdField27Bit;
typedef UInt32	SCSICmdField28Bit;
typedef UInt32	SCSICmdField29Bit;
typedef UInt32	SCSICmdField30Bit;
typedef UInt32	SCSICmdField31Bit;
typedef UInt32	SCSICmdField4Byte;

/* 5 Bytes or smaller fields. */
typedef UInt64	SCSICmdField33Bit;
typedef UInt64	SCSICmdField34Bit;
typedef UInt64	SCSICmdField35Bit;
typedef UInt64	SCSICmdField36Bit;
typedef UInt64	SCSICmdField37Bit;
typedef UInt64	SCSICmdField38Bit;
typedef UInt64	SCSICmdField39Bit;
typedef UInt64	SCSICmdField5Byte;

/* 6 Bytes or smaller fields. */
typedef UInt64	SCSICmdField41Bit;
typedef UInt64	SCSICmdField42Bit;
typedef UInt64	SCSICmdField43Bit;
typedef UInt64	SCSICmdField44Bit;
typedef UInt64	SCSICmdField45Bit;
typedef UInt64	SCSICmdField46Bit;
typedef UInt64	SCSICmdField47Bit;
typedef UInt64	SCSICmdField6Byte;

/* 7 Bytes or smaller fields. */
typedef UInt64	SCSICmdField49Bit;
typedef UInt64	SCSICmdField50Bit;
typedef UInt64	SCSICmdField51Bit;
typedef UInt64	SCSICmdField52Bit;
typedef UInt64	SCSICmdField53Bit;
typedef UInt64	SCSICmdField54Bit;
typedef UInt64	SCSICmdField55Bit;
typedef UInt64	SCSICmdField7Byte;

/* 8 Bytes or smaller fields. */
typedef UInt64	SCSICmdField57Bit;
typedef UInt64	SCSICmdField58Bit;
typedef UInt64	SCSICmdField59Bit;
typedef UInt64	SCSICmdField60Bit;
typedef UInt64	SCSICmdField61Bit;
typedef UInt64	SCSICmdField62Bit;
typedef UInt64	SCSICmdField63Bit;
typedef UInt64	SCSICmdField8Byte;


#pragma mark Field Mask Definitions
/* These are masks that are used to verify that the values passed into the
 * parameters for the fields are not larger than the field size.
 *
 * NB: These have changed from enums to #define since enums greater than
 * 32 bits in size are not well-defined in C99.
 */

/* 1 Byte or smaller fields. */
#define 	kSCSICmdFieldMask1Bit		0x01
#define 	kSCSICmdFieldMask2Bit		0x03
#define 	kSCSICmdFieldMask3Bit		0x07
#define 	kSCSICmdFieldMask4Bit		0x0F
#define 	kSCSICmdFieldMask5Bit		0x1F
#define 	kSCSICmdFieldMask6Bit		0x3F
#define 	kSCSICmdFieldMask7Bit		0x7F
#define 	kSCSICmdFieldMask1Byte		0xFF

/* 2 Bytes or smaller fields. */
#define 	kSCSICmdFieldMask9Bit		0x01FF
#define 	kSCSICmdFieldMask10Bit		0x03FF
#define 	kSCSICmdFieldMask11Bit		0x07FF
#define 	kSCSICmdFieldMask12Bit		0x0FFF
#define 	kSCSICmdFieldMask13Bit		0x1FFF
#define 	kSCSICmdFieldMask14Bit		0x3FFF
#define 	kSCSICmdFieldMask15Bit		0x7FFF
#define 	kSCSICmdFieldMask2Byte		0xFFFF

/* 3 Bytes or smaller fields. */
#define 	kSCSICmdFieldMask17Bit		0x01FFFF
#define 	kSCSICmdFieldMask18Bit		0x03FFFF
#define 	kSCSICmdFieldMask19Bit		0x07FFFF
#define 	kSCSICmdFieldMask20Bit		0x0FFFFF
#define 	kSCSICmdFieldMask21Bit		0x1FFFFF
#define 	kSCSICmdFieldMask22Bit		0x3FFFFF
#define 	kSCSICmdFieldMask23Bit		0x7FFFFF
#define 	kSCSICmdFieldMask3Byte		0xFFFFFF

/* 4 Bytes or smaller fields. */
#define 	kSCSICmdFieldMask25Bit		0x01FFFFFFUL
#define 	kSCSICmdFieldMask26Bit		0x03FFFFFFUL
#define 	kSCSICmdFieldMask27Bit		0x07FFFFFFUL
#define 	kSCSICmdFieldMask28Bit		0x0FFFFFFFUL
#define 	kSCSICmdFieldMask29Bit		0x1FFFFFFFUL
#define 	kSCSICmdFieldMask30Bit		0x3FFFFFFFUL
#define 	kSCSICmdFieldMask31Bit		0x7FFFFFFFUL
#define 	kSCSICmdFieldMask4Byte		0xFFFFFFFFUL

/* 5 Bytes or smaller fields. */
#define 	kSCSICmdFieldMask33Bit		0x01FFFFFFFFULL
#define 	kSCSICmdFieldMask34Bit		0x03FFFFFFFFULL
#define 	kSCSICmdFieldMask35Bit		0x07FFFFFFFFULL
#define 	kSCSICmdFieldMask36Bit		0x0FFFFFFFFFULL
#define 	kSCSICmdFieldMask37Bit		0x1FFFFFFFFFULL
#define 	kSCSICmdFieldMask38Bit		0x3FFFFFFFFFULL
#define 	kSCSICmdFieldMask39Bit		0x7FFFFFFFFFULL
#define 	kSCSICmdFieldMask5Byte		0xFFFFFFFFFFULL

/* 6 Bytes or smaller fields. */
#define 	kSCSICmdFieldMask41Bit		0x01FFFFFFFFFFULL
#define 	kSCSICmdFieldMask42Bit		0x03FFFFFFFFFFULL
#define 	kSCSICmdFieldMask43Bit		0x07FFFFFFFFFFULL
#define 	kSCSICmdFieldMask44Bit		0x0FFFFFFFFFFFULL
#define 	kSCSICmdFieldMask45Bit		0x1FFFFFFFFFFFULL
#define 	kSCSICmdFieldMask46Bit		0x3FFFFFFFFFFFULL
#define 	kSCSICmdFieldMask47Bit		0x7FFFFFFFFFFFULL
#define 	kSCSICmdFieldMask6Byte		0xFFFFFFFFFFFFULL

/* 7 Bytes or smaller fields. */
#define 	kSCSICmdFieldMask49Bit		0x01FFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask50Bit		0x03FFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask51Bit		0x07FFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask52Bit		0x0FFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask53Bit		0x1FFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask54Bit		0x3FFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask55Bit		0x7FFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask7Byte		0xFFFFFFFFFFFFFFULL

/* 8 Bytes or smaller fields. */
#define 	kSCSICmdFieldMask57Bit		0x01FFFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask58Bit		0x03FFFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask59Bit		0x07FFFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask60Bit		0x0FFFFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask61Bit		0x1FFFFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask62Bit		0x3FFFFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask63Bit		0x7FFFFFFFFFFFFFFFULL
#define 	kSCSICmdFieldMask8Byte		0xFFFFFFFFFFFFFFFFULL

#endif	/* _IOKIT_SCSI_COMMAND_DEFINITIONS_H_ */
