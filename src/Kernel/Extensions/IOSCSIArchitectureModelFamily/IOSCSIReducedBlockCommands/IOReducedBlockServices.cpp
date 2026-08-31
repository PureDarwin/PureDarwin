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
#include "IOReducedBlockServices.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"RBC Services"

#if DEBUG
#define SCSI_REDUCED_BLOCK_SERVICES_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_REDUCED_BLOCK_SERVICES_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_REDUCED_BLOCK_SERVICES_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_REDUCED_BLOCK_SERVICES_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors ( IOReducedBlockServices, IOBlockStorageDevice );


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
	IOReducedBlockServices *	owner;

	// The request parameters provided by the client.
	IOStorageCompletion			completionData;
	IOMemoryDescriptor * 		clientBuffer;
	UInt32 						clientStartingBlock;
	UInt32 						clientRequestedBlockCount;
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
IOReducedBlockServices::message ( UInt32 		type,
								  IOService *	nub,
								  void *		arg )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kIOMessageMediaStateHasChanged:
		{
			
			STATUS_LOG ( ( "type = kIOMessageMediaStateHasChanged, nub = %p\n", nub ) );
			status = messageClients ( type, arg );
			STATUS_LOG ( ( "status = %ld\n", ( UInt32 ) status ) );
			
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
IOReducedBlockServices::doAsyncReadWrite (
				IOMemoryDescriptor *	buffer,
				UInt32					block,
				UInt32					nblks,
				IOStorageCompletion		completion )
{
	
	BlockServicesClientData	*	clientData	= NULL;
	IOReturn					status 		= kIOReturnNotAttached;
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
	
	// Allocated data for clientData. Eventually this should be moved to a pooling
	// method of some sort (allocations on paging path seem bad...)
	clientData = IONew ( BlockServicesClientData, 1 );
	require_nonzero_action ( clientData, ErrorExit, status = kIOReturnNoResources );
	
	// Make sure we don't go away while the command is being executed.
	retain ( );
	fProvider->retain ( );
	
	// Set the owner of this request.
	clientData->owner = this;
	
	// Save the client's request parameters.
	clientData->completionData 				= completion;
	clientData->clientBuffer 				= buffer;
	clientData->clientStartingBlock 		= block;
	clientData->clientRequestedBlockCount 	= nblks;
	
	// Set the retry limit to the maximum
	clientData->retriesLeft = kNumberRetries;
	
	// Make sure our provider is in the correct power state to handle the I/O.
	fProvider->CheckPowerState ( );
	
	status = fProvider->AsyncReadWrite ( buffer, block, nblks, ( void * ) clientData );
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
IOReducedBlockServices::doEjectMedia ( void )
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
	status = fProvider->EjectTheMedia ( );
	
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
IOReducedBlockServices::doFormatMedia ( UInt64 byteCapacity )
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
	status = fProvider->FormatMedia ( byteCapacity );
	
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
IOReducedBlockServices::doGetFormatCapacities (
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
IOReducedBlockServices::doLockUnlockMedia ( bool doLock )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	// Return an error for incoming activity if we have been terminated
	require ( isInactive ( ) == false, ErrorExit );
	
	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Execute the command
	status = fProvider->LockUnlockMedia ( doLock );
	
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
IOReducedBlockServices::doSynchronizeCache ( void )
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
IOReducedBlockServices::getVendorString ( void )
{
	
	return fProvider->GetVendorString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getProductString - Gets the product string for the device		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOReducedBlockServices::getProductString ( void )
{
	
	return fProvider->GetProductString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getRevisionString - Gets the product revision string for the device
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOReducedBlockServices::getRevisionString ( void )
{
	
	return fProvider->GetRevisionString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAdditionalDeviceInfoString - Gets the additional device info string
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOReducedBlockServices::getAdditionalDeviceInfoString ( void )
{
	
	return ( "No Additional Device Info" );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportBlockSize - Reports the medium block size				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportBlockSize ( UInt64 * blockSize )
{
	
	return fProvider->ReportBlockSize ( blockSize );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportEjectability - Reports the medium ejectability characteristic.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportEjectability ( bool * isEjectable )
{
	
	return fProvider->ReportEjectability ( isEjectable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportLockability - Reports the medium lockability characteristic.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportLockability ( bool * isLockable )
{
	
	return fProvider->ReportLockability ( isLockable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportPollRequirements - Reports polling requirements.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportPollRequirements ( 	bool * pollIsRequired,
													bool * pollIsExpensive )
{
	
	return fProvider->ReportPollRequirements ( pollIsRequired, pollIsExpensive );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxReadTransfer - Reports maximum read transfer in bytes.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportMaxReadTransfer (
								UInt64 		blockSize,
								UInt64 *	max )
{
	
	return kIOReturnUnsupported;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxWriteTransfer - Reports maximum write transfer in bytes.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportMaxWriteTransfer ( UInt64		blockSize,
												 UInt64 *	max )
{
	
	return kIOReturnUnsupported;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxValidBlock - Reports maximum valid block on the media.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportMaxValidBlock ( UInt64 * maxBlock )
{

	return fProvider->ReportMaxValidBlock ( maxBlock );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMediaState - Reports state of media in the device		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportMediaState ( 	bool * mediaPresent,
												bool * changed )	
{
	
	return fProvider->ReportMediaState ( mediaPresent, changed );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportRemovability - Reports removability characteristic of media
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportRemovability ( bool * isRemovable )
{
	
	return fProvider->ReportRemovability ( isRemovable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportWriteProtection - Reports write protection characteristic of media
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOReducedBlockServices::reportWriteProtection ( bool * isWriteProtected )
{
	
	return fProvider->ReportWriteProtection ( isWriteProtected );
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ attach - 	Attach to our provider in the registry.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOReducedBlockServices::attach ( IOService * provider )
{
	
	bool	result = false;
	
	require_string ( super::attach ( provider ), ErrorExit,
					 "Superclass didn't attach" );
	
	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceType0E, provider );
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
//	¥ detach - 	Detach from our provider in the registry.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOReducedBlockServices::detach ( IOService * provider )
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
IOReducedBlockServices::AsyncReadWriteComplete (
				void * 			clientData,
				IOReturn		status,
				UInt64			actualByteCount )
{
	
	IOReducedBlockServices *	owner			= NULL;
	BlockServicesClientData * 	servicesData	= NULL;
	IOStorageCompletion			returnData		= { 0 };
	bool						commandComplete = true;
	
	// Save the IOCompletion information so that it may be returned
	// to the client.
	servicesData = ( BlockServicesClientData * ) clientData;	
	
	check ( servicesData );
	
	returnData	= servicesData->completionData;
	owner 		= servicesData->owner;
	
	check ( owner );
	
	STATUS_LOG ( ( "IOReducedBlockServices: AsyncReadWriteComplete; command status %x\n",
				   status ) );
	
	// Check to see if an error occurred that the request should be retried on
	if ( ( ( status != kIOReturnNotAttached ) && ( status != kIOReturnOffline ) &&
		   ( status != kIOReturnSuccess ) ) && ( servicesData->retriesLeft > 0 ) )
	{
		
		IOReturn 	requestStatus;
		
		STATUS_LOG ( ( "IOReducedBlockServices: AsyncReadWriteComplete; retry command\n" ) );
		// An error occurred, but it is one on which the command should be retried.
		// Decrement the retry counter and try again.
		servicesData->retriesLeft--;
		requestStatus = owner->fProvider->AsyncReadWrite ( 
										servicesData->clientBuffer, 
										servicesData->clientStartingBlock, 
										servicesData->clientRequestedBlockCount, 
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
OSMetaClassDefineReservedUnused ( IOReducedBlockServices, 1 );
OSMetaClassDefineReservedUnused ( IOReducedBlockServices, 2 );
OSMetaClassDefineReservedUnused ( IOReducedBlockServices, 3 );
OSMetaClassDefineReservedUnused ( IOReducedBlockServices, 4 );
OSMetaClassDefineReservedUnused ( IOReducedBlockServices, 5 );
OSMetaClassDefineReservedUnused ( IOReducedBlockServices, 6 );
OSMetaClassDefineReservedUnused ( IOReducedBlockServices, 7 );
OSMetaClassDefineReservedUnused ( IOReducedBlockServices, 8 );