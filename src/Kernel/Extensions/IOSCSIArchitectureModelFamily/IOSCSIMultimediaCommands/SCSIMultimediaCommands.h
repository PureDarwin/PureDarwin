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
 
 
/*
 * ABOUT THIS FILE
 *		This file contains the definition for the command object that 
 * 		represents the SCSI Multimedia Commands specification.  
 *
 * NOTES
 *		Some commands have been changed between versions of the specification.
 *		These changes may include adding fields to a command, obsoleting fields
 * 		in a command or obsoleting of an entire commmand.  Methods that 
 *		implement the previous version are bracketted with the tag "LEGACY
 *		COMMAND SUPPORT".
 *
 */

#ifndef _IOKIT_SCSI_MULTIMEDIA_COMMANDS_H_
#define _IOKIT_SCSI_MULTIMEDIA_COMMANDS_H_

#if defined(KERNEL) && defined(__cplusplus)


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// General IOKit headers
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

// SCSI Architecture Model Family includes
#include "SCSIPrimaryCommands.h"
#include <IOKit/scsi/SCSICommandDefinitions.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define		SECONDS_IN_A_MINUTE					60
#define		FRAMES_IN_A_SECOND					75
#define		MAX_TRACK_NUMBER					99
#define		LBA_0_OFFSET						2 * FRAMES_IN_A_SECOND
#define		READ_CAPACITY_MAX_DATA				8
#define		FORMAT_CODE_CD_RW					7
#define		FORMAT_CODE_DVD_RAM					1
#define		INTERLEAVE_VALUE_CD_RW				0
#define		INTERLEAVE_VALUE_DVD_RAM			0
#define		FMT_DATA_PRESENT					1
#define		PERFORMANCE_HEADER_SIZE				8
#define		PERFORMANCE_DESCRIPTOR_SIZE			8
#define		C2_ERROR_BLOCK_DATA_SIZE			294
#define		C2_AND_BLOCK_ERROR_BITS_SIZE		296
#define		SUBCHANNEL_DATA_SIZE				96
#define		SUBCHANNELQ_DATA_SIZE				16


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSIMultimediaCommands : public SCSIPrimaryCommands
{
	
	OSDeclareDefaultStructors ( SCSIMultimediaCommands )
	
public:

	// singleton accessor method
	static SCSIMultimediaCommands * CreateSCSIMultimediaCommandObject ( void );

   	// SCSI Multimedia Commands as defined in T10:1228-D MMC
   	// Revision 11a, August 30, 1999
	
	// The BLANK command as defined in section 6.1.1
    virtual bool	BLANK (
						SCSITask *					request,
		    			SCSICmdField1Bit 			IMMED,
						SCSICmdField3Bit 			BLANKING_TYPE,
						SCSICmdField4Byte 			START_ADDRESS_TRACK_NUMBER,
						SCSICmdField1Byte 			CONTROL );
	
	// The CLOSE TRACK/SESSION command as defined in section 6.1.2
    virtual bool	CLOSE_TRACK_SESSION (
						SCSITask *					request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			SESSION,
						SCSICmdField1Bit 			TRACK,
						SCSICmdField2Byte 			TRACK_NUMBER,
						SCSICmdField1Byte 			CONTROL );
	
	// The FORMAT UNIT command as defined in section 6.1.3
    virtual bool	FORMAT_UNIT (
						SCSITask *					request,
		    			IOMemoryDescriptor *		dataBuffer,
		    			IOByteCount					parameterListSize,
						SCSICmdField1Bit 			FMT_DATA,
						SCSICmdField1Bit 			CMP_LIST,
						SCSICmdField3Bit 			FORMAT_CODE,
						SCSICmdField2Byte 			INTERLEAVE_VALUE,
						SCSICmdField1Byte 			CONTROL );
	
	// The GET CONFIGURATION command as defined in section 6.1.4
    virtual bool	GET_CONFIGURATION (
						SCSITask *					request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			RT,
						SCSICmdField2Byte 			STARTING_FEATURE_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The GET EVENT/STATUS NOTIFICATION command as defined in section 6.1.5
    virtual bool	GET_EVENT_STATUS_NOTIFICATION (
						SCSITask *					request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Byte 			NOTIFICATION_CLASS_REQUEST,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The GET PERFORMANCE command as defined in section 6.1.6
    virtual bool	GET_PERFORMANCE (
						SCSITask *					request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			TOLERANCE,
						SCSICmdField1Bit 			WRITE,
						SCSICmdField2Bit 			EXCEPT,
						SCSICmdField4Byte 			STARTING_LBA,
						SCSICmdField2Byte 			MAXIMUM_NUMBER_OF_DESCRIPTORS,
						SCSICmdField1Byte 			CONTROL );
	
	// The LOAD/UNLOAD MEDIUM command as defined in section 6.1.7
    virtual bool	LOAD_UNLOAD_MEDIUM (
						SCSITask *					request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			LO_UNLO,
						SCSICmdField1Bit 			START,
						SCSICmdField1Byte 			SLOT,
						SCSICmdField1Byte 			CONTROL );
	
	// The MECHANISM STATUS command as defined in section 6.1.8
    virtual bool	MECHANISM_STATUS (
						SCSITask *					request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The PAUSE/RESUME command as defined in section 6.1.9
    virtual bool	PAUSE_RESUME (
						SCSITask *					request,
						SCSICmdField1Bit 			RESUME,
						SCSICmdField1Byte 			CONTROL );
	
	// The PLAY AUDIO (10) command as defined in section 6.1.10
    virtual bool	PLAY_AUDIO_10 (
						SCSITask *					request,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			PLAY_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The PLAY AUDIO (12) command as defined in section 6.1.11
    virtual bool	PLAY_AUDIO_12 (
						SCSITask *					request,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			PLAY_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The PLAY AUDIO MSF command as defined in section 6.1.12
    virtual bool	PLAY_AUDIO_MSF (
						SCSITask *					request,
						SCSICmdField3Byte 			STARTING_MSF,
						SCSICmdField3Byte 			ENDING_MSF,
						SCSICmdField1Byte 			CONTROL );
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The PLAY CD command as defined in section 6.1.13
    virtual bool	PLAY_CD (
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
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The READ BUFFER CAPACITY command as defined in section 6.1.14
    virtual bool	READ_BUFFER_CAPACITY (
						SCSITask *					request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
	// The READ CD command as defined in section 6.1.15
    virtual bool	READ_CD (
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
						SCSICmdField1Byte 			CONTROL );
	
	// The READ CD MSF command as defined in section 6.1.16
    virtual bool	READ_CD_MSF (
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
						SCSICmdField1Byte 			CONTROL );
	
	// The READ CAPACITY command as defined in section 6.1.17
    virtual bool	READ_CAPACITY (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Bit 			PMI,
						SCSICmdField1Byte 			CONTROL );
	
	// The READ DISC INFORMATION command as defined in section 6.1.18
    virtual bool	READ_DISC_INFORMATION (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The READ DVD STRUCTURE command as defined in section 6.1.19
    virtual bool	READ_DVD_STRUCTURE (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte 			ADDRESS,
						SCSICmdField1Byte 			LAYER_NUMBER,
						SCSICmdField1Byte 			FORMAT,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField1Byte 			CONTROL );
	
	// The READ FORMAT CAPACITIES command as defined in section 6.1.20
    virtual bool	READ_FORMAT_CAPACITIES (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );

	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The READ HEADER command as defined in section 6.1.21
    virtual bool	READ_HEADER (
						SCSITask *					request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The READ MASTER CUE command as defined in section 6.1.22
    virtual bool	READ_MASTER_CUE (
						SCSITask *					request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte 			SHEET_NUMBER,
						SCSICmdField3Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/

	// The READ SUB-CHANNEL command as defined in section 6.1.23
    virtual bool	READ_SUB_CHANNEL (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField1Bit 			SUBQ,
						SCSICmdField1Byte 			SUB_CHANNEL_PARAMETER_LIST,
						SCSICmdField1Byte 			TRACK_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The READ TOC/PMA/ATIP command as defined in section 6.1.24/25
    virtual bool	READ_TOC_PMA_ATIP (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField4Bit 			FORMAT,
						SCSICmdField1Byte			TRACK_SESSION_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The READ TRACK INFORMATION command as defined in section 6.1.26
    virtual bool	READ_TRACK_INFORMATION (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			ADDRESS_NUMBER_TYPE,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL );

	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The REPAIR TRACK command as defined in section 6.1.27
    virtual bool	REPAIR_TRACK (
						SCSITask *					request,
						SCSICmdField2Byte 			TRACK_NUMBER,
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/

	// The REPORT KEY command as defined in section 6.1.28
    virtual bool	REPORT_KEY (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField6Bit 			KEY_FORMAT,
						SCSICmdField1Byte 			CONTROL );
	
	// The RESERVE TRACK command as defined in section 6.1.29
    virtual bool	RESERVE_TRACK (
						SCSITask *					request,
						SCSICmdField4Byte			RESERVATION_SIZE,
						SCSICmdField1Byte 			CONTROL );
	
	// The SCAN command as defined in section 6.1.30
    virtual bool	SCAN (
						SCSITask *					request,
						SCSICmdField1Bit 			DIRECT,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte			SCAN_STARTING_ADDRESS_FIELD,
						SCSICmdField2Bit 			TYPE,
						SCSICmdField1Byte 			CONTROL );
	
	// The SEND CUE SHEET command as defined in section 6.1.31
    virtual bool	SEND_CUE_SHEET (
						SCSITask *					request,
	    				IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Byte			CUE_SHEET_SIZE,
						SCSICmdField1Byte 			CONTROL );
	
	// The SEND DVD STRUCTURE command as defined in section 6.1.32
    virtual bool	SEND_DVD_STRUCTURE (
						SCSITask *					request,
	   	 				IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte			FORMAT,
						SCSICmdField2Byte 			STRUCTURE_DATA_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The SEND EVENT command as defined in section 6.1.33
    virtual bool	SEND_EVENT (
						SCSITask *					request,
	   		 			IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The SEND KEY command as defined in section 6.1.34
    virtual bool	SEND_KEY (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField6Bit 			KEY_FORMAT,
						SCSICmdField1Byte 			CONTROL );
	
	// The SEND OPC INFORMATION command as defined in section 6.1.35
    virtual bool	SEND_OPC_INFORMATION (
						SCSITask *					request,
	    				IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DO_OPC,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL );

	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The SET CD SPEED command as defined in section 6.1.36
    virtual bool	SET_CD_SPEED (
						SCSITask *					request,
						SCSICmdField2Byte 			LOGICAL_UNIT_READ_SPEED,
						SCSICmdField2Byte 			LOGICAL_UNIT_WRITE_SPEED,
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/

	// The SET READ AHEAD command as defined in section 6.1.37
    virtual bool	SET_READ_AHEAD (
						SCSITask *					request,
						SCSICmdField4Byte 			TRIGGER_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Byte 			CONTROL );
	
	// The SET STREAMING command as defined in section 6.1.38
    virtual bool	SET_STREAMING (
						SCSITask *					request,
	 		   			IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The STOP PLAY/SCAN command as defined in section 6.1.39
    virtual bool	STOP_PLAY_SCAN (
						SCSITask *					request,
						SCSICmdField1Byte 			CONTROL );
	
	// The SYNCHRONIZE CACHE command as defined in section 6.1.40
    virtual bool	SYNCHRONIZE_CACHE (
						SCSITask *					request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte 			CONTROL );
	
	// The WRITE (10) command as defined in section 6.1.41
    virtual bool	WRITE_10 (
						SCSITask *					request,
		    			IOMemoryDescriptor *		dataBuffer,
		    			UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
	// The WRITE AND VERIFY (10) command as defined in section 6.1.42
    virtual bool	WRITE_AND_VERIFY_10 (
						SCSITask *					request,
						IOMemoryDescriptor *		dataBuffer,
		    			UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BYT_CHK,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL );
	
protected:
	
	// The block size decoding for Read CD and Read CD MSF  as defined in table 255
    virtual bool	GetBlockSize (
						UInt32 *					requestedByteCount,
						SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
						SCSICmdField1Bit 			SYNC,
						SCSICmdField2Bit 			HEADER_CODES,
						SCSICmdField1Bit 			USER_DATA,
						SCSICmdField1Bit 			EDC_ECC,
						SCSICmdField2Bit 			ERROR_FIELD,
						SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS );
	
	virtual SCSICmdField4Byte ConvertMSFToLBA ( SCSICmdField3Byte MSF );
	
};

#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif	/* _IOKIT_SCSI_MULTIMEDIA_COMMANDS_H_ */