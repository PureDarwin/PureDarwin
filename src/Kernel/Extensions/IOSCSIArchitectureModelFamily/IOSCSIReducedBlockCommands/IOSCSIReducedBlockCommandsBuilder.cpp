/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// SCSI Architecture Model Family includes
#include <IOKit/scsi/SCSICommandDefinitions.h>

#include "IOSCSIReducedBlockCommandsDevice.h"
#include "SCSIReducedBlockCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"RBC"

#if DEBUG
#define SCSI_RBC_DEVICE_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if 0
#pragma mark -
#pragma mark ¥ Reduced Block Commands Builders
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FORMAT_UNIT - Builds a FORMAT_UNIT command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::FORMAT_UNIT (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit			IMMED,
   						SCSICmdField1Bit			PROGRESS,
   						SCSICmdField1Bit			PERCENT_TIME,
   						SCSICmdField1Bit			INCREMENT )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIReducedBlockCommandObject ( )->FORMAT_UNIT (
											scsiRequest,
											IMMED,
											PROGRESS,
											PERCENT_TIME,
											INCREMENT );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ INQUIRY - Builds an INQUIRY command.					 		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::INQUIRY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			CMDDT, 
						SCSICmdField1Bit			EVPD, 
						SCSICmdField1Byte			PAGE_OR_OPERATION_CODE,
						SCSICmdField1Byte			ALLOCATION_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->INQUIRY ( 	
								scsiRequest,
								dataBuffer,
								CMDDT,
								EVPD,
								PAGE_OR_OPERATION_CODE,
								ALLOCATION_LENGTH,
								0x00 );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MODE_SELECT_6 - Builds a MODE_SELECT_6 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::MODE_SELECT_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			PF,
						SCSICmdField1Bit 			SP,
						SCSICmdField1Byte 			PARAMETER_LIST_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->MODE_SELECT_6 (
									scsiRequest,
									dataBuffer,
									PF,
									SP,
									PARAMETER_LIST_LENGTH,
									0x00 );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MODE_SENSE_6 - Builds a MODE_SENSE_6 command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::MODE_SENSE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DBD,
	   					SCSICmdField2Bit 			PC,
	   					SCSICmdField6Bit 			PAGE_CODE,
	   					SCSICmdField1Byte 			ALLOCATION_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->MODE_SENSE_6 (
									scsiRequest,
									dataBuffer,
									DBD,
									PC,
									PAGE_CODE,
									ALLOCATION_LENGTH,
									0x00 );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PERSISTENT_RESERVE_IN - Builds a PERSISTENT_RESERVE_IN command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::PERSISTENT_RESERVE_IN (
						SCSITaskIdentifier			request,
   						IOMemoryDescriptor *		dataBuffer,
	   					SCSICmdField5Bit 			SERVICE_ACTION, 
	   					SCSICmdField2Byte 			ALLOCATION_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->PERSISTENT_RESERVE_IN (
											scsiRequest,
											dataBuffer,
											SERVICE_ACTION,
											ALLOCATION_LENGTH,
											0x00 );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PERSISTENT_RESERVE_OUT - Builds a PERSISTENT_RESERVE_OUT command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::PERSISTENT_RESERVE_OUT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
	   					SCSICmdField5Bit			SERVICE_ACTION,
	   					SCSICmdField4Bit			SCOPE,
	   					SCSICmdField4Bit			TYPE )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->PERSISTENT_RESERVE_OUT (
											scsiRequest,
											dataBuffer,
											SERVICE_ACTION,
											SCOPE,
											TYPE,
											0x00 );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PREVENT_ALLOW_MEDIUM_REMOVAL - 	Builds a PREVENT_ALLOW_MEDIUM_REMOVAL
//										command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::PREVENT_ALLOW_MEDIUM_REMOVAL (
						SCSITaskIdentifier			request,
						SCSICmdField2Bit			PREVENT )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->PREVENT_ALLOW_MEDIUM_REMOVAL (
									scsiRequest,
									PREVENT,
									0x00 );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_10 - Builds a READ_10 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::READ_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte			TRANSFER_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIReducedBlockCommandObject ( )->READ_10 (
											scsiRequest,
											dataBuffer,
											blockSize,
											LOGICAL_BLOCK_ADDRESS,
											TRANSFER_LENGTH );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_CAPACITY - Builds a READ_CAPACITY command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::READ_CAPACITY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIReducedBlockCommandObject ( )->READ_CAPACITY (
											scsiRequest,
											dataBuffer );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RELEASE_6 - Builds a RELEASE_6 command.				 		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::RELEASE_6 ( 
						SCSITaskIdentifier			request )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RELEASE_6 ( 
												scsiRequest,
												0x00 );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REQUEST_SENSE - Builds a REQUEST_SENSE command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::REQUEST_SENSE ( 
						SCSITaskIdentifier			request,
   						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte 			ALLOCATION_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->REQUEST_SENSE (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								0x00 );
	
	
ErrorExit:
	
	
	return status;
	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RESERVE_6 - Builds a RESERVE_6 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::RESERVE_6 (
 						SCSITaskIdentifier			request )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RESERVE_6 ( 
								scsiRequest,
								0x00 );
	
	
ErrorExit:
	
	
	return status;
	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ START_STOP_UNIT - Builds a START_STOP_UNIT command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::START_STOP_UNIT (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit			IMMED,
						SCSICmdField4Bit			POWER_CONDITIONS,
						SCSICmdField1Bit			LOEJ,
						SCSICmdField1Bit			START )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIReducedBlockCommandObject ( )->START_STOP_UNIT (
											scsiRequest,
											IMMED,
											POWER_CONDITIONS,
											LOEJ,
											START );
	
	
ErrorExit:
	
	
	return status;
	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SYNCHRONIZE_CACHE - Builds a SYNCHRONIZE_CACHE command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::SYNCHRONIZE_CACHE (
 						SCSITaskIdentifier			request )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIReducedBlockCommandObject ( )->SYNCHRONIZE_CACHE ( scsiRequest );
	
	
ErrorExit:
	
	
	return status;
	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TEST_UNIT_READY - Builds a TEST_UNIT_READY command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::TEST_UNIT_READY (
						SCSITaskIdentifier			request )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->TEST_UNIT_READY (
							scsiRequest,
							0x00 );
	
	
ErrorExit:
	
	
	return status;
	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VERIFY - Builds a VERIFY command.			  					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::VERIFY (
						SCSITaskIdentifier			request,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			VERIFICATION_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIReducedBlockCommandObject ( )->VERIFY (
										scsiRequest,
										LOGICAL_BLOCK_ADDRESS,
										VERIFICATION_LENGTH );
	
	
ErrorExit:
	
	
	return status;
	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_10 - Builds a WRITE_10 command.			  				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::WRITE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit	   		FUA,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIReducedBlockCommandObject ( )->WRITE_10 (
										scsiRequest,
										dataBuffer,
										blockSize,
										FUA,
										LOGICAL_BLOCK_ADDRESS,
										TRANSFER_LENGTH );
	
	
ErrorExit:
	
	
	return status;
	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_BUFFER - Builds a WRITE_BUFFER command.			  		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::WRITE_BUFFER ( 
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Bit 			MODE,
						SCSICmdField1Byte 			BUFFER_ID,
						SCSICmdField3Byte 			BUFFER_OFFSET,
						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->WRITE_BUFFER (
								scsiRequest,
								dataBuffer,
								MODE,
								BUFFER_ID,
								BUFFER_OFFSET,
								PARAMETER_LIST_LENGTH,
								0x00 );
	
	
ErrorExit:
	
	
	return status;
	
	
}