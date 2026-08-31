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
#include "IOSCSIPrimaryCommandsDevice.h"
#include "SCSITaskDefinition.h"
#include "SCSIPrimaryCommands.h"

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"SPC"

#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if 0
#pragma mark -
#pragma mark ¥ Commands Builder Utility Routines
#pragma mark -
#endif

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		IOSCSIPrimaryCommandsDevice::IsParameterValid
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Validate Parameter used for 1 bit to 1 byte paramaters
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::IsParameterValid ( SCSICmdField1Byte param,
										SCSICmdField1Byte mask )
{
	
	bool	valid = false;
	
	require ( ( param | mask ) == mask, ErrorExit );
	valid = true;
	
	
ErrorExit:
	
	
	return valid;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		IOSCSIPrimaryCommandsDevice::IsParameterValid
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Validate Parameter used for 9 bit to 2 byte paramaters
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::IsParameterValid ( SCSICmdField2Byte param,
										SCSICmdField2Byte mask )
{
	
	bool	valid = false;
	
	require ( ( param | mask ) == mask, ErrorExit );
	valid = true;
	
	
ErrorExit:
	
	
	return valid;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		IOSCSIPrimaryCommandsDevice::IsParameterValid
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Validate Parameter used for 17 bit to 4 byte paramaters
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::IsParameterValid ( SCSICmdField4Byte param,
										SCSICmdField4Byte mask )
{
	
	bool	valid = false;
	
	require ( ( param | mask ) == mask, ErrorExit );
	valid = true;
	
	
ErrorExit:
	
	
	return valid;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		IOSCSIPrimaryCommandsDevice::IsParameterValid
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Validate Parameter used for 33 bit to 8 byte paramaters
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::IsParameterValid ( SCSICmdField8Byte param,
										SCSICmdField8Byte mask )
{
	
	bool	valid = false;
	
	require ( ( param | mask ) == mask, ErrorExit );
	valid = true;
	
	
ErrorExit:
	
	
	return valid;
	
}



bool
IOSCSIPrimaryCommandsDevice::IsMemoryDescriptorValid (
							IOMemoryDescriptor * 		dataBuffer )
{
	bool	valid = false;

	require_nonzero ( dataBuffer, ErrorExit );
	valid = true;
	
	
ErrorExit:
	
	return valid;
	
}	


bool 				
IOSCSIPrimaryCommandsDevice::IsMemoryDescriptorValid (
							IOMemoryDescriptor * 		dataBuffer,
							UInt64						requiredSize )
{
	
	bool	valid = false;

	require_nonzero ( dataBuffer, ErrorExit );
	require ( ( dataBuffer->getLength ( ) >= requiredSize ), ErrorExit );
	valid = true;
	
	
ErrorExit:
	
	
	return valid;
	
}	


#if 0
#pragma mark -
#pragma mark ¥ Primary Commands Builders
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CHANGE_DEFINITION - Builds a CHANGE_DEFINITION command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::CHANGE_DEFINITION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			SAVE,
						SCSICmdField7Bit 			DEFINITION_PARAMETER,
						SCSICmdField1Byte 			PARAMETER_DATA_LENGTH,
		 				SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->CHANGE_DEFINITION (
											scsiRequest,
											dataBuffer,
											SAVE,
											DEFINITION_PARAMETER,
											PARAMETER_DATA_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ COMPARE - Builds a COMPARE command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::COMPARE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
 						SCSICmdField1Bit 			PAD,
						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->COMPARE (
											scsiRequest,
											dataBuffer,
											PAD,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ COPY - Builds a COPY command.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
IOSCSIPrimaryCommandsDevice::COPY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			PAD, 
						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,  
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->COPY (
											scsiRequest,
											dataBuffer,
											PAD,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ COPY_AND_VERIFY - Builds a COPY_AND_VERIFY command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
IOSCSIPrimaryCommandsDevice::COPY_AND_VERIFY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			BYTCHK, 
						SCSICmdField1Bit 			PAD, 
						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->COPY_AND_VERIFY (
											scsiRequest,
											dataBuffer,
											BYTCHK,
											PAD,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EXTENDED_COPY - Builds a EXTENDED_COPY command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::EXTENDED_COPY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Byte			PARAMETER_LIST_LENGTH, 
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->EXTENDED_COPY (
											scsiRequest,
											dataBuffer,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ INQUIRY - Builds a INQUIRY command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
IOSCSIPrimaryCommandsDevice::INQUIRY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			CMDDT,
						SCSICmdField1Bit			EVPD,
						SCSICmdField1Byte			PAGE_OR_OPERATION_CODE,
						SCSICmdField1Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
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
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LOG_SELECT - Builds a LOG_SELECT command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
IOSCSIPrimaryCommandsDevice::LOG_SELECT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			PCR,
						SCSICmdField1Bit			SP,
						SCSICmdField2Bit			PC,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->LOG_SELECT (
											scsiRequest,
											dataBuffer,
											PCR,
											SP,
											PC,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LOG_SENSE - Builds a LOG_SENSE command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::LOG_SENSE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			PPC,
						SCSICmdField1Bit			SP,
						SCSICmdField2Bit 			PC,
						SCSICmdField6Bit			PAGE_CODE,
						SCSICmdField2Byte			PARAMETER_POINTER,
						SCSICmdField2Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->LOG_SENSE (
											scsiRequest,
											dataBuffer,
											PPC,
											SP,
											PC,
											PAGE_CODE,
											PARAMETER_POINTER,
											ALLOCATION_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MODE_SELECT_6 - Builds a MODE_SELECT_6 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
IOSCSIPrimaryCommandsDevice::MODE_SELECT_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			PF,
						SCSICmdField1Bit			SP,
						SCSICmdField1Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
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
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MODE_SELECT_10 - Builds a MODE_SELECT_10 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::MODE_SELECT_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			PF,
						SCSICmdField1Bit			SP,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->MODE_SELECT_10 (
											scsiRequest,
											dataBuffer,
											PF,
											SP,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MODE_SENSE_6 - Builds a MODE_SENSE_6 command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::MODE_SENSE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			DBD,
						SCSICmdField2Bit			PC,
						SCSICmdField6Bit			PAGE_CODE,
						SCSICmdField1Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
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
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MODE_SENSE_10 - Builds a MODE_SENSE_10 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::MODE_SENSE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			LLBAA,
						SCSICmdField1Bit 			DBD,
						SCSICmdField2Bit 			PC,
						SCSICmdField6Bit 			PAGE_CODE,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->MODE_SENSE_10 (
											scsiRequest,
											dataBuffer,
											LLBAA,
											DBD,
											PC,
											PAGE_CODE,
											ALLOCATION_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PERSISTENT_RESERVE_IN - Builds a PERSISTENT_RESERVE_IN command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::PERSISTENT_RESERVE_IN (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField5Bit			SERVICE_ACTION,
						SCSICmdField2Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
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
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PERSISTENT_RESERVE_OUT - Builds a PERSISTENT_RESERVE_OUT command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::PERSISTENT_RESERVE_OUT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField5Bit			SERVICE_ACTION, 
   						SCSICmdField4Bit			SCOPE, 
   						SCSICmdField4Bit			TYPE,
						SCSICmdField1Byte			CONTROL )
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
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PREVENT_ALLOW_MEDIUM_REMOVAL - 	Builds a PREVENT_ALLOW_MEDIUM_REMOVAL
//										command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::PREVENT_ALLOW_MEDIUM_REMOVAL (
						SCSITaskIdentifier			request,
						SCSICmdField2Bit			PREVENT,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->PREVENT_ALLOW_MEDIUM_REMOVAL (
											scsiRequest,
											PREVENT,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_BUFFER - Builds a READ_BUFFER command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::READ_BUFFER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Bit			MODE,
						SCSICmdField1Byte			BUFFER_ID,
						SCSICmdField3Byte			BUFFER_OFFSET,
						SCSICmdField3Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->READ_BUFFER (
											scsiRequest,
											dataBuffer,
											MODE,
											BUFFER_ID,
											BUFFER_OFFSET,
											ALLOCATION_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RECEIVE - Builds a RECEIVE command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RECEIVE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Byte			TRANSFER_LENGTH, 
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RECEIVE(
											scsiRequest,
											dataBuffer,
											TRANSFER_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RECEIVE_DIAGNOSTICS_RESULTS - Builds a RECEIVE_DIAGNOSTICS_RESULTS
//									command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RECEIVE_DIAGNOSTICS_RESULTS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			PCV,
						SCSICmdField1Byte			PAGE_CODE,
						SCSICmdField2Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RECEIVE_DIAGNOSTICS_RESULTS (
											scsiRequest,
											dataBuffer,
											PCV,
											PAGE_CODE,
											ALLOCATION_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RELEASE_6 - Builds a RELEASE_6 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RELEASE_6 (
						SCSITaskIdentifier			request,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RELEASE_6 (
										scsiRequest,
										CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RELEASE_6 - Builds a RELEASE_6 command.	*OBSOLETE*			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RELEASE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			EXTENT,
						SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RELEASE_6 (
										scsiRequest,
										EXTENT,
										RESERVATION_IDENTIFICATION,
										CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RELEASE_10 - Builds a RELEASE_10 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RELEASE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			THRDPTY,
						SCSICmdField1Bit			LONGID,
						SCSICmdField1Byte			THIRD_PARTY_DEVICE_ID,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RELEASE_10 (
											scsiRequest,
											dataBuffer,
											THRDPTY,
											LONGID,
											THIRD_PARTY_DEVICE_ID,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RELEASE_10 - Builds a RELEASE_10 command.		*OBSOLETE*		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RELEASE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			THRDPTY,
						SCSICmdField1Bit			LONGID,
						SCSICmdField1Bit			EXTENT,
						SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
						SCSICmdField1Byte			THIRD_PARTY_DEVICE_ID,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RELEASE_10 (
											scsiRequest,
											dataBuffer,
											THRDPTY,
											LONGID,
											EXTENT,
											RESERVATION_IDENTIFICATION,
											THIRD_PARTY_DEVICE_ID,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REPORT_DEVICE_IDENTIFIER - Builds a REPORT_DEVICE_IDENTIFIER command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::REPORT_DEVICE_IDENTIFIER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->REPORT_DEVICE_IDENTIFIER (
											scsiRequest,
											dataBuffer,
											ALLOCATION_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REPORT_LUNS - Builds a REPORT_LUNS command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::REPORT_LUNS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->REPORT_LUNS (
											scsiRequest,
											dataBuffer,
											ALLOCATION_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REQUEST_SENSE - Builds a REQUEST_SENSE command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::REQUEST_SENSE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
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
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RESERVE_6 - Builds a RESERVE_6 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RESERVE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RESERVE_6 (
											scsiRequest,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RESERVE_6 - Builds a RESERVE_6 command.		*OBSOLETE*		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RESERVE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer, 
						SCSICmdField1Bit			EXTENT, 
						SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RESERVE_6 (
											scsiRequest,
											dataBuffer,
											EXTENT,
											RESERVATION_IDENTIFICATION,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RESERVE_10 - Builds a RESERVE_10 command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RESERVE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			THRDPTY,
						SCSICmdField1Bit			LONGID,
						SCSICmdField1Byte			THIRD_PARTY_DEVICE_ID,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RESERVE_10 (
											scsiRequest,
											dataBuffer,
											THRDPTY,
											LONGID,
											THIRD_PARTY_DEVICE_ID,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RESERVE_10 - Builds a RESERVE_10 command.		*OBSOLETE*		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::RESERVE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			THRDPTY,
						SCSICmdField1Bit			LONGID,
						SCSICmdField1Bit			EXTENT,
						SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
						SCSICmdField1Byte			THIRD_PARTY_DEVICE_ID,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->RESERVE_10 (
											scsiRequest,
											dataBuffer,
											THRDPTY,
											LONGID,
											EXTENT,
											RESERVATION_IDENTIFICATION,
											THIRD_PARTY_DEVICE_ID,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEND - Builds a SEND command.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::SEND (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit			AER,
						SCSICmdField3Byte			TRANSFER_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->SEND (
											scsiRequest,
											dataBuffer,
											AER,
											TRANSFER_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEND_DIAGNOSTICS - Builds a SEND_DIAGNOSTICS command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
IOSCSIPrimaryCommandsDevice::SEND_DIAGNOSTICS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit			SELF_TEST_CODE,
						SCSICmdField1Bit			PF,
						SCSICmdField1Bit			SELF_TEST,
						SCSICmdField1Bit			DEVOFFL,
						SCSICmdField1Bit			UNITOFFL,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->SEND_DIAGNOSTICS (
											scsiRequest,
											dataBuffer,
											SELF_TEST_CODE,
											PF,
											SELF_TEST,
											DEVOFFL,
											UNITOFFL,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SET_DEVICE_IDENTIFIER - Builds a SET_DEVICE_IDENTIFIER command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::SET_DEVICE_IDENTIFIER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField5Bit			SERVICE_ACTION,
						SCSICmdField4Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->SET_DEVICE_IDENTIFIER (
											scsiRequest,
											dataBuffer,
											SERVICE_ACTION,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TEST_UNIT_READY - Builds a TEST_UNIT_READY command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::TEST_UNIT_READY (
						SCSITaskIdentifier			request,
						SCSICmdField1Byte			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIPrimaryCommandObject ( )->TEST_UNIT_READY (
											scsiRequest,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_BUFFER - Builds a WRITE_BUFFER command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPrimaryCommandsDevice::WRITE_BUFFER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Bit			MODE,
						SCSICmdField1Byte			BUFFER_ID,
						SCSICmdField3Byte			BUFFER_OFFSET,
						SCSICmdField3Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
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
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}