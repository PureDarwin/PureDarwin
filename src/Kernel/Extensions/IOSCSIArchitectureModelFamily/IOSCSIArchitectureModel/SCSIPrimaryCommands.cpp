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

// Libkern includes
#include <libkern/OSByteOrder.h>

// SCSI Architecture Model Family includes
#include "SCSIPrimaryCommands.h"
#include "SCSICommandOperationCodes.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"SPC Command Set"

#if DEBUG
#define SCSI_SPC_COMMANDS_DEBUGGING_LEVEL		0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_SPC_COMMANDS_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#define DEBUG_ASSERT(x)		assert x
#else
#define PANIC_NOW(x)
#define DEBUG_ASSERT(x)
#endif

#if ( SCSI_SPC_COMMANDS_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_SPC_COMMANDS_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super OSObject
OSDefineMetaClassAndStructors ( SCSIPrimaryCommands, OSObject );


#if 0
#pragma mark -
#pragma mark ¥ SPC Command Methods
#pragma mark -
#endif


// SCSI Primary Commands as defined in T10:1236D SPC-2,
// Revision 18, dated 21 May 2000

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::CHANGE_DEFINITION
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//	¥¥¥ OBSOLETE ¥¥¥
//		
//		The CHANGE_DEFINITION command as defined in SPC
//		revision 11a, section 7.1.  SPC-2 obsoleted this command.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::CHANGE_DEFINITION (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			SAVE,
    			SCSICmdField7Bit 			DEFINITION_PARAMETER,
    			SCSICmdField1Byte 			PARAMETER_DATA_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::CHANGE_DEFINITION *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( SAVE, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( DEFINITION_PARAMETER, kSCSICmdFieldMask7Bit ), ErrorExit );	
	require ( IsParameterValid ( PARAMETER_DATA_LENGTH, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_DATA_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_CHANGE_DEFINITION,
								0x00,
								SAVE,
								DEFINITION_PARAMETER,
								0x00,
								0x00,
								0x00,
								0x00,
								PARAMETER_DATA_LENGTH,
								CONTROL );

	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_DATA_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::COMPARE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The COMPARE command as defined in section 7.2.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::COMPARE (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PAD,
    			SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::COMPARE called\n" ) );
	
	require ( IsParameterValid ( PAD, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_COMPARE,
								PAD,
								0x00,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
	                   			0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::COPY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The COPY command as defined in section 7.3.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::COPY (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PAD,
    			SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	STATUS_LOG ( ( "SCSIPrimaryCommands::COPY called\n" ) );
	
	require ( IsParameterValid ( PAD, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
 								kSCSICmd_COPY,
								PAD,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
	                   			0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::COPY_AND_VERIFY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The COPY_AND_VERIFY command as defined in section 7.4.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::COPY_AND_VERIFY (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			BYTCHK,
    			SCSICmdField1Bit 			PAD,
    			SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::COPY_AND_VERIFY called\n" ) );
	
	require ( IsParameterValid ( BYTCHK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PAD, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_COPY_AND_VERIFY,
								( BYTCHK << 1 ) | PAD,
								0x00,
 								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
	                  			0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::EXTENDED_COPY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The EXTENDED_COPY command as defined in section 7.5.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::EXTENDED_COPY (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::EXTENDED_COPY called\n" ) );
	
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 16-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_EXTENDED_COPY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 24 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::INQUIRY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The INQUIRY command as defined in section 7.6.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::INQUIRY (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			CMDDT,
    			SCSICmdField1Bit 			EVPD,
    			SCSICmdField1Byte 			PAGE_OR_OPERATION_CODE,
    			SCSICmdField1Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::INQUIRY called\n" ) );
	
	require ( IsParameterValid ( CMDDT, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( EVPD, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PAGE_OR_OPERATION_CODE, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// If the PAGE_OR_OPERATION_CODE parameter is not zero and both the CMDDT
	// and EVPD parameters are, indicate that the PAGE_OR_OPERATION_CODE is not 
	// valid.
	if ( PAGE_OR_OPERATION_CODE != 0 )
    {
		
		if ( ( ( CMDDT == 1 ) && ( EVPD == 1 ) ) || ( ( CMDDT == 0 ) && ( EVPD == 0 ) ) )
		{
			goto ErrorExit;
		}
		
		//require ( ( CMDDT != 0 ) && ( EVPD != 0 ), ErrorExit );
		
    }
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_INQUIRY,
								( CMDDT << 1 ) | EVPD,
								PAGE_OR_OPERATION_CODE,
								0x00,
								ALLOCATION_LENGTH,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::LOG_SELECT
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The LOG_SELECT command as defined in section 7.7.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::LOG_SELECT (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PCR,
    			SCSICmdField1Bit 			SP,
    			SCSICmdField2Bit 			PC,
    			SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::LOG_SELECT called\n" ) );
	
	require ( IsParameterValid ( PCR, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SP, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_LOG_SELECT,
								( PCR << 1) | SP,
								PC << 6,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}  


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::LOG_SENSE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The LOG_SENSE command as defined in section 7.8.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::LOG_SENSE (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PPC,
    			SCSICmdField1Bit 			SP,
    			SCSICmdField2Bit 			PC,
    			SCSICmdField6Bit 			PAGE_CODE,
    			SCSICmdField2Byte 			PARAMETER_POINTER,
    			SCSICmdField2Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::LOG_SENSE called\n" ) );
	
	require ( IsParameterValid ( PPC, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SP, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask6Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_POINTER, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_LOG_SENSE,
								( PPC << 1 ) | SP,
								( PC << 6 ) | PAGE_CODE,
								0x00,
								0x00,
 								( PARAMETER_POINTER >> 8 ) & 0xFF,
								  PARAMETER_POINTER 	   & 0xFF,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}  


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::MODE_SELECT_6
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The MODE_SELECT(6) command as defined in section 7.9.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::MODE_SELECT_6 (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PF,
    			SCSICmdField1Bit 			SP,
    			SCSICmdField1Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::MODE_SELECT_6 called\n" ) );
	
	require ( IsParameterValid ( PF, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SP, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SELECT_6,
								( PF << 4) | SP,
								0x00,
								0x00,
								PARAMETER_LIST_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}  


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::MODE_SELECT_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The MODE_SELECT(10) command as defined in section 7.10.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::MODE_SELECT_10 (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PF,
    			SCSICmdField1Bit 			SP,
    			SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::MODE_SELECT_10 called\n" ) );
	
	require ( IsParameterValid ( PF, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SP, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SELECT_10,
								( PF << 4 ) | SP,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}  


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::MODE_SENSE_6
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The MODE_SENSE(6) command as defined in section 7.11.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::MODE_SENSE_6 (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DBD,
   				SCSICmdField2Bit 			PC,
   				SCSICmdField6Bit 			PAGE_CODE,
   				SCSICmdField1Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::MODE_SENSE_6 called\n" ) );
	
	require ( IsParameterValid ( DBD, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask6Bit ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SENSE_6,
								DBD << 3,
								( PC << 6) | PAGE_CODE,
								0x00,
								ALLOCATION_LENGTH,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::MODE_SENSE_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The MODE_SENSE(10) command as defined in section 7.12.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::MODE_SENSE_10 (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			LLBAA,
				SCSICmdField1Bit 			DBD,
				SCSICmdField2Bit 			PC,
				SCSICmdField6Bit 			PAGE_CODE,
				SCSICmdField2Byte 			ALLOCATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::MODE_SENSE_10 called\n" ) );
	
	require ( IsParameterValid ( LLBAA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( DBD, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask6Bit ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SENSE_10,
								( LLBAA << 4 ) | ( DBD << 3 ),
								( PC << 6 ) | PAGE_CODE,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::PERSISTENT_RESERVE_IN
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The PERSISTENT_RESERVE_IN command as defined in section 7.13.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::PERSISTENT_RESERVE_IN (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
   				SCSICmdField5Bit 			SERVICE_ACTION,
   				SCSICmdField2Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::PERSISTENT_RESERVE_IN called\n" ) );
	
	require ( IsParameterValid ( SERVICE_ACTION, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PERSISTENT_RESERVE_IN,
								SERVICE_ACTION,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::PERSISTENT_RESERVE_OUT
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The PERSISTENT_RESERVE_OUT command as defined in section 7.14.
//		
//	NB:	There is no PARAMETER_LIST_LENGTH parameter as this value is
// 		always 0x18 for the SPC version of this command. The buffer for
//		the data must be at least of that size.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::PERSISTENT_RESERVE_OUT (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
   				SCSICmdField5Bit 			SERVICE_ACTION,
   				SCSICmdField4Bit 			SCOPE,
   				SCSICmdField4Bit 			TYPE,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::PERSISTENT_RESERVE_OUT called\n" ) );
	
	require ( IsParameterValid ( SERVICE_ACTION, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( SCOPE, kSCSICmdFieldMask4Bit ), ErrorExit );
	require ( IsParameterValid ( TYPE, kSCSICmdFieldMask4Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, 0x18 ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PERSISTENT_RESERVE_OUT,
								SERVICE_ACTION,
								( SCOPE << 4 ) | TYPE,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x18,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								0x18 );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::PREVENT_ALLOW_MEDIUM_REMOVAL
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The PREVENT_ALLOW_MEDIUM_REMOVAL command as defined in
//		section 7.15.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::PREVENT_ALLOW_MEDIUM_REMOVAL ( 
				SCSITask *					request,
     			SCSICmdField2Bit 			PREVENT,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::PREVENT_ALLOW_MEDIUM_REMOVAL called\n" ) );
	
	require ( IsParameterValid ( PREVENT, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL,
								0x00,
								0x00,
								0x00,
								PREVENT,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::READ_BUFFER
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ_BUFFER command as defined in section 7.16.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::READ_BUFFER ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField4Bit 			MODE,
				SCSICmdField1Byte 			BUFFER_ID,
				SCSICmdField3Byte 			BUFFER_OFFSET,
				SCSICmdField3Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::READ_BUFFER called\n" ) );
	
	require ( IsParameterValid ( MODE, kSCSICmdFieldMask4Bit ), ErrorExit );
	require ( IsParameterValid ( BUFFER_ID, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( BUFFER_OFFSET, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_BUFFER,
								MODE,
								BUFFER_ID,
								( BUFFER_OFFSET >> 16 ) & 0xFF,
								( BUFFER_OFFSET >> 8 )  & 0xFF,
								  BUFFER_OFFSET			& 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8 )  & 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RECEIVE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The RECEIVE command as defined in section 9.2.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RECEIVE ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
	 			SCSICmdField3Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RECEIVE called\n" ) );
	
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, TRANSFER_LENGTH ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RECEIVE,
								0x00,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8  ) & 0xFF,
								  TRANSFER_LENGTH 		  & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								TRANSFER_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RECEIVE_COPY_RESULTS
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The RECEIVE_COPY_RESULTS command as defined in section 7.17.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RECEIVE_COPY_RESULTS (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
 				SCSICmdField5Bit 			SERVICE_ACTION,
 				SCSICmdField1Byte			LIST_IDENTIFIER,
 				SCSICmdField4Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RECEIVE_COPY_RESULTS called\n" ) );
	
	require ( IsParameterValid ( SERVICE_ACTION, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( LIST_IDENTIFIER, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// This is a 16-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RECEIVE_COPY_RESULTS,
								SERVICE_ACTION,
								LIST_IDENTIFIER,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 24 ) & 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8  ) & 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RECEIVE_DIAGNOSTICS_RESULTS
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The RECEIVE_DIAGNOSTICS_RESULTS command as defined in
//		section 7.18.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RECEIVE_DIAGNOSTICS_RESULTS ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
	 			SCSICmdField1Bit 			PCV,
	 			SCSICmdField1Byte			PAGE_CODE,
	 			SCSICmdField2Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RECEIVE_DIAGNOSTICS_RESULTS called\n" ) );
	
	require ( IsParameterValid ( PCV, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS,
								PCV,
								PAGE_CODE,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RELEASE_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The RELEASE(10) command as defined in section 7.19.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RELEASE_10 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			THRDPTY,
				SCSICmdField1Bit 			LONGID,
				SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RELEASE_10 called\n" ) );
	
	require ( IsParameterValid ( THRDPTY, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LONGID, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( THIRD_PARTY_DEVICE_ID, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RELEASE_10,
								( THRDPTY << 4 ) | ( LONGID << 1 ),
								0x00,
								THIRD_PARTY_DEVICE_ID,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RELEASE_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//	¥¥¥ OBSOLETE ¥¥¥
//
//		The RELEASE(10) command as defined in SPC revision 11a,
//		section 7.17. The SPC-2 specification obsoleted the EXTENT and
//		RESERVATION_IDENTIFICATION fields.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RELEASE_10 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			THRDPTY,
				SCSICmdField1Bit 			LONGID,
				SCSICmdField1Bit 			EXTENT,
				SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
				SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RELEASE_10 *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( THRDPTY, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LONGID, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( EXTENT, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( RESERVATION_IDENTIFICATION, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( THIRD_PARTY_DEVICE_ID, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RELEASE_10,
								( THRDPTY << 4 ) | ( LONGID << 1 ) | EXTENT,
								RESERVATION_IDENTIFICATION,
								THIRD_PARTY_DEVICE_ID,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH 	   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RELEASE_6
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The RELEASE(6) command as defined in section 7.20
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RELEASE_6 ( 
				SCSITask *					request,
				SCSICmdField1Byte			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RELEASE_6 called\n" ) );
	
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RELEASE_6,
								0x00,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RELEASE_6
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//	¥¥¥ OBSOLETE ¥¥¥
//
//		The RELEASE(6) command as defined in SPC revision 11a,
//		section 7.18. The SPC-2 specification obsoleted the EXTENT and
//		RESERVATION_IDENTIFICATION fields.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RELEASE_6 ( 
				SCSITask *					request,
				SCSICmdField1Bit 			EXTENT,
				SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RELEASE_6 *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( EXTENT, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( RESERVATION_IDENTIFICATION, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RELEASE_6,
								EXTENT,
								RESERVATION_IDENTIFICATION,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::REPORT_DEVICE_IDENTIFIER
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The REPORT_DEVICE_IDENTIFIER command as defined in section 7.21.
//
//	NB:	There is no SERVICE_ACTION parameter as the value for this field
//		for the SPC version of this command is defined to always be 0x05.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::REPORT_DEVICE_IDENTIFIER ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField4Byte 			ALLOCATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::REPORT_DEVICE_IDENTIFIER called\n" ) );
	
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REPORT_DEVICE_IDENTIFIER,
								0x05,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 24 ) & 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8  ) & 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::REPORT_LUNS
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The REPORT_LUNS command as defined in section 7.22.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::REPORT_LUNS ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
			 	SCSICmdField4Byte 			ALLOCATION_LENGTH,
			 	SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::REPORT_LUNS called\n" ) );
	
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );	
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );	
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REPORT_LUNS,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 24 ) & 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8  ) & 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::REQUEST_SENSE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The REQUEST_SENSE command as defined in section 7.23.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::REQUEST_SENSE (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
			 	SCSICmdField1Byte 			ALLOCATION_LENGTH,  
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::REQUEST_SENSE called\n" ) );
	
	require ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REQUEST_SENSE,
								0x00,
								0x00,
								0x00,
								ALLOCATION_LENGTH,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RESERVE_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The RESERVE(10) command as defined in section 7.24.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RESERVE_10 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			THRDPTY, 
				SCSICmdField1Bit 			LONGID, 
				SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RESERVE_10 called\n" ) );
	
	require ( IsParameterValid ( THRDPTY, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LONGID, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( THIRD_PARTY_DEVICE_ID, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );	
	require ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_10,
								( THRDPTY << 4 ) | ( LONGID << 1 ),
								0x00,
								THIRD_PARTY_DEVICE_ID,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RESERVE_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//	¥¥¥ OBSOLETE ¥¥¥
//
//		The RESERVE(10) command as defined in SPC revision 11a,
//		section 7.21. The SPC-2 specification obsoleted the EXTENT and 
//		RESERVATION_IDENTIFICATION fields.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RESERVE_10 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			THRDPTY, 
				SCSICmdField1Bit 			LONGID, 
				SCSICmdField1Bit 			EXTENT, 
				SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
				SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RESERVE_10 *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( THRDPTY, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LONGID, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( EXTENT, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( RESERVATION_IDENTIFICATION, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( THIRD_PARTY_DEVICE_ID, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_10,
								( THRDPTY << 4 ) | ( LONGID << 1 ) | EXTENT,
								RESERVATION_IDENTIFICATION,
								THIRD_PARTY_DEVICE_ID,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								  PARAMETER_LIST_LENGTH			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RESERVE_6
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The RESERVE(6) command as defined in section 7.25.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RESERVE_6 ( 
				SCSITask *					request,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RESERVE_6 called\n" ) );
	
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_6,
								0x00,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::RESERVE_6
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//	¥¥¥ OBSOLETE ¥¥¥
//
//		The RESERVE(6) command as defined in SPC revision 11a,
//		section 7.22. The SPC-2 specification obsoleted the EXTENT,
//		RESERVATION_IDENTIFICATION and PARAMETER_LIST_LENGTH fields.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::RESERVE_6 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			EXTENT,
				SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RESERVE_6 *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( EXTENT, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( RESERVATION_IDENTIFICATION, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_6,
								EXTENT,
								RESERVATION_IDENTIFICATION,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								  PARAMETER_LIST_LENGTH			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::SEND
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SEND command as defined in section 9.3.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::SEND ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit			AER,
	 			SCSICmdField3Byte 			TRANSFER_LENGTH, 
    			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::SEND called\n" ) );
	
	require ( IsParameterValid ( AER, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND,
								AER,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8  ) & 0xFF,
								  TRANSFER_LENGTH		  & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								TRANSFER_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::SEND_DIAGNOSTICS
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SEND_DIAGNOSTICS command as defined in section 7.26.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::SEND_DIAGNOSTICS ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
			  	SCSICmdField3Bit 			SELF_TEST_CODE, 
				SCSICmdField1Bit 			PF, 
				SCSICmdField1Bit 			SELF_TEST, 
				SCSICmdField1Bit 			DEVOFFL, 
				SCSICmdField1Bit 			UNITOFFL, 
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::SEND_DIAGNOSTICS called\n" ) );
	
	require ( IsParameterValid ( SELF_TEST_CODE, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( PF, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SELF_TEST, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( DEVOFFL, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( UNITOFFL, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );

	if ( SELF_TEST == 1 )
	{
		
		require ( ( SELF_TEST_CODE == 0x00 ), ErrorExit );
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_DIAGNOSTICS,
								( SELF_TEST_CODE << 5 ) | ( PF << 4 ) |
									( SELF_TEST << 2 ) | ( DEVOFFL << 1 ) | UNITOFFL,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								  PARAMETER_LIST_LENGTH 		& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::SET_DEVICE_IDENTIFIER
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SET_DEVICE_IDENTIFIER command as defined in section 7.27.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::SET_DEVICE_IDENTIFIER ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField5Bit 			SERVICE_ACTION,
				SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::SET_DEVICE_IDENTIFIER called\n" ) );
	
	require ( IsParameterValid ( SERVICE_ACTION, kSCSICmdFieldMask5Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SET_DEVICE_IDENTIFIER,
								SERVICE_ACTION,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 24 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8  ) & 0xFF,
								  PARAMETER_LIST_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::TEST_UNIT_READY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The TEST_UNIT_READY command as defined in section 7.28.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::TEST_UNIT_READY (
				SCSITask *					request,
    			SCSICmdField1Byte			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::TEST_UNIT_READY called\n" ) );
	
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_TEST_UNIT_READY,
								0x00,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								10 * 1000,
								kSCSIDataTransfer_NoDataTransfer );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::WRITE_BUFFER
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The TEST_UNIT_READY command as defined in section 7.29.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::WRITE_BUFFER ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField4Bit 			MODE, 
				SCSICmdField1Byte 			BUFFER_ID, 
				SCSICmdField3Byte 			BUFFER_OFFSET, 
				SCSICmdField3Byte 			PARAMETER_LIST_LENGTH, 
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::WRITE_BUFFER called\n" ) );
	
	require ( IsParameterValid ( MODE, kSCSICmdFieldMask4Bit ), ErrorExit );
	require ( IsParameterValid ( BUFFER_ID, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( BUFFER_OFFSET, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_BUFFER,
								MODE,
								BUFFER_ID,
								( BUFFER_OFFSET >> 16 ) & 0xFF,
								( BUFFER_OFFSET >> 8  ) & 0xFF,
								  BUFFER_OFFSET			& 0xFF,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8  ) & 0xFF,
								  PARAMETER_LIST_LENGTH			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Utility Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::IsParameterValid
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Validate Parameter used for 1 bit to 1 byte paramaters
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::IsParameterValid ( SCSICmdField1Byte param,
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
//		SCSIPrimaryCommands::IsParameterValid
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Validate Parameter used for 9 bit to 2 byte paramaters
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::IsParameterValid ( SCSICmdField2Byte param,
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
//		SCSIPrimaryCommands::IsParameterValid
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Validate Parameter used for 17 bit to 4 byte paramaters
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::IsParameterValid ( SCSICmdField4Byte param,
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
//		SCSIPrimaryCommands::IsBufferAndCapacityValid
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Check that the buffer is valid and of the required size
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::IsBufferAndCapacityValid (
				IOMemoryDescriptor *		dataBuffer,
				UInt32						requiredSize )
{
	
	bool	valid = false;
	require_nonzero ( dataBuffer, ErrorExit );
	require ( ( dataBuffer->getLength ( ) >= requiredSize ), ErrorExit );
	valid = true;
	
	
ErrorExit:
	
	
	return valid;
	
}


// ---- Methods for accessing the SCSITask attributes ----
// The SetCommandDescriptorBlock methods will populate the CDB of the
// appropriate size.  These methods will returns true if the CDB could 
// be filled out, false if it couldn't.

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::SetCommandDescriptorBlock
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Populate the 6 Byte Command Descriptor Block
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::SetCommandDescriptorBlock ( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5 )
{
	
	check ( request != NULL );
	
	return request->SetCommandDescriptorBlock (
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::SetCommandDescriptorBlock
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Populate the 10 Byte Command Descriptor Block
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::SetCommandDescriptorBlock ( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9 )
{
	
	check ( request != NULL );
	
	return request->SetCommandDescriptorBlock (
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5,
					cdbByte6,
					cdbByte7,
					cdbByte8,
					cdbByte9 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::SetCommandDescriptorBlock
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Populate the 12 Byte Command Descriptor Block
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::SetCommandDescriptorBlock ( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9,
							UInt8			cdbByte10,
							UInt8			cdbByte11 )
{
	
	check ( request != NULL );
	
	return request->SetCommandDescriptorBlock (
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5,
					cdbByte6,
					cdbByte7,
					cdbByte8,
					cdbByte9,
					cdbByte10,
					cdbByte11 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::SetCommandDescriptorBlock
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Populate the 16 Byte Command Descriptor Block
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::SetCommandDescriptorBlock ( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9,
							UInt8			cdbByte10,
							UInt8			cdbByte11,
							UInt8			cdbByte12,
							UInt8			cdbByte13,
							UInt8			cdbByte14,
							UInt8			cdbByte15 )
{
	
	check ( request != NULL );
	
	return request->SetCommandDescriptorBlock (
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5,
					cdbByte6,
					cdbByte7,
					cdbByte8,
					cdbByte9,
					cdbByte10,
					cdbByte11,
					cdbByte12,
					cdbByte13,
					cdbByte14,
					cdbByte15 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::SetDataTransferControl
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Set up the control information for the transfer, including
//		the transfer direction and the number of bytes to transfer.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIPrimaryCommands::SetDataTransferControl ( 
							SCSITask *				request,
						    UInt32					timeoutDuration,
							UInt8					dataTransferDirection,
							IOMemoryDescriptor *	dataBuffer,
							UInt64					transferCountInBytes )
{
	
	bool	result = false;
	
	check ( request != NULL );
	
	if ( transferCountInBytes != 0 )
	{
		
		require_nonzero ( dataBuffer, ErrorExit );
		
	}
	
	result = request->SetTimeoutDuration ( timeoutDuration );
	require ( result, ErrorExit );
	
	result = request->SetDataTransferDirection ( dataTransferDirection );
	require ( result, ErrorExit );
	
	result = request->SetDataBuffer ( dataBuffer );
	require ( result, ErrorExit );
	
	result = request->SetRequestedDataTransferCount ( transferCountInBytes );
	require ( result, ErrorExit );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIPrimaryCommands::CreateSCSIPrimaryCommandObject
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		Factory method for getting an instance of the command builder
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIPrimaryCommands *
SCSIPrimaryCommands::CreateSCSIPrimaryCommandObject ( void )
{
	return OSTypeAlloc ( SCSIPrimaryCommands );
}