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
#include "SCSITask.h"
#include "SCSITaskDefinition.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSITask"

#if DEBUG
#define SCSI_TASK_DEBUGGING_LEVEL							0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_TASK_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_TASK_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_TASK_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOCommand
OSDefineMetaClassAndStructors ( SCSITask, IOCommand );


#if 0
#pragma mark -
#pragma mark ¥ Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ init - Initializes the object									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::init ( void )
{
	
	bool	result = false;
	
	require ( super::init ( ), ErrorExit );
	
 	// Clear the owner here since it should be set when the object
 	// is instantiated and never reset.
 	fOwner					= NULL;
	fAutosenseDescriptor 	= NULL;
	
 	// Set this task to the default task state.  
	fTaskState = kSCSITaskState_NEW_TASK;
	
	// Reset all the task's fields to their defaults.
	result = ResetForNewTask ( );
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ free - Called to free any resources allocated					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSITask::free ( void )
{
	
	if ( fOwner != NULL )
	{
		fOwner->release ( );
	}
	
	if ( fAutosenseDescriptor != NULL )
	{
		
		fAutosenseDescriptor->release ( );
		fAutosenseDescriptor = NULL;
		
	}
	
	if ( ( fAutosenseTaskMap == kernel_task ) && ( fAutoSenseData != NULL ) )
	{
		
		IOFree ( fAutoSenseData, fAutoSenseDataSize );
		fAutoSenseData = NULL;
		
	}
	
	super::free ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResetForNewTask - Utility method to reset the object so that it may be
//						used for a new Task. This method will return true if
//						the reset was successful and false if it failed because
//						it represents an active task				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::ResetForNewTask ( void )
{
	
	bool				result = false;
	SCSI_Sense_Data *	buffer = NULL;
	
	// If this is a pending task, do not allow it to be reset until
	// it has completed.
	require ( ( IsTaskActive ( ) == false ), ErrorExit );
	
	fTaskAttribute 					= kSCSITask_SIMPLE;
	fTaskTagIdentifier				= kSCSIUntaggedTaskIdentifier;
	
   	fTaskState 						= kSCSITaskState_NEW_TASK;
	fTaskStatus						= kSCSITaskStatus_GOOD;
	fLogicalUnitNumber				= 0;	
	
	bzero ( &fCommandDescriptorBlock, kSCSICDBSize_Maximum );
	
	fCommandSize 					= 0;
	fTransferDirection 				= 0;
	fDataBuffer						= NULL;
  	fDataBufferOffset 				= 0;
	fRequestedByteCountOfTransfer	= 0;
   	fRealizedByteCountOfTransfer	= 0;
	
	fTimeoutDuration				= 0;
	fCompletionCallback				= NULL;
	fServiceResponse				= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	fNextTaskInQueue				= NULL;
	
	fProtocolLayerReference			= NULL;
	fApplicationLayerReference		= NULL;
	
	// Autosense member variables
   	fAutosenseDataRequested			= false;
	fAutosenseCDBSize				= 0;
	fAutoSenseDataIsValid			= false;
	
	bzero ( &fAutosenseCDB, kSCSICDBSize_Maximum );
	
	if ( fAutoSenseData == NULL )
	{
		
		fAutoSenseDataSize = sizeof ( SCSI_Sense_Data );
		buffer = ( SCSI_Sense_Data * ) IOMalloc ( fAutoSenseDataSize );
		require_nonzero ( buffer, ErrorExit );
		bzero ( buffer, fAutoSenseDataSize );
		result = SetAutoSenseDataBuffer ( buffer, fAutoSenseDataSize, kernel_task );
		require ( result, ErrorExit );
		
	}
	
	fAutoSenseRealizedByteCountOfTransfer = 0;
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTaskOwner - Utility method for setting the OSObject that owns
//					 the instantiation of the SCSI Task				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetTaskOwner ( OSObject * taskOwner )
{
	
	bool	result = false;
	
	require_nonzero ( taskOwner, ErrorExit );
	taskOwner->retain ( );
	
	if ( fOwner != NULL )
	{
		
		// If this already has an owner, release
		// the retain on that one.
		fOwner->release ( );
		
	}
	
	fOwner = taskOwner;
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTaskOwner - Utility method for retreiving the OSObject that owns the
//					 instantiation of the SCSI Task					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

OSObject *
SCSITask::GetTaskOwner ( void )
{
	
	check ( fOwner );
	return fOwner;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTaskOwner - Utility method to check if this task represents an active.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::IsTaskActive ( void )
{
	
	// If the state of this task is either new or it is an ended task,
	// return false since this does not qualify as active.
	if ( ( fTaskState == kSCSITaskState_NEW_TASK ) ||
		 ( fTaskState == kSCSITaskState_ENDED ) )
	{
		return false;
	}
	
	// If the task is in any other state, it is considered active.	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetLogicalUnitNumber - 	Utility method for setting the Logical Unit
//								Number for which this Task is intended.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetLogicalUnitNumber ( UInt8 newLUN )
{
	
	fLogicalUnitNumber = newLUN;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetLogicalUnitNumber - 	Utility method for getting the Logical Unit
//								Number for which this Task is intended.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
SCSITask::GetLogicalUnitNumber ( void )
{
	return fLogicalUnitNumber;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTaskAttribute - Sets the SCSITaskAttribute to the new value.  [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::SetTaskAttribute ( SCSITaskAttribute newAttributeValue )
{
	
	fTaskAttribute = newAttributeValue;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTaskAttribute - Gets the SCSITaskAttribute. 				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaskAttribute	
SCSITask::GetTaskAttribute ( void )
{
	return fTaskAttribute;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTaggedTaskIdentifier - Sets the SCSITaggedTaskIdentifier.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetTaggedTaskIdentifier ( SCSITaggedTaskIdentifier newTag )
{
	fTaskTagIdentifier = newTag;
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTaggedTaskIdentifier - Gets the SCSITaggedTaskIdentifier. 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaggedTaskIdentifier 
SCSITask::GetTaggedTaskIdentifier ( void )
{
	return fTaskTagIdentifier;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTaskState - Sets the SCSITaskState to the new value. 		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetTaskState ( SCSITaskState newTaskState )
{
	fTaskState = newTaskState;
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTaskState - Gets the SCSITaskState. 						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaskState	
SCSITask::GetTaskState ( void )
{
	return fTaskState;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTaskStatus - Sets the SCSITaskStatus to the new value. 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetTaskStatus ( SCSITaskStatus newTaskStatus )
{
	
	fTaskStatus = newTaskStatus;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTaskStatus - Gets the SCSITaskStatus.					 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaskStatus	
SCSITask::GetTaskStatus ( void )
{
	return fTaskStatus;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetCommandDescriptorBlock - Populate the 6 Byte Command Descriptor Block
//																 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
SCSITask::SetCommandDescriptorBlock ( 
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5 )
{
	
	fCommandDescriptorBlock[0] = cdbByte0;
	fCommandDescriptorBlock[1] = cdbByte1;
	fCommandDescriptorBlock[2] = cdbByte2;
	fCommandDescriptorBlock[3] = cdbByte3;
	fCommandDescriptorBlock[4] = cdbByte4;
	fCommandDescriptorBlock[5] = cdbByte5;
	fCommandDescriptorBlock[6] = 0x00;
	fCommandDescriptorBlock[7] = 0x00;
	fCommandDescriptorBlock[8] = 0x00;
	fCommandDescriptorBlock[9] = 0x00;
	fCommandDescriptorBlock[10] = 0x00;
	fCommandDescriptorBlock[11] = 0x00;
	fCommandDescriptorBlock[12] = 0x00;
	fCommandDescriptorBlock[13] = 0x00;
	fCommandDescriptorBlock[14] = 0x00;
	fCommandDescriptorBlock[15] = 0x00;
	
	fCommandSize = kSCSICDBSize_6Byte;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetCommandDescriptorBlock - Populate the 10 Byte Command Descriptor Block
//																 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
SCSITask::SetCommandDescriptorBlock ( 
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
	
	fCommandDescriptorBlock[0] = cdbByte0;
	fCommandDescriptorBlock[1] = cdbByte1;
	fCommandDescriptorBlock[2] = cdbByte2;
	fCommandDescriptorBlock[3] = cdbByte3;
	fCommandDescriptorBlock[4] = cdbByte4;
	fCommandDescriptorBlock[5] = cdbByte5;
	fCommandDescriptorBlock[6] = cdbByte6;
	fCommandDescriptorBlock[7] = cdbByte7;
	fCommandDescriptorBlock[8] = cdbByte8;
	fCommandDescriptorBlock[9] = cdbByte9;
	fCommandDescriptorBlock[10] = 0x00;
	fCommandDescriptorBlock[11] = 0x00;
	fCommandDescriptorBlock[12] = 0x00;
	fCommandDescriptorBlock[13] = 0x00;
	fCommandDescriptorBlock[14] = 0x00;
	fCommandDescriptorBlock[15] = 0x00;
	
	fCommandSize = kSCSICDBSize_10Byte;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetCommandDescriptorBlock - Populate the 12 Byte Command Descriptor Block
//																 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
SCSITask::SetCommandDescriptorBlock ( 
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
	
	fCommandDescriptorBlock[0] = cdbByte0;
	fCommandDescriptorBlock[1] = cdbByte1;
	fCommandDescriptorBlock[2] = cdbByte2;
	fCommandDescriptorBlock[3] = cdbByte3;
	fCommandDescriptorBlock[4] = cdbByte4;
	fCommandDescriptorBlock[5] = cdbByte5;
	fCommandDescriptorBlock[6] = cdbByte6;
	fCommandDescriptorBlock[7] = cdbByte7;
	fCommandDescriptorBlock[8] = cdbByte8;
	fCommandDescriptorBlock[9] = cdbByte9;
	fCommandDescriptorBlock[10] = cdbByte10;
	fCommandDescriptorBlock[11] = cdbByte11;
	fCommandDescriptorBlock[12] = 0x00;
	fCommandDescriptorBlock[13] = 0x00;
	fCommandDescriptorBlock[14] = 0x00;
	fCommandDescriptorBlock[15] = 0x00;
	
	fCommandSize = kSCSICDBSize_12Byte;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetCommandDescriptorBlock - Populate the 16 Byte Command Descriptor Block
//																 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
SCSITask::SetCommandDescriptorBlock ( 
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
	
	fCommandDescriptorBlock[0] = cdbByte0;
	fCommandDescriptorBlock[1] = cdbByte1;
	fCommandDescriptorBlock[2] = cdbByte2;
	fCommandDescriptorBlock[3] = cdbByte3;
	fCommandDescriptorBlock[4] = cdbByte4;
	fCommandDescriptorBlock[5] = cdbByte5;
	fCommandDescriptorBlock[6] = cdbByte6;
	fCommandDescriptorBlock[7] = cdbByte7;
	fCommandDescriptorBlock[8] = cdbByte8;
	fCommandDescriptorBlock[9] = cdbByte9;
	fCommandDescriptorBlock[10] = cdbByte10;
	fCommandDescriptorBlock[11] = cdbByte11;
	fCommandDescriptorBlock[12] = cdbByte12;
	fCommandDescriptorBlock[13] = cdbByte13;
	fCommandDescriptorBlock[14] = cdbByte14;
	fCommandDescriptorBlock[15] = cdbByte15;
	
	fCommandSize = kSCSICDBSize_16Byte;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetCommandDescriptorBlockSize - Gets the Command Descriptor Block size.
//																 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
SCSITask::GetCommandDescriptorBlockSize ( void )
{
	return fCommandSize;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetCommandDescriptorBlock - Gets the Command Descriptor Block.   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::GetCommandDescriptorBlock ( SCSICommandDescriptorBlock * cdbData )
{
	
	bcopy ( fCommandDescriptorBlock,
			cdbData,
			sizeof ( SCSICommandDescriptorBlock ) );
	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetDataTransferDirection - Sets the data transfer direction.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetDataTransferDirection ( UInt8 newDataTransferDirection )
{
	
	fTransferDirection = newDataTransferDirection;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetDataTransferDirection - Gets the data transfer direction.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
SCSITask::GetDataTransferDirection ( void )
{
	return fTransferDirection;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetRequestedDataTransferCount - 	Sets the requested data transfer count
//										in bytes.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::SetRequestedDataTransferCount ( UInt64 requestedTransferCountInBytes )
{
	
	fRequestedByteCountOfTransfer = requestedTransferCountInBytes;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetRequestedDataTransferCount - 	Gets the requested data transfer count
//										in bytes.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
SCSITask::GetRequestedDataTransferCount ( void )
{
	return fRequestedByteCountOfTransfer;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetRealizedDataTransferCount - Sets the realized data transfer count
//									 in bytes.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes )
{
	
	fRealizedByteCountOfTransfer = realizedTransferCountInBytes;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetRealizedDataTransferCount - Gets the realized data transfer count
//									 in bytes.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
SCSITask::GetRealizedDataTransferCount ( void )
{
	return fRealizedByteCountOfTransfer;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetDataBuffer - Sets the data transfer buffer.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetDataBuffer ( IOMemoryDescriptor * newDataBuffer )
{
	
	fDataBuffer = newDataBuffer;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetDataBuffer - Gets the data transfer buffer.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOMemoryDescriptor *
SCSITask::GetDataBuffer ( void )
{
	
	return fDataBuffer;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetDataBufferOffset - Sets the data transfer buffer offset.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetDataBufferOffset ( UInt64 newDataBufferOffset )
{
	
	fDataBufferOffset = newDataBufferOffset;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetDataBufferOffset - Gets the data transfer buffer offset.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
SCSITask::GetDataBufferOffset ( void )
{
	return fDataBufferOffset;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTimeoutDuration - 	Sets the command timeout value in milliseconds.
//							Timeout values of zero indicate the largest
//							possible timeout on that transport.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetTimeoutDuration ( UInt32 timeoutValue )
{
	
	fTimeoutDuration = timeoutValue;
 	return true;
 	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTimeoutDuration - 	Gets the command timeout value in milliseconds.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
SCSITask::GetTimeoutDuration ( void )
{
	return fTimeoutDuration;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTaskCompletionCallback - Sets the command completion routine.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetTaskCompletionCallback ( SCSITaskCompletion newCallback )
{
	
	fCompletionCallback = newCallback;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TaskCompletedNotification - Calls the command completion routine.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSITask::TaskCompletedNotification ( void )
{
	fCompletionCallback ( this );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetServiceResponse - Sets the SCSIServiceResponse.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetServiceResponse ( SCSIServiceResponse serviceResponse )
{
	
	fServiceResponse = serviceResponse;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetServiceResponse - Gets the SCSIServiceResponse.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
SCSITask::GetServiceResponse ( void )
{
	return fServiceResponse;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetAutoSenseDataBuffer - Sets the auto sense data buffer.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::SetAutoSenseDataBuffer ( SCSI_Sense_Data * 	senseData,
								   UInt8				senseDataSize,
								   task_t				task )
{
	
	// Release any old memory descriptors
	if ( fAutosenseDescriptor != NULL )
	{
		
		fAutosenseDescriptor->release ( );
		fAutosenseDescriptor = NULL;
		
	}
	
	// Release any old memory
	if ( ( fAutosenseTaskMap == kernel_task ) && ( fAutoSenseData != NULL ) )
	{
		
		IOFree ( fAutoSenseData, fAutoSenseDataSize );
		fAutoSenseData = NULL;
		fAutoSenseDataSize = 0;
		
	}
	
	// Set the new memory
	fAutoSenseData			= senseData;
	fAutoSenseDataSize		= senseDataSize;
	fAutoSenseDataIsValid	= false;
	fAutosenseTaskMap		= task;
	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetAutoSenseData - Sets the auto sense data.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::SetAutoSenseData ( SCSI_Sense_Data * senseData, UInt8 senseDataSize )
{
	
	UInt8	size = min ( fAutoSenseDataSize, senseDataSize );
	
	require_nonzero ( size, Exit );
	
	if ( fAutosenseTaskMap == kernel_task )
	{
		
		bcopy ( senseData, fAutoSenseData, size );
		
	}
	
	else
	{
		
		require_nonzero ( fAutosenseDescriptor, Exit );
		fAutosenseDescriptor->writeBytes ( 0, senseData, size );
		
	}
	
	fAutoSenseDataIsValid = true;
	SetAutosenseRealizedDataCount ( size );
	
	
Exit:
	
	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAutoSenseData - Gets the auto sense data.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::GetAutoSenseData ( SCSI_Sense_Data *	receivingBuffer,
							 UInt8				senseDataSize )
{
	
	bool	result 	= false;
	UInt8	size	= 0;
	
	require ( fAutoSenseDataIsValid, Exit );
	require_nonzero_action ( receivingBuffer, Exit, result = fAutoSenseDataIsValid );
	
	size = min ( fAutoSenseDataSize, senseDataSize );
	require_nonzero ( size, Exit );
	
	if ( fAutosenseTaskMap == kernel_task )
	{
		
		bcopy ( fAutoSenseData, receivingBuffer, size );
		
	}
	
	else
	{
		
		require_nonzero ( fAutosenseDescriptor, Exit );
		
		// Copy the data, but don't overflow the buffer
		fAutosenseDescriptor->readBytes ( 0, receivingBuffer, size );
		
	}
	
	result = true;
	
	
Exit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAutoSenseDataSize - Gets the auto sense data size.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
SCSITask::GetAutoSenseDataSize ( void )
{
	
	return fAutoSenseDataSize;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EnsureAutosenseDescriptorExists - Ensures autosense descriptor is allocated.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void	
SCSITask::EnsureAutosenseDescriptorExists ( void )
{
	
	if ( fAutosenseDescriptor == NULL )
	{
		
		fAutosenseDescriptor = IOMemoryDescriptor::withAddress (
						( vm_address_t ) fAutoSenseData,
						fAutoSenseDataSize,
						kIODirectionIn,
						fAutosenseTaskMap );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetProtocolLayerReference - Sets the protocol layer reference value.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::SetProtocolLayerReference ( void * newReferenceValue )
{
	
	check ( newReferenceValue );
	fProtocolLayerReference = newReferenceValue;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetProtocolLayerReference - Gets the protocol layer reference value.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void *
SCSITask::GetProtocolLayerReference ( void )
{
	
	check ( fProtocolLayerReference );
	return fProtocolLayerReference;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetApplicationLayerReference - Sets the application layer reference value.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::SetApplicationLayerReference ( void * newReferenceValue )
{
	
	check ( newReferenceValue );
	fApplicationLayerReference = newReferenceValue;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetApplicationLayerReference - Gets the application layer reference value.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void *
SCSITask::GetApplicationLayerReference ( void )
{

	check ( fApplicationLayerReference );
	return fApplicationLayerReference;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTargetLayerReference - Sets the target layer reference value.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::SetTargetLayerReference ( void * newReferenceValue )
{
	
	check ( newReferenceValue );
	fTargetLayerReference = newReferenceValue;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTargetLayerReference - Gets the target layer reference value.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void *
SCSITask::GetTargetLayerReference ( void )
{
	
	check ( fTargetLayerReference );
	return fTargetLayerReference;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetPathLayerReference - Sets the path layer reference value.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
SCSITask::SetPathLayerReference ( void * newReferenceValue )
{
	
	check ( newReferenceValue );
	fPathLayerReference = newReferenceValue;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetPathLayerReference - Gets the path layer reference value.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void *
SCSITask::GetPathLayerReference ( void )
{
	
	check ( fPathLayerReference );
	return fPathLayerReference;
	
}


#if 0
#pragma mark -
#pragma mark ¥ SCSI Protocol Layer Mode methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTaskExecutionMode - Sets the SCSITaskMode.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetTaskExecutionMode ( SCSITaskMode newTaskMode )
{
	
	fTaskExecutionMode = newTaskMode;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTaskExecutionMode - Gets the SCSITaskMode.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaskMode
SCSITask::GetTaskExecutionMode ( void )
{
	return fTaskExecutionMode;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsAutosenseRequested - Reports whether autosense data is requested.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::IsAutosenseRequested ( void )
{
	return fAutosenseDataRequested;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetAutosenseIsValid - Sets the auto sense validity flag.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool				
SCSITask::SetAutosenseIsValid ( bool newAutosenseState )
{
	
	fAutoSenseDataIsValid = newAutosenseState;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAutosenseCommandDescriptorBlockSize - Gets the auto sense
//											   Command Descriptor Block Size.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
SCSITask::GetAutosenseCommandDescriptorBlockSize ( void )
{
	return fAutosenseCDBSize;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAutosenseCommandDescriptorBlock -	Gets the auto sense
//											Command Descriptor Block.  [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::GetAutosenseCommandDescriptorBlock ( SCSICommandDescriptorBlock * cdbData )
{
	
	bcopy ( &fAutosenseCDB, cdbData, sizeof ( SCSICommandDescriptorBlock ) );
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAutosenseDataTransferDirection -	Gets the auto sense data transfer
//											direction. 				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
SCSITask::GetAutosenseDataTransferDirection ( void )
{
	return kSCSIDataTransfer_FromTargetToInitiator;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAutosenseRequestedDataTransferCount -	Gets the auto sense requested
//												data transfer count.   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
SCSITask::GetAutosenseRequestedDataTransferCount ( void )
{
	return GetAutoSenseDataSize ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetAutosenseRealizedDataCount -	Sets the auto sense realized data
//										transfer count.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetAutosenseRealizedDataCount ( UInt64 realizedTransferCountInBytes )
{
	
	fAutoSenseRealizedByteCountOfTransfer = realizedTransferCountInBytes;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAutosenseRealizedDataCount -	Gets the auto sense realized data
//										transfer count.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
SCSITask::GetAutosenseRealizedDataCount ( void )
{
	return fAutoSenseRealizedByteCountOfTransfer;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAutosenseDataBuffer -	Gets the auto sense data buffer.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOMemoryDescriptor *
SCSITask::GetAutosenseDataBuffer ( void )
{
	
	EnsureAutosenseDescriptorExists ( );
	return fAutosenseDescriptor;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetAutosenseCommand -	Sets the auto sense command.	 		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITask::SetAutosenseCommand (
					UInt8			cdbByte0,
					UInt8			cdbByte1,
					UInt8			cdbByte2,
					UInt8			cdbByte3,
					UInt8			cdbByte4,
					UInt8			cdbByte5 )
{
	
	bool	result = false;
	
	fAutosenseCDB[0] = cdbByte0;
	fAutosenseCDB[1] = cdbByte1;
	fAutosenseCDB[2] = cdbByte2;
	fAutosenseCDB[3] = cdbByte3;
	fAutosenseCDB[4] = cdbByte4;
	fAutosenseCDB[5] = cdbByte5;
	fAutosenseCDB[6] = 0x00;
	fAutosenseCDB[7] = 0x00;
	fAutosenseCDB[8] = 0x00;
	fAutosenseCDB[9] = 0x00;
	fAutosenseCDB[10] = 0x00;
	fAutosenseCDB[11] = 0x00;
	fAutosenseCDB[12] = 0x00;
	fAutosenseCDB[13] = 0x00;
	fAutosenseCDB[14] = 0x00;
	fAutosenseCDB[15] = 0x00;
	
	fAutosenseCDBSize = kSCSICDBSize_6Byte;
	fAutosenseDataRequested = true;
	result = true;
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ SCSI Task Queue Management Methods
#pragma mark -
#endif

// These are the methods used for adding and removing the SCSI Task object
// to a queue.  These are mainly for use by the SCSI Protocol Layer, but can be
// used by the SCSI Application Layer if the task is currently not active (the
// Task state is kSCSITaskState_NEW_TASK or kSCSITaskState_ENDED).

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EnqueueFollowingSCSITask - Enqueues the specified Task after this one.
//															 		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSITask::EnqueueFollowingSCSITask ( SCSITask * followingTask )
{
	fNextTaskInQueue = followingTask;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetFollowingSCSITask - Returns the pointer to the SCSI Task that is
//							 queued after this one. Returns NULL if one is not
//							 currently queued.				 		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITask *
SCSITask::GetFollowingSCSITask ( void )
{
	
	return fNextTaskInQueue;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DequeueFollowingSCSITask - Returns the pointer to the SCSI Task that is
//							 	 queued after this one. Returns NULL if one is
//							 	 not currently queued.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITask *
SCSITask::DequeueFollowingSCSITask ( void )
{
	
	SCSITask *	returnTask;
	
	returnTask 			= fNextTaskInQueue;
	fNextTaskInQueue 	= NULL;
	
	return returnTask;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReplaceFollowingSCSITask - Returns the pointer to the SCSI Task that is
//								 queued after this one and removes it from the
//								 queue.  Returns NULL if one is not currently
//								 queued.  After dequeueing the following Task,
//								 the specified newFollowingTask will be
//								 enqueued after this task.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
SCSITask *
SCSITask::ReplaceFollowingSCSITask ( SCSITask * newFollowingTask )
{
	
	SCSITask *	returnTask = NULL;
	
	returnTask 			= fNextTaskInQueue;
	fNextTaskInQueue 	= newFollowingTask;
	
	return returnTask;
	
}