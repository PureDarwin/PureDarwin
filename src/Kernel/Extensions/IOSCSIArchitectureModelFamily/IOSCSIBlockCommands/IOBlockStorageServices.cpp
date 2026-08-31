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

// IOKit includes
#include <IOKit/IOLib.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOBlockStorageDriver.h>

// SCSI Architecture Model Family includes
#include "IOBlockStorageServices.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SBC Services"

#if DEBUG
#define SCSI_BLOCK_SERVICES_DEBUGGING_LEVEL					0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_BLOCK_SERVICES_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_BLOCK_SERVICES_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_BLOCK_SERVICES_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors ( IOBlockStorageServices, IOBlockStorageDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// The command should be tried 5 times.  The original attempt 
// plus 4 retries.
#define kNumberRetries		4


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Structures
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Structure for the asynch client data
struct BlockServicesClientData
{
	// The object that owns the copy of this structure.
	IOBlockStorageServices *	owner;

	// The request parameters provided by the client.
	IOStorageCompletion			completionData;
	IOMemoryDescriptor * 		clientBuffer;
	UInt64 						clientStartingBlock;
	UInt64 						clientRequestedBlockCount;
	UInt32 						clientRequestedBlockSize;
	
	// The internally needed parameters.
	UInt32						retriesLeft;
};

typedef struct BlockServicesClientData	BlockServicesClientData;


