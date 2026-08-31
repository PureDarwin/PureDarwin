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
 * 		represents the SCSI Primary Commands specification.  
 *
 * NOTES
 *		Some commands have been changed between versions of the specification.
 *		These changes may include adding fields to a command, obsoleting fields
 * 		in a command or obsoleting of an entire commmand.  Methods that 
 *		implement the previous version are bracketted with the tag "LEGACY
 *		COMMAND SUPPORT".
 *
 */


#ifndef _IOKIT_SCSI_PRIMARY_COMMANDS_H_
#define _IOKIT_SCSI_PRIMARY_COMMANDS_H_

#if defined(KERNEL) && defined(__cplusplus)


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Libkern includes
#include <libkern/c++/OSObject.h>

// General IOKit headers
#include <IOKit/IOLib.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/SCSICommandDefinitions.h>
#include <IOKit/scsi/SCSITask.h>
#include "SCSITaskDefinition.h"

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSIPrimaryCommands : public OSObject
{
	
	OSDeclareDefaultStructors ( SCSIPrimaryCommands )
	
protected:
	
	// Utility methods used by all SCSI Command Set objects
	
	// isParameterValid are used to validate that the parameter passed into
	// the command methods are of the correct value.
	
	// Validate Parameter used for 1 bit to 1 byte paramaters
    bool 	IsParameterValid ( 
							SCSICmdField1Byte 			param,
							SCSICmdField1Byte 			mask );
	
	// Validate Parameter used for 9 bit to 2 byte paramaters
	bool 	IsParameterValid ( 
							SCSICmdField2Byte 			param,
							SCSICmdField2Byte 			mask );
	
	// Validate Parameter used for 17 bit to 4 byte paramaters
	bool 	IsParameterValid ( 
							SCSICmdField4Byte 			param,
							SCSICmdField4Byte 			mask );
	
	bool 	IsBufferAndCapacityValid (
							IOMemoryDescriptor *		dataBuffer,
							UInt32 						requiredSize );
	
	// ----- Methods for accessing the SCSITask ----------
	// Populate the 6 Byte Command Descriptor Block
	bool 	SetCommandDescriptorBlock (
							SCSITask *					request,
							UInt8						cdbByte0,
							UInt8						cdbByte1,
							UInt8						cdbByte2,
							UInt8						cdbByte3,
							UInt8						cdbByte4,
							UInt8						cdbByte5 );
	
	// Populate the 10 Byte Command Descriptor Block
	bool 	SetCommandDescriptorBlock (
							SCSITask *					request,
							UInt8						cdbByte0,
							UInt8						cdbByte1,
							UInt8						cdbByte2,
							UInt8						cdbByte3,
							UInt8						cdbByte4,
							UInt8						cdbByte5,
							UInt8						cdbByte6,
							UInt8						cdbByte7,
							UInt8						cdbByte8,
							UInt8						cdbByte9 );
	
	// Populate the 12 Byte Command Descriptor Block
	bool 	SetCommandDescriptorBlock (
							SCSITask *					request,
							UInt8						cdbByte0,
							UInt8						cdbByte1,
							UInt8						cdbByte2,
							UInt8						cdbByte3,
							UInt8						cdbByte4,
							UInt8						cdbByte5,
							UInt8						cdbByte6,
							UInt8						cdbByte7,
							UInt8						cdbByte8,
							UInt8						cdbByte9,
							UInt8						cdbByte10,
							UInt8						cdbByte11 );
	
	// Populate the 16 Byte Command Descriptor Block
	bool 	SetCommandDescriptorBlock (
							SCSITask *					request,
							UInt8						cdbByte0,
							UInt8						cdbByte1,
							UInt8						cdbByte2,
							UInt8						cdbByte3,
							UInt8						cdbByte4,
							UInt8						cdbByte5,
							UInt8						cdbByte6,
							UInt8						cdbByte7,
							UInt8						cdbByte8,
							UInt8						cdbByte9,
							UInt8						cdbByte10,
							UInt8						cdbByte11,
							UInt8						cdbByte12,
							UInt8						cdbByte13,
							UInt8						cdbByte14,
							UInt8						cdbByte15 );
	
	// Set up the control information for the transfer, including
	// the transfer direction and the number of bytes to transfer.
	// Need to add new version of method and remove the default values
	// from this one.
	bool	SetDataTransferControl (
							SCSITask *					request,
							UInt32						timeoutDuration,
							UInt8						dataTransferDirection,
							IOMemoryDescriptor *		dataBuffer = NULL,
							UInt64						transferCountInBytes = 0 );
	
public:
	
	// static factory method
	static SCSIPrimaryCommands * CreateSCSIPrimaryCommandObject ( void );
	
	// SCSI Primary Commands as defined in T10:1236D SPC-2 , Revision 18,
   	// dated 21 May 2000
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
  	// The CHANGE_DEFINITION command as defined in SPC revision 11a, section 
  	// 7.1.  SPC-2 obsoleted this command.
    virtual bool	CHANGE_DEFINITION (
							SCSITask *					request,
 							IOMemoryDescriptor *		dataBuffer,
   							SCSICmdField1Bit 			SAVE,
    						SCSICmdField7Bit 			DEFINITION_PARAMETER,
    						SCSICmdField1Byte 			PARAMETER_DATA_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
      
  	// The COMPARE command as defined in section 7.2.   
    virtual bool	COMPARE (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
  							SCSICmdField1Bit 			PAD,
    						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
    
  	// The COPY command as defined in section 7.3.
    virtual bool	COPY (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			PAD,
    						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
      
  	// The COPY_AND_VERIFY command as defined in section 7.4.
    virtual bool	COPY_AND_VERIFY (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			BYTCHK,
    						SCSICmdField1Bit 			PAD,
    						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
      
  	// The EXTENDED_COPY command as defined in section 7.5.   
    virtual bool	EXTENDED_COPY (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
      
  	// The INQUIRY command as defined in section 7.6.
    virtual bool	INQUIRY (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			CMDDT,
    						SCSICmdField1Bit 			EVPD,
    						SCSICmdField1Byte 			PAGE_OR_OPERATION_CODE,
    						SCSICmdField1Byte 			ALLOCATION_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
  	// The LOG_SELECT command as defined in section 7.7.
    virtual bool	LOG_SELECT (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			PCR,
    						SCSICmdField1Bit 			SP,
    						SCSICmdField2Bit 			PC,
    						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
    						SCSICmdField1Byte 			CONTROL );  
	
  	// The LOG_SENSE command as defined in section 7.8.
    virtual bool	LOG_SENSE (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			PPC,
    						SCSICmdField1Bit 			SP,
    						SCSICmdField2Bit 			PC,
    						SCSICmdField6Bit 			PAGE_CODE,
    						SCSICmdField2Byte 			PARAMETER_POINTER,
    						SCSICmdField2Byte 			ALLOCATION_LENGTH,
    						SCSICmdField1Byte 			CONTROL );  
	
  	// The MODE_SELECT(6) command as defined in section 7.9.
    virtual bool	MODE_SELECT_6 (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			PF,
    						SCSICmdField1Bit 			SP,
    						SCSICmdField1Byte 			PARAMETER_LIST_LENGTH,
    						SCSICmdField1Byte 			CONTROL );  
	
   	// The MODE_SELECT(10) command as defined in section 7.10.
   	virtual bool	MODE_SELECT_10 (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			PF,
    						SCSICmdField1Bit 			SP,
    						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
    						SCSICmdField1Byte 			CONTROL );  
	
   	// The MODE_SENSE(6) command as defined in section 7.11.
   	virtual bool	MODE_SENSE_6 (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			DBD,
   							SCSICmdField2Bit 			PC,
   							SCSICmdField6Bit 			PAGE_CODE,
   							SCSICmdField1Byte 			ALLOCATION_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The MODE_SENSE(10) command as defined in section 7.12.
   	virtual bool	MODE_SENSE_10 (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
    						SCSICmdField1Bit 			LLBAA,
    						SCSICmdField1Bit 			DBD,
   							SCSICmdField2Bit 			PC,
   							SCSICmdField6Bit 			PAGE_CODE,
   							SCSICmdField2Byte 			ALLOCATION_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The PERSISTENT_RESERVE_IN command as defined in section 7.13.
  	virtual bool	PERSISTENT_RESERVE_IN (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
   							SCSICmdField5Bit 			SERVICE_ACTION,
   							SCSICmdField2Byte 			ALLOCATION_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The PERSISTENT_RESERVE_OUT command as defined in section 7.14.
   	// There is no PARAMETER_LIST_LENGTH parameter as this value is always
   	// 0x18 for the SPC version of this command.  The buffer for the data must 
   	// be at least of that size.
   	virtual bool	PERSISTENT_RESERVE_OUT (
							SCSITask *					request,
   							IOMemoryDescriptor *		dataBuffer,
  							SCSICmdField5Bit 			SERVICE_ACTION,
   							SCSICmdField4Bit 			SCOPE,
   							SCSICmdField4Bit 			TYPE,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The PREVENT_ALLOW_MEDIUM_REMOVAL command as defined in section 7.15.
    virtual bool	PREVENT_ALLOW_MEDIUM_REMOVAL ( 
							SCSITask *					request,
     						SCSICmdField2Bit 			PREVENT,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The READ_BUFFER command as defined in section 7.16.
	virtual bool	READ_BUFFER ( 
							SCSITask *					request,
   							IOMemoryDescriptor *		dataBuffer,
   							SCSICmdField4Bit 			MODE,
							SCSICmdField1Byte 			BUFFER_ID,
							SCSICmdField3Byte 			BUFFER_OFFSET,
							SCSICmdField3Byte 			ALLOCATION_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The RECEIVE command as defined in section 9.2.
	virtual bool	RECEIVE ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
	 						SCSICmdField3Byte 			TRANSFER_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The RECEIVE_COPY_RESULTS command as defined in section 7.17.
	virtual bool	RECEIVE_COPY_RESULTS ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
	 						SCSICmdField5Bit 			SERVICE_ACTION,
	 						SCSICmdField1Byte			LIST_IDENTIFIER,
	 						SCSICmdField4Byte 			ALLOCATION_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The RECEIVE_DIAGNOSTICS_RESULTS command as defined in section 7.18.
	virtual bool	RECEIVE_DIAGNOSTICS_RESULTS ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
	 						SCSICmdField1Bit 			PCV,
	 						SCSICmdField1Byte			PAGE_CODE,
	 						SCSICmdField2Byte 			ALLOCATION_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The RELEASE(10) command as defined in section 7.19.
	virtual bool	RELEASE_10 ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			THRDPTY,
							SCSICmdField1Bit 			LONGID,
							SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
							SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
							SCSICmdField1Byte 			CONTROL );
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// The RELEASE(10) command as defined in SPC revision 11a, section 7.17.  
   	// The SPC-2 specification obsoleted the EXTENT and 
   	// RESERVATION_IDENTIFICATION fields.
	virtual bool	RELEASE_10 ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			THRDPTY,
							SCSICmdField1Bit 			LONGID,
							SCSICmdField1Bit 			EXTENT,
							SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
							SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
							SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
   	// The RELEASE(6) command as defined in section 7.20.
	virtual bool	RELEASE_6 ( 
							SCSITask *					request,
							SCSICmdField1Byte 			CONTROL );

	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// The RELEASE(6) command as defined in SPC revision 11a, section 7.18.
   	// The SPC-2 specification obsoleted the EXTENT and 
   	// RESERVATION_IDENTIFICATION fields.
	virtual bool	RELEASE_6 ( 
							SCSITask *					request,
							SCSICmdField1Bit 			EXTENT,
							SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
   	// The REPORT_DEVICE_IDENTIFIER command as defined in section 7.21.
   	// There is no SERVICE_ACTION parameter as the value for this field
   	// for the SPC version of this command is defined to always be 0x05.
	virtual bool	REPORT_DEVICE_IDENTIFIER ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField4Byte 			ALLOCATION_LENGTH,
							SCSICmdField1Byte 			CONTROL );
	
   	// The REPORT_LUNS command as defined in section 7.22.
   	virtual bool	REPORT_LUNS ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
			   				SCSICmdField4Byte 			ALLOCATION_LENGTH,
			   				SCSICmdField1Byte 			CONTROL );
    
   	// The REQUEST_SENSE command as defined in section 7.23.
    virtual bool	REQUEST_SENSE (
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
			    			SCSICmdField1Byte 			ALLOCATION_LENGTH,
			    			SCSICmdField1Byte 			CONTROL );
 	
   	// The RESERVE(10) command as defined in section 7.24.
	virtual bool	RESERVE_10 ( 
							SCSITask *					request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			THRDPTY,
							SCSICmdField1Bit 			LONGID,
							SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
							SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
							SCSICmdField1Byte 			CONTROL );
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// The RESERVE(10) command as defined in SPC revision 11a, section 7.17.
    // The SPC-2 specification obsoleted the EXTENT and 
	// RESERVATION_IDENTIFICATION fields.
	virtual bool	RESERVE_10 ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			THRDPTY,
							SCSICmdField1Bit 			LONGID,
							SCSICmdField1Bit 			EXTENT,
							SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
							SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
							SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
   	// The RESERVE(6) command as defined in section 7.25.
 	virtual bool	RESERVE_6 ( 
							SCSITask *					request,
							SCSICmdField1Byte 			CONTROL );
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
   	// The RESERVE(6) command as defined in SPC revision 11a, section 7.17.
    // The SPC-2 specification obsoleted the THRDPTY, THIRD_PARTY_DEVICE_ID, 
	// EXTENT, RESERVATION_IDENTIFICATION and EXTENT_LIST_LENGTH fields.
 	virtual bool	RESERVE_6 ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			EXTENT,
							SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
							SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
							SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/
	
   	// The SEND command as defined in section 9.3.
	virtual bool	SEND ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit			AER,
	 						SCSICmdField3Byte 			TRANSFER_LENGTH,
    						SCSICmdField1Byte 			CONTROL );
	
   	// The SEND_DIAGNOSTICS command as defined in section 7.26.
	virtual bool	SEND_DIAGNOSTICS ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
			  		 		SCSICmdField3Bit 			SELF_TEST_CODE,
			  		 		SCSICmdField1Bit 			PF,
			  		 		SCSICmdField1Bit 			SELF_TEST,
			  		 		SCSICmdField1Bit 			DEVOFFL,
			  		 		SCSICmdField1Bit 			UNITOFFL,
			  	 			SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
			  	 			SCSICmdField1Byte 			CONTROL );
	
   	// The SET_DEVICE_IDENTIFIER command as defined in section 7.27.
	virtual bool	SET_DEVICE_IDENTIFIER ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField5Bit 			SERVICE_ACTION,
							SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
							SCSICmdField1Byte 			CONTROL );
    
   	// The TEST_UNIT_READY command as defined in section 7.28.
	virtual bool	TEST_UNIT_READY (  
							SCSITask *					request,
    						SCSICmdField1Byte 			CONTROL );
 	
   	// The WRITE_BUFFER command as defined in section 7.29.
	virtual bool	WRITE_BUFFER ( 
							SCSITask *					request,
  							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField4Bit 			MODE,
							SCSICmdField1Byte 			BUFFER_ID,
							SCSICmdField3Byte 			BUFFER_OFFSET,
							SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
							SCSICmdField1Byte 			CONTROL );
	
};

#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif	/* _IOKIT_SCSI_PRIMARY_COMMANDS_H_ */