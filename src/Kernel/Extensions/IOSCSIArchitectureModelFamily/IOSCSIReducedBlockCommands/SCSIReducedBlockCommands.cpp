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
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include "SCSIReducedBlockCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"RBC Command Set"

#if DEBUG
#define SCSI_RBC_COMMANDS_DEBUGGING_LEVEL		0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_RBC_COMMANDS_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_RBC_COMMANDS_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_RBC_COMMANDS_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define	READ_CAPACITY_DATA_SIZE		8


#define super SCSIPrimaryCommands
OSDefineMetaClassAndStructors ( SCSIReducedBlockCommands, SCSIPrimaryCommands );


#if 0
#pragma mark -
#pragma mark RBC Command Methods
#pragma mark -
#endif


// SCSI Block Commands as defined in T10:990-D SBC
// Revision 8c, November 13, 1997

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIReducedBlockCommands::FORMAT_UNIT
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//		The FORMAT_UNIT command as defined in section 5.1.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIReducedBlockCommands::FORMAT_UNIT (
						SCSITask *				request,
   					    SCSICmdField1Bit		IMMED,
   						SCSICmdField1Bit		PROGRESS,
   						SCSICmdField1Bit		PERCENT_TIME,
   						SCSICmdField1Bit		INCREMENT )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::FORMAT_UNIT called\n" ) );
	
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PROGRESS, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PERCENT_TIME, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( INCREMENT, kSCSICmdFieldMask1Bit ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_FORMAT_UNIT,
								0x00,
								( IMMED << 3 ) | ( PROGRESS << 2 ) |
									( PERCENT_TIME << 1 ) | INCREMENT,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_NoDataTransfer );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIReducedBlockCommands::READ_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//		The READ_10 command as defined in section 5.2.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIReducedBlockCommands::READ_10 (
					SCSITask *				request,
   					IOMemoryDescriptor *	dataBuffer,
	    			UInt32					blockSize,
					SCSICmdField4Byte 		LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 		TRANSFER_LENGTH )
{
	
	UInt32 		requestedByteCount	= 0;
	bool		result 				= false;
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::READ_10 called\n" ) );
	
	// Check the validity of the media (make sure there is media in the
	// device, and the blocksize has been determined i.e. it's formatted )
	require_nonzero ( blockSize, ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	require ( IsBufferAndCapacityValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_10,
								0x00,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								requestedByteCount );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIReducedBlockCommands::READ_CAPACITY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//		The READ_CAPACITY command as defined in section 5.3.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIReducedBlockCommands::READ_CAPACITY (
						SCSITask *				request,
						IOMemoryDescriptor *	dataBuffer )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::READ_CAPACITY called\n" ) );
	
	require ( IsBufferAndCapacityValid ( dataBuffer, READ_CAPACITY_DATA_SIZE ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CAPACITY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								READ_CAPACITY_DATA_SIZE );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIReducedBlockCommands::START_STOP_UNIT
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//		The START_STOP_UNIT command as defined in section 5.4.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIReducedBlockCommands::START_STOP_UNIT (
				SCSITask *					request,
				SCSICmdField1Bit 			IMMED,
				SCSICmdField4Bit 			POWER_CONDITIONS,
				SCSICmdField1Bit 			LOEJ,
				SCSICmdField1Bit 			START )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::START_STOP_UNIT called\n" ) );
	
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );	
	require ( IsParameterValid ( POWER_CONDITIONS, kSCSICmdFieldMask4Bit ), ErrorExit );
	require ( IsParameterValid ( LOEJ, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( START, kSCSICmdFieldMask1Bit ), ErrorExit );
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_START_STOP_UNIT,
								IMMED,
								0x00,
								0x00,
								( POWER_CONDITIONS << 4 ) | ( LOEJ << 1 ) | START,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_NoDataTransfer );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIReducedBlockCommands::SYNCHRONIZE_CACHE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//		The SYNCHRONIZE_CACHE command as defined in section 5.5.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIReducedBlockCommands::SYNCHRONIZE_CACHE (
							SCSITask *		request )
{
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::SYNCRONIZE_CACHE called\n" ) );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SYNCHRONIZE_CACHE,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIReducedBlockCommands::WRITE_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//		The WRITE_10 command as defined in section 5.6.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIReducedBlockCommands::WRITE_10 (
						SCSITask *				request,
						IOMemoryDescriptor *	dataBuffer,
			    		UInt32					blockSize,
						SCSICmdField1Bit        FUA,
						SCSICmdField4Byte 		LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 		TRANSFER_LENGTH )
{
	
	bool		result				= false;
	UInt32		requestedByteCount	= 0;
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::WRITE_10 called\n" ) );
	
	// Check the validity of the media (make sure there is media in the
	// device, and the blocksize has been determined i.e. it's formatted )
	require_nonzero ( blockSize, ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	require ( IsBufferAndCapacityValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_10,
								( FUA << 3 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								requestedByteCount );	
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIReducedBlockCommands::VERIFY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//		The VERIFY command as defined in section 5.7.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIReducedBlockCommands::VERIFY (
					SCSITask *				request,
					SCSICmdField4Byte 		LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 		VERIFICATION_LENGTH )
{
	
	bool	result	= false;
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::VERIFY called\n" ) );
	
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( VERIFICATION_LENGTH, kSCSICmdFieldMask2Byte ), ErrorExit );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_VERIFY_10,
								0x00,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( VERIFICATION_LENGTH >> 8 ) & 0xFF,
								  VERIFICATION_LENGTH		 & 0xFF,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_NoDataTransfer );	
	
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
//		SCSIReducedBlockCommands::CreateSCSIReducedBlockCommandObject
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//		Get an instance of the command builder
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIReducedBlockCommands *
SCSIReducedBlockCommands::CreateSCSIReducedBlockCommandObject ( void )
{
	
	return OSTypeAlloc ( SCSIReducedBlockCommands );
	
}