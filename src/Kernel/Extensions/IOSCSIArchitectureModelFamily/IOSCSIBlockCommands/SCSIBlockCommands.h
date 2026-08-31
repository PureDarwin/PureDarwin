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
 * 		represents the SCSI Block Commands specification.  
 *
 * NOTES
 *		Some commands have been changed between versions of the specification.
 *		These changes may include adding fields to a command, obsoleting fields
 * 		in a command or obsoleting of an entire commmand.  Methods that 
 *		implement the previous version are bracketted with the tag "LEGACY
 *		COMMAND SUPPORT".
 *
 */


#ifndef _IOKIT_SCSI_BLOCK_COMMANDS_H_
#define _IOKIT_SCSI_BLOCK_COMMANDS_H_

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

#define	SEARCH_DATA_PARAMETER_LIST_MIN_LENGTH		20


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSIBlockCommands : public SCSIPrimaryCommands
{
	
	OSDeclareDefaultStructors ( SCSIBlockCommands )
	
public:
	
	// factory method accessor
	static SCSIBlockCommands * CreateSCSIBlockCommandObject ( void );
	
	// SCSI Block Commands as defined in T10:990D SBC, Revision 8c,
	// dated 13 November 1997
	
	// The ERASE(10) command as defined in section 6.2.1
	virtual bool	ERASE_10 (
							SCSITask *					request,
							SCSICmdField1Bit 			ERA, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
	// The ERASE(12) command as defined in section 6.2.2
	virtual bool	ERASE_12 (
							SCSITask *					request,
							SCSICmdField1Bit 			ERA, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	
	// The FORMAT_UNIT command as defined in section 6.1.1
	virtual bool	FORMAT_UNIT (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							IOByteCount					defectListSize,
							SCSICmdField1Bit 			FMTDATA, 
							SCSICmdField1Bit 			CMPLST, 
							SCSICmdField3Bit 			DEFECT_LIST_FORMAT, 
							SCSICmdField1Byte 			VENDOR_SPECIFIC, 
							SCSICmdField2Byte 			INTERLEAVE, 
							SCSICmdField1Byte 			CONTROL );
	  
	// The LOCK_UNLOCK_CACHE command as defined in section 6.1.2
	virtual bool	LOCK_UNLOCK_CACHE (
							SCSITask *					request,
							SCSICmdField1Bit 			LOCK, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			NUMBER_OF_BLOCKS, 
							SCSICmdField1Byte 			CONTROL );

	// The MEDIUM_SCAN command as defined in section 6.2.3
	virtual bool	MEDIUM_SCAN (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
   							SCSICmdField1Bit 			WBS, 
   							SCSICmdField1Bit 			ASA, 
   							SCSICmdField1Bit 			RSD, 
   							SCSICmdField1Bit 			PRA, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
							SCSICmdField1Byte 			CONTROL );

 	// The PREFETCH command as defined in section 6.1.3
	virtual bool	PREFETCH (
							SCSITask *					request,
							SCSICmdField1Bit 			IMMED, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );

 	// The READ(6) command as defined in section 6.1.4
	virtual bool	READ_6 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt64						transferCount,
							SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
 	// The READ(10) command as defined in section 6.1.5
	virtual bool	READ_10 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
  							UInt64						transferCount,
 							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
 	// The READ(12) command as defined in section 6.2.4
	virtual bool	READ_12 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
  							UInt64						transferCount,
 							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
 	// The READ_CAPACITY command as defined in section 6.1.6
	virtual bool	READ_CAPACITY (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Bit 			PMI, 
							SCSICmdField1Byte 			CONTROL );
	  
 	// The READ_DEFECT_DATA(10) command as defined in section 6.1.7
	virtual bool	READ_DEFECT_DATA_10 (
							SCSITask *					request,
	 						IOMemoryDescriptor *		dataBuffer,
   							SCSICmdField1Bit 			PLIST, 
							SCSICmdField1Bit 			GLIST, 
							SCSICmdField3Bit 			DEFECT_LIST_FORMAT, 
							SCSICmdField2Byte 			ALLOCATION_LENGTH, 
							SCSICmdField1Byte 			CONTROL );

 	// The READ_DEFECT_DATA(12) command as defined in section 6.2.5
	virtual bool	READ_DEFECT_DATA_12 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			PLIST, 
							SCSICmdField1Bit 			GLIST, 
							SCSICmdField3Bit 			DEFECT_LIST_FORMAT, 
							SCSICmdField4Byte 			ALLOCATION_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The READ_GENERATION command as defined in section 6.2.6
	virtual bool	READ_GENERATION (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Byte 			ALLOCATION_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The READ_LONG command as defined in section 6.1.8
	virtual bool	READ_LONG (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			CORRCT, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			BYTE_TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The READ_UPDATED_BLOCK(10) command as defined in section 6.2.7
	virtual bool	READ_UPDATED_BLOCK_10 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt32						transferCount,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			FUA,
			 				SCSICmdField1Bit 			RELADR, 
			 				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
			 				SCSICmdField1Bit 			LATEST, 
			 				SCSICmdField15Bit 			GENERATION_ADDRESS, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The REASSIGN_BLOCKS command as defined in section 6.1.9
	virtual bool	REASSIGN_BLOCKS (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Byte 			CONTROL );
	  
  	// The REBUILD command as defined in section 6.1.10
	virtual bool	REBUILD (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			FUA,
			 				SCSICmdField1Bit 			INTDATA, 
			 				SCSICmdField2Bit 			PORT_CONTROL, 
			 				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			REBUILD_LENGTH, 
							SCSICmdField4Byte 			PARAMETER_LIST_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The REGENERATE command as defined in section 6.1.11
	virtual bool	REGENERATE (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit			DPO, 
							SCSICmdField1Bit 			FUA,
			 				SCSICmdField1Bit 			INTDATA, 
			 				SCSICmdField2Bit 			PORT_CONTROL, 
			 				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			REGENERATE_LENGTH, 
							SCSICmdField4Byte 			PARAMETER_LIST_LENGTH, 
							SCSICmdField1Byte 			CONTROL );

	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// REZERO_UNIT is obsoleted by the SBC specification.  This
   	// is the command definition as defined in SCSI-2 section 9.2.13.
   	virtual bool	REZERO_UNIT ( 
							SCSITask *					request,
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// SEARCH_DATA_EQUAL(10) is obsoleted by the SBC specification.  This
   	// is the command definition as defined in SCSI-2 section 9.2.14.
	virtual bool	SEARCH_DATA_EQUAL_10 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt32						transferCount,
							SCSICmdField1Bit 			INVERT, 
							SCSICmdField1Bit 			SPNDAT, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH, 
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// SEARCH_DATA_HIGH(10) is obsoleted by the SBC specification.  This
   	// is the command definition as defined in SCSI-2 section 9.2.14.
	virtual bool	SEARCH_DATA_HIGH_10 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt32						transferCount,
							SCSICmdField1Bit 			INVERT, 
							SCSICmdField1Bit 			SPNDAT, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH, 
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// SEARCH_DATA_LOW(10) is obsoleted by the SBC specification.  This
   	// is the command definition as defined in SCSI-2 section 9.2.14.
	virtual bool	SEARCH_DATA_LOW_10 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt32						transferCount,
							SCSICmdField1Bit 			INVERT, 
							SCSICmdField1Bit 			SPNDAT, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH, 
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// SEEK(6) is obsoleted by the SBC specification.  This
   	// is the command definition as defined in SCSI-2 section 9.2.15.
	virtual bool	SEEK_6 ( 
							SCSITask *					request,
							SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/

  	// The SEEK(10) command as defined in section 6.1.12
	virtual bool	SEEK_10 ( 
							SCSITask *					request,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Byte 			CONTROL );
	
  	// The SET_LIMITS(10) command as defined in section 6.1.13
	virtual bool	SET_LIMITS_10 ( 
							SCSITask *					request,
							SCSICmdField1Bit 			RDINH, 
							SCSICmdField1Bit 			WRINH, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
							SCSICmdField1Byte 			CONTROL );
	
  	// The SET_LIMITS(12) command as defined in section 6.2.8
	virtual bool	SET_LIMITS_12 ( 
							SCSITask *					request,
							SCSICmdField1Bit 			RDINH, 
							SCSICmdField1Bit 			WRINH, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			NUMBER_OF_BLOCKS,
							SCSICmdField1Byte 			CONTROL );
	
  	// The START_STOP_UNIT command as defined in section 6.1.14
	virtual bool	START_STOP_UNIT ( 
							SCSITask *					request,
							SCSICmdField1Bit 			IMMED, 
							SCSICmdField4Bit 			POWER_CONDITIONS, 
							SCSICmdField1Bit 			LOEJ, 
							SCSICmdField1Bit 			START, 
							SCSICmdField1Byte 			CONTROL );

  	// The SYNCHRONIZE_CACHE command as defined in section 6.1.15
	virtual bool	SYNCHRONIZE_CACHE ( 
							SCSITask *					request,
							SCSICmdField1Bit 			IMMED, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			NUMBER_OF_BLOCKS, 
							SCSICmdField1Byte 			CONTROL );

  	// The UPDATE_BLOCK command as defined in section 6.2.9
	virtual bool	UPDATE_BLOCK ( 
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt64						transferCount,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Byte 			CONTROL );

  	// The VERIFY(10) command as defined in sections 6.1.16 and 6.2.10
	virtual bool	VERIFY_10 ( 
							SCSITask *					request,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			BLKVFY, 
							SCSICmdField1Bit 			BYTCHK, 
							SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			VERIFICATION_LENGTH, 
							SCSICmdField1Byte 			CONTROL );

  	// The VERIFY(12) command as defined in sections 6.2.11
	virtual bool	VERIFY_12 ( 
							SCSITask *					request,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			BLKVFY, 
							SCSICmdField1Bit 			BYTCHK, 
							SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			VERIFICATION_LENGTH, 
							SCSICmdField1Byte 			CONTROL );

  	// The WRITE(6) command as defined in section 6.1.17
	virtual bool	WRITE_6 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt64						transferCount,
							SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS,
							SCSICmdField1Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The WRITE(10) command as defined in sections 6.1.18 and 6.2.12
	virtual bool	WRITE_10 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt64						transferCount,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			EBP, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
	// The WRITE(12) command as defined in section 6.2.13
	virtual bool	WRITE_12 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt64						transferCount,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			EBP, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
	// The WRITE_AND_VERIFY(10) command as defined in sections 6.1.19 and 6.2.14
	virtual bool	WRITE_AND_VERIFY_10 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt64						transferCount,
							SCSICmdField1Bit 			DPO,
							SCSICmdField1Bit 			EBP, 
							SCSICmdField1Bit 			BYTCHK, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The WRITE_AND_VERIFY(12) command as defined in sections 6.2.15
	virtual bool	WRITE_AND_VERIFY_12 (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							UInt64						transferCount,
							SCSICmdField1Bit 			DPO,
							SCSICmdField1Bit 			EBP, 
							SCSICmdField1Bit 			BYTCHK, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The WRITE_LONG command as defined in section 6.1.20
	virtual bool	WRITE_LONG (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	  
  	// The WRITE_SAME command as defined in section 6.1.21
	virtual bool	WRITE_SAME (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			PBDATA, 
							SCSICmdField1Bit 			LBDATA, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	
  	// The XDREAD command as defined in section 6.1.22
	virtual bool	XDREAD (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	
	// The XDWRITE command as defined in section 6.1.23
	virtual bool	XDWRITE (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			FUA, 
							SCSICmdField1Bit 			DISABLE_WRITE, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
	
	// The XDWRITE_EXTENDED command as defined in section 6.1.24
	virtual bool	XDWRITE_EXTENDED (
							SCSITask *					request,
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
							SCSICmdField1Byte 			CONTROL );
	
	// The XPWRITE command as defined in section 6.1.25
	virtual bool	XPWRITE (
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			FUA, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH, 
							SCSICmdField1Byte 			CONTROL );
};

#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif	/* _IOKIT_SCSI_BLOCK_COMMANDS_H_ */