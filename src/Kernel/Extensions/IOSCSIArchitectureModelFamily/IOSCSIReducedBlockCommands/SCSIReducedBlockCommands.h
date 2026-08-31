/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_SCSI_REDUCED_BLOCK_COMMANDS_H_
#define _IOKIT_SCSI_REDUCED_BLOCK_COMMANDS_H_

#if defined(KERNEL) && defined(__cplusplus)


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// General IOKit headers
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

// SCSI Architecture Model Family includes
#include "SCSIPrimaryCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSIReducedBlockCommands : public SCSIPrimaryCommands
{
	
	OSDeclareDefaultStructors ( SCSIReducedBlockCommands );
	
public:
	
	static SCSIReducedBlockCommands * CreateSCSIReducedBlockCommandObject ( void );
	
	// SCSI Reduced Block Commands as defined in T10:1240-D RBC
	
	// The FORMAT_UNIT command as defined in section 5.1
	virtual bool 	FORMAT_UNIT (
							SCSITask *				request,
   							SCSICmdField1Bit		IMMED,
   							SCSICmdField1Bit		PROGRESS,
   							SCSICmdField1Bit		PERCENT_TIME,
   							SCSICmdField1Bit		INCREMENT );
   	
   	// The READ_10 command as defined in section 5.2
	virtual bool 	READ_10 (
							SCSITask *				request,
   							IOMemoryDescriptor *	dataBuffer,
			    			UInt32					blockSize,
							SCSICmdField4Byte		LOGICAL_BLOCK_ADDRESS,
							SCSICmdField2Byte		TRANSFER_LENGTH );
	
	// The READ_CAPACITY command as defined in section 5.3
	virtual bool	READ_CAPACITY (
							SCSITask *				request,
							IOMemoryDescriptor *	dataBuffer );
	
	// The START_STOP_UNIT command as defined in section 5.4
	virtual bool	START_STOP_UNIT (
							SCSITask *				request,
							SCSICmdField1Bit		IMMED,
							SCSICmdField1Byte		POWER_CONDITIONS,
							SCSICmdField1Bit		LEOJ,
							SCSICmdField1Bit		START );
	
	// The SYNCHRONIZE_CACHE command as defined in section 5.5
	virtual bool	SYNCHRONIZE_CACHE ( 
							SCSITask *				request );
	
	// The VERIFY command as defined in section 5.7
	virtual bool	VERIFY (
							SCSITask *				request,
							SCSICmdField4Byte 		LOGICAL_BLOCK_ADDRESS,
							SCSICmdField2Byte 		VERIFICATION_LENGTH );
			
	// The WRITE_10 command	as defined in section 5.6
	virtual bool	WRITE_10 (
							SCSITask *				request,
							IOMemoryDescriptor *	dataBuffer,
			    			UInt32					blockSize,
							SCSICmdField1Bit        FUA,
							SCSICmdField4Byte 		LOGICAL_BLOCK_ADDRESS,
							SCSICmdField2Byte 		TRANSFER_LENGTH );
	
};

#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif	/* _IOKIT_SCSI_REDUCED_BLOCK_COMMANDS_H_ */