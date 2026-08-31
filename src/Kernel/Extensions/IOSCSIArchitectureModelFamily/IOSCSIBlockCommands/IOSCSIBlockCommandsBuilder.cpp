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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// SCSI Architecture Model Family includes
#include <IOKit/scsi/SCSICommandDefinitions.h>
#include "IOSCSIBlockCommandsDevice.h"
#include "SCSIBlockCommands.h"
#include "SCSICommandOperationCodes.h"

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"SBC"

#if DEBUG
#define SCSI_SBC_DEVICE_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if 0
#pragma mark -
#pragma mark ¥ Block Commands Builders
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ERASE_10 - 	Builds a ERASE_10 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::ERASE_10 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			ERA, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			TRANSFER_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->ERASE_10 (
					scsiRequest,
					ERA, 
					RELADR, 
					LOGICAL_BLOCK_ADDRESS, 
					TRANSFER_LENGTH, 
					CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ERASE_12 - Builds a ERASE_12 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::ERASE_12 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			ERA, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField4Byte 			TRANSFER_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->ERASE_12 (
				scsiRequest,
				ERA, 
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				TRANSFER_LENGTH, 
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FORMAT_UNIT - Builds a FORMAT_UNIT command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::FORMAT_UNIT(
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						IOByteCount					defectListSize,
						SCSICmdField1Bit 			FMTDATA, 
						SCSICmdField1Bit 			CMPLST, 
						SCSICmdField3Bit 			DEFECT_LIST_FORMAT, 
						SCSICmdField1Byte 			VENDOR_SPECIFIC, 
						SCSICmdField2Byte 			INTERLEAVE, 
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );

	status = GetSCSIBlockCommandObject ( )->FORMAT_UNIT (
				scsiRequest,
				dataBuffer,
				defectListSize,
				FMTDATA, 
				CMPLST, 
				DEFECT_LIST_FORMAT, 
				VENDOR_SPECIFIC, 
				INTERLEAVE, 
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FORMAT_UNIT - Builds a FORMAT_UNIT command.					[PROTECTED]
//  Defined in SBC-2 Section 5.41
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::FORMAT_UNIT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						IOByteCount					defectListSize,
						SCSICmdField1Bit			FMTPINFO,
						SCSICmdField1Bit			RTO_REQ,
						SCSICmdField1Bit			LONGLIST,
						SCSICmdField1Bit 			FMTDATA, 
						SCSICmdField1Bit 			CMPLST,
						SCSICmdField3Bit 			DEFECT_LIST_FORMAT, 
						SCSICmdField1Byte 			VENDOR_SPECIFIC, 
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( FMTPINFO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( RTO_REQ, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LONGLIST, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FMTDATA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( CMPLST, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( DEFECT_LIST_FORMAT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( VENDOR_SPECIFIC, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	if ( defectListSize > 0 )
	{
		
		// We have data to send to the device, 
		// make sure that we were given a valid buffer
		require ( IsMemoryDescriptorValid ( dataBuffer, defectListSize ), ErrorExit );
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_FORMAT_UNIT,
								( FMTPINFO << 7 ) | ( RTO_REQ << 6 ) | ( LONGLIST << 5 ) | ( FMTDATA << 4 ) | ( CMPLST << 3 ) | DEFECT_LIST_FORMAT,
								VENDOR_SPECIFIC,
								0x00,
								0x00,
								CONTROL );
 	
 	if ( FMTDATA == 0 )
	{
		
		// No DEFECT LIST is being sent to the device, so there
		// will be no data transfer for this request.
		SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
		SetTimeoutDuration ( request, 0 );
		
	}
	
	else
	{
		
		// The client has requested a DEFECT LIST be sent to the device
		// to be used with the format command
		SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
		SetTimeoutDuration ( request, 0 );
		SetDataBuffer ( request, dataBuffer );
		SetRequestedDataTransferCount ( request,  defectListSize );
		
	}
	
	status = true;
	
	
ErrorExit:
	
	
	return status;
	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LOCK_UNLOCK_CACHE - Builds a LOCK_UNLOCK_CACHE command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::LOCK_UNLOCK_CACHE (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			LOCK, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS, 
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );

	status = GetSCSIBlockCommandObject ( )->LOCK_UNLOCK_CACHE (
				scsiRequest,
				LOCK, 
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				NUMBER_OF_BLOCKS, 
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LOCK_UNLOCK_CACHE - Builds a LOCK_UNLOCK_CACHE command.		[PROTECTED]
//  Defined in SBC-2 section 5.5
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::LOCK_UNLOCK_CACHE (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			LOCK,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( LOCK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( NUMBER_OF_BLOCKS, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_LOCK_UNLOCK_CACHE,
								( LOCK << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( NUMBER_OF_BLOCKS >> 8 ) 	& 0xFF,
								  NUMBER_OF_BLOCKS			& 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );

	status = true;
	
	
ErrorExit:


	return status;

}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LOCK_UNLOCK_CACHE_16 - Builds a LOCK_UNLOCK_CACHE_16 command. [PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::LOCK_UNLOCK_CACHE_16 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit			LOCK,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte			CONTROL )
{

	bool		status = false;

	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters.
	require( IsParameterValid( LOCK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require( IsParameterValid( NUMBER_OF_BLOCKS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require( IsParameterValid( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 16-byte command, fill out the cdb appropriately.
	SetCommandDescriptorBlock ( request,
								kSCSICmd_LOCK_UNLOCK_CACHE_16,
								LOCK << 1,
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								( NUMBER_OF_BLOCKS >> 24 ) & 0xFF,
								( NUMBER_OF_BLOCKS >> 16 ) & 0xFF,
								( NUMBER_OF_BLOCKS >> 8 ) & 0xFF,
								NUMBER_OF_BLOCKS & 0xFF,
								0x00,
								CONTROL );
								
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );
												
	status = true;
	
ErrorExit:

	
	return status;
	
}
						
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MEDIUM_SCAN - Builds a MEDIUM_SCAN command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::MEDIUM_SCAN (
						SCSITaskIdentifier			request,
				 		IOMemoryDescriptor *		dataBuffer,
			   			SCSICmdField1Bit 			WBS,
			   			SCSICmdField1Bit 			ASA,
			   			SCSICmdField1Bit 			RSD,
			   			SCSICmdField1Bit 			PRA,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->MEDIUM_SCAN (
				scsiRequest,
				dataBuffer,
   				WBS,
   				ASA,
   				RSD,
   				PRA,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				PARAMETER_LIST_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PREFETCH - Builds a PREFETCH command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::PREFETCH (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->PREFETCH (
				scsiRequest,
				IMMED,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PREFETCH - Builds a PREFETCH command.							[PROTECTED]
// Defined in SBC-2 section 5.7
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::PREFETCH (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require( IsParameterValid( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PREFETCH,
								( IMMED << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS 		& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );

	status = true;


ErrorExit:
	
	
	return status;

}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PREFETCH_16 - Builds a PREFETCH_16 command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::PREFETCH_16 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit			IMMED,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte			TRANSFER_LENGTH,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte			CONTROL )
{

	bool		status = false;

	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters
	require( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require( IsParameterValid( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require( IsParameterValid( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require( IsParameterValid( CONTROL, kSCSICmdFieldMask1Bit ), ErrorExit );
	
	// This is a 16-byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock ( request,
								kSCSICmd_PREFETCH_16,
								IMMED << 1,
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF, 
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								( TRANSFER_LENGTH >> 24 ) & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								TRANSFER_LENGTH & 0xFF,
								GROUP_NUMBER,
								CONTROL );
							
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );
								
	status = true;
	
	
ErrorExit:
	
	
	return status;
	
}
		
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_6 - Builds a READ_6 command.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::READ_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest			= NULL;
	bool		status 				= false;
	UInt32 		requestedByteCount	= 0;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	if ( TRANSFER_LENGTH == 0 )
	{
		
		// The TRANSFER_LENGTH is zero, this indicates that 256 blocks
		// should be transfer from the device
		requestedByteCount = 256 * blockSize;
		
	}
	
	else
	{
		requestedByteCount = TRANSFER_LENGTH * blockSize;
	}
	
	status = GetSCSIBlockCommandObject ( )->READ_6 (
				scsiRequest,
				dataBuffer,
				requestedByteCount,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_10 - Builds a READ_10 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 	
IOSCSIBlockCommandsDevice::READ_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit			FUA,
						SCSICmdField1Bit			RELADR,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte			TRANSFER_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest			= NULL;
	bool		status 				= false;
	UInt64		requestedByteCount	= 0;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	status = GetSCSIBlockCommandObject ( )->READ_10 (
				scsiRequest,
				dataBuffer,
				requestedByteCount,
				DPO,
				FUA,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_10 - Builds a READ_10 command.							[PROTECTED]
// Defined in SBC-2 section 5.10
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::READ_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit 			RDPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			FUA_NV,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 				= false;
	UInt64		requestedByteCount	= 0;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( RDPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount  ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_10,
								( RDPROTECT << 5 ) | ( DPO << 4 ) | ( FUA << 3 ) | ( FUA_NV << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromTargetToInitiator );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );

	status = true;
	
	
ErrorExit:
	
	
	return status;
	

}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_12 - Builds a READ_12 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 	
IOSCSIBlockCommandsDevice::READ_12 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest			= NULL;
	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;

	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	status = GetSCSIBlockCommandObject ( )->READ_12 (
				scsiRequest,
				dataBuffer,
				requestedByteCount,
				DPO,
				FUA,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_12 - Builds a READ_12 command.							[PROTECTED]
//  Defined in SBC-2 section 5.11
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::READ_12 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit			RDPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			FUA_NV,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte 			CONTROL )
{
	
	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;

	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( RDPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount  ), ErrorExit );
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_12,
								( RDPROTECT << 5 ) | ( DPO << 4 ) | ( FUA << 3 ) | ( FUA_NV << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								( TRANSFER_LENGTH >> 24 ) 		& 0xFF,
								( TRANSFER_LENGTH >> 16 ) 		& 0xFF,
								( TRANSFER_LENGTH >> 8  ) 		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								GROUP_NUMBER,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromTargetToInitiator );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );

	status = true;


ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_16 - Builds a READ_16 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::READ_16 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit			RDPROTECT,
						SCSICmdField1Bit			DPO,
						SCSICmdField1Bit			FUA,
						SCSICmdField1Bit			FUA_NV,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte			TRANSFER_LENGTH,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte			CONTROL )
{

	bool		status = false;
	UInt64 		requestedByteCount	= 0;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );

	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( RDPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// This is a 16-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock ( request,
								kSCSICmd_READ_16,
								( RDPROTECT << 5 ) | ( DPO << 4 ) | ( FUA << 3 ) | ( FUA_NV << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								( TRANSFER_LENGTH >> 24 ) & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								TRANSFER_LENGTH & 0xFF,
								GROUP_NUMBER,
								CONTROL );
								
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromTargetToInitiator );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );

	status = true;
	
	
ErrorExit:


	return status;
	
}
		
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_CAPACITY - Builds a READ_CAPACITY command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::READ_CAPACITY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Bit 			PMI,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->READ_CAPACITY (
				scsiRequest,
				dataBuffer,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				PMI,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_CAPACITY_16 - Builds a READ_CAPACITY_16 command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::READ_CAPACITY_16 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte			ALLOCATION_LENGTH,
						SCSICmdField1Bit			PMI,
						SCSICmdField1Byte			CONTROL )
{

	bool		status  = false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed parameters
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( PMI, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// This is a 16 byte command, fill out the cdb appropriately.
	SetCommandDescriptorBlock ( request,
								kSCSICmd_SERVICE_ACTION_IN,
								kSCSIServiceAction_READ_CAPACITY_16,
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								( ALLOCATION_LENGTH >> 24 ) & 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								ALLOCATION_LENGTH & 0xFF,
								PMI,
								CONTROL );
								
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromTargetToInitiator );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  ALLOCATION_LENGTH );

	status = true;
	
ErrorExit:

	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_DEFECT_DATA_10 - Builds a READ_DEFECT_DATA_10 command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::READ_DEFECT_DATA_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			PLIST,
						SCSICmdField1Bit 			GLIST,
						SCSICmdField3Bit 			DEFECT_LIST_FORMAT,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->READ_DEFECT_DATA_10 (
				scsiRequest,
				dataBuffer,
				PLIST,
				GLIST,
				DEFECT_LIST_FORMAT,
				ALLOCATION_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_DEFECT_DATA_12 - Builds a READ_DEFECT_DATA_12 command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::READ_DEFECT_DATA_12 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			PLIST,
						SCSICmdField1Bit 			GLIST,
						SCSICmdField3Bit 			DEFECT_LIST_FORMAT,
						SCSICmdField4Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->READ_DEFECT_DATA_12 (
				scsiRequest,
				dataBuffer,
				PLIST,
				GLIST,
				DEFECT_LIST_FORMAT,
				ALLOCATION_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_GENERATION - Builds a READ_GENERATION command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::READ_GENERATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->READ_GENERATION (
				scsiRequest,
				dataBuffer,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				ALLOCATION_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_LONG - Builds a READ_LONG command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::READ_LONG (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			CORRCT,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			BYTE_TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->READ_LONG (
				scsiRequest,
				dataBuffer,
				CORRCT,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				BYTE_TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_LONG_16 - Builds a READ_LONG_16 command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::READ_LONG_16 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte			BYTE_TRANSFER_LENGTH,
						SCSICmdField1Bit			CORRCT,
						SCSICmdField1Byte			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( BYTE_TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CORRCT, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, BYTE_TRANSFER_LENGTH ), ErrorExit );

	// This is a 16-Byte command, fill out the cdb appropriately 
	SetCommandDescriptorBlock ( request,
								kSCSICmd_SERVICE_ACTION_IN,
								kSCSIServiceAction_READ_LONG_16,
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								0x00,
								0x00,
								( BYTE_TRANSFER_LENGTH >> 8 ) & 0xFF,
								BYTE_TRANSFER_LENGTH & 0xFF,
								CORRCT,
								CONTROL );
								
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromTargetToInitiator );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  BYTE_TRANSFER_LENGTH );

	status = true;
	
ErrorExit:


	return status;
	
}
		
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_UPDATED_BLOCK_10 - Builds a READ_UPDATED_BLOCK_10 command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::READ_UPDATED_BLOCK_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
					 	SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Bit 			LATEST,
					 	SCSICmdField15Bit 			GENERATION_ADDRESS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->READ_UPDATED_BLOCK_10 (
				scsiRequest,
				dataBuffer,
				fMediumBlockSize,
				DPO,
				FUA,
			 	RELADR,
				LOGICAL_BLOCK_ADDRESS,
				LATEST,
			 	GENERATION_ADDRESS,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REASSIGN_BLOCKS - Builds a REASSIGN_BLOCKS command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::REASSIGN_BLOCKS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->REASSIGN_BLOCKS (
				scsiRequest,
				dataBuffer,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REASSIGN_BLOCKS - Builds a REASSIGN_BLOCKS command.			[PROTECTED]
//  Defined in SBC-2 section 5.20
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::REASSIGN_BLOCKS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			LONGLBA,
						SCSICmdField1Bit			LONGLIST,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( LONGLBA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LONGLIST, kSCSICmdFieldMask1Bit ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REASSIGN_BLOCKS,
								( LONGLBA << 1 ) | LONGLIST,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );

	status = true;
	
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REBUILD - Builds a REBUILD command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::REBUILD (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
					 	SCSICmdField1Bit 			INTDATA,
						SCSICmdField2Bit 			PORT_CONTROL,
					 	SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			REBUILD_LENGTH,
						SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->REBUILD (
				scsiRequest,
				dataBuffer,
				DPO,
				FUA,
			 	INTDATA,
				PORT_CONTROL,
			 	LOGICAL_BLOCK_ADDRESS,
				REBUILD_LENGTH,
				PARAMETER_LIST_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REGENERATE - Builds a REGENERATE command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::REGENERATE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			DPO,
						SCSICmdField1Bit 			FUA,
					 	SCSICmdField1Bit 			INTDATA,
					 	SCSICmdField2Bit 			PORT_CONTROL,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			REBUILD_LENGTH,
						SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->REGENERATE (
				scsiRequest,
				dataBuffer,
				DPO,
				FUA,
			 	INTDATA,
			 	PORT_CONTROL,
				LOGICAL_BLOCK_ADDRESS,
				REBUILD_LENGTH,
				PARAMETER_LIST_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REZERO_UNIT - Builds a REZERO_UNIT command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::REZERO_UNIT ( 
						SCSITaskIdentifier			request,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->REZERO_UNIT (
					scsiRequest,
					CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEARCH_DATA_EQUAL_10 - Builds a SEARCH_DATA_EQUAL_10 command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::SEARCH_DATA_EQUAL_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			INVERT,
						SCSICmdField1Bit 			SPNDAT,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->SEARCH_DATA_EQUAL_10 (
				scsiRequest,
				dataBuffer,
				dataBuffer->getLength ( ),
				INVERT,
				SPNDAT,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				NUMBER_OF_BLOCKS_TO_SEARCH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEARCH_DATA_HIGH_10 - Builds a SEARCH_DATA_HIGH_10 command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::SEARCH_DATA_HIGH_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			INVERT,
						SCSICmdField1Bit 			SPNDAT,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->SEARCH_DATA_HIGH_10 (
				scsiRequest,
				dataBuffer,
				dataBuffer->getLength ( ),
				INVERT,
				SPNDAT,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				NUMBER_OF_BLOCKS_TO_SEARCH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEARCH_DATA_LOW_10 - Builds a SEARCH_DATA_LOW_10 command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::SEARCH_DATA_LOW_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			INVERT,
						SCSICmdField1Bit 			SPNDAT,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->SEARCH_DATA_LOW_10 (
				scsiRequest,
				dataBuffer,
				dataBuffer->getLength ( ),
				INVERT,
				SPNDAT,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				NUMBER_OF_BLOCKS_TO_SEARCH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEEK_6 - Builds a SEEK_6 command.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::SEEK_6 (
						SCSITaskIdentifier			request,
						SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->SEEK_6 (
				scsiRequest,
				LOGICAL_BLOCK_ADDRESS, 
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEEK_10 - Builds a SEEK_10 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::SEEK_10 (
						SCSITaskIdentifier			request,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->SEEK_10 (
				scsiRequest,
				LOGICAL_BLOCK_ADDRESS,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SET_LIMITS_10 - Builds a SET_LIMITS_10 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::SET_LIMITS_10 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RDINH,
						SCSICmdField1Bit 			WRINH,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->SET_LIMITS_10 (
				scsiRequest,
				RDINH,
				WRINH,
				LOGICAL_BLOCK_ADDRESS,
				NUMBER_OF_BLOCKS,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SET_LIMITS_12 - Builds a SET_LIMITS_12 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::SET_LIMITS_12 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RDINH,
						SCSICmdField1Bit 			WRINH,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->SET_LIMITS_12 (
				scsiRequest,
				RDINH,
				WRINH,
				LOGICAL_BLOCK_ADDRESS,
				NUMBER_OF_BLOCKS,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ START_STOP_UNIT - Builds a START_STOP_UNIT command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::START_STOP_UNIT (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField4Bit 			POWER_CONDITIONS,
						SCSICmdField1Bit 			LOEJ,
						SCSICmdField1Bit 			START,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->START_STOP_UNIT (
				scsiRequest,
				IMMED,
				POWER_CONDITIONS,
				LOEJ,
				START,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SYNCHRONIZE_CACHE - Builds a SYNCHRONIZE_CACHE command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::SYNCHRONIZE_CACHE (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->SYNCHRONIZE_CACHE (
				scsiRequest,
				IMMED,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				NUMBER_OF_BLOCKS,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SYNCHRONIZE_CACHE - Builds a SYNCHRONIZE_CACHE command.		[PROTECTED]
//  Defined in SBC-2 section 5.22
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::SYNCHRONIZE_CACHE (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			SYNC_NV,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SYNC_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( NUMBER_OF_BLOCKS, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SYNCHRONIZE_CACHE,
								( SYNC_NV << 2 ) | ( IMMED << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( NUMBER_OF_BLOCKS >> 8 )	& 0xFF,
								  NUMBER_OF_BLOCKS			& 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );
								
	status = true;
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SYNCRONIZE_CACHE_16 - Builds a SYNCRONIZE_CACHE_16 command.   [PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::SYNCRONIZE_CACHE_16 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit			SYNC_NV,
						SCSICmdField1Bit			IMMED,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte			NUMBER_OF_BLOCKS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( SYNC_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( NUMBER_OF_BLOCKS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 16-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock ( request,
								kSCSICmd_SYNCHRONIZE_CACHE_16,
								( SYNC_NV << 2 ) | ( IMMED << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								( NUMBER_OF_BLOCKS >> 24 ) & 0xFF,
								( NUMBER_OF_BLOCKS >> 16 ) & 0xFF,
								( NUMBER_OF_BLOCKS >> 8 ) & 0xFF,
								NUMBER_OF_BLOCKS & 0xFF,
								GROUP_NUMBER,
								CONTROL );

	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );

	status = true;
	
	
ErrorExit:

	
	return status;
	
}
						
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ UPDATE_BLOCK - Builds a UPDATE_BLOCK command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::UPDATE_BLOCK (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->UPDATE_BLOCK (
				scsiRequest,
				dataBuffer,
				fMediumBlockSize,
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VERIFY_10 - Builds a VERIFY_10 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::VERIFY_10 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BLKVFY,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			VERIFICATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->VERIFY_10 (
				scsiRequest,
				DPO,
				BLKVFY,
				BYTCHK,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				VERIFICATION_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VERIFY_10 - Builds a VERIFY_10 command.						[PROTECTED]
//  Defined in SBC-2 section 5.24
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::VERIFY_10 (
						SCSITaskIdentifier			request,
						SCSICmdField3Bit 			VRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			VERIFICATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( VRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( BYTCHK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( VERIFICATION_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_VERIFY_10,
								( VRPROTECT << 5 ) | ( DPO << 4 ) | ( BYTCHK << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( VERIFICATION_LENGTH >> 8 )	& 0xFF,
								  VERIFICATION_LENGTH			& 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );

	status = true;
	
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VERIFY_12 - Builds a VERIFY_12 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::VERIFY_12 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BLKVFY,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			VERIFICATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->VERIFY_12 (
				scsiRequest,
				DPO,
				BLKVFY,
				BYTCHK,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				VERIFICATION_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VERIFY_12 - Builds a VERIFY_12 command.						[PROTECTED]
//  Defined in SBC-2 section 5.25
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::VERIFY_12 (
						SCSITaskIdentifier			request,
						SCSICmdField3Bit 			VRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField4Byte 			VERIFICATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( VRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( BYTCHK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );	
	require ( IsParameterValid ( VERIFICATION_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_VERIFY_12,
								( VRPROTECT << 5 ) | ( DPO << 4 ) | ( BYTCHK << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								( VERIFICATION_LENGTH >> 24 )	& 0xFF,
								( VERIFICATION_LENGTH >> 16 )	& 0xFF,
								( VERIFICATION_LENGTH >>  8 )	& 0xFF,
								  VERIFICATION_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VERIFY_16 - Builds a VERIFY_16 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::VERIFY_16 (
						SCSITaskIdentifier			request,
						SCSICmdField3Bit			VRPROTECT,
						SCSICmdField1Bit			DPO,
						SCSICmdField1Bit			BYTCHK,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte			VERIFICATION_LENGTH,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( VRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( BYTCHK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( VERIFICATION_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 16-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock ( request,
								kSCSICmd_VERIFY_16,
								( VRPROTECT << 5 ) | ( DPO << 4 ) | ( BYTCHK << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								( VERIFICATION_LENGTH >> 24 ) & 0xFF,
								( VERIFICATION_LENGTH >> 16 ) & 0xFF,
								( VERIFICATION_LENGTH >> 8 ) & 0xFF,
								VERIFICATION_LENGTH & 0xFF,
								GROUP_NUMBER,
								CONTROL );

	SetDataTransferDirection ( 	request, kSCSIDataTransfer_NoDataTransfer );
	SetTimeoutDuration ( request, 0 );
									
	status = true;
	

ErrorExit:


	return status;
	
}
	
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_6 - Builds a WRITE_6 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::WRITE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField2Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
		
	SCSITask *	scsiRequest			= NULL;
	bool		status 				= false;
	UInt32 		requestedByteCount	= 0;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	if ( TRANSFER_LENGTH == 0 )
	{
		
		// The TRANSFER_LENGTH is zero, this indicates that 256 blocks
		// should be transfer from the device
		requestedByteCount = 256 * blockSize;
		
	}
	
	else
	{
		requestedByteCount = TRANSFER_LENGTH * blockSize;
	}
	
	status = GetSCSIBlockCommandObject ( )->WRITE_6 (
				scsiRequest,
				dataBuffer,
				requestedByteCount,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_10 - Builds a WRITE_10 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


bool
IOSCSIBlockCommandsDevice::WRITE_10 (
						SCSITaskIdentifier			request,
		   				IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			EBP,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
		
	SCSITask *	scsiRequest			= NULL;
	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	status = GetSCSIBlockCommandObject ( )->WRITE_10 (
				scsiRequest,
				dataBuffer,
				requestedByteCount,
				DPO,
				FUA,
				EBP,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_10 - Builds a WRITE_10 command.							[PROTECTED]
//  Defined in SBC-2 section 5.29
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			FUA_NV,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_10,
								( WRPROTECT << 5 ) | ( DPO << 4 ) | ( FUA << 3 ) | ( FUA_NV << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );

	status = true;
	
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_12 - Builds a WRITE_12 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::WRITE_12 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			EBP,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
		
	SCSITask *	scsiRequest			= NULL;
	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	status = GetSCSIBlockCommandObject ( )->WRITE_12 (
				scsiRequest,
				dataBuffer,
				requestedByteCount,
				DPO,
				FUA,
				EBP,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_12 - Builds a WRITE_12 command.							[PROTECTED]
//  Defined in SBC-2 section 5.30
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_12 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			FUA_NV,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_12,
								( WRPROTECT << 5 ) | ( DPO << 4 ) | ( FUA << 3 ) | ( FUA_NV << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								( TRANSFER_LENGTH >> 24 )		& 0xFF,
								( TRANSFER_LENGTH >> 16 )		& 0xFF,
								( TRANSFER_LENGTH >>  8 )		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								GROUP_NUMBER,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );

	status = true;
	
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_16 - Builds a WRITE_16 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_16 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit			DPO,
						SCSICmdField1Bit			FUA,
						SCSICmdField1Bit			FUA_NV,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte			TRANSFER_LENGTH,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte			CONTROL )
{

	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// This is a 16-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock ( request,
								kSCSICmd_WRITE_16,
								( WRPROTECT << 5 ) | ( DPO << 4 ) | ( FUA << 3 ) | ( FUA_NV << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								( TRANSFER_LENGTH >> 24 ) & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								TRANSFER_LENGTH & 0xFF,
								GROUP_NUMBER,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );

	status = true;
	
	
ErrorExit:


	return status;
	
}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_AND_VERIFY_10 - Builds a WRITE_AND_VERIFY_10 command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::WRITE_AND_VERIFY_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			EBP,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
		
	SCSITask *	scsiRequest			= NULL;
	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	status = GetSCSIBlockCommandObject ( )->WRITE_AND_VERIFY_10 (
				scsiRequest,
				dataBuffer,
				requestedByteCount,
				DPO,
				EBP,
				BYTCHK,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_AND_VERIFY_10 - Builds a WRITE_AND_VERIFY_10 command.	[PROTECTED]
//  Defined in SBC-2 section 5.33
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_AND_VERIFY_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( BYTCHK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount ), ErrorExit );

	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_AND_VERIFY_10,
								( WRPROTECT << 5 ) | ( DPO << 4 ) | ( BYTCHK << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 )		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_AND_VERIFY_12 - Builds a WRITE_AND_VERIFY_12 command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::WRITE_AND_VERIFY_12 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			EBP,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
		
	SCSITask *	scsiRequest			= NULL;
	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	status = GetSCSIBlockCommandObject ( )->WRITE_AND_VERIFY_12 (
				scsiRequest,
				dataBuffer,
				requestedByteCount,
				DPO,
				EBP,
				BYTCHK,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_AND_VERIFY_12 - Builds a WRITE_AND_VERIFY_12 command.	[PROTECTED]
//  Defined in SBC-2 section 5.34
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_AND_VERIFY_12 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( BYTCHK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount ), ErrorExit );

	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_AND_VERIFY_10,
								( WRPROTECT << 5 ) | ( DPO << 4 ) | ( BYTCHK << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								( TRANSFER_LENGTH >> 24 ) & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								TRANSFER_LENGTH & 0xFF,
								GROUP_NUMBER,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;

}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_AND_VERIFY_16 - Builds a WRITE_AND_VERIFY_12 command.	[PROTECTED]
//  Defined in SBC-2 section 5.34
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_AND_VERIFY_16 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BYTCHK,
						SCSICmdField8Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 				= false;
	UInt64 		requestedByteCount	= 0;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( BYTCHK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, requestedByteCount ), ErrorExit );

	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_AND_VERIFY_16,
								( WRPROTECT << 5 ) | ( DPO << 4 ) | ( BYTCHK << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								( TRANSFER_LENGTH >> 24 ) & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								TRANSFER_LENGTH & 0xFF,
								GROUP_NUMBER,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  requestedByteCount );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;

}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_LONG - Builds a WRITE_LONG command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::WRITE_LONG (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->WRITE_LONG (
				scsiRequest,
				dataBuffer,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_LONG_16 - Builds a WRITE_LONG_16 command.				[PROTECTED]
//  Defined in SBC-2 section 5.38
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_LONG_16 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField8Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Bit			CORRCT,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CORRCT, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, TRANSFER_LENGTH ), ErrorExit );
	
	// This is a 16-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_LONG,
								0x11, // Need to make a constant later, see SBC-2 spec section 5.38
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								0x00,
								( TRANSFER_LENGTH >> 8 )		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								CORRCT,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  TRANSFER_LENGTH );

	status = true;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_SAME - Builds a WRITE_SAME command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::WRITE_SAME (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			PBDATA,
						SCSICmdField1Bit 			LBDATA,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->WRITE_SAME (
				scsiRequest,
				dataBuffer,
				PBDATA,
				LBDATA,
				RELADR,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_SAME - Builds a WRITE_SAME command.						[PROTECTED]
//  Defined in SBC-2 section 5.39
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_SAME (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit 			PBDATA,
						SCSICmdField1Bit 			LBDATA,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( PBDATA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LBDATA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, TRANSFER_LENGTH ), ErrorExit );
	
	// Can't have PBDATA and LBDATA set in same command
	require ( ( PBDATA == !LBDATA ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_SAME,
								( WRPROTECT << 5 ) | ( PBDATA << 2 ) | ( LBDATA << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 )		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  TRANSFER_LENGTH );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_SAME_16 - Builds a WRITE_SAME_16 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::WRITE_SAME_16 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit			PBDATA,
						SCSICmdField1Bit			LBDATA,
						SCSICmdField8Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte			TRANSFER_LENGTH,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField1Byte			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );
	
	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( PBDATA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LBDATA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask8Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, TRANSFER_LENGTH ), ErrorExit );
	
	// Can't have PBDATA and LBDATA set in same command
	require ( ( PBDATA == !LBDATA ), ErrorExit );
	
	// This is a 16-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock ( request,
								kSCSICmd_WRITE_SAME_16,
								( WRPROTECT << 5 ) | ( PBDATA << 2 ) | ( LBDATA << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 56 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 48 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 40 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 32 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								LOGICAL_BLOCK_ADDRESS & 0xFF,
								( TRANSFER_LENGTH >> 24 ) & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								TRANSFER_LENGTH & 0xFF,
								GROUP_NUMBER,
								CONTROL );

	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  TRANSFER_LENGTH );
		
	status = true;
	
	
ErrorExit:

	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ XDREAD - Builds a XDREAD command.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::XDREAD (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->XDREAD (
				scsiRequest,
				dataBuffer,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ XDREAD - Builds a XDREAD command.								[PROTECTED]
//  Defined in SBC-2 section 5.42.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::XDREAD (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			XORPINFO,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( XORPINFO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, TRANSFER_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_XDREAD,
								XORPINFO,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 )		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  TRANSFER_LENGTH );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ XDWRITE - Builds a XDWRITE command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::XDWRITE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			DISABLE_WRITE,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->XDWRITE (
				scsiRequest,
				dataBuffer,
				DPO,
				FUA,
				DISABLE_WRITE,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ XDWRITE - Builds a XDWRITE command.							[PROTECTED]
//  Defined in SBC-2 section 5.43
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::XDWRITE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			DISABLE_WRITE,
						SCSICmdField1Bit			FUA_NV,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( DISABLE_WRITE, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, TRANSFER_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_XDWRITE,
								( WRPROTECT << 5 ) | ( DPO << 4 ) | ( FUA << 3 ) | ( DISABLE_WRITE << 2 ) | ( FUA_NV << 1 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 )		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  TRANSFER_LENGTH );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ XDWRITE_EXTENDED - Builds a XDWRITE_EXTENDED command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::XDWRITE_EXTENDED (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			TABLE_ADDRESS,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			DISABLE_WRITE,
						SCSICmdField2Bit 			PORT_CONTROL,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			SECONDARY_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			SECONDARY_ADDRESS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->XDWRITE_EXTENDED (
				scsiRequest,
				dataBuffer,
				TABLE_ADDRESS,
				DPO,
				FUA,
				DISABLE_WRITE,
				PORT_CONTROL,
				LOGICAL_BLOCK_ADDRESS,
				SECONDARY_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				SECONDARY_ADDRESS,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ XDWRITEREAD_10 - Builds a XDWRITEREAD_10 command.				[PROTECTED]
//  Defined in SBC-2 section 5.46
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::XDWRITEREAD_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit			WRPROTECT,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			DISABLE_WRITE,
						SCSICmdField1Bit 			FUA_NV,
						SCSICmdField1Bit 			XORPINFO,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	// Do the pre-flight check on the passed in parameters
	require ( IsParameterValid ( WRPROTECT, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( DISABLE_WRITE, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( XORPINFO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsMemoryDescriptorValid ( dataBuffer, TRANSFER_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_XDWRITE,
								( WRPROTECT << 5 ) | ( DPO << 4 ) | ( FUA << 3 ) | ( DISABLE_WRITE << 2 ) | ( FUA_NV << 1 ) | XORPINFO,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 )		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  TRANSFER_LENGTH );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;
		
}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ XPWRITE - Builds a XPWRITE command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::XPWRITE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->XPWRITE (
				scsiRequest,
				dataBuffer,
				DPO,
				FUA,
				LOGICAL_BLOCK_ADDRESS,
				TRANSFER_LENGTH,
				CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ XPWRITE - Builds a XPWRITE command.							[PROTECTED]
//  Defined in SBC-2 section 5.48
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSIBlockCommandsDevice::XPWRITE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			FUA_NV,
						SCSICmdField1Bit 			XORPINFO,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField5Bit			GROUP_NUMBER,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{

	bool		status 		= false;
	
	require_nonzero ( request, ErrorExit );
	require ( ResetForNewTask ( request ), ErrorExit );

	require ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( FUA_NV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( GROUP_NUMBER, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_XPWRITE,
								( DPO << 4 ) | ( FUA << 3 ) | (FUA_NV << 1) | XORPINFO,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS 		& 0xFF,
								GROUP_NUMBER,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								CONTROL );
	
	SetDataTransferDirection ( 	request, kSCSIDataTransfer_FromInitiatorToTarget );
	SetTimeoutDuration ( request, 0 );
	SetDataBuffer ( request, dataBuffer );
	SetRequestedDataTransferCount ( request,  TRANSFER_LENGTH );
	
	status = true;
	
	
ErrorExit:
	
	
	return status;
	
}
