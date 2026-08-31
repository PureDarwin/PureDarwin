/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSDictionary.h>

// IOKit includes
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOBlockStorageDriver.h>

// SCSI Architecture Model Family includes
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "IOSCSIProtocolInterface.h"
#include "IOCompactDiscServices.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"CD Services"

#if DEBUG
#define SCSI_DVD_SERVICES_DEBUGGING_LEVEL					0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_COMPACT_DISC_SERVICES_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_COMPACT_DISC_SERVICES_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_COMPACT_DISC_SERVICES_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define	super IOCDBlockStorageDevice
OSDefineMetaClassAndStructors ( IOCompactDiscServices, IOCDBlockStorageDevice );


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
	IOCompactDiscServices *		owner;
	
	// The request parameters provided by the client.
	IOStorageCompletion			completionData;
	IOMemoryDescriptor * 		clientBuffer;
	UInt32 						clientStartingBlock;
	UInt32 						clientRequestedBlockCount;
	bool						clientReadCDCall;
	CDSectorArea				clientSectorArea;
	CDSectorType				clientSectorType;
	
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
//	¥ start - Start our services									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOCompactDiscServices::start ( IOService * provider )
{
	
	UInt32			cdFeaturesFlags = 0;
	bool			result			= false;
	
	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceType05, provider );
	require_nonzero ( fProvider, ErrorExit );
	require ( super::start ( fProvider ), ErrorExit );
	
	cdFeaturesFlags = ( ( OSNumber * ) fProvider->getProperty (
					kIOPropertySupportedCDFeatures ) )->unsigned32BitValue ( );
	
	if ( ( cdFeaturesFlags & kCDFeaturesWriteOnceMask ) ||
		 ( cdFeaturesFlags & kCDFeaturesReWriteableMask ) )
	{
		
		require ( setProperty ( kIOMatchCategoryKey,
								kSCSITaskUserClientIniterKey ), ErrorExit );
		
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey,
				  fProvider->GetProtocolCharacteristicsDictionary ( ) );
	setProperty ( kIOPropertyDeviceCharacteristicsKey,
				  fProvider->GetDeviceCharacteristicsDictionary ( ) );
	
	registerService ( );
	
	result = true;
	

