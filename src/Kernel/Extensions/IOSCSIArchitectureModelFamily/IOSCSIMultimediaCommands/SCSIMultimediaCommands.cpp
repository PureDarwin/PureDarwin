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
#include "SCSIMultimediaCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"MMC Command Set"

#if DEBUG
#define SCSI_MMC_COMMANDS_DEBUGGING_LEVEL		0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_MMC_COMMANDS_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_MMC_COMMANDS_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_MMC_COMMANDS_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super SCSIPrimaryCommands
OSDefineMetaClassAndStructors ( SCSIMultimediaCommands, SCSIPrimaryCommands );


#if 0
#pragma mark -
#pragma mark MMC Command Methods
#pragma mark -
#endif


// SCSI Multimedia Commands as defined in T10:1228-D MMC
// Revision 11a, August 30, 1999

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::BLANK
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The BLANK command as defined in section 6.1.1.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::BLANK (
				SCSITask *					request,
				SCSICmdField1Bit			IMMED,
				SCSICmdField3Bit			BLANKING_TYPE,
				SCSICmdField4Byte			START_ADDRESS_TRACK_NUMBER,
				SCSICmdField1Byte			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::BLANK called\n" ) );
	
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( BLANKING_TYPE, kSCSICmdFieldMask3Bit ), ErrorExit );
	require ( IsParameterValid ( START_ADDRESS_TRACK_NUMBER,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_BLANK,
								( IMMED << 4 ) | BLANKING_TYPE,
								( START_ADDRESS_TRACK_NUMBER >> 24 )	& 0xFF,
								( START_ADDRESS_TRACK_NUMBER >> 16 )	& 0xFF,
								( START_ADDRESS_TRACK_NUMBER >>  8 )	& 0xFF,
    							  START_ADDRESS_TRACK_NUMBER			& 0xFF,
								0x00,
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
//		SCSIMultimediaCommands::CLOSE_TRACK_SESSION
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The CLOSE TRACK/SESSION command as defined in section 6.1.2.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::CLOSE_TRACK_SESSION (
						SCSITask *					request,
						SCSICmdField1Bit			IMMED,
						SCSICmdField1Bit			SESSION,
						SCSICmdField1Bit			TRACK,
						SCSICmdField2Byte			TRACK_NUMBER,
						SCSICmdField1Byte			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::CLOSE_TRACK_SESSION called\n" ) );
	
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SESSION, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( TRACK, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( TRACK_NUMBER, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_CLOSE_TRACK_SESSION,
								IMMED,
								( SESSION << 1 ) | TRACK,
								0x00,
								( TRACK_NUMBER >>  8 ) & 0xFF,
								  TRACK_NUMBER         & 0xFF,
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
//		SCSIMultimediaCommands::FORMAT_UNIT
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The FORMAT UNIT command as defined in section 6.1.3.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::FORMAT_UNIT (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
	    			IOByteCount					parameterListSize,
					SCSICmdField1Bit 			FMT_DATA,
					SCSICmdField1Bit 			CMP_LIST,
					SCSICmdField3Bit 			FORMAT_CODE,
					SCSICmdField2Byte 			INTERLEAVE_VALUE,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::FORMAT_UNIT called\n" ) );
	
	require ( IsParameterValid ( FMT_DATA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( CMP_LIST, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( INTERLEAVE_VALUE, kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( ( FORMAT_CODE == FORMAT_CODE_CD_RW ) ||
			  ( FORMAT_CODE == FORMAT_CODE_DVD_RAM ), ErrorExit );
	
	if ( FORMAT_CODE == FORMAT_CODE_CD_RW )
	{
		
		require ( ( INTERLEAVE_VALUE == INTERLEAVE_VALUE_CD_RW ), ErrorExit );
		
	}
	
	else if ( FORMAT_CODE == FORMAT_CODE_DVD_RAM )
	{
		
		require ( ( INTERLEAVE_VALUE == INTERLEAVE_VALUE_DVD_RAM ), ErrorExit );
		
	}
		
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	if ( FMT_DATA == FMT_DATA_PRESENT )
	{
		
		// Make sure we have a parameter list
		require_nonzero ( parameterListSize, ErrorExit );
		
		// is the buffer large enough to accomodate this request?
		require ( IsBufferAndCapacityValid ( dataBuffer,
											 parameterListSize ), ErrorExit );
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_FORMAT_UNIT,
								( FMT_DATA << 4 ) | ( CMP_LIST << 3 ) | FORMAT_CODE,
								0x00,
								( INTERLEAVE_VALUE >> 8 ) 	& 0xFF,
								  INTERLEAVE_VALUE        	& 0xFF,
								CONTROL );
	
	if ( parameterListSize == 0 )
	{
		
		// No DEFECT LIST is being sent to the device, so there
		// will be no data transfer for this request.
		SetDataTransferControl ( 	request,
									0,
									kSCSIDataTransfer_NoDataTransfer );
		
	}
	
	else
	{
		
		// The client has requested a DEFECT LIST be sent to the device
		// to be used with the format command
		SetDataTransferControl ( 	request,
                           			0,
									kSCSIDataTransfer_FromInitiatorToTarget,
									dataBuffer,
									parameterListSize );
		
	}
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::GET_CONFIGURATION
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The GET CONFIGURATION command as defined in section 6.1.4.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::GET_CONFIGURATION (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				SCSICmdField2Bit 			RT,
				SCSICmdField2Byte 			STARTING_FEATURE_NUMBER,
				SCSICmdField2Byte 			ALLOCATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::GET_CONFIGURATION called\n" ) );	
	
	require ( IsParameterValid ( RT, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( STARTING_FEATURE_NUMBER,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_GET_CONFIGURATION,
								RT,
								( STARTING_FEATURE_NUMBER >> 8 ) & 0xFF,
								  STARTING_FEATURE_NUMBER        & 0xFF,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) 	& 0xFF,
								  ALLOCATION_LENGTH        	& 0xFF,
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
//		SCSIMultimediaCommands::GET_EVENT_STATUS_NOTIFICATION
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The GET EVENT/STATUS NOTIFICATION command as defined in
//		section 6.1.5.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::GET_EVENT_STATUS_NOTIFICATION (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			IMMED,
				SCSICmdField1Byte 			NOTIFICATION_CLASS_REQUEST,
				SCSICmdField2Byte 			ALLOCATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::GET_EVENT_STATUS_NOTIFICATION called\n" ) );
	
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( NOTIFICATION_CLASS_REQUEST,
								 kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_GET_EVENT_STATUS_NOTIFICATION,
								IMMED,
								0x00,
								0x00,
								NOTIFICATION_CLASS_REQUEST,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 )	& 0xFF,
								  ALLOCATION_LENGTH        	& 0xFF,
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
//		SCSIMultimediaCommands::GET_PERFORMANCE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The GET PERFORMANCE command as defined in section 6.1.6.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::GET_PERFORMANCE (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				SCSICmdField2Bit 			TOLERANCE,
				SCSICmdField1Bit 			WRITE,
				SCSICmdField2Bit 			EXCEPT,
				SCSICmdField4Byte 			STARTING_LBA,
				SCSICmdField2Byte 			MAXIMUM_NUMBER_OF_DESCRIPTORS,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool		result			= false;
	UInt32		returnDataCount = 0;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::GET_PERFORMANCE called\n" ) );
	
	require ( IsParameterValid ( TOLERANCE, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( WRITE, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( EXCEPT, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( STARTING_LBA,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( MAXIMUM_NUMBER_OF_DESCRIPTORS,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// Compute the data count size
	returnDataCount = PERFORMANCE_HEADER_SIZE +
			( PERFORMANCE_DESCRIPTOR_SIZE * MAXIMUM_NUMBER_OF_DESCRIPTORS );
	
	require ( IsBufferAndCapacityValid ( dataBuffer, returnDataCount ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_GET_PERFORMANCE,
								( TOLERANCE << 3 ) | ( WRITE << 2 ) | EXCEPT,
								( STARTING_LBA >> 24 ) 	& 0xFF,
								( STARTING_LBA >> 16 ) 	& 0xFF,
								( STARTING_LBA >>  8 ) 	& 0xFF,
								  STARTING_LBA         	& 0xFF,
								0x00,
								0x00,
								( MAXIMUM_NUMBER_OF_DESCRIPTORS >> 8 )	& 0xFF,
								  MAXIMUM_NUMBER_OF_DESCRIPTORS			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								returnDataCount );	
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::LOAD_UNLOAD_MEDIUM
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The LOAD/UNLOAD MEDIUM command as defined in section 6.1.7.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::LOAD_UNLOAD_MEDIUM (
					SCSITask *					request,
					SCSICmdField1Bit 			IMMED,
					SCSICmdField1Bit 			LO_UNLO,
					SCSICmdField1Bit 			START,
					SCSICmdField1Byte 			SLOT,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::LOAD_UNLOAD_MEDIUM called\n" ) );
	
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LO_UNLO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( START, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SLOT, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_LOAD_UNLOAD_MEDIUM,
								IMMED,
								0x00,
								0x00,
								( LO_UNLO << 1 ) | START,
								0x00,
								0x00,
								0x00,
								SLOT,
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
//		SCSIMultimediaCommands::MECHANISM_STATUS
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The MECHANISM STATUS command as defined in section 6.1.8.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::MECHANISM_STATUS (
					SCSITask *					request,
    				IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::MECHANISM_STATUS called\n" ) );
	
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MECHANISM_STATUS,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) 	& 0xFF,
								  ALLOCATION_LENGTH        	& 0xFF,
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
//		SCSIMultimediaCommands::PAUSE_RESUME
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The PAUSE/RESUME command as defined in section 6.1.9.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::PAUSE_RESUME (
					SCSITask *					request,
					SCSICmdField1Bit 			RESUME,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PAUSE_RESUME called\n" ) );
	
	require ( IsParameterValid ( RESUME, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PAUSE_RESUME,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								RESUME,
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
//		SCSIMultimediaCommands::PLAY_AUDIO_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The PLAY AUDIO (10) command as defined in section 6.1.10.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::PLAY_AUDIO_10 (
			SCSITask *					request,
			SCSICmdField1Bit 			RELADR,
			SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
			SCSICmdField2Byte 			PLAY_LENGTH,
			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PLAY_AUDIO_10 called\n" ) );
	
	require ( ( RELADR == 0 ), ErrorExit );
	require ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( PLAY_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PLAY_AUDIO_10,
								RELADR,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
								0x00,
								( PLAY_LENGTH >> 8 ) & 0xFF,
								  PLAY_LENGTH        & 0xFF,
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
//		SCSIMultimediaCommands::PLAY_AUDIO_12
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The PLAY AUDIO (12) command as defined in section 6.1.11.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::PLAY_AUDIO_12 (
			SCSITask *					request,
			SCSICmdField1Bit 			RELADR,
			SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
			SCSICmdField4Byte 			PLAY_LENGTH,
			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PLAY_AUDIO_12 called\n" ) );
	
	require ( ( RELADR == 0 ), ErrorExit );
	require ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( PLAY_LENGTH,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PLAY_AUDIO_12,
								RELADR,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
								( PLAY_LENGTH >> 24 ) & 0xFF,
								( PLAY_LENGTH >> 16 ) & 0xFF,
								( PLAY_LENGTH >>  8 ) & 0xFF,
								  PLAY_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::PLAY_AUDIO_MSF
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The PLAY AUDIO MSF command as defined in section 6.1.12.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::PLAY_AUDIO_MSF (
					SCSITask *					request,
					SCSICmdField3Byte 			STARTING_MSF,
					SCSICmdField3Byte 			ENDING_MSF,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PLAY_AUDIO_MSF called\n" ) );
	
	require ( IsParameterValid ( STARTING_MSF,
								 kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( ENDING_MSF,
								 kSCSICmdFieldMask3Byte ), ErrorExit );
	
	if ( ( ( STARTING_MSF & 0xFF ) >= FRAMES_IN_A_SECOND )  ||
		 ( ( ( STARTING_MSF >> 8 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ) )
	{
		
		require ( ( STARTING_MSF == 0xFFFFFF ), ErrorExit );
		
	}
	
	require ( ( ( ENDING_MSF & 0xFF ) < FRAMES_IN_A_SECOND ) &&
			  ( ( ( ENDING_MSF >> 8 ) & 0xFF ) < SECONDS_IN_A_MINUTE ),
			  ErrorExit );
	
	require ( ( STARTING_MSF <= ENDING_MSF ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PLAY_AUDIO_MSF,
								0x00,
								0x00,
								( STARTING_MSF >> 16 )	& 0xFF,
								( STARTING_MSF >>  8 )	& 0xFF,
								  STARTING_MSF       	& 0xFF,
								( ENDING_MSF >> 16 )	& 0xFF,
								( ENDING_MSF >>  8 )	& 0xFF,
								  ENDING_MSF			& 0xFF,
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
//		SCSIMultimediaCommands::PLAY_CD
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//	¥¥¥ OBSOLETE ¥¥¥
//
//		The PLAY CD command as defined in section 6.1.13. PLAY CD
//		is obsoleted by the MMC-2 specification.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::PLAY_CD (
			SCSITask *					request,
			SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
			SCSICmdField1Bit 			CMSF,
			SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS, 
			SCSICmdField4Byte 			PLAY_LENGTH_IN_BLOCKS, 
			SCSICmdField1Bit 			SPEED,
			SCSICmdField1Bit 			PORT2,
			SCSICmdField1Bit 			PORT1,
			SCSICmdField1Bit 			COMPOSITE,
			SCSICmdField1Bit 			AUDIO,
			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PLAY_CD *OBSOLETE* called\n" ) );
	
	require ( ( EXPECTED_SECTOR_TYPE < 6 ), ErrorExit );
	
	// Check if CMSF is set for LBA mode
	if ( CMSF == 0 )
	{
		
		STATUS_LOG ( ( "Using LBA Addressing Mode\n" ) );
		
		require ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
									 kSCSICmdFieldMask4Byte ), ErrorExit );
		require ( IsParameterValid ( PLAY_LENGTH_IN_BLOCKS,
									 kSCSICmdFieldMask4Byte ), ErrorExit );
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "Using MSF Addressing Mode\n" ) );
		
		require ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
									 kSCSICmdFieldMask3Byte ), ErrorExit );
		
		require ( ( ( STARTING_LOGICAL_BLOCK_ADDRESS & 0xFF ) < FRAMES_IN_A_SECOND ) &&
				  ( ( ( STARTING_LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF ) < SECONDS_IN_A_MINUTE ),
				  ErrorExit );
		
		// since CMSF is set to 1, bytes 6-8 (the high three bytes of
		// PLAY_LENGTH_IN_BLOCKS) is where our end MSF address is.
		require ( IsParameterValid ( PLAY_LENGTH_IN_BLOCKS >> 8,
									 kSCSICmdFieldMask3Byte ), ErrorExit );
		
		require ( ( ( PLAY_LENGTH_IN_BLOCKS >> 8 & 0xFF ) < FRAMES_IN_A_SECOND ) &&
				  ( ( ( PLAY_LENGTH_IN_BLOCKS >> 16 ) & 0xFF ) < SECONDS_IN_A_MINUTE ),
				  ErrorExit );
		
	}

	require ( IsParameterValid ( SPEED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PORT2, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PORT1, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( COMPOSITE, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( AUDIO, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	if ( CMSF == 0 )
	{
		
		SetCommandDescriptorBlock (	request,
									kSCSICmd_PLAY_CD,
									( EXPECTED_SECTOR_TYPE << 2 ) | ( CMSF << 1 ),
									( STARTING_LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
									( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
									( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
									  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >> 24 ) & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >> 16 ) & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >>  8 ) & 0xFF,
									  PLAY_LENGTH_IN_BLOCKS         & 0xFF,
									( SPEED << 7 ) | ( PORT2 << 3 ) | ( PORT1 << 2 ) | ( COMPOSITE << 1 ) | AUDIO,
									CONTROL );
		
	}
	
	else
	{
		
		// bytes 2 and 9 are reserved!
		SetCommandDescriptorBlock (	request,
									kSCSICmd_PLAY_CD,
									( EXPECTED_SECTOR_TYPE << 2 ) | ( CMSF << 1 ),
									0x00,
									( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
									( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
									  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >> 24 ) & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >> 16 ) & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >>  8 ) & 0xFF,
									0x00,
									( SPEED << 7 ) | ( PORT2 << 3 ) | ( PORT1 << 2 ) | ( COMPOSITE << 1 ) | AUDIO,
									CONTROL );
		
	}
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::READ_BUFFER_CAPACITY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//	¥¥¥ OBSOLETE ¥¥¥
//		
//		The READ BUFFER CAPACITY command as defined in section 6.1.14.
//		READ BUFFER CAPACITY is obsoleted by the MMC-2 specification.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_BUFFER_CAPACITY (
					SCSITask *					request,
    				IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_BUFFER_CAPACITY *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_BUFFER_CAPACITY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH        & 0xFF,
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
//		SCSIMultimediaCommands::READ_CD
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ CD command as defined in section 6.1.15.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_CD (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
				SCSICmdField3Byte 			TRANSFER_LENGTH,
				SCSICmdField1Bit 			SYNC,
				SCSICmdField2Bit 			HEADER_CODES,
				SCSICmdField1Bit 			USER_DATA,
				SCSICmdField1Bit 			EDC_ECC,
				SCSICmdField2Bit 			ERROR_FIELD,
				SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS,
				SCSICmdField1Byte 			CONTROL )
{
	
	UInt32			blockSize			= 0;
	UInt32 			requestedByteCount	= 0;
	bool			validBlockSize		= false;
	bool			result				= false
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_CD called\n" ) );
	
	require ( ( RELADR == 0 ), ErrorExit );
	require ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// determine the size of the transfer
	validBlockSize = GetBlockSize (	&blockSize,
									EXPECTED_SECTOR_TYPE,
									SYNC,
									HEADER_CODES,
									USER_DATA,
									EDC_ECC,
									ERROR_FIELD,
									SUBCHANNEL_SELECTION_BITS );
	
	require ( validBlockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	STATUS_LOG ( ( "blockSize = %ld\n", blockSize ) );
	STATUS_LOG ( ( "TRANSFER_LENGTH = %ld\n", TRANSFER_LENGTH ) );
	STATUS_LOG ( ( "requestedByteCount = %ld\n", requestedByteCount ) );
	
	require ( IsBufferAndCapacityValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CD,
								( EXPECTED_SECTOR_TYPE << 2 ) | RELADR,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >>  8 ) & 0xFF,
								  TRANSFER_LENGTH         & 0xFF,
								( SYNC << 7 ) | ( HEADER_CODES << 5 ) | ( USER_DATA << 4 ) | ( EDC_ECC << 3 ) | ( ERROR_FIELD << 1 ),
								SUBCHANNEL_SELECTION_BITS,
								CONTROL );
	
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
//		SCSIMultimediaCommands::READ_CD_MSF
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ CD MSF command as defined in section 6.1.16.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_CD_MSF (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
				SCSICmdField3Byte 			STARTING_MSF,
				SCSICmdField3Byte 			ENDING_MSF,
				SCSICmdField1Bit 			SYNC,
				SCSICmdField2Bit 			HEADER_CODES,
				SCSICmdField1Bit 			USER_DATA,
				SCSICmdField1Bit 			EDC_ECC,
				SCSICmdField2Bit 			ERROR_FIELD,
				SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS,
				SCSICmdField1Byte 			CONTROL )
{
	
	UInt32			blockSize			= 0;
	UInt32 			requestedByteCount	= 0;
	bool			validBlockSize		= 0;
	bool			result				= 0;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_CD_MSF called\n" ) );
	
	require ( IsParameterValid ( STARTING_MSF,
								 kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( ENDING_MSF,
								 kSCSICmdFieldMask3Byte ), ErrorExit );
	
	require ( ( ( STARTING_MSF & 0xFF ) < FRAMES_IN_A_SECOND ) &&
		 	  ( ( ( STARTING_MSF >> 8 ) & 0xFF ) < SECONDS_IN_A_MINUTE ), ErrorExit );
	require ( ( ( ENDING_MSF & 0xFF ) < FRAMES_IN_A_SECOND ) &&
		 	  ( ( ( ENDING_MSF >> 8 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ), ErrorExit );
	
	require ( ( STARTING_MSF <= ENDING_MSF ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// determine the size of the transfer
	validBlockSize = GetBlockSize (	&blockSize,
									EXPECTED_SECTOR_TYPE,
									SYNC,
									HEADER_CODES,
									USER_DATA,
									EDC_ECC,
									ERROR_FIELD,
									SUBCHANNEL_SELECTION_BITS );
	
	require ( validBlockSize, ErrorExit );
	
	requestedByteCount = ( ConvertMSFToLBA ( ENDING_MSF ) -
							ConvertMSFToLBA ( STARTING_MSF ) ) * blockSize;
	
	STATUS_LOG ( ( "requestedByteCount = %x\n", requestedByteCount ) );
	
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 requestedByteCount ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CD_MSF,
								EXPECTED_SECTOR_TYPE << 2,
								0x00,
								( STARTING_MSF >> 16 )	& 0xFF,
								( STARTING_MSF >>  8 )	& 0xFF,
								  STARTING_MSF        	& 0xFF,
								( ENDING_MSF >> 16 )	& 0xFF,
								( ENDING_MSF >>  8 )	& 0xFF,
								  ENDING_MSF        	& 0xFF,
								( SYNC << 7 ) | ( HEADER_CODES << 5 ) | ( USER_DATA << 4 ) | ( EDC_ECC << 3 ) | ( ERROR_FIELD << 1 ),
								SUBCHANNEL_SELECTION_BITS,
								CONTROL );
	
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
//		SCSIMultimediaCommands::READ_CAPACITY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ CAPACITY command as defined in section 6.1.17.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_CAPACITY (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField1Bit 			PMI,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_CAPACITY called\n" ) );
	
	require ( ( RELADR == 0 ), ErrorExit );
	require ( ( LOGICAL_BLOCK_ADDRESS == 0 ), ErrorExit );
	require ( ( PMI == 0 ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 READ_CAPACITY_MAX_DATA ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CAPACITY,
								RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS         & 0xFF,
								0x00,
								0x00,
								PMI,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								READ_CAPACITY_MAX_DATA );	
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::READ_DISC_INFORMATION
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ DISC INFORMATION command as defined in section 6.1.18.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_DISC_INFORMATION (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_DISC_INFORMATION called\n" ) );
	
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_DISC_INFORMATION,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH        & 0xFF,
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
//		SCSIMultimediaCommands::READ_DVD_STRUCTURE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ DVD STRUCTURE command as defined in section 6.1.19.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_DVD_STRUCTURE (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField4Byte 			ADDRESS,
					SCSICmdField1Byte 			LAYER_NUMBER,
					SCSICmdField1Byte 			FORMAT,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField2Bit 			AGID,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_DVD_STRUCTURE called\n" ) );
	
	require ( IsParameterValid ( ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( LAYER_NUMBER,
								 kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( FORMAT, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( AGID, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_DVD_STRUCTURE,
								0x00,
								( ADDRESS >> 24 ) & 0xFF,
								( ADDRESS >> 16 ) & 0xFF,
								( ADDRESS >>  8 ) & 0xFF,
								  ADDRESS         & 0xFF,
								LAYER_NUMBER,
								FORMAT,
								( ALLOCATION_LENGTH >> 8 )	& 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								AGID << 6,
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
//		SCSIMultimediaCommands::READ_FORMAT_CAPACITIES
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ FORMAT CAPACITIES command as defined in section 6.1.20.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_FORMAT_CAPACITIES (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_FORMAT_CAPACITIES called\n" ) );
	
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_FORMAT_CAPACITIES,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 )	& 0xFF,
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
//		SCSIMultimediaCommands::READ_HEADER
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//	¥¥¥ OBSOLETE ¥¥¥
//		
//		The READ HEADER command as defined in section 6.1.21. READ HEADER
//		is obsoleted by the MMC-2 specification.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_HEADER (
					SCSITask *					request,
    				IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			MSF,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_HEADER *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( MSF, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_HEADER,
								MSF << 1,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS         & 0xFF,
								0x00,
								( ALLOCATION_LENGTH >>  8 ) & 0xFF,
								  ALLOCATION_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::READ_MASTER_CUE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//	¥¥¥ OBSOLETE ¥¥¥
//		
//		The READ MASTER CUE command as defined in section 6.1.22.
//		READ MASTER CUE is obsoleted by the MMC-2 specification.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_MASTER_CUE (
					SCSITask *					request,
    				IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Byte 			SHEET_NUMBER, 
					SCSICmdField3Byte 			ALLOCATION_LENGTH, 
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_MASTER_CUE *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( SHEET_NUMBER,
								 kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_MASTER_CUE,
								0x00,
								0x00,
								0x00,
								SHEET_NUMBER,
								0x00,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >>  8 ) & 0xFF,
								  ALLOCATION_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::READ_SUB_CHANNEL
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ SUB-CHANNEL command as defined in section 6.1.23.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_SUB_CHANNEL (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			MSF,
					SCSICmdField1Bit 			SUBQ,
					SCSICmdField1Byte 			SUB_CHANNEL_PARAMETER_LIST,
					SCSICmdField1Byte 			TRACK_NUMBER,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_SUB_CHANNEL called\n" ) );
	
	require ( IsParameterValid ( MSF, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SUBQ, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( SUB_CHANNEL_PARAMETER_LIST,
								 kSCSICmdFieldMask1Byte ), ErrorExit );
	
	if ( SUB_CHANNEL_PARAMETER_LIST == 3 )
	{
		
		require ( TRACK_NUMBER <= MAX_TRACK_NUMBER, ErrorExit );
		
	}
	
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_SUB_CHANNEL,
								MSF << 1,
								SUBQ << 6,
								SUB_CHANNEL_PARAMETER_LIST,
								0x00,
								0x00,
								( SUB_CHANNEL_PARAMETER_LIST == 3 ) ? TRACK_NUMBER : 0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH        & 0xFF,
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
//		SCSIMultimediaCommands::READ_TOC_PMA_ATIP
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ TOC/PMA/ATIP command as defined in section 6.1.24/25.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_TOC_PMA_ATIP (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			MSF,
					SCSICmdField4Bit 			FORMAT,
					SCSICmdField1Byte			TRACK_SESSION_NUMBER,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
		
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_TOC_PMA_ATIP called\n" ) );
	
	require ( IsParameterValid ( MSF, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( ( ( FORMAT & kSCSICmdFieldMask4Bit ) <= 5 ), ErrorExit );
	require ( IsParameterValid ( TRACK_SESSION_NUMBER,
								 kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// Should we use Òold-styleÓ ATAPI SFF-8020i way?
	if ( FORMAT <= 0x02 )
	{
		
		// Use the ATAPI-SFF 8020i "old style" way of issuing READ_TOC_PMA_ATIP
		
		// fill out the cdb appropriately  
		SetCommandDescriptorBlock (	request,
									kSCSICmd_READ_TOC_PMA_ATIP,
									MSF << 1,
									0x00,
									0x00,
									0x00,
									0x00,
									TRACK_SESSION_NUMBER,
									( ALLOCATION_LENGTH >> 8 ) & 0xFF,
									  ALLOCATION_LENGTH        & 0xFF,
									( FORMAT & 0x03 ) << 6 );
		
	}
	
	else
	{
		
		// Use the MMC-2 "new style" way of issuing READ_TOC_PMA_ATIP
		
		// fill out the cdb appropriately  
		SetCommandDescriptorBlock (	request,
									kSCSICmd_READ_TOC_PMA_ATIP,
									MSF << 1,
									FORMAT,
									0x00,
									0x00,
									0x00,
									TRACK_SESSION_NUMBER,
									( ALLOCATION_LENGTH >> 8 ) & 0xFF,
									ALLOCATION_LENGTH        & 0xFF,
									CONTROL );
		
	}
	
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
//		SCSIMultimediaCommands::READ_TRACK_INFORMATION
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The READ TRACK INFORMATION command as defined in section 6.1.26.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::READ_TRACK_INFORMATION (
			SCSITask *					request,
			IOMemoryDescriptor *		dataBuffer,
			SCSICmdField2Bit 			ADDRESS_NUMBER_TYPE,
			SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
			SCSICmdField2Byte 			ALLOCATION_LENGTH,
			SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_TRACK_INFORMATION called\n" ) );
	
	require ( IsParameterValid ( ADDRESS_NUMBER_TYPE,
								 kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 ALLOCATION_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_TRACK_INFORMATION,
								ADDRESS_NUMBER_TYPE,
								( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >>  8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER         & 0xFF,
								0x00,
								( ALLOCATION_LENGTH >>  8 ) & 0xFF,
								  ALLOCATION_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::REPAIR_TRACK
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//	¥¥¥ OBSOLETE ¥¥¥
//		
//		The REPAIR TRACK command as defined in section 6.1.27.
//		REPAIR TRACK is obsoleted by the MMC-2 specification.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::REPAIR_TRACK (
					SCSITask *					request,
					SCSICmdField2Byte 			TRACK_NUMBER,
					SCSICmdField1Byte 			CONTROL )

{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::REPAIR_TRACK *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( TRACK_NUMBER,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REPAIR_TRACK,
								0x00,
								0x00,
								0x00,
								( TRACK_NUMBER >>  8 ) & 0xFF,
								  TRACK_NUMBER         & 0xFF,
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
//		SCSIMultimediaCommands::REPORT_KEY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The REPORT KEY command as defined in section 6.1.28.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::REPORT_KEY (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField2Bit 			AGID,
					SCSICmdField6Bit 			KEY_FORMAT,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::REPORT_KEY called\n" ) );
	
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( ALLOCATION_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( AGID, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( KEY_FORMAT,
								 kSCSICmdFieldMask6Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// Only check the buffer if the key format is not INVALIDATE_AGID
	if ( KEY_FORMAT != 0x3F )
	{
		
		// is the buffer large enough to accomodate this request?
		require ( IsBufferAndCapacityValid ( dataBuffer,
											 ALLOCATION_LENGTH ), ErrorExit );
		
		SetDataTransferControl (	request,
									0,
									kSCSIDataTransfer_FromTargetToInitiator,
									dataBuffer,
									ALLOCATION_LENGTH );	
		
	}
	
	else
	{
		
		SetDataTransferControl (	request,
									0,
									kSCSIDataTransfer_NoDataTransfer );
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REPORT_KEY,
								0x00,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS         & 0xFF,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH        & 0xFF,
								( AGID << 6 ) | KEY_FORMAT,
								CONTROL );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::RESERVE_TRACK
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The RESERVE TRACK command as defined in section 6.1.29.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::RESERVE_TRACK (
					SCSITask *					request,
					SCSICmdField4Byte			RESERVATION_SIZE,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::RESERVE_TRACK called\n" ) );
	
	require ( IsParameterValid ( RESERVATION_SIZE,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_TRACK,
								0x00,
								0x00,
								0x00,
								0x00,
								( RESERVATION_SIZE >> 24 ) & 0xFF,
								( RESERVATION_SIZE >> 16 ) & 0xFF,
								( RESERVATION_SIZE >>  8 ) & 0xFF,
								  RESERVATION_SIZE         & 0xFF,
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
//		SCSIMultimediaCommands::SCAN
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SCAN command as defined in section 6.1.30.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SCAN (
				SCSITask *					request,
				SCSICmdField1Bit 			DIRECT,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte			SCAN_STARTING_ADDRESS_FIELD,
				SCSICmdField2Bit 			TYPE,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SCAN called\n" ) );
	
	require ( IsParameterValid ( DIRECT, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( ( RELADR == 0 ), ErrorExit );
	
	// did we receive a valid TYPE?
	switch ( TYPE )
	{
		
		// LBA
		case 0:
		{
			
			STATUS_LOG ( ( "Using LBA TYPE\n" ) );
			
			require ( IsParameterValid ( SCAN_STARTING_ADDRESS_FIELD,
										 kSCSICmdFieldMask4Byte ), ErrorExit );
			break;
			
		}
		
		// MSF
		case 1:
		{
			
			STATUS_LOG ( ( "Using MSF TYPE\n" ) );
			
			require ( IsParameterValid ( SCAN_STARTING_ADDRESS_FIELD,
										 kSCSICmdFieldMask3Byte ), ErrorExit );
			
			require ( ( ( SCAN_STARTING_ADDRESS_FIELD & 0xFF ) < FRAMES_IN_A_SECOND ) &&
				 	  ( ( ( SCAN_STARTING_ADDRESS_FIELD >> 8 ) & 0xFF ) < SECONDS_IN_A_MINUTE ),
				 	  ErrorExit );
			break;
			
		}
		
		// track number
		case 2:
		{
			
			STATUS_LOG ( ( "Using Track Number TYPE\n" ) );
			
			require ( ( SCAN_STARTING_ADDRESS_FIELD <= MAX_TRACK_NUMBER ), ErrorExit );
			
			break;
			
		}
		
		// invalid TYPE
		default:
		{
			
			ERROR_LOG ( ( "TYPE = %x not valid \n", TYPE ) );
			goto ErrorExit;
			break;
			
		}
		
	}
	
	// did we receive a valid CONTROL?
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SCAN_MMC,
								( DIRECT << 4 ) | RELADR,
								( SCAN_STARTING_ADDRESS_FIELD >> 24 ) & 0xFF,
								( SCAN_STARTING_ADDRESS_FIELD >> 16 ) & 0xFF,
								( SCAN_STARTING_ADDRESS_FIELD >>  8 ) & 0xFF,
								  SCAN_STARTING_ADDRESS_FIELD         & 0xFF,
								0x00,
								0x00,
								0x00,
								TYPE << 6,
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
//		SCSIMultimediaCommands::SEND_CUE_SHEET
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SEND CUE SHEET command as defined in section 6.1.31.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SEND_CUE_SHEET (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField3Byte			CUE_SHEET_SIZE,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_CUE_SHEET called\n" ) );
	
	require_nonzero ( CUE_SHEET_SIZE, ErrorExit );
	require ( IsParameterValid ( CUE_SHEET_SIZE,
								 kSCSICmdFieldMask3Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 CUE_SHEET_SIZE ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_CUE_SHEET,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( CUE_SHEET_SIZE >> 16 ) & 0xFF,
								( CUE_SHEET_SIZE >>  8 ) & 0xFF,
								  CUE_SHEET_SIZE         & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								CUE_SHEET_SIZE );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::SEND_DVD_STRUCTURE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SEND DVD STRUCTURE command as defined in section 6.1.32.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SEND_DVD_STRUCTURE (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Byte			FORMAT,
					SCSICmdField2Byte 			STRUCTURE_DATA_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_DVD_STRUCTURE called\n" ) );
	
	require ( IsParameterValid ( FORMAT, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsParameterValid ( STRUCTURE_DATA_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 STRUCTURE_DATA_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_DVD_STRUCTURE,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								FORMAT,
								( STRUCTURE_DATA_LENGTH >>  8 ) & 0xFF,
								  STRUCTURE_DATA_LENGTH         & 0xFF,
								0x00,
								CONTROL );
	
	if ( STRUCTURE_DATA_LENGTH == 0 )
	{
		
		// No DVD structure is being sent to the device, so there
		// will be no data transfer for this request.
		SetDataTransferControl ( 	request,
									0,
									kSCSIDataTransfer_NoDataTransfer );
		
	}
	
	else
	{
		
		// The client has requested a DVD structure be sent to the device
		// to be used with the format command
		SetDataTransferControl ( 	request,
	                           		0,
									kSCSIDataTransfer_FromInitiatorToTarget,
									dataBuffer,
									STRUCTURE_DATA_LENGTH );
		
	}
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::SEND_EVENT
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SEND EVENT command as defined in section 6.1.33.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SEND_EVENT (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			IMMED,
					SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
					SCSICmdField1Byte 			CONTROL )

{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_EVENT called\n" ) );
	
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_EVENT,
								IMMED,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >>  8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH         & 0xFF,
								0x00,
								CONTROL );

	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::SEND_KEY
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SEND KEY command as defined in section 6.1.34.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SEND_KEY (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
					SCSICmdField2Bit 			AGID,
					SCSICmdField6Bit 			KEY_FORMAT,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_KEY called\n" ) );
	
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( AGID, kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( KEY_FORMAT, kSCSICmdFieldMask6Bit ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// Only check the buffer if the key format is not INVALIDATE_AGID
	if ( KEY_FORMAT != 0x3F )
	{
		
		// is the buffer large enough to accomodate this request?
		require ( IsBufferAndCapacityValid ( dataBuffer,
											 PARAMETER_LIST_LENGTH ), ErrorExit );
		
		SetDataTransferControl (	request,
									0,
									kSCSIDataTransfer_FromInitiatorToTarget,
									dataBuffer,
									PARAMETER_LIST_LENGTH );	
		
	}
	
	else
	{
		
		SetDataTransferControl (	request,
									0,
									kSCSIDataTransfer_NoDataTransfer );
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_KEY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH        & 0xFF,
								( AGID << 6 ) | KEY_FORMAT,
								CONTROL );
		
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::SEND_OPC_INFORMATION
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SEND OPC INFORMATION command as defined in section 6.1.35.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SEND_OPC_INFORMATION (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			DO_OPC,
					SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
					SCSICmdField1Byte 			CONTROL )

{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_OPC_INFORMATION called\n" ) );
	
	require ( IsParameterValid ( DO_OPC, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_OPC_INFORMATION,
								DO_OPC,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >>  8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::SET_CD_SPEED
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		
//	¥¥¥ OBSOLETE ¥¥¥
//		
//		The SET CD SPEED command as defined in section 6.1.36.
//		SET CD SPEED is obsoleted by the MMC-2 specification.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SET_CD_SPEED (
				SCSITask *					request,
				SCSICmdField2Byte 			LOGICAL_UNIT_READ_SPEED,
				SCSICmdField2Byte 			LOGICAL_UNIT_WRITE_SPEED,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SET_CD_SPEED *OBSOLETE* called\n" ) );
	
	require ( IsParameterValid ( LOGICAL_UNIT_READ_SPEED,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_UNIT_WRITE_SPEED,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SET_CD_SPEED,
								0x00,
								( LOGICAL_UNIT_READ_SPEED >>   8 )	& 0xFF,
								  LOGICAL_UNIT_READ_SPEED			& 0xFF,
								( LOGICAL_UNIT_WRITE_SPEED >>  8 )	& 0xFF,
								  LOGICAL_UNIT_WRITE_SPEED			& 0xFF,
								0x00,
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
//		SCSIMultimediaCommands::SET_READ_AHEAD
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SET READ AHEAD command as defined in section 6.1.37.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SET_READ_AHEAD (
				SCSITask *					request,
				SCSICmdField4Byte 			TRIGGER_LOGICAL_BLOCK_ADDRESS,
				SCSICmdField4Byte 			READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
				SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SET_READ_AHEAD called\n" ) );
	
	require ( IsParameterValid ( TRIGGER_LOGICAL_BLOCK_ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SET_READ_AHEAD,
								0x00,
								( TRIGGER_LOGICAL_BLOCK_ADDRESS >> 24 )		& 0xFF,
								( TRIGGER_LOGICAL_BLOCK_ADDRESS >> 16 )		& 0xFF,
								( TRIGGER_LOGICAL_BLOCK_ADDRESS >>  8 )		& 0xFF,
								  TRIGGER_LOGICAL_BLOCK_ADDRESS				& 0xFF,
								( READ_AHEAD_LOGICAL_BLOCK_ADDRESS >> 24 )	& 0xFF,
								( READ_AHEAD_LOGICAL_BLOCK_ADDRESS >> 16 )	& 0xFF,
								( READ_AHEAD_LOGICAL_BLOCK_ADDRESS >>  8 )	& 0xFF,
								  READ_AHEAD_LOGICAL_BLOCK_ADDRESS			& 0xFF,
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
//		SCSIMultimediaCommands::SET_STREAMING
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SET STREAMING command as defined in section 6.1.38.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SET_STREAMING (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SET_STREAMING called\n" ) );
	
	require ( IsParameterValid ( PARAMETER_LIST_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	require ( IsBufferAndCapacityValid ( dataBuffer,
										 PARAMETER_LIST_LENGTH ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SET_STREAMING,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >>  8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::STOP_PLAY_SCAN
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The STOP PLAY/SCAN command as defined in section 6.1.39.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::STOP_PLAY_SCAN (
					SCSITask *					request,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::STOP_PLAY_SCAN called\n" ) );
	
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_STOP_PLAY_SCAN,
								0x00,
								0x00,
								0x00,
								0x00,
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
//		SCSIMultimediaCommands::SYNCHRONIZE_CACHE
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The SYNCHRONIZE CACHE command as defined in section 6.1.40.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::SYNCHRONIZE_CACHE (
					SCSITask *					request,
					SCSICmdField1Bit 			IMMED,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
					SCSICmdField1Byte 			CONTROL )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SYNCHRONIZE_CACHE called\n" ) );
	
	require ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( ( RELADR == 0 ), ErrorExit );
	require ( ( LOGICAL_BLOCK_ADDRESS == 0 ), ErrorExit );
	require ( ( NUMBER_OF_BLOCKS == 0 ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SYNCHRONIZE_CACHE,
								( IMMED << 1 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >>  24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>   8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS          & 0xFF,
								0x00,
								( NUMBER_OF_BLOCKS >>   8 ) & 0xFF,
								  NUMBER_OF_BLOCKS          & 0xFF,
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
//		SCSIMultimediaCommands::WRITE_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The WRITE (10) command as defined in section 6.1.41.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::WRITE_10 (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer, 
	    			UInt32						blockSize,
					SCSICmdField1Bit 			DPO,
					SCSICmdField1Bit 			FUA,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 			TRANSFER_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	UInt32 		requestedByteCount	= 0;
	bool		result 				= false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::WRITE_10 called\n" ) );
	
	require_nonzero ( blockSize, ErrorExit );
	require ( ( DPO == 0 ), ErrorExit );
	require ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( ( RELADR == 0 ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS,
								 kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH,
								 kSCSICmdFieldMask2Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// is the buffer large enough to accomodate this request?
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	require ( IsBufferAndCapacityValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_10,
								( DPO << 4 ) | ( FUA << 3 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >>  24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>   8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS          & 0xFF,
								0x00,
 								( TRANSFER_LENGTH >>  8 ) & 0xFF,
								  TRANSFER_LENGTH         & 0xFF,
								CONTROL );
	
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
//		SCSIMultimediaCommands::WRITE_AND_VERIFY_10
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The WRITE AND VERIFY (10) command as defined in section 6.1.42.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::WRITE_AND_VERIFY_10 (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
	    			UInt32						blockSize,
					SCSICmdField1Bit 			DPO,
					SCSICmdField1Bit 			BYT_CHK,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField4Byte 			TRANSFER_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	UInt32 		requestedByteCount 	= 0;
	bool		result				= false;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::WRITE_AND_VERIFY_10 called\n" ) );
	
	require_nonzero ( blockSize, ErrorExit );
	require ( ( DPO == 0 ), ErrorExit );
	require ( ( BYT_CHK == 0 ), ErrorExit );
	require ( ( RELADR == 0 ), ErrorExit );
	require ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ), ErrorExit );
	require ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ), ErrorExit );
	
	// is the buffer large enough to accomodate this request?
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	require ( IsBufferAndCapacityValid ( dataBuffer, requestedByteCount ), ErrorExit );
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_AND_VERIFY_10,
								( DPO << 4 ) | ( BYT_CHK << 1 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >>  24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>   8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS          & 0xFF,
								( TRANSFER_LENGTH >>  24 ) & 0xFF,
								( TRANSFER_LENGTH >>  16 ) & 0xFF,
								( TRANSFER_LENGTH >>   8 ) & 0xFF,
								  TRANSFER_LENGTH          & 0xFF,
								0x00,
								CONTROL );
	
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
//		SCSIMultimediaCommands::GetBlockSize
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The block size decoding for Read CD and Read CD MSF as
//		defined in table 255.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIMultimediaCommands::GetBlockSize (
					UInt32 *					requestedByteCount,
					SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
					SCSICmdField1Bit 			SYNC,
					SCSICmdField2Bit 			HEADER_CODES,
					SCSICmdField1Bit 			USER_DATA,
					SCSICmdField1Bit 			EDC_ECC,
					SCSICmdField2Bit 			ERROR_FIELD,
					SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS )
{
	
	bool			result			= false;
	UInt32			userDataSize	= 0;
	UInt32			edcEccSize		= 0;
	UInt32			headerSize		= 0;
	UInt32			subHeaderSize	= 0;
	UInt32			syncSize		= 0;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::GetBlockSize called\n" ) );
	
	check ( requestedByteCount );
	
	require ( ( EXPECTED_SECTOR_TYPE < 6 ), ErrorExit );
	require ( IsParameterValid ( SYNC, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( HEADER_CODES,
								 kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( USER_DATA,
								 kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( EDC_ECC, kSCSICmdFieldMask1Bit ), ErrorExit );
	require ( IsParameterValid ( ERROR_FIELD,
								 kSCSICmdFieldMask2Bit ), ErrorExit );
	require ( IsParameterValid ( SUBCHANNEL_SELECTION_BITS,
								 kSCSICmdFieldMask3Bit ), ErrorExit );
	
	// valid flag combination?
	switch ( ( SYNC << 4 ) | ( HEADER_CODES << 2 ) | ( USER_DATA << 1 ) |  EDC_ECC )
	{
		
		// invalid flag combinations
		case	0x00:		// nothing
		case	0x01:		// EDC_ECC
		case	0x05:		// HEADER + EDC_ECC
		case	0x09:		// SUB-HEADER + EDC_ECC
		case	0x0D:		// SUB-HEADER + HEADER + EDC_ECC
		case	0x10:		// SYNC
		case	0x11:		// SYNC + EDC_ECC
		case	0x12:		// SYNC + USER_DATA
		case	0x13:		// SYNC + USER_DATA + EDC_ECC
		case	0x15:		// SYNC + HEADER + EDC_ECC
		case	0x18:		// SYNC + SUB-HEADER
		case	0x19:		// SYNC + SUB-HEADER + EDC_ECC
		case	0x1A:		// SYNC + SUB-HEADER + USER_DATA
		case	0x1B:		// SYNC + SUB-HEADER + USER_DATA + EDC_ECC
		case	0x1D:		// SYNC + SUB-HEADER + HEADER + EDC_ECC
		{
		
			ERROR_LOG ( ( "invalid flag combo\n" ) );
			return false;
			
		}
		
		case	0x02:		// USER_DATA
		case	0x03:		// USER_DATA + EDC_ECC
		case	0x04:		// HEADER
		case	0x08:		// SUB-HEADER
		case	0x0A:		// SUB-HEADER + USER_DATA
		case	0x0B:		// SUB-HEADER + USER_DATA + EDC_ECC
		case	0x0C:		// SUB-HEADER + HEADER
		case	0x0E:		// SUB-HEADER + HEADER + USER_DATA
		case	0x0F:		// SUB-HEADER + HEADER + USER_DATA + EDC_ECC
		case	0x14:		// SYNC + HEADER
		case	0x1E:		// SYNC + SUB-HEADER + HEADER + USER_DATA
		case	0x1F:		// SYNC + SUB-HEADER + HEADER + USER_DATA + EDC_ECC
		{
			
			// legal combination
			break;
			
		}
		
		case	0x06:		// HEADER + USER_DATA
		case	0x07:		// HEADER + USER_DATA + EDC_ECC
		case	0x16:		// SYNC + HEADER + USER_DATA
		case	0x17:		// SYNC + HEADER + USER_DATA + EDC_ECC
		case	0x1C:		// SYNC + SUB-HEADER + HEADER
		{
			
			// illegal for mode 2, form 1 and mode 2, form 2 sectors
			
			require ( ( EXPECTED_SECTOR_TYPE < 4 ), ErrorExit );			
			break;
			
		}
		
		default:
		{
			goto ErrorExit;
		}
		
	}
	
	headerSize	= 4;
	syncSize	= 12;
	
	switch ( EXPECTED_SECTOR_TYPE )
	{
		
		case 0:		// all types
		case 1:		// CD-DA
		{
			break;
		}
		
		case 2:		// mode 1
		{
			userDataSize	= 2048;
			edcEccSize		= 288;
			subHeaderSize	= 0;
			break;
		}
		
		case 3:		// mode 2, formless
		{
			userDataSize	= 2048 + 288;
			edcEccSize		= 0;
			subHeaderSize	= 0;
			break;
		}
		
		case 4:		// mode 2, form 1
		{
			userDataSize	= 2048;
			edcEccSize		= 280;
			subHeaderSize	= 8;
			break;
		}
		
		case 5:		// mode 2, form 2
		{
			userDataSize	= 2048 + 280;
			edcEccSize		= 0;
			subHeaderSize	= 8;
			break;
		}
		
		default:
		{
			goto ErrorExit;
		}
		
	}
	
	if ( ( EXPECTED_SECTOR_TYPE == 0 ) || ( EXPECTED_SECTOR_TYPE == 1 ) )
	{
		
		*requestedByteCount = 2352;
		
	}
	
	else
	{
		
		*requestedByteCount = 0;
		
		if ( SYNC )
		{
			
			*requestedByteCount += syncSize;
			
		}
		
		if ( HEADER_CODES & 0x01 )
		{
			
			*requestedByteCount += headerSize;
			
		}
		
		if ( HEADER_CODES & 0x02 )
		{
			
			*requestedByteCount += subHeaderSize;
			
		}
		
		if ( USER_DATA )
		{
			
			*requestedByteCount += userDataSize;
			
		}
		
		if ( EDC_ECC )
		{
			
			*requestedByteCount += edcEccSize;
			
		}
		
	}
	
	if ( ( ERROR_FIELD & 0x03 ) == 0x01 )
	{
		
		*requestedByteCount += C2_ERROR_BLOCK_DATA_SIZE;
		
	}
	
	else if ( ( ERROR_FIELD & 0x03 ) == 0x02 )
	{
		
		*requestedByteCount += C2_AND_BLOCK_ERROR_BITS_SIZE;
		
	}
	
	if ( SUBCHANNEL_SELECTION_BITS == 0x01 )
	{
		
		*requestedByteCount += SUBCHANNEL_DATA_SIZE;
		
	}

	else if ( SUBCHANNEL_SELECTION_BITS == 0x02 )
	{
		
		*requestedByteCount += SUBCHANNELQ_DATA_SIZE;
 		
 	}
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::ConvertMSFToLBA
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		The MSF to LBA conversion routine.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSICmdField4Byte
SCSIMultimediaCommands::ConvertMSFToLBA (
								SCSICmdField3Byte 	MSF )
{
	
	SCSICmdField4Byte	LBA = 0;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::ConvertMSFToLBA called\n" ) );
	
	LBA  = MSF >> 16;				// start with minutes
	LBA *= SECONDS_IN_A_MINUTE;		// convert minutes to seconds
	LBA += ( MSF >>  8 ) & 0xFF;	// add seconds
	LBA *= FRAMES_IN_A_SECOND;		// convert seconds to frames
	LBA += MSF & 0xFF;				// add frames
	
	require_action ( ( LBA >= LBA_0_OFFSET ), ErrorExit, LBA = 0 );
	
	LBA -= LBA_0_OFFSET;		// subtract the offset of LBA 0
	
	
ErrorExit:
	
	
	return LBA;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		SCSIMultimediaCommands::CreateSCSIMultimediaCommandObject
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		return instance of the command builder
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIMultimediaCommands *
SCSIMultimediaCommands::CreateSCSIMultimediaCommandObject ( void )
{

	return OSTypeAlloc ( SCSIMultimediaCommands );

}