#if 0
#pragma mark -
#pragma mark ¥ Public Methods - API Exported to layers above
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Handle messages sent to this object.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::message ( UInt32 		type,
								  IOService *	nub,
								  void *		arg )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kIOMessageMediaStateHasChanged:
		{
			
			IOMediaState	state = ( IOMediaState ) arg;
			
			switch ( state )
			{
				
				case kIOMediaStateOnline:
					fMediaPresent	= true;
					break;
					
				case kIOMediaStateOffline:
					fMediaPresent	= false;
					break;
					
				default:
					break;
				
			}
			
			status = messageClients ( type, arg, 0 );
			
		}
		break;
		
		default:
		{
			status = super::message ( type, nub, arg );
		}
		
		break;
		
	}
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doAsyncReadWrite - Performs an asynchronous read or write		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::doAsyncReadWrite (
				IOMemoryDescriptor *	buffer,
				UInt32					block,
				UInt32					nblks,
				IOStorageCompletion		completion )
{

	return doAsyncReadWrite ( buffer, (UInt64) block, (UInt64) nblks, completion );

}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doAsyncReadWrite - Performs an asynchronous read or write		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::doAsyncReadWrite (
				IOMemoryDescriptor *	buffer,
				UInt64					block,
				UInt64					nblks,
				IOStorageCompletion		completion )
{
	
	BlockServicesClientData	*	clientData			= NULL;
	IOReturn					status 				= kIOReturnNotAttached;
	UInt32						requestBlockSize	= 0;
	IODirection					direction;
	
	// Return an error for incoming I/O if we have been terminated	
	require ( isInactive ( ) == false, ErrorExit );
	
	// Determine the direction for I/O. If this is neither a read
	// nor a write request (since this is a read/write method,
	// what kind of request is it?) return an error to the client.
	direction = buffer->getDirection ( );
	require_action ( ( direction == kIODirectionIn ) ||
					 ( direction == kIODirectionOut ),
					 ErrorExit,
					 status = kIOReturnBadArgument );
	
	clientData = IONew ( BlockServicesClientData, 1 );
	require_nonzero_action ( clientData, ErrorExit, status = kIOReturnNoResources );
	
	// Make sure we don't go away while the command is being executed.
	retain ( );
	fProvider->retain ( );
	
	requestBlockSize = fProvider->ReportMediumBlockSize ( );
	
	STATUS_LOG ( ( "IOBlockStorageServices: doAsyncReadWrite; save completion data!\n" ) );
	
	// Set the owner of this request.
	clientData->owner = this;
	
	// Save the client's request parameters.
	clientData->completionData 				= completion;
	clientData->clientBuffer 				= buffer;
	clientData->clientStartingBlock 		= block;
	clientData->clientRequestedBlockCount 	= nblks;
	clientData->clientRequestedBlockSize 	= requestBlockSize;
	
	// Set the retry limit to the maximum
	clientData->retriesLeft = kNumberRetries;
	
	fProvider->CheckPowerState ( );
	
	status = fProvider->AsyncReadWrite ( buffer, block, nblks, (UInt64) requestBlockSize, (void *) clientData );
	require_success ( status, ReleaseClientDataAndRetain );
	
	
	return status;
	
	
ReleaseClientDataAndRetain:
	
	
	require_nonzero ( clientData, ErrorExit );
	IODelete ( clientData, BlockServicesClientData, 1 );
	clientData = NULL;
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doEjectMedia - Ejects the media								   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::doEjectMedia ( void )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	// Return an error for incoming activity if we have been terminated
	require ( isInactive ( ) == false, ErrorExit );
	
	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );

	// Make sure our provider is in the correct power state to handle the I/O.	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->EjectTheMedium ( );
	if ( status == kIOReturnSuccess )
	{
		fMediaPresent = false;
	}
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doFormatMedia - Formats the media								   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::doFormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	// Return an error for incoming activity if we have been terminated
	require ( isInactive ( ) == false, ErrorExit );
	
	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Make sure our provider is in the correct power state to handle the I/O.	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->FormatMedium ( byteCapacity / ( fProvider->ReportMediumBlockSize ( ) ),
									   fProvider->ReportMediumBlockSize ( ) );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doGetFormatCapacities - Gets format capacities				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOBlockStorageServices::doGetFormatCapacities (
								UInt64 * capacities,
								UInt32   capacitiesMaxCount ) const
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	// Return an error for incoming activity if we have been terminated
	require ( isInactive ( ) == false, ErrorExit );
	
	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Make sure our provider is in the correct power state to handle the I/O.	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->GetFormatCapacities ( capacities, capacitiesMaxCount );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doLockUnlockMedia - Locks/Unlocks the media					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::doLockUnlockMedia ( bool doLock )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	// Return an error for incoming activity if we have been terminated
	require ( isInactive ( ) == false, ErrorExit );
	
	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Execute the command
	status = fProvider->LockUnlockMedium ( doLock );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doSynchronizeCache - Synchronizes the write cache				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::doSynchronizeCache ( void )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	// Return an error for incoming activity if we have been terminated
	require ( isInactive ( ) == false, ErrorExit );
	
	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Make sure our provider is in the correct power state to handle the I/O.	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->SynchronizeCache ( );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getVendorString - Gets the vendor string for the device		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOBlockStorageServices::getVendorString ( void )
{

	return fProvider->GetVendorString ( );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getProductString - Gets the product string for the device		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOBlockStorageServices::getProductString ( void )
{
	
	return fProvider->GetProductString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getRevisionString - Gets the product revision string for the device
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOBlockStorageServices::getRevisionString ( void )
{
	
	return fProvider->GetRevisionString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAdditionalDeviceInfoString - Gets the additional device info string
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOBlockStorageServices::getAdditionalDeviceInfoString ( void )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return ( "No Additional Device Info" );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportBlockSize - Reports the medium block size				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportBlockSize ( UInt64 * blockSize )
{
	
	*blockSize = fProvider->ReportMediumBlockSize ( );
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportEjectability - Reports the medium ejectability characteristic.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportEjectability ( bool * isEjectable )
{
	
	*isEjectable = fProvider->ReportDeviceMediaRemovability ( );
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportLockability - Reports the medium lockability characteristic.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportLockability ( bool * isLockable )
{
	
	*isLockable = true;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportPollRequirements - Reports polling requirements.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportPollRequirements ( 	bool * pollIsRequired,
													bool * pollIsExpensive )
{
	
	*pollIsRequired 	= false;
	*pollIsExpensive 	= false;
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxReadTransfer - Reports maximum read transfer in bytes.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportMaxReadTransfer ( UInt64 		blockSize,
												UInt64 * 	max )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxWriteTransfer - Reports maximum write transfer in bytes.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportMaxWriteTransfer ( UInt64		blockSize,
												 UInt64 * 	max )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxValidBlock - Reports maximum valid block on the media.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportMaxValidBlock ( UInt64 * maxBlock )
{
	
	*maxBlock = ( fProvider->ReportMediumTotalBlockCount ( ) - 1 );
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMediaState - Reports state of media in the device		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportMediaState ( 	bool * mediaPresent,
											bool * changed )	
{
	
	*mediaPresent 	= fMediaPresent;
	*changed 		= false;
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportRemovability - Reports removability characteristic of media
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportRemovability ( bool * isRemovable )
{
	
	*isRemovable = fProvider->ReportDeviceMediaRemovability ( );
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportWriteProtection - Reports write protection characteristic of media
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::reportWriteProtection ( bool * isWriteProtected )
{
	
	*isWriteProtected = fProvider->ReportMediumWriteProtection ( );
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getWriteCacheState - Gets write cache state					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::getWriteCacheState ( bool * enabled )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	// Return an error for incoming activity if we have been terminated
	require ( isInactive ( ) == false, ErrorExit );
	
	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Make sure our provider is in the correct power state to handle the I/O.	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->GetWriteCacheState ( enabled );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ setWriteCacheState - Sets write cache state					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOBlockStorageServices::setWriteCacheState ( bool enabled )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	// Return an error for incoming activity if we have been terminated
	require ( isInactive ( ) == false, ErrorExit );
	
	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Make sure our provider is in the correct power state to handle the I/O.	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->SetWriteCacheState ( enabled );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ attach - Attaches this object to its provider.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOBlockStorageServices::attach ( IOService * provider )
{
	
	bool	result = false;
	
	require_string ( super::attach ( provider ), ErrorExit,
					 "Superclass didn't attach" );
	
	fProvider = OSDynamicCast ( IOSCSIBlockCommandsDevice, provider );
	require_nonzero_string ( fProvider, ErrorExit, "Incorrect provider type\n" );
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey,
				  fProvider->GetProtocolCharacteristicsDictionary ( ) );
	setProperty ( kIOPropertyDeviceCharacteristicsKey,
				  fProvider->GetDeviceCharacteristicsDictionary ( ) );
		
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ detach - Detaches this object from its provider.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOBlockStorageServices::detach ( IOService * provider )
{
	
	super::detach ( provider );
	
}


#if 0
#pragma mark -
#pragma mark ¥ Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteComplete - Completion routine for I/O			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOBlockStorageServices::AsyncReadWriteComplete (
				void * 			clientData,
				IOReturn		status,
				UInt64 			actualByteCount )
{
	
	IOBlockStorageServices *	owner			= NULL;
	BlockServicesClientData * 	servicesData	= NULL;
	IOStorageCompletion			returnData		= { 0 };
	bool						commandComplete = true;
	
	// Save the IOCompletion information so that it may be returned
	// to the client.
	servicesData 	= ( BlockServicesClientData * ) clientData;
	returnData 		= servicesData->completionData;
	owner 			= servicesData->owner;
	
	STATUS_LOG ( ( "IOBlockStorageServices: AsyncReadWriteComplete; command status %x\n",
					status  ) );
	
	// Check to see if an error occurred that on which the request should be retried.
	if  ( ( ( status != kIOReturnNotAttached ) && ( status != kIOReturnOffline ) &&
			( status != kIOReturnSuccess ) ) && ( servicesData->retriesLeft > 0 ) )
	{
		
		IOReturn 	requestStatus;
		
		STATUS_LOG ( ( "IOBlockStorageServices: AsyncReadWriteComplete; retry command\n" ) );
		// An error occurred, but it is one on which the command should be retried.
		// Decrement the retry counter and try again.
		servicesData->retriesLeft--;
		requestStatus = owner->fProvider->AsyncReadWrite ( 
										servicesData->clientBuffer, 
										servicesData->clientStartingBlock, 
										servicesData->clientRequestedBlockCount, 
										servicesData->clientRequestedBlockSize, 
										clientData );
		
		if ( requestStatus == kIOReturnSuccess )
		{
			commandComplete = false;
		}
		
	}
	
	if ( commandComplete == true )
	{		
		
		IODelete ( clientData, BlockServicesClientData, 1 );
		
		// Release the retains for this command.
		owner->fProvider->release ( );	
		owner->release ( );
		
		IOStorage::complete ( returnData, status, actualByteCount );
		
	}
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOBlockStorageServices, 1 );
OSMetaClassDefineReservedUnused ( IOBlockStorageServices, 2 );
OSMetaClassDefineReservedUnused ( IOBlockStorageServices, 3 );
OSMetaClassDefineReservedUnused ( IOBlockStorageServices, 4 );
OSMetaClassDefineReservedUnused ( IOBlockStorageServices, 5 );
OSMetaClassDefineReservedUnused ( IOBlockStorageServices, 6 );
OSMetaClassDefineReservedUnused ( IOBlockStorageServices, 7 );
OSMetaClassDefineReservedUnused ( IOBlockStorageServices, 8 );