ErrorExit:
	
	
	return false;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ open - Open the driver for business							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOCompactDiscServices::open ( IOService *		client,
							  IOOptionBits		options,
							  IOStorageAccess	access )
{
	
	// Same as IOService::open(), but with correct parameter types.
    return super::open ( client, options, ( void * ) access );
    
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Handle and relay any necessary messages				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::message ( UInt32 		type,
								 IOService *	nub,
								 void *			arg )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kSCSIServicesNotification_ExclusivityChanged:
		case kIOMessageMediaStateHasChanged:
		case kIOMessageTrayStateChange:
		case kIOMessageMediaAccessChange:
		{
			
			status = messageClients ( type, arg );
			
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
//	¥ setProperties - Used by autodiskmount to eject/inject the tray   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOCompactDiscServices::setProperties ( OSObject * properties )
{
	
	IOReturn		status 				= kIOReturnBadArgument;
	OSDictionary *	dict 				= NULL;
	UInt8			trayState			= 0xFF;
	Boolean			userClientActive	= false;
	
	require_nonzero ( properties, ErrorExit );
	
	dict = OSDynamicCast ( OSDictionary, properties );
	require_nonzero ( dict, ErrorExit );
	
	require_nonzero_action ( fProvider,
							 ErrorExit,
							 status = kIOReturnNotAttached );
	
	fProvider->retain ( );
	
	require_nonzero ( dict->getObject ( "TrayState" ), ReleaseProvider );
	
	// The user client is active, reject this call.
	userClientActive = fProvider->GetUserClientExclusivityState ( );
	require_action (
		( userClientActive == false ),
		ReleaseProvider,
		status = kIOReturnExclusiveAccess;
		messageClients ( kIOMessageTrayStateChange,
						 ( void * ) kMessageTrayStateChangeRequestRejected ) );
	
	fProvider->CheckPowerState ( );
	
	status = fProvider->GetTrayState ( &trayState );
	require_success ( status, ReleaseProvider );
	
	status = fProvider->SetTrayState ( !trayState );
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
    
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doAsyncReadCD - Sends READ_CD style commands to the driver 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doAsyncReadCD ( 	IOMemoryDescriptor *	buffer,
										UInt32					block,
										UInt32					nblks,
										CDSectorArea			sectorArea,
										CDSectorType			sectorType,
										IOStorageCompletion		completion )
{
	
	BlockServicesClientData	*	clientData	= NULL;
	IOReturn					status		= kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	clientData = IONew ( BlockServicesClientData, 1 );
	require_nonzero_action ( clientData, ErrorExit, status = kIOReturnNoResources );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Set the owner of this request.
	clientData->owner 						= this;
	
	// Save the client's request parameters.
	clientData->completionData 				= completion;
	clientData->clientBuffer 				= buffer;
	clientData->clientStartingBlock 		= block;
	clientData->clientRequestedBlockCount 	= nblks;
	clientData->clientReadCDCall			= true;
	clientData->clientSectorArea			= sectorArea;
	clientData->clientSectorType			= sectorType;
	
	// Set the retry limit to the maximum
	clientData->retriesLeft 				= kNumberRetries;
	
	fProvider->CheckPowerState ( );
	
	status = fProvider->AsyncReadCD ( buffer,
									  block,
									  nblks,
									  sectorArea,
									  sectorType,
									  ( void * ) clientData );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doAsyncReadWrite - Sends an asynchronous I/O to the driver	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doAsyncReadWrite ( IOMemoryDescriptor *		buffer,
										  UInt32					block,
										  UInt32					nblks,
										  IOStorageCompletion		completion )
{
	
	BlockServicesClientData	*	clientData 	= NULL;
	IOReturn					status		= kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	clientData = IONew ( BlockServicesClientData, 1 );
	require_nonzero_action ( clientData, ErrorExit, status = kIOReturnNoResources );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );

	STATUS_LOG ( ( "IOCompactDiscServices: doAsyncReadWrite; save completion data!\n" ) );

	// Set the owner of this request.
	clientData->owner 						= this;
	
	// Save the client's request parameters.
	clientData->completionData 				= completion;
	clientData->clientBuffer 				= buffer;
	clientData->clientStartingBlock 		= block;
	clientData->clientRequestedBlockCount 	= nblks;
	clientData->clientReadCDCall 			= false;
	
	// Set the retry limit to the maximum
	clientData->retriesLeft 				= kNumberRetries;

	fProvider->CheckPowerState ( );

	status = fProvider->AsyncReadWrite ( buffer, block, nblks, ( void * ) clientData );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doFormatMedia - Sends a format media request to the driver  	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doFormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
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
//	¥ doGetFormatCapacities - 	Sends a get format capacities request to
//								the driver 						 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOCompactDiscServices::doGetFormatCapacities ( UInt64 *	capacities,
											   UInt32	capacitiesMaxCount ) const
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );

	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
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
//	¥ doEjectMedia - 	Sends an eject media request to the driver 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doEjectMedia ( void )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
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
//	¥ doLockUnlockMedia - Sends an (un)lock media request to the driver
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doLockUnlockMedia ( bool doLock )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->LockUnlockMedia ( doLock );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
		
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getVendorString - Returns the vendor string					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOCompactDiscServices::getVendorString ( void )
{
	
	return fProvider->GetVendorString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getProductString - Returns the product string					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOCompactDiscServices::getProductString ( void )
{
	
	return fProvider->GetProductString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getRevisionString - Returns the product revision level string	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOCompactDiscServices::getRevisionString ( void )
{

	return fProvider->GetRevisionString ( );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAdditionalDeviceInfoString - Returns nothing				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOCompactDiscServices::getAdditionalDeviceInfoString ( void )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return ( "No Additional Device Info" );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportBlockSize - Reports media block size					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportBlockSize ( UInt64 * blockSize )
{
	
	return fProvider->ReportBlockSize ( blockSize );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportEjectability - Reports media ejectability characteristic   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportEjectability ( bool * isEjectable )
{
	
	return fProvider->ReportEjectability ( isEjectable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportLockability - Reports media lockability characteristic	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportLockability ( bool * isLockable )
{
	
	return fProvider->ReportLockability ( isLockable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMediaState - Reports media state						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportMediaState ( bool * mediaPresent,
										  bool * changed )    
{
	
	return fProvider->ReportMediaState ( mediaPresent, changed );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportPollRequirements - Reports polling requirements			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportPollRequirements (	bool * pollIsRequired,
												bool * pollIsExpensive )
{
	
	return fProvider->ReportPollRequirements ( pollIsRequired, pollIsExpensive );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxReadTransfer - Reports maximum read transfer size *OBSOLETE*
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportMaxReadTransfer ( 	UInt64   blockSize,
												UInt64 * max )
{
	
	return fProvider->ReportMaxReadTransfer ( blockSize, max );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxValidBlock - Reports maximum valid block on media	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportMaxValidBlock ( UInt64 * maxBlock )
{
	
	return fProvider->ReportMaxValidBlock ( maxBlock );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportRemovability -	Reports removability characteristic of the
//							media									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportRemovability ( bool * isRemovable )
{
	
	return fProvider->ReportRemovability ( isRemovable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readISRC - Reads the ISRC code from the media					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readISRC ( UInt8 track, CDISRC isrc )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadISRC ( track, isrc );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readMCN - Reads the MCN code from the media					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readMCN ( CDMCN mcn )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadMCN ( mcn );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readTOC - Reads the TOC from the media	*OBSOLETE*			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readTOC ( IOMemoryDescriptor * buffer )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadTOC ( buffer );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readTOC - Reads the TOC from the media						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readTOC ( IOMemoryDescriptor * 		buffer,
								 CDTOCFormat				format,
								 UInt8						msf,
								 UInt8						trackSessionNumber,
								 UInt16 *					actualByteCount )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadTOC ( buffer,
								  format,
								  msf,
								  trackSessionNumber,
								  actualByteCount );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readDiscInfo - Reads the disc info from the media				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readDiscInfo ( IOMemoryDescriptor * 	buffer,
									  UInt16 *				actualByteCount )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadDiscInfo ( buffer, actualByteCount );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readTrackInfo - Reads the track info from the media			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readTrackInfo ( IOMemoryDescriptor *		buffer,
									   UInt32					address,
									   CDTrackInfoAddressType	addressType,
									   UInt16 *					actualByteCount )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadTrackInfo ( buffer,
										address,
										addressType,
										actualByteCount );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ audioPause - Pauses audio playback				*OBSOLETE*	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::audioPause ( bool pause )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->AudioPause ( pause );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ audioPlay - Starts audio playback				*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::audioPlay ( CDMSF timeStart, CDMSF timeStop )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->AudioPlay ( timeStart, timeStop );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ audioScan - Starts audio scanning				*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::audioScan ( CDMSF timeStart, bool reverse )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->AudioScan ( timeStart, reverse );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ audioStop - Stops audio playback				*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::audioStop ( void )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->AudioStop ( );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAudioStatus - Gets audio status			*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::getAudioStatus ( CDAudioStatus * cdAudioStatus )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->GetAudioStatus ( cdAudioStatus );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAudioVolume - Gets audio volume			*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::getAudioVolume ( UInt8 * leftVolume,
										UInt8 * rightVolume )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->GetAudioVolume ( leftVolume, rightVolume );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ setAudioVolume - Sets audio volume			*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::setAudioVolume ( UInt8 leftVolume, UInt8 rightVolume )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->SetAudioVolume ( leftVolume, rightVolume );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doSynchronizeCache - Synchronizes the write cache				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doSynchronizeCache ( void )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->SynchronizeCache ( );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxWriteTransfer - Reports the maximum write transfer size [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportMaxWriteTransfer ( UInt64   blockSize,
												UInt64 * max )
{

	return fProvider->ReportMaxWriteTransfer ( blockSize, max );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportWriteProtection - 	Reports the write protect characteristic
//								of the media						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportWriteProtection ( bool * isWriteProtected )
{
	
	return fProvider->ReportWriteProtection ( isWriteProtected );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getMediaType - Reports the media type							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOCompactDiscServices::getMediaType ( void )
{
	
	return fProvider->GetMediaType ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getSpeed - Reports the media access speed						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::getSpeed ( UInt16 * kilobytesPerSecond )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->GetMediaAccessSpeed ( kilobytesPerSecond );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ setSpeed - Sets the media access speed						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::setSpeed ( UInt16 kilobytesPerSecond )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->SetMediaAccessSpeed ( kilobytesPerSecond );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleOpen - Handles opens on the object						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOCompactDiscServices::handleOpen ( IOService *		client,
									IOOptionBits	options,
									void *			access )
{
	
	bool	result = false;
	
	// If this isn't a user client, pass through to superclass.
	if ( ( options & kIOSCSITaskUserClientAccessMask ) == 0 )
	{
		
		result = super::handleOpen ( client, options, access );
		goto Exit;
		
	}
	
	// It's the user client, so add it to the set
	if ( fClients == NULL )
	{
		
		fClients = OSSet::withCapacity ( 1 );
		
	}
	
	require_nonzero ( fClients, ErrorExit );
	fClients->setObject ( client );
	
	result = true;
	
	
Exit:
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleClose - Handles closes on the object					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOCompactDiscServices::handleClose ( IOService * client, IOOptionBits options )
{
	
	// If this isn't a user client, pass through to superclass.
	if ( ( options & kIOSCSITaskUserClientAccessMask ) == 0 )
	{
		
		super::handleClose ( client, options );
		
	}
	
	else
	{
		
		fClients->removeObject ( client );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleIsOpen - Figures out if there are any opens on this object
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOCompactDiscServices::handleIsOpen ( const IOService * client ) const
{
	
	bool	result	= false;
	
	// General case (is anybody open)
	if ( client == NULL )
	{
		
		require_nonzero ( fClients, CallSuperClassError );
		require_nonzero ( fClients->getCount ( ), CallSuperClassError );
		result = true;
		
	}
	
	else
	{
		
		// specific case (is this client open)
		require_nonzero ( fClients, CallSuperClassError );
		require ( fClients->containsObject ( client ), CallSuperClassError );
		result = true;
		
	}
	
	
	return result;
	
	
CallSuperClassError:
	
	
	result = super::handleIsOpen ( client );
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Public Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteComplete - Static read/write completion routine
//															   [STATIC][PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOCompactDiscServices::AsyncReadWriteComplete ( void * 			clientData,
                                				IOReturn		status,
                                				UInt64 			actualByteCount )
{
	
	IOCompactDiscServices *		owner;
	IOStorageCompletion			returnData;
	BlockServicesClientData *	bsClientData;
	bool						commandComplete = true;

	bsClientData = ( BlockServicesClientData * ) clientData;
	
	// Save the IOCompletion information so that it may be returned
	// to the client.
	returnData 	= bsClientData->completionData;
	owner 		= bsClientData->owner;
	
	if ( ( ( status != kIOReturnNotAttached )		&&
		   ( status != kIOReturnOffline )			&&
		   ( status != kIOReturnUnsupportedMode )	&&
		   ( status != kIOReturnNotPrivileged )		&&
		   ( status != kIOReturnSuccess ) )			&&
		   ( bsClientData->retriesLeft > 0 ) )
	{
		
		IOReturn 	requestStatus;
		
		STATUS_LOG ( ( "IOCompactDiscServices: AsyncReadWriteComplete retry\n" ) );
		// An error occurred, but it is one on which the command
		// should be retried.  Decrement the retry counter and try again.
		bsClientData->retriesLeft--;
		if ( bsClientData->clientReadCDCall == true )
		{
			
			requestStatus = owner->fProvider->AsyncReadCD (
									bsClientData->clientBuffer,
									bsClientData->clientStartingBlock,
									bsClientData->clientRequestedBlockCount,
									bsClientData->clientSectorArea,
									bsClientData->clientSectorType,
									clientData );
			
		}
		
		else
		{
			
			requestStatus = owner->fProvider->AsyncReadWrite (
									bsClientData->clientBuffer, 
									bsClientData->clientStartingBlock, 
									bsClientData->clientRequestedBlockCount, 
									clientData );
			
		}
		
		if ( requestStatus != kIOReturnSuccess )
		{
			commandComplete = true;
		}
		
		else
		{
			commandComplete = false;
		}
		
	}
	
	if ( commandComplete == true )
	{		

		IODelete ( clientData, BlockServicesClientData, 1 );
		
		// Release the retain for this command.	
		owner->fProvider->release ( );
		owner->release ( );
		
		IOStorage::complete ( returnData, status, actualByteCount );
		
	}
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ free - Release any memory allocated at start time				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOCompactDiscServices::free ( void )
{
		
    super::free ( );
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 1 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 2 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 3 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 4 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 5 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 6 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 7 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 8 );