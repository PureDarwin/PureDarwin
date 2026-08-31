/*
 * Copyright (c) 1998-2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_SCSI_CMDS_READ_CAPACITY_H_
#define _IOKIT_SCSI_CMDS_READ_CAPACITY_H_

#if KERNEL
#include <IOKit/IOTypes.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif


#pragma mark About this file
/* This file contains all the definitions for the data returned from
 * the READ CAPACITY 10 (0x25) and READ CAPACITY 16 (0x9E).
 */

enum
{
	kREPORT_CAPACITY_DataSize		= 8,
	kREPORT_CAPACITY_16_DataSize	= 32
};

/* NB: These are #defines instead of enums since
 * enums greater than 32 bits in size are not well-defined
 * by C99.
 */
#define 	kREPORT_CAPACITY_MaximumLBA			0xFFFFFFFFUL
#define 	kREPORT_CAPACITY_16_MaximumLBA		0xFFFFFFFFFFFFFFFFULL


/* Capacity return structure for READ CAPACITY 10 */
struct SCSI_Capacity_Data
{
	UInt32		RETURNED_LOGICAL_BLOCK_ADDRESS;
	UInt32		BLOCK_LENGTH_IN_BYTES;
};
typedef struct SCSI_Capacity_Data SCSI_Capacity_Data;

/* Capacity return structure for READ CAPACITY 16 */
struct SCSI_Capacity_Data_Long
{
	UInt64		RETURNED_LOGICAL_BLOCK_ADDRESS;
	UInt32		BLOCK_LENGTH_IN_BYTES;
	UInt8		RTO_EN_PROT_EN;
	UInt8		Reserved[19];
};
typedef struct SCSI_Capacity_Data_Long SCSI_Capacity_Data_Long;

/* Values for the REFERENCE TAG OWN (RTO_EN) bit in the READ CAPACITY
 * Long Data structure.
 */
enum
{
	kREAD_CAPACITY_RTO_Enabled								= 0x02,
	kREAD_CAPACITY_RTO_Disabled								= 0x00,
	kREAD_CAPACITY_RTO_Mask									= 0x02
};

/* Values for the PROTECTION INFORMATION (RTO_EN) bit in the READ CAPACITY
 * Long Data structure.
 */
enum
{
	kREAD_CAPACITY_PROT_Enabled								= 0x01,
	kREAD_CAPACITY_PROT_Disabled							= 0x00,
	kREAD_CAPACITY_PROT_Mask								= 0x01
};

#endif	/* _IOKIT_SCSI_CMDS_READ_CAPACITY_H_ */
