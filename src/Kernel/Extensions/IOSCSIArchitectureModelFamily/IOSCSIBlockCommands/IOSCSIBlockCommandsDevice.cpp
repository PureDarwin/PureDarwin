/*
 * Copyright (c) 1998-2005 Apple Computer, Inc. All rights reserved.
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
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>

// Generic IOKit related headers
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOKitKeys.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>

// SCSI Architecture Model Family includes
#include "IOSCSIBlockCommandsDevice.h"
#include "SCSIBlockCommands.h"

#include <IOKit/scsi/SCSICommandDefinitions.h>
#include <IOKit/scsi/SCSICmds_MODE_Definitions.h>
#include <IOKit/scsi/SCSICmds_READ_CAPACITY_Definitions.h>
#include <IOKit/scsi/IOBlockStorageServices.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"SBC"

#if DEBUG
#define SCSI_SBC_DEVICE_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClass ( IOSCSIBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );
OSDefineAbstractStructors ( IOSCSIBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kMaxRetryCount						8
#define kNumberMediumGeometryKeys			4
#define kAppleKeySwitchProperty				"AppleKeyswitch"
#define kFibreChannelHDIconKey				"FibreChannelHD.icns"
#define kFireWireHDIconKey					"FireWireHD.icns"
#define kUSBHDIconKey						"USBHD.icns"
#define	kDefaultMaxBlocksPerIO				65535


#if 0
#pragma mark -
#pragma mark ¥ Public Methods - API Exported to layers above
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SyncReadWrite - 	Translates a synchronous I/O request into a
//						read or a write.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::SyncReadWrite (
						IOMemoryDescriptor *	buffer,
						UInt64					startBlock,
						UInt64					blockCount,
						UInt64					blockSize )
{
	return kIOReturnUnsupported;	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWrite - 	Translates an asynchronous I/O request into a
//						read or a write.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::AsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
						 					UInt64					blockSize,
											void *					clientData )
{
	
	IODirection		direction;
	IOReturn		status = kIOReturnBadArgument;
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
					
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	direction = buffer->getDirection ( );
	if ( direction == kIODirectionIn )
	{
		
		status = IssueRead ( buffer, startBlock, blockCount, clientData );
		
	}
	
	else if ( direction == kIODirectionOut )
	{
		
		status = IssueWrite ( buffer, startBlock, blockCount, clientData );
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EjectTheMedium - 	Unlocks and ejects the medium if it is removable.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::EjectTheMedium ( void )
{
	
	IOReturn				status				= kIOReturnNoResources;
	SCSIServiceResponse		serviceResponse 	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request				= NULL;
	bool					doPollForRemoval 	= false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
	
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	// Is the media removable?
	if ( fMediaIsRemovable == false )
	{

		if ( getProperty ( "Power Off" ) != NULL )
		{
			
			// Spin down the media now. We use to queue up a power change but we found
			// that since the PM stuff happens at a deferred point, the machine might shutdown
			// before we finish the PM change. So, we do the spindown now, then sync with PM
			// so it knows the state.
			if ( ( fCurrentPowerState > kSBCPowerStateSleep ) && ( fDeviceIsShared == false ) )
			{
				
				request = GetSCSITask ( );
				require_nonzero ( request, ErrorExit );
				
				// At a minimum, make sure the drive is spun down
				if ( START_STOP_UNIT ( request, 1, 0, 0, 0, 0 ) == true )
				{
					
					serviceResponse = SendCommand ( request, 0 );
					
				}
				
				// Give the drive some time to park the heads.
				IOSleep ( 500 );
				
				ReleaseSCSITask ( request );
				request = NULL;
				
			}
			
			fCurrentPowerState = kSBCPowerStateSleep;
			
		}
		
		// Sync ourselves with PM.
		changePowerStateToPriv ( kSBCPowerStateSleep );
		
	}

	else
	{
		
		// We have removable media. First, if it is not a manual eject device,
		// we know we can unlock it and then eject it. Otherwise, a dialog will
		// be brought up which tells the user it is safe to eject the medium
		// manually.
		
		// Is the device a known manual eject device?
		if ( fKnownManualEject == false )
		{
			
			request = GetSCSITask ( );
			require_nonzero ( request, ErrorExit );
			
			// Unlock it.
			if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateUnlocked, 0 ) == true )
			{
				
				// The command was successfully built, now send it
				( void ) SendCommand ( request, kTenSecondTimeoutInMS );
				
			}
			
			// Eject it.
			if ( START_STOP_UNIT ( request, 0, 0, 1, 0, 0 ) == true )
			{
				
				// The command was successfully built, now send it
				serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
				
				if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
					 ( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
				{
					
					// The eject command failed.  This is most likely a manually ejectable
					// device, start the polling to determine when the media has been removed.
					doPollForRemoval = true;
					
				}
				
			}
			
			ReleaseSCSITask ( request );
			request = NULL;
			
		}
		
		else
		{
			
			// It is a known manual eject device. Must poll here as well.
			doPollForRemoval = true;
			
		}
		
		ResetMediumCharacteristics ( );
		fMediumIsWriteProtected = true;
		
		if ( ( doPollForRemoval == true ) || ( fMediumRemovalPrevented == false ) )
		{
			
			// ¥¥¥ Add code here to put up the dialog "It is now safe to remove
			// this drive from the machine"
			
			// Set the polling to determine when media has been removed
 			fPollingMode = kPollingMode_MediaRemoval;
 			
   		}
   		
		else
		{
			
			// Set the polling to determine when new media has been inserted
 			fPollingMode = kPollingMode_NewMedia;
 			
		}
		
		EnablePolling ( );
		
	}
	
	status = kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FormatMedium - Unsupported.									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::FormatMedium ( UInt64 blockCount, UInt64 blockSize )
{
	
	IOReturn	status = kIOReturnUnsupported;
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
	
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetFormatCapacities - Unsupported.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIBlockCommandsDevice::GetFormatCapacities (
								UInt64 * capacities,
								UInt32   capacitiesMaxCount ) const
{
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LockUnlockMedium - Unsupported.								   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::LockUnlockMedium ( bool doLock )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	require_action ( IsProtocolAccessEnabled ( ), ErrorExit, status = kIOReturnNotAttached );
	require_action ( IsProtocolAccessEnabled ( ), ErrorExit, status = kIOReturnOffline );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SynchronizeCache - Synchronizes the write cache.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::SynchronizeCache ( void )
{
	
	IOReturn				status			= kIOReturnSuccess;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::SynchronizeCache called\n" ) );
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
	
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	require ( fWriteCacheEnabled, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request,
							 ErrorExit,
							 status = kIOReturnNoResources );
	
	if ( SYNCHRONIZE_CACHE ( request, 0, 0, 0, 0, 0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		status = kIOReturnSuccess;
	}
	
	else
	{
		status = kIOReturnError;
	}
	
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportBlockSize - Reports the medium block size.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIBlockCommandsDevice::ReportMediumBlockSize ( void )
{
	
	return fMediumBlockSize;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediumTotalBlockCount - Reports total number of blocks on medium.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIBlockCommandsDevice::ReportMediumTotalBlockCount ( void )
{
	
	return fMediumBlockCount64;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediumWriteProtection - Reports write protection characteristic
//									of medium						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::ReportMediumWriteProtection ( void )
{
	
	return fMediumIsWriteProtected;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportDeviceMaxBlocksReadTransfer - Reports maximum read transfer blocks.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksReadTransfer ( void )
{

	UInt32	maxBlockCount 	= kDefaultMaxBlocksPerIO;
	UInt64	maxByteCount	= 0;
	bool	supported		= false;
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksWriteTransfer.\n" ) );
	
	// See if the transport driver wants us to limit the block transfer count
	supported = GetProtocolDriver ( )->IsProtocolServiceSupported (
						kSCSIProtocolFeature_MaximumReadBlockTransferCount,
						&maxBlockCount );
	
	if ( supported == false )
		maxBlockCount = kDefaultMaxBlocksPerIO;
	
	// See if the transport driver wants us to limit the transfer byte count
	supported = GetProtocolDriver ( )->IsProtocolServiceSupported (
						kSCSIProtocolFeature_MaximumReadTransferByteCount,
						&maxByteCount );	
	
	if ( ( supported == true ) && ( maxByteCount > 0 ) )
	{
		
		setProperty ( kIOMaximumByteCountReadKey, maxByteCount, 64 );
		
		if ( fMediumBlockSize > 0 )
			maxBlockCount = min ( maxBlockCount, ( maxByteCount / fMediumBlockSize ) );
		
	}
	
	return maxBlockCount;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportDeviceMaxBlocksWriteTransfer -	Reports maximum write transfer
//											in blocks.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksWriteTransfer ( void )
{

	UInt32	maxBlockCount 	= kDefaultMaxBlocksPerIO;
	UInt64	maxByteCount	= 0;
	bool	supported		= false;
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksWriteTransfer.\n" ) );
	
	// See if the transport driver wants us to limit the block transfer count
	supported = GetProtocolDriver ( )->IsProtocolServiceSupported (
						kSCSIProtocolFeature_MaximumWriteBlockTransferCount,
						&maxBlockCount );
	
	if ( supported == false )
		maxBlockCount = kDefaultMaxBlocksPerIO;
	
	// See if the transport driver wants us to limit the transfer byte count
	supported = GetProtocolDriver ( )->IsProtocolServiceSupported (
						kSCSIProtocolFeature_MaximumWriteTransferByteCount,
						&maxByteCount );	
	
	if ( ( supported == true ) && ( maxByteCount > 0 ) )
	{
		
		setProperty ( kIOMaximumByteCountWriteKey, maxByteCount, 64 );
		
		if ( fMediumBlockSize > 0 )
			maxBlockCount = min ( maxBlockCount, ( maxByteCount / fMediumBlockSize ) );
		
	}
	
	return maxBlockCount;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportDeviceMediaRemovability -	Reports removability characteristic
//										of media					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::ReportDeviceMediaRemovability ( void )
{
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportMediaRemovability fMediaIsRemovable = %d\n",
					( int ) fMediaIsRemovable ) );
	
	return fMediaIsRemovable;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods - Methods used by this class and subclasses
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ InitializeDeviceSupport - Initializes device support			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::InitializeDeviceSupport ( void )
{
	
	bool	setupSuccessful = false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	// Allocate space for our reserved data.
	// Do this first to make sure that all expansion data is available before
	// any real work is done since that work may need these.
	fIOSCSIBlockCommandsDeviceReserved = IONew ( IOSCSIBlockCommandsDeviceExpansionData, 1 );
	require_nonzero ( fIOSCSIBlockCommandsDeviceReserved, ErrorExit );
	
	bzero ( fIOSCSIBlockCommandsDeviceReserved,
			sizeof ( IOSCSIBlockCommandsDeviceExpansionData ) );
	
	// Initialize the device characteristics flags
	fMediaIsRemovable 		= false;
	fKnownManualEject		= false;
	
	// Initialize the medium characteristics
	ResetMediumCharacteristics ( );
	
	// Grab any device information from the IORegistry
	if ( getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) != NULL )
	{
		
		// There is a characteristics property for this device, check for known entries.
		OSDictionary * characterDict = NULL;
		
		characterDict = OSDynamicCast (
					OSDictionary,
					getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) );
		
		// Check if the personality for this device specifies that this is known to be manual ejectable.
		if ( characterDict->getObject ( kIOPropertySCSIManualEjectKey ) != NULL )
		{
			
			STATUS_LOG ( ( "%s: found a Manual Eject property.\n", getName ( ) ) );
			fKnownManualEject = true;
			fDeviceIsShared = true;		
			
		}
		
	}
	
	if ( GetProtocolDriver ( )->getProperty ( kIOPropertyProtocolCharacteristicsKey ) != NULL )
	{
		
		// There is a characteristics property for this device, check for known entries.
		OSDictionary * characterDict = NULL;
		
		characterDict = OSDynamicCast (
					OSDictionary,
					GetProtocolDriver ( )->getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
		
		// Check if the personality for this device specifies that this is known to be manual ejectable.
		if ( characterDict->getObject ( kIOPropertySCSIProtocolMultiInitKey ) != NULL )
		{
			
			fDeviceIsShared = true;		
			
		}
		
	}
	
	// Make sure the drive is ready for us!
	require ( ClearNotReadyStatus ( ), ReleaseReservedMemory );
	
	setupSuccessful = DetermineDeviceCharacteristics ( );
	
	if ( setupSuccessful == true )
	{		
		
		fPollingMode = kPollingMode_NewMedia;
		fPollingThread = thread_call_allocate (
						( thread_call_func_t ) IOSCSIBlockCommandsDevice::sProcessPoll,
						( thread_call_param_t ) this );
		
		require_nonzero_action_string ( fPollingThread,
										ErrorExit,
										setupSuccessful = false,
										"fPollingThread allocation failed.\n" );
		
		InitializePowerManagement ( GetProtocolDriver ( ) );
		
	}
	
	STATUS_LOG ( ( "%s::%s setupSuccessful = %d\n", getName ( ),
					__FUNCTION__, setupSuccessful ) );

	// This needs to be called regardless of the state of the Medium Characteristics
	// to insure that the IORegistry properties for the maximum transfers are set, otherwise,
	// the IOStorageFamily will use the defaults.
	SetMediumCharacteristics ( ( UInt64 ) 0, ( UInt64 ) 0 );
	
	return setupSuccessful;
	
	
ReleaseReservedMemory:
	
	
	require_nonzero_quiet ( fIOSCSIBlockCommandsDeviceReserved, ErrorExit );
	IODelete ( fIOSCSIBlockCommandsDeviceReserved, IOSCSIBlockCommandsDeviceExpansionData, 1 );
	fIOSCSIBlockCommandsDeviceReserved = NULL;	
	
	
ErrorExit:
	
	
	return setupSuccessful;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StartDeviceSupport - Starts device support					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::StartDeviceSupport ( void )
{
	
	// Start polling
	EnablePolling ( );
	
	CreateStorageServiceNub ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SuspendDeviceSupport - Suspends device support				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::SuspendDeviceSupport ( void )
{
	
	if ( fPollingMode != kPollingMode_Suspended )
	{
		DisablePolling ( );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResumeDeviceSupport - Resumes device support					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::ResumeDeviceSupport ( void )
{
	
	if ( fMediumPresent == false )
	{
		
		// The driver has not found media in the device, restart
		// the polling for new media.
		fPollingMode = kPollingMode_NewMedia;
		EnablePolling ( );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StopDeviceSupport - Stops device support						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::StopDeviceSupport ( void )
{
	DisablePolling ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TerminateDeviceSupport - Terminates device support			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::TerminateDeviceSupport ( void )
{
	
	if ( fPollingThread != NULL )
	{
		
		thread_call_free ( fPollingThread );
		fPollingThread = NULL;
		
	}
	
	// Release all memory/objects associated with the reserved fields.
	if ( fPowerDownNotifier != NULL )
	{
		
		// remove() will also call release() on this object (IONotifier).
		// See IONotifier.h for more info.
		fPowerDownNotifier->remove ( );
		fPowerDownNotifier = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateCommandSetObjects - Creates command set objects			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::CreateCommandSetObjects ( void )
{
	
	bool	result = false;
	
	fSCSIBlockCommandObject =
		SCSIBlockCommands::CreateSCSIBlockCommandObject ( );
	require_nonzero ( fSCSIBlockCommandObject, ErrorExit );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FreeCommandSetObjects - Releases command set objects			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::FreeCommandSetObjects ( void )
{
	
	if ( fSCSIBlockCommandObject != NULL )
	{
		
		fSCSIBlockCommandObject->release ( );
		fSCSIBlockCommandObject = NULL;
  		
	}
	
	// Release the reserved structure.
	if ( fIOSCSIBlockCommandsDeviceReserved != NULL )
	{
		
		IODelete ( fIOSCSIBlockCommandsDeviceReserved, IOSCSIBlockCommandsDeviceExpansionData, 1 );
		fIOSCSIBlockCommandsDeviceReserved = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIBlockCommandObject - Accessor method					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIBlockCommands *
IOSCSIBlockCommandsDevice::GetSCSIBlockCommandObject ( void )
{
	
	check ( fSCSIBlockCommandObject );
	return fSCSIBlockCommandObject;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIPrimaryCommandObject - Accessor method					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIPrimaryCommands	*
IOSCSIBlockCommandsDevice::GetSCSIPrimaryCommandObject ( void )
{
	
	check ( fSCSIBlockCommandObject );
	return OSDynamicCast ( SCSIPrimaryCommands,
						   GetSCSIBlockCommandObject ( ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearNotReadyStatus - Clears any NOT_READY status on device	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::ClearNotReadyStatus ( void )
{
	
	SCSI_Sense_Data				senseBuffer		= { 0 };
	IOMemoryDescriptor *		bufferDesc		= NULL;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request			= NULL;
	bool						driveReady 		= false;
	bool						result 			= true;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	
	check ( bufferDesc );
	
	request = GetSCSITask ( );
	
	check ( request );
	
	do
	{
		
		if ( TEST_UNIT_READY ( request, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			bool validSense = false;
			
			if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				validSense = GetAutoSenseData ( request, &senseBuffer );
				if ( validSense == false )
				{
					
					if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0  ) == true )
					{
						
						// The command was successfully built, now send it
						serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
						
					}
					
					if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
					{
						
						validSense = true;
						
					}
					
				}
				
				if ( validSense == true )
				{
					
					if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY ) &&
								( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x01 ) )
					{
						
						STATUS_LOG ( ( "%s::drive not ready\n", getName ( ) ) );
						driveReady = false;
						IOSleep ( 200 );
						
					}
					
					else if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ||
							  ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) ||
							  ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x03 ) ) )
					{
						
						// The drive needs to be spun up. Issue a START_STOP_UNIT to it.
						if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01, 0x00 ) == true )
						{
							
							serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
							
						}
						
					}
					
					else
					{
						
						driveReady = true;
						STATUS_LOG ( ( "%s::drive READY\n", getName ( ) ) );
						
					}
					
					STATUS_LOG ( ( "sense data: %01x, %02x, %02x\n",
								 ( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ),
								   senseBuffer.ADDITIONAL_SENSE_CODE,
								   senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
					
				}
				
			}
			
			else
			{
				
				driveReady = true;
				
			}
			
		}
		
		else
		{
			
			// the command failed - perhaps the device was hot unplugged
			// give other threads some time to run.
			IOSleep ( 200 );
			
		}
	
	// check isInactive in case device was hot unplugged during sleep
	// and we are in an infinite loop here
	} while ( ( driveReady == false ) && ( isInactive ( ) == false ) );
	
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	ReleaseSCSITask ( request );
	
	result = isInactive ( ) ? false : true;
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EnablePolling - Schedules the polling thread to run			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::EnablePolling ( void )
{		
	
	AbsoluteTime	time;
	
	// No reason to start a thread if we've been terminatated
	require ( ( isInactive ( ) == false ), Exit );
	require ( fPollingThread, Exit );
	require ( ( fPollingMode != kPollingMode_Suspended ), Exit );
	
	// Retain ourselves so that this object doesn't go away
	// while we are polling
	
	retain ( );
	
	clock_interval_to_deadline ( 1000, kMillisecondScale, &time );
	thread_call_enter_delayed ( fPollingThread, time );
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DisablePolling - Unschedules the polling thread if it hasn't run yet
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::DisablePolling ( void )
{		
	
	fPollingMode = kPollingMode_Suspended;
	
	// Cancel the thread if it is scheduled to run
	require ( thread_call_cancel ( fPollingThread ), Exit );
	
	// It was running, so we balance out the retain()
	// with a release()
	release ( );
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineDeviceCharacteristics - Determines device characteristics
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::DetermineDeviceCharacteristics ( void )
{
	
	SCSIServiceResponse 			serviceResponse 	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier				request 			= NULL;
	IOBufferMemoryDescriptor *		buffer	 			= NULL;
	IOReturn						status				= kIOReturnSuccess;
	SCSICmd_INQUIRY_StandardData * 	inquiryBuffer 		= NULL;
	UInt8							inquiryBufferSize	= 0;
	UInt8							loop				= 0;
	bool							succeeded			= false;
	bool							WCEBit				= false;

	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	if ( fDefaultInquiryCount == 0 )
	{
		
		// There is no default Inquiry count for this device, use the standard
		// structure size.
		STATUS_LOG ( ( "%s: use sizeof(SCSICmd_INQUIRY_StandardData) for Inquiry.\n", getName ( ) ) );
		inquiryBufferSize = sizeof ( SCSICmd_INQUIRY_StandardData );
		
	}
	
	else
	{
		
		// This device has a default inquiry count, use it.
		STATUS_LOG ( ( "%s: use fDefaultInquiryCount for Inquiry.\n", getName ( ) ) );
		inquiryBufferSize = fDefaultInquiryCount;
		
	}
	
	buffer = IOBufferMemoryDescriptor::withCapacity ( inquiryBufferSize, kIODirectionIn );
	require_nonzero_string ( buffer, ErrorExit,
							 "Couldn't allocate INQUIRY memory descriptor" );
	
	inquiryBuffer = ( SCSICmd_INQUIRY_StandardData * ) buffer->getBytesNoCopy ( );
	require_nonzero_string ( inquiryBuffer, ReleaseDescriptor,
							 "Couldn't allocate INQUIRY buffer" );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	for ( loop = 0; ( ( loop < kMaxRetryCount ) && ( isInactive ( ) == false ) ); loop++ )
	{
		
		if ( INQUIRY ( 	request,
						buffer,
						0,
						0,
						0x00,
						inquiryBufferSize,
						0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			succeeded = true;
			break;
			
		}
		
		ERROR_LOG ( ( "!!! INQUIRY failed !!!, serviceResponse = %d, taskStatus = %d\n",
					  serviceResponse, GetTaskStatus ( request ) ) );
		
		// Sleep for 1 second to try to let the device come online in case it
		// just failed by freak occurrence (INQUIRY should NEVER fail, but apparently
		// it sometimes does...)
		IOSleep ( 1000 );
		
	}
	
	require ( succeeded, ReleaseTask );
	
	// Save ANSI version of the device
	SetANSIVersion ( inquiryBuffer->VERSION & kINQUIRY_ANSI_VERSION_Mask );
	
	// Deprecated, but left here for sake of compatibility
	fANSIVersion = GetANSIVersion ( );
	
	if ( ( inquiryBuffer->RMB & kINQUIRY_PERIPHERAL_RMB_BitMask ) ==
		   kINQUIRY_PERIPHERAL_RMB_MediumRemovable )
	{
		
		STATUS_LOG ( ( "Media is removable\n" ) );
		fMediaIsRemovable = true;
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "Media is NOT removable\n" ) );
		fMediaIsRemovable = false;
		
	}
	
	// Set the CMDQUE value so we know whether or not to enable TCQ.
	SetCMDQUE ( inquiryBuffer->flags2 & kINQUIRY_Byte7_CMDQUE_Mask );
	
	buffer->release ( );
	buffer = NULL;
	
	// Check the current state of the write cache.
	status = GetWriteCacheState ( &WCEBit );
	
	// Is the write cache turned off?
	if ( ( WCEBit == false ) && ( status == kIOReturnSuccess ) )
	{
		
		// Try to turn the write cache on.
		status = SetWriteCacheState ( true );
		if ( status == kIOReturnSuccess )
		{
			
			// We succeeded turning the write cache on.
			WCEBit = true;
			
		}
		
	}
	
	fWriteCacheEnabled = WCEBit;
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseDescriptor );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( buffer, ErrorExit );
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "%s::%s succeeded = %d\n", getName ( ), __FUNCTION__, succeeded ) );
	
	return succeeded;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediumCharacteristics - Sets medium characteristics		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::SetMediumCharacteristics (
							UInt32	blockSize,
							UInt32	blockCount )
{
	
	SetMediumCharacteristics ( ( UInt64 ) blockSize, ( UInt64 ) blockCount );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediumCharacteristics64 - Sets medium characteristics		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::SetMediumCharacteristics (
							UInt64	blockSize,
							UInt64	blockCount )
{
	
	UInt64		maxBlocksRead	= 0;
	UInt64		maxBlocksWrite	= 0;
	
	STATUS_LOG ( ( "mediumBlockSize = %qd, blockCount = %qd\n",
					blockSize, blockCount ) );
	
	fMediumBlockSize	= blockSize;
	fMediumBlockCount64 = blockCount;
	
	if ( fMediumBlockCount64 > kREPORT_CAPACITY_MaximumLBA )
	{
		
		// The medium has more LBAs than can be reported in the 32 bit
		// fMediumBlockCount variable, so set it to the maximum that it can report.
		fMediumBlockCount = kREPORT_CAPACITY_MaximumLBA;
		
	}
	
	else
	{
		fMediumBlockCount = blockCount;
	}
	
	maxBlocksRead	= ReportDeviceMaxBlocksReadTransfer ( );
	maxBlocksWrite	= ReportDeviceMaxBlocksWriteTransfer ( );
	
	setProperty ( kIOMaximumBlockCountReadKey, maxBlocksRead, 64 );
	setProperty ( kIOMaximumBlockCountWriteKey, maxBlocksWrite, 64 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResetMediumCharacteristics -	Resets medium characteristics to known
//									values							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::ResetMediumCharacteristics ( void )
{
	
	fMediumBlockSize		= 0;
	fMediumBlockCount 		= 0;
	fMediumBlockCount64		= 0;
	fMediumPresent			= false;
	fMediumIsWriteProtected	= true;
	fMediumRemovalPrevented	= false;

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateStorageServiceNub - Creates the linkage object for IOStorageFamily
//								to use.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::CreateStorageServiceNub ( void )
{
	
	IOService * 	nub = NULL;
	
	nub = OSTypeAlloc ( IOBlockStorageServices );
	require_nonzero ( nub, ErrorExit );
	
	nub->init ( );
	require ( nub->attach ( this ), ErrorExit );
	
	nub->registerService ( );
	nub->release ( );
	
	return;
	
	
ErrorExit:
	
	
	PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::CreateStorageServiceNub failed" ) );
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ProcessPoll - Processes a poll for media.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::ProcessPoll ( void )
{
	
	switch ( fPollingMode )
	{
		
		case kPollingMode_NewMedia:
		{
			PollForNewMedia ( );
		}
		break;
		
		case kPollingMode_MediaRemoval:
		{
			PollForMediaRemoval ( );
		}
		break;
		
		default:
		{
			
			// This is an unknown polling mode -- do nothing.
			ERROR_LOG ( ( "%s:ProcessPoll Unknown polling mode.\n", getName ( ) ) );
			
		}
		break;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PollForNewMedia - Polls for new media insertion.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::PollForNewMedia ( void )
{
	
	bool			result	 	= false;
	UInt64			blockCount	= 0;
	UInt64			blockSize	= 0;
	
	// Since this is a poll for new media...	
	fMediumPresent = false;
	
	result = DetermineMediaPresence ( );
	require_quiet ( result, Exit );
	
	// If we got here, then we have found media
	if ( fMediaIsRemovable == true )
	{
		fMediumRemovalPrevented = PreventMediumRemoval ( );
	}
	
	else
	{
		fMediumRemovalPrevented = true;
	}
	
	result = DetermineMediumCapacity ( &blockSize, &blockCount );
	require ( result, Exit );
	
	// Attempt to determine its geometry now.
	DetermineMediumGeometry ( );
	
	// What happens if the medium is unformatted?
	// A check should be added to handle this case.
	
	SetMediumCharacteristics ( blockSize, blockCount );
	
	fMediumIsWriteProtected = DetermineMediumWriteProtectState ( );
	
	fMediumPresent	= true;
	fPollingMode	= kPollingMode_Suspended;
	
	// Set the icon for this type of media.
	SetMediumIcon ( );
	
	// Message up the chain that we have media
	messageClients ( kIOMessageMediaStateHasChanged,
					 ( void * ) kIOMediaStateOnline,
					 0 );
	
	if ( fMediumRemovalPrevented == false )
	{
		
		// Media is not locked into the drive, so this is most likely
		// a manually ejectable device, start polling for media removal.
		fPollingMode = kPollingMode_MediaRemoval;
		fKnownManualEject = true;
		fDeviceIsShared = true;
		
	}
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediaPresence - Checks if media has been inserted.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::DetermineMediaPresence ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	bool					mediaFound 		= false;
	OSBoolean *				keySwitchLocked = NULL;
	
	keySwitchLocked = OSDynamicCast (
							OSBoolean,
							getProperty ( kAppleKeySwitchProperty ) );
	
	if ( keySwitchLocked != NULL )
	{
		
		// See if we should poll for media.
		if ( keySwitchLocked->isTrue ( ) )
		{
			goto ErrorExit;
		}
		
	}
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	// Do a TEST_UNIT_READY to generate sense data
	if ( TEST_UNIT_READY ( request, 0 ) == true )
	{
		
		// The command was successfully built, now send it, set timeout to 10 seconds.
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			bool					validSense 	= false;
			SCSI_Sense_Data			senseBuffer	= { 0 };
			IOMemoryDescriptor *	bufferDesc	= NULL;
			
			validSense = GetAutoSenseData ( request, &senseBuffer );
			if ( validSense == false )
			{
				
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				require_nonzero ( bufferDesc, ReleaseTask );
				
				// Get the sense data to determine if media is present.
				if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
				{
					
					// The command was successfully built, now send it
					serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
					
				}
				
				bufferDesc->release ( );
				bufferDesc = NULL;
				
				if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		 			 ( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
		 		{
					
					ERROR_LOG ( ( "%s: REQUEST_SENSE failed\n", getName ( ) ) );
					goto ReleaseTask;
					
		 		}
				
			}
			
			if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x00 ) &&
				 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
			{
				
				STATUS_LOG ( ( "Media found\n" ) );
				mediaFound = true;
				
			}
			
			else
			{
				
				ERROR_LOG ( ( "ASC = 0x%02x, ASCQ = 0x%02x\n",
								senseBuffer.ADDITIONAL_SENSE_CODE,
								senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
			}
			
		}
		
		else
		{
			
			STATUS_LOG ( ( "Media found\n" ) );
			mediaFound = true;
			
		}
		
	}
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ErrorExit );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
	
	
	return mediaFound;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PreventMediumRemoval - Prevents Medium Removal.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::PreventMediumRemoval ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	bool  					mediumLocked	= false;
	
	// Before forcing work to be done, verify that it is necessary by checking
	// if this is a known manual eject device.
	require ( ( fKnownManualEject == false ), Exit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, Exit );
	
	if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateLocked, 0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
	 	 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		mediumLocked = true;
		
	}
	
	ReleaseSCSITask ( request );
	
	
Exit:
	
	
	return mediumLocked;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediumCapacity - Determines capacity of the medium.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::DetermineMediumCapacity (
					UInt64 * blockSize,
					UInt64 * blockCount )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSI_Capacity_Data		capacityData	= { 0 };
	IOMemoryDescriptor *	bufferDesc		= NULL;
	SCSITaskIdentifier		request			= NULL;
	bool					result			= false;
	
	*blockSize 	= 0;
	*blockCount = 0;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( &capacityData,
												   kREPORT_CAPACITY_DataSize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseTask );
	
	// We found media, get its capacity
	if ( READ_CAPACITY ( request, bufferDesc, 0, 0x00, 0, 0 ) == true )
	{
		
		// The command was successfully built, now send it.
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		*blockSize 	= OSSwapBigToHostInt32 ( capacityData.BLOCK_LENGTH_IN_BYTES );
		*blockCount = ( ( UInt64 ) OSSwapBigToHostInt32 ( capacityData.RETURNED_LOGICAL_BLOCK_ADDRESS ) ) + 1;
		
		// Since valid capacity data was retreived from the device, set the
		// result to true here so that if the READ CAPACITY 16 fails for any reason,
		// the caller will still be notified that valid data was retrieved from the device.		
		result = true;

		// SBC-2 Spec 5.14 states that if the LBA address is 0xFFFFFFFF (kREPORT_CAPACITY_MaximumLBA),
		// and the device is SPC-3/SBC-2 compliant, we shall issue a READ_CAPACITY_16 command.
		if ( ( capacityData.RETURNED_LOGICAL_BLOCK_ADDRESS == kREPORT_CAPACITY_MaximumLBA ) &&
			 ( GetANSIVersion ( ) >= kINQUIRY_ANSI_VERSION_SCSI_SPC_3_Compliant ) )
		{
			
			SCSI_Capacity_Data_Long  longCapacityData = { 0 };
			
			// We need to release the previous bufferDesc
			bufferDesc->release ( );
			bufferDesc = IOMemoryDescriptor::withAddress (  &longCapacityData, 
															kREPORT_CAPACITY_16_DataSize,
															kIODirectionIn );
			require_nonzero ( bufferDesc, ReleaseTask );
			
			if ( READ_CAPACITY_16 ( request, bufferDesc, 0, kREPORT_CAPACITY_16_DataSize, 0, 0 ) == true )
			{
			
				serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
				
				if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
					 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
				{
					
					*blockSize  = OSSwapBigToHostInt32 ( longCapacityData.BLOCK_LENGTH_IN_BYTES );
					*blockCount = OSSwapBigToHostInt64 ( longCapacityData.RETURNED_LOGICAL_BLOCK_ADDRESS ) + 1;
					
				}
				
			}
			
		}
		
		STATUS_LOG ( ( "%s: Media capacity: %qx and block size: %qx\n", getName ( ), *blockCount, *blockSize ) );
		
	}
	
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ErrorExit );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediumGeometry - Determines geometry of the medium.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::DetermineMediumGeometry ( void )
{
	
	IOBufferMemoryDescriptor *	bufferDesc				= NULL;
	OSDictionary *				dict					= NULL;
	IOReturn					status					= kIOReturnSuccess;
	UInt32						minBufferSize			= 0;
	UInt16						blockDescriptorLength	= 0;
	UInt32						modeDataLength			= 0;
	UInt8						headerSize				= 0;
	UInt8 *						buffer					= NULL;
	bool						use10ByteModeSense		= true;
	
	require_quiet ( ( fMediaIsRemovable == false ), ErrorExit );
	
	dict = OSDictionary::withCapacity ( kNumberMediumGeometryKeys );
	require_nonzero ( dict, ErrorExit );
	
	bufferDesc = IOBufferMemoryDescriptor::withCapacity ( sizeof ( SPCModeParameterHeader10 ), kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseDictionary );
	
	buffer = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( buffer, bufferDesc->getLength ( ) );
	
	status = GetModeSense ( bufferDesc,
							kSBCModePageFormatDeviceCode,
							sizeof ( SPCModeParameterHeader10 ),
							&use10ByteModeSense );
	require_success ( status, RigidDisk );
	
	if ( use10ByteModeSense )
	{
		
		SPCModeParameterHeader10 *	header = ( SPCModeParameterHeader10 * ) buffer;
		
		blockDescriptorLength = OSSwapBigToHostInt16 ( header->BLOCK_DESCRIPTOR_LENGTH );
		
		// Sanity check. Make sure BLOCK_DESCRIPTOR_LENGTH is a multiple of 16.
		require ( ( ( blockDescriptorLength & 15 ) == 0 ), RigidDisk );
		
		// Get the length of data the device reports.
		modeDataLength = OSSwapBigToHostInt16 ( header->MODE_DATA_LENGTH ) + sizeof ( header->MODE_DATA_LENGTH );
		
		// Some devices report more data than we need,
		// and if we ask for the full amount, they don't complete the command,
		// so, trim the data length to be the size of the header, the block descriptor length reported
		// and the size of the mode page.
		modeDataLength = min ( modeDataLength, sizeof ( SPCModeParameterHeader10 ) + blockDescriptorLength + sizeof ( SBCModePageFormatDevice ) );
		
	}
	
	else
	{
		
		SPCModeParameterHeader6 *	header = ( SPCModeParameterHeader6 * ) buffer;
		
		blockDescriptorLength = header->BLOCK_DESCRIPTOR_LENGTH;
		
		// Sanity check. Make sure BLOCK_DESCRIPTOR_LENGTH is a multiple of 8.
		require ( ( ( blockDescriptorLength & 7 ) == 0 ), RigidDisk );
		
		// Get the length of data the device reports.
		modeDataLength = header->MODE_DATA_LENGTH + sizeof ( header->MODE_DATA_LENGTH );
		
		// Some devices report more data than we need,
		// and if we ask for the full amount, they don't complete the command,
		// so, trim the data length to be the size of the header, the block descriptor length reported
		// and the size of the mode page.
		modeDataLength = min ( modeDataLength, sizeof ( SPCModeParameterHeader6 ) + blockDescriptorLength + sizeof ( SBCModePageFormatDevice ) );
		
	}
	
	bufferDesc->release ( );
	bufferDesc = IOBufferMemoryDescriptor::withCapacity ( modeDataLength, kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseDictionary );
	
	buffer = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( buffer, bufferDesc->getLength ( ) );
	
	status = GetModeSense ( bufferDesc,
							kSBCModePageFormatDeviceCode,
							modeDataLength,
							&use10ByteModeSense );
	
	if ( use10ByteModeSense )
	{
		
		SPCModeParameterHeader10 *	header = ( SPCModeParameterHeader10 * ) buffer;
		
		headerSize 				= sizeof ( SPCModeParameterHeader10 );
		blockDescriptorLength	= OSSwapBigToHostInt16 ( header->BLOCK_DESCRIPTOR_LENGTH );
		
		// Sanity check. Make sure BLOCK_DESCRIPTOR_LENGTH is a multiple of 16.
		require ( ( ( blockDescriptorLength & 15 ) == 0 ), RigidDisk );
		
	}
	
	else
	{
		
		SPCModeParameterHeader6 *	header = ( SPCModeParameterHeader6 * ) buffer;
		
		headerSize				= sizeof ( SPCModeParameterHeader6 );
		blockDescriptorLength	= header->BLOCK_DESCRIPTOR_LENGTH;
		
		// Sanity check. Make sure BLOCK_DESCRIPTOR_LENGTH is a multiple of 8.
		require ( ( ( blockDescriptorLength & 7 ) == 0 ), RigidDisk );
		
	}
	
	// Ensure that our buffer is of the minimum correct size.
	// Also, check that the first byte of the page is the
	// correct PAGE_CODE.
	minBufferSize = headerSize + blockDescriptorLength + offsetof ( SBCModePageFormatDevice, INTERLEAVE );
	require ( ( modeDataLength >= minBufferSize ), RigidDisk );
	
	{
		
		SBCModePageFormatDevice *	formatDevice 	= NULL;
		OSNumber *					number			= NULL;
		UInt8						pageCode		= 0;
		
		formatDevice = ( SBCModePageFormatDevice * ) &buffer[headerSize + blockDescriptorLength];
		pageCode	 = formatDevice->header.PS_PAGE_CODE & kModePageFormat_PAGE_CODE_Mask;
		
		require ( ( pageCode == kSBCModePageFormatDeviceCode ), RigidDisk );
		
		// Make sure we are using host endianness
		formatDevice->SECTORS_PER_TRACK				 = OSSwapBigToHostInt16 ( formatDevice->SECTORS_PER_TRACK );
		formatDevice->DATA_BYTES_PER_PHYSICAL_SECTOR = OSSwapBigToHostInt16 ( formatDevice->DATA_BYTES_PER_PHYSICAL_SECTOR );
		
		number = OSNumber::withNumber ( formatDevice->SECTORS_PER_TRACK, 16 );
		if ( number != NULL )
		{
			
			dict->setObject ( kIOPropertySectorCountPerTrackKey, number );
			number->release ( );
			number = NULL;
			
		}
		
		number = OSNumber::withNumber ( formatDevice->DATA_BYTES_PER_PHYSICAL_SECTOR, 16 );
		if ( number != NULL )
		{
			
			dict->setObject ( kIOPropertyBytesPerPhysicalSectorKey, number );
			number->release ( );
			number = NULL;
			
		}
		
	}
	
	
RigidDisk:	
	
	
	bufferDesc->release ( );
	bufferDesc = IOBufferMemoryDescriptor::withCapacity ( sizeof ( SPCModeParameterHeader10 ), kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseDictionary );
	
	buffer = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( buffer, bufferDesc->getLength ( ) );
	
	status = GetModeSense ( bufferDesc,
							kSBCModePageRigidDiskGeometryCode,
							sizeof ( SPCModeParameterHeader10 ),
							&use10ByteModeSense );
	require_success ( status, ReleaseDescriptor );
	
	if ( use10ByteModeSense )
	{
		
		SPCModeParameterHeader10 *	header = ( SPCModeParameterHeader10 * ) buffer;
		
		blockDescriptorLength = OSSwapBigToHostInt16 ( header->BLOCK_DESCRIPTOR_LENGTH );
		
		// Sanity check. Make sure BLOCK_DESCRIPTOR_LENGTH is a multiple of 16.
		require ( ( ( blockDescriptorLength & 15 ) == 0 ), ReleaseDescriptor );
		
		// Get the length of data the device reports.
		modeDataLength = OSSwapBigToHostInt16 ( header->MODE_DATA_LENGTH ) + sizeof ( header->MODE_DATA_LENGTH );
		
		// Some devices report more data than we need,
		// and if we ask for the full amount, they don't complete the command,
		// so, trim the data length to be the size of the header, the block descriptor length reported
		// and the size of the mode page.
		modeDataLength = min ( modeDataLength, sizeof ( SPCModeParameterHeader10 ) + blockDescriptorLength + sizeof ( SBCModePageRigidDiskGeometry ) );
		
	}
	
	else
	{
		
		SPCModeParameterHeader6 *	header = ( SPCModeParameterHeader6 * ) buffer;
		
		blockDescriptorLength = header->BLOCK_DESCRIPTOR_LENGTH;
		
		// Sanity check. Make sure BLOCK_DESCRIPTOR_LENGTH is a multiple of 8.
		require ( ( ( blockDescriptorLength & 7 ) == 0 ), ReleaseDescriptor );
		
		// Get the length of data the device reports.
		modeDataLength = header->MODE_DATA_LENGTH + sizeof ( header->MODE_DATA_LENGTH );
		
		// Some devices report more data than we need,
		// and if we ask for the full amount, they don't complete the command,
		// so, trim the data length to be the size of the header, the block descriptor length reported
		// and the size of the mode page.
		modeDataLength = min ( modeDataLength, sizeof ( SPCModeParameterHeader6 ) + blockDescriptorLength + sizeof ( SBCModePageRigidDiskGeometry ) );
		
	}
	
	bufferDesc->release ( );
	bufferDesc = IOBufferMemoryDescriptor::withCapacity ( modeDataLength, kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseDictionary );
	
	buffer = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( buffer, bufferDesc->getLength ( ) );	
	status = GetModeSense ( bufferDesc,
							kSBCModePageRigidDiskGeometryCode,
							modeDataLength,
							&use10ByteModeSense );
	
	require_success ( status, ReleaseDescriptor );
	
	if ( use10ByteModeSense )
	{
		
		SPCModeParameterHeader10 *	header = ( SPCModeParameterHeader10 * ) buffer;
		
		headerSize 				= sizeof ( SPCModeParameterHeader10 );
		blockDescriptorLength	= OSSwapBigToHostInt16 ( header->BLOCK_DESCRIPTOR_LENGTH );
		
		// Sanity check. Make sure BLOCK_DESCRIPTOR_LENGTH is a multiple of 16.
		require ( ( ( blockDescriptorLength & 15 ) == 0 ), ReleaseDescriptor );
		
	}
	
	else
	{
		
		SPCModeParameterHeader6 *	header = ( SPCModeParameterHeader6 * ) buffer;
		
		headerSize				= sizeof ( SPCModeParameterHeader6 );
		blockDescriptorLength	= header->BLOCK_DESCRIPTOR_LENGTH;
		
		// Sanity check. Make sure BLOCK_DESCRIPTOR_LENGTH is a multiple of 8.
		require ( ( ( blockDescriptorLength & 7 ) == 0 ), ReleaseDescriptor );
		
	}
	
	// Ensure that our buffer is of the minimum correct size.
	// Also, check that the first byte of the page is the
	// correct PAGE_CODE.
	minBufferSize = headerSize + blockDescriptorLength + offsetof ( SBCModePageRigidDiskGeometry, STARTING_CYLINDER_WRITE_PRECOMPENSATION );
	require ( ( modeDataLength >= minBufferSize ), ReleaseDescriptor );
	
	{
		
		SBCModePageRigidDiskGeometry *	rigidDisk 	= NULL;
		OSNumber *						number		= NULL;
		UInt32							cylinders	= 0;	
		UInt8							pageCode	= 0;
		
		rigidDisk	= ( SBCModePageRigidDiskGeometry * ) &buffer[headerSize + blockDescriptorLength];
		pageCode	= rigidDisk->header.PS_PAGE_CODE & kModePageFormat_PAGE_CODE_Mask;
		
		require ( ( pageCode == kSBCModePageRigidDiskGeometryCode ), ReleaseDescriptor );
		
		cylinders = ( rigidDisk->NUMBER_OF_CYLINDERS[0] << 16 ) +
					( rigidDisk->NUMBER_OF_CYLINDERS[1] << 8 ) +
					  rigidDisk->NUMBER_OF_CYLINDERS[2];
		
		number = OSNumber::withNumber ( cylinders, 32 );
		if ( number != NULL )
		{
			
			dict->setObject ( kIOPropertyCylinderCountKey, number );
			number->release ( );
			number = NULL;
			
		}
		
		number = OSNumber::withNumber ( rigidDisk->NUMBER_OF_HEADS, 8 );
		if ( number != NULL )
		{
			
			dict->setObject ( kIOPropertyHeadCountKey, number );
			number->release ( );
			number = NULL;
			
		}
		
	}
	
	GetDeviceCharacteristicsDictionary ( )->setObject ( kIOPropertyRigidDiskGeometryKey, dict );
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, ReleaseDictionary );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseDictionary:
	
	
	require_nonzero_quiet ( dict, ErrorExit );
	dict->release ( );
	dict = NULL;
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediumWriteProtectState - Determines medium write protect state.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::DetermineMediumWriteProtectState ( void )
{
	
	SCSIServiceResponse 	serviceResponse 		= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *	bufferDesc				= NULL;
	SCSITaskIdentifier		request					= NULL;
	bool					writeProtectDetermined 	= false;
	bool					mediumIsProtected 		= true;
	SCSI_Sense_Data			senseBuffer				= { 0 };
	UInt8					modeBuffer[16]			= { 0 };
	
	// For now, report all fixed disks as writable since most have
	// no way of changing this.
	require_action_quiet ( fMediaIsRemovable, Exit, mediumIsProtected = false );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	// Send a Request Sense to the device, this seems to make some happy again.
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	check ( bufferDesc );
	
	if ( bufferDesc != NULL )
	{
		
		// Issue a Request Sense
		// Whether the command completes successfully or not is irrelevent.
		if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		// release the sense data buffer;
		bufferDesc->release ( );
		bufferDesc = NULL;
		
	}
	
	// Now back to normal programming.
	bufferDesc = IOMemoryDescriptor::withAddress ( 	modeBuffer,
													8,
													kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseTask );
	
	if ( GetANSIVersion ( ) == kINQUIRY_ANSI_VERSION_NoClaimedConformance )
	{
		
		// The device does not claim compliance with any ANSI version, so it
		// is most likely an ATAPI device, try the 10 byte command first.
		
		if ( MODE_SENSE_10 ( 	request,
								bufferDesc,
								0x00,
								0x00,
								0x00,	// Normally, we set DBD=1, but since we're only
								0x3F,	// interested in modeBuffer[3], we set DBD=0 since
								8,		// it makes legacy devices happier
								0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			if ( ( modeBuffer[3] & kModeSenseSBCDeviceSpecific_WriteProtectMask ) == 0 )
			{
			 	mediumIsProtected = false;
			}
			
			writeProtectDetermined = true;
			
		}
		
	}
	
	// Check if the write protect status has been successfully determined.
	if ( writeProtectDetermined == false )
	{
		
		// Either this device reports an ANSI version, or the 10 Byte command failed.
		// Try the six byte mode sense.	
		if ( MODE_SENSE_6 ( request,
							bufferDesc,
							0x00,
							0x00,	// Normally, we set DBD=1, but since we're only
							0x3F,	// interested in modeBuffer[3], we set DBD=0 since
							8,		// it makes legacy devices happier
							0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			if ( ( modeBuffer[2] & kModeSenseSBCDeviceSpecific_WriteProtectMask ) == 0 )
			{
				mediumIsProtected = false;
			}
			
		}
		
		else
		{
			
			ERROR_LOG ( ( "%s: Mode Sense failed\n", getName ( ) ) );
			
			// The mode sense failed, mark as write protected to be safe.
			mediumIsProtected = true;
			
		}
		
	}
	
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ErrorExit );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
Exit:
	
	
	return mediumIsProtected;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediumIcon - Sets an icon key in the registry if desired.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::SetMediumIcon ( void )
{
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::SetMediumIcon called\n" ) );
	
	// Methodology for setting icons
	// 1. Find out if an icon key is already present. If there is one, do
	// nothing since it might be from a plist entry. This makes it easy for
	// subclassers to set a media property for their device (if it is known
	// and can only be one media type - e.g. Zip) in their PB project, rather
	// than adding a method to their subclass.
	// 
	// 2. Find out if the device is on an internal bus or external bus and
	// what type of bus it is. If it is a bus for which we have a special
	// icon at this point in time, then we set that icon key. The icons are
	// located in the IOSCSIArchitectureModelFamily.kext/Contents/Resources
	// directory.
	//
	// We currently have icons for the following:
	//	¥ Firewire HD
	//	¥ Fibre Channel HD
	//	¥ USB HD
	//	¥ SuperDisk
	//	¥ MagnetoOptical
	//	¥ SmartMedia
	//	¥ MemoryStick
	//	¥ Floppy
	//
	// We plan to have the following icons supported once we get artwork/licensing:
	//	¥ CompactFlash
	//	¥ Clik!
	//	¥ Zip
	//	¥ Jaz
	
	// Step 1. Find out if a media icon is present
	if ( getProperty ( kIOMediaIconKey, gIOServicePlane ) == NULL )
	{
		
		OSDictionary *	dict = NULL;
		
		STATUS_LOG ( ( "No current icon key\n" ) );
		
		// Step 2. No icon is present, see if we can provide one.
		dict = GetProtocolCharacteristicsDictionary ( );
		if ( dict != NULL )
		{
			
			OSString *	protocolString 	= NULL;
			
			STATUS_LOG ( ( "Got Protocol Characteristics Dictionary\n" ) );

			protocolString = OSDynamicCast ( OSString, dict->getObject ( kIOPropertyPhysicalInterconnectTypeKey ) );
			if ( protocolString != NULL )
			{
				
				const char *	protocol = NULL;
				
				STATUS_LOG ( ( "Got Protocol string\n" ) );

				protocol = protocolString->getCStringNoCopy ( );
				if ( protocol != NULL )
				{
					
					OSString *	identifier		= NULL;
					OSString *	resourceFile	= NULL;
					
					STATUS_LOG ( ( "Protocol = %s\n", protocol ) );
					
					identifier 	= OSString::withCString ( kIOSCSIArchitectureBundleIdentifierKey );
					dict		= OSDictionary::withCapacity ( 2 );
					
					if ( fMediaIsRemovable == false )
					{
						
						// If the protocol is FireWire, it needs an icon.
						if ( strcmp ( protocol, "FireWire" ) == 0 )
						{
							
							resourceFile = OSString::withCString ( kFireWireHDIconKey );
							
						}
						
						// If the protocol is USB and media is not removable, it needs an icon.
						if ( strcmp ( protocol, "USB" ) == 0 )
						{
							
							resourceFile = OSString::withCString ( kUSBHDIconKey );
							
						}
						
						// If the protocol is FibreChannel, it needs an icon.
						if ( strcmp ( protocol, "Fibre Channel Interface" ) == 0 )
						{
							
							resourceFile = OSString::withCString ( kFibreChannelHDIconKey );
							
						}
						
					}
					
					// Do we have an icon to set?
					if ( resourceFile != NULL )
					{
						
						STATUS_LOG ( ( "Resource file is non-NULL\n" ) );

						// Make sure other resources are allocated
						if ( ( dict != NULL ) && ( identifier != NULL ) )
						{
							
							STATUS_LOG ( ( "Setting keys\n" ) );
							
							dict->setObject ( kCFBundleIdentifierKey, identifier );
							dict->setObject ( kIOBundleResourceFileKey, resourceFile );
							
							setProperty ( kIOMediaIconKey, dict );
							
						}
						
						resourceFile->release ( );
						
					}
					
					if ( dict != NULL )
					{
						
						dict->release ( );
						dict = NULL;
						
					}
					
					if ( identifier != NULL )
					{
						
						identifier->release ( );
						identifier = NULL;
						
					}
					
				}
				
			}
			
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetWriteCacheState - Gets the write cache state.				  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::GetWriteCacheState (
							IOMemoryDescriptor * 	buffer,
							UInt8					modePageControlValue )
{
	
	IOReturn				status			= kIOReturnError;
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request 		= NULL;
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ErrorExit, status = kIOReturnNoResources );
	
	if ( MODE_SENSE_6 ( request,
						buffer,
						0x00,
						modePageControlValue,
						kSBCModePageCachingCode,
						buffer->getLength ( ),
						0x00 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		if ( GetRequestedDataTransferCount ( request ) == GetRealizedDataTransferCount ( request ) )
		{
			status = kIOReturnSuccess;
		}
		
	}
	
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetWriteCacheState - Gets the write cache state.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::GetWriteCacheState ( bool * enabled )
{
	
	IOReturn					status 			= kIOReturnUnsupported;
	IOBufferMemoryDescriptor *	buffer			= NULL;
	SPCModeParameterHeader6 *	header			= NULL;
	SBCModePageCaching *		cachePage		= NULL;
	UInt8 *						bufferPtr		= NULL;
	UInt8						pageCode		= 0;
	UInt8						minSize			= 0;
	UInt8						pageSize		= 0;
	UInt8						blockDescLength	= 0;
	
	// There are a whole class of devices which do not like to be sent
	// any MODE_SENSE or MODE_SELECT commands which they don't implement.
	// Since removable devices don't usually have a cache, we restrict
	// the device set to fixed hard disks for the following calls.
	require ( ( fMediaIsRemovable == false ), ErrorExit );
	
	// Allocate the buffer.
	buffer = IOBufferMemoryDescriptor::withCapacity ( sizeof ( SPCModeParameterHeader6 ), kIODirectionIn );
	require_nonzero_action ( buffer, ErrorExit, status = kIOReturnNoResources );
	
	// Obtain the header.
	status = GetWriteCacheState ( buffer, kModePageControlCurrentValues );
	require_success ( status, ReleaseDescriptor );
	
	bufferPtr 	= ( UInt8 * ) buffer->getBytesNoCopy ( );
	header		= ( SPCModeParameterHeader6 * ) bufferPtr;
	
	// Save off the page size.
	pageSize = header->MODE_DATA_LENGTH + sizeof ( header->MODE_DATA_LENGTH );
	
	// Sanity check the block descriptor length.
	if ( ( header->BLOCK_DESCRIPTOR_LENGTH & 7 ) == 0 )
	{
		
		// Block descriptor is in fact a multiple of 8.
		// Assume we can trust it.
		blockDescLength = header->BLOCK_DESCRIPTOR_LENGTH;		
		
	}
	
	// Sanity check on buffer size.
	minSize = sizeof ( SPCModeParameterHeader6 ) + blockDescLength +
		offsetof ( SBCModePageCaching, DEMAND_READ_WRITE_RETENTION_PRIORITY );
	
	require_action ( ( pageSize >= minSize ), ReleaseDescriptor, status = kIOReturnError );
	
	// Is pageSize greater than it needs to be?
	pageSize = min ( pageSize, sizeof ( SPCModeParameterHeader6 ) + blockDescLength + sizeof ( SBCModePageCaching ) );
	
	buffer->release ( );
	
	// Allocate a buffer for data we need.
	buffer = IOBufferMemoryDescriptor::withCapacity ( pageSize, kIODirectionOutIn );
	require_nonzero_action ( buffer, ErrorExit, status = kIOReturnNoResources );
	
	bufferPtr 	= ( UInt8 * ) buffer->getBytesNoCopy ( );
	header		= ( SPCModeParameterHeader6 * ) bufferPtr;
	
	// Obtain the full page.
	status = GetWriteCacheState ( buffer, kModePageControlCurrentValues );
	require_success ( status, ReleaseDescriptor );
	
	cachePage = ( SBCModePageCaching * ) &bufferPtr[ sizeof ( SPCModeParameterHeader6 ) + blockDescLength ];
	pageCode = cachePage->header.PS_PAGE_CODE & kModePageFormat_PAGE_CODE_Mask;
	
	// Sanity check on returned data from drive.
	require_action ( ( pageCode == kSBCModePageCachingCode ), ReleaseDescriptor, status = kIOReturnError );
	
	// Find out if the WCE bit is set.
	*enabled = cachePage->flags & kSBCModePageCaching_WCE_Mask;
	
	status = kIOReturnSuccess;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( buffer, ErrorExit );
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetWriteCacheState - Sets the write cache state.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::SetWriteCacheState ( bool enabled )
{
	
	IOReturn					status 			= kIOReturnUnsupported;
	SCSIServiceResponse			serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request 		= NULL;
	IOBufferMemoryDescriptor *	buffer			= NULL;
	SPCModeParameterHeader6 *	header			= NULL;
	SBCModePageCaching *		cachePage		= NULL;
	UInt8 *						bufferPtr		= NULL;
	UInt8						pageCode		= 0;
	UInt8						minSize			= 0;
	UInt8						pageSize		= 0;
	UInt8						blockDescLength	= 0;
	
	// There are a whole class of devices which do not like to be sent
	// any MODE_SENSE or MODE_SELECT commands which they don't implement.
	// Since removable devices don't usually have a cache, we restrict
	// the device set to fixed hard disks for the following calls.
	require ( ( fMediaIsRemovable == false ), ErrorExit );
	
	// Allocate the buffer.
	buffer = IOBufferMemoryDescriptor::withCapacity ( sizeof ( SPCModeParameterHeader6 ), kIODirectionIn );
	require_nonzero ( buffer, ErrorExit );
	
	// Obtain the header.
	status = GetWriteCacheState ( buffer, kModePageControlCurrentValues );
	require_success ( status, ReleaseDescriptor );
	
	bufferPtr 	= ( UInt8 * ) buffer->getBytesNoCopy ( );
	header		= ( SPCModeParameterHeader6 * ) bufferPtr;

	// Save off the page size.
	pageSize = header->MODE_DATA_LENGTH + sizeof ( header->MODE_DATA_LENGTH );
	
	// Sanity check the block descriptor length.
	if ( ( header->BLOCK_DESCRIPTOR_LENGTH & 7 ) == 0 )
	{
		
		// Block descriptor is in fact a multiple of 8. Assume we can trust it.
		blockDescLength = header->BLOCK_DESCRIPTOR_LENGTH;		
		
	}
	
	// Sanity check on buffer size.
	minSize = sizeof ( SPCModeParameterHeader6 ) + blockDescLength +
		offsetof ( SBCModePageCaching, DEMAND_READ_WRITE_RETENTION_PRIORITY );
	
	require_action ( ( pageSize >= minSize ), ReleaseDescriptor, status = kIOReturnError );
	
	// Can't truncate pageSize like we do for MODE_SENSE command since MODE_SELECT requires full data
	// buffer including header, block descriptors, and full mode page.
	
	buffer->release ( );
	
	// Allocate a buffer for full page size.
	buffer = IOBufferMemoryDescriptor::withCapacity ( pageSize, kIODirectionOutIn );
	require_nonzero_action ( buffer, ErrorExit, status = kIOReturnNoResources );
	
	bufferPtr 	= ( UInt8 * ) buffer->getBytesNoCopy ( );
	header		= ( SPCModeParameterHeader6 * ) bufferPtr;
	
	// Obtain the full page.
	status = GetWriteCacheState ( buffer, kModePageControlCurrentValues );
	require_success ( status, ReleaseDescriptor );
	
	cachePage = ( SBCModePageCaching * ) &bufferPtr[ sizeof ( SPCModeParameterHeader6 ) + blockDescLength ];
	pageCode = cachePage->header.PS_PAGE_CODE & kModePageFormat_PAGE_CODE_Mask;
	
	// Sanity check on returned data from drive.
	require_action ( ( pageCode == kSBCModePageCachingCode ), ReleaseDescriptor, status = kIOReturnError );
	
	// Set the page code in the buffer to the cache page code (remove the PS bit).
	cachePage->header.PS_PAGE_CODE = kSBCModePageCachingCode;
	
	// Enable/disable cache as appropriate.
	if ( enabled == true )
		cachePage->flags |= kSBCModePageCaching_WCE_Mask;
	else
		cachePage->flags &= ~kSBCModePageCaching_WCE_Mask;
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ReleaseDescriptor, status = kIOReturnNoResources );
	
	// Write back out the new settings.
	if ( MODE_SELECT_6 ( request,
						 buffer,
						 0x01,	// PageFormat		= 1
						 0x00,	// SaveParameters	= 0
						 pageSize,
						 0x00 ) == true )
	{
		
		// The command was successfully built, now send it.
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			fWriteCacheEnabled = enabled;
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			
			status = kIOReturnError;
			
		}
		
	}
	
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( buffer, ErrorExit );
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:	
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PollForMediaRemoval - Polls for media removal.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::PollForMediaRemoval ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	// Do a TEST_UNIT_READY to generate sense data
	if ( TEST_UNIT_READY ( request, 0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			bool						validSense 	= false;
			SCSI_Sense_Data				senseBuffer	= { 0 };
			IOMemoryDescriptor *		bufferDesc	= NULL;
			
			validSense = GetAutoSenseData ( request, &senseBuffer );
			if ( validSense == false )
			{
				
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				require_nonzero ( bufferDesc, ReleaseTask );
				
				if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
				{
					
					// The command was successfully built, now send it
					serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
					
				}
				
				bufferDesc->release ( );
				bufferDesc = NULL;
				
				require ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ), ReleaseTask );
				require ( ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ), ReleaseTask );
				
			}
			
			// Check the sense data to see if media is no longer present ( ASC == 0x3A )
			// or if media has changed ( ASC==0x28, ASCQ==0x00 )
			if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) ||
			   ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) &&
				 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ) )
			{
				
				ERROR_LOG ( ( "Media was removed. Tearing down the media object." ) );
				
				// Media was removed, set the polling to determine when new media has been inserted
				fPollingMode = kPollingMode_NewMedia;
				
				// Message up the chain that we do not have media
				messageClients ( kIOMessageMediaStateHasChanged,
								 ( void * ) kIOMediaStateOffline );
				
				ResetMediumCharacteristics ( );
				EnablePolling ( );
				
			}
			
		}
		
	}
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ErrorExit );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - DEPRECATED.										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::IssueRead (
							IOMemoryDescriptor *	buffer,
							UInt64					startBlock,
							UInt64					blockCount )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - Issues an asynchronous read command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::IssueRead (
							IOMemoryDescriptor *	buffer,
							UInt64					startBlock,
							UInt64					blockCount,
							void *					clientData )
{
	
	IOReturn 				status		= kIOReturnNoResources;
	SCSITaskIdentifier		request		= NULL;
	bool					cmdStatus   = false;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( startBlock > kREPORT_CAPACITY_MaximumLBA )
	{

		cmdStatus = READ_16 ( request,
					buffer,
					fMediumBlockSize,
					0,
					0,
					0,
					0,
					( SCSICmdField8Byte ) startBlock,
					( SCSICmdField4Byte ) blockCount,
					0,
					0 );
								
	}
	
	else
	{

		cmdStatus = READ_10 ( request,
					buffer,
					fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 );

	}
	
	if ( cmdStatus == true )
	{
		
		// The command was successfully built, now send it
		SetApplicationLayerReference ( request, clientData );
		
		// Tag the command if requested
		if ( GetCMDQUE ( ) == true )
		{
			
			SetTaskAttribute ( request, kSCSITask_SIMPLE );
			SetTaggedTaskIdentifier ( request, GetUniqueTagID ( ) );
			
		}

		SendCommand ( request,
					  fReadTimeoutDuration,
					  &IOSCSIBlockCommandsDevice::AsyncReadWriteComplete );
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		ReleaseSCSITask ( request );
		request	= NULL;
		status	= kIOReturnBadArgument;
		
	}

ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueWrite - DEPRECATED.										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::IssueWrite ( IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueWrite - Issues an asynchronous write command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::IssueWrite (
						IOMemoryDescriptor *	buffer,
						UInt64					startBlock,
						UInt64					blockCount,
						void *					clientData )
{
	IOReturn				status		= kIOReturnNoResources;
	SCSITaskIdentifier		request		= NULL;
	bool					cmdStatus   = false;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( startBlock > kREPORT_CAPACITY_MaximumLBA )
	{
		
		cmdStatus = WRITE_16 ( request,
					buffer,
					fMediumBlockSize,
					0,
					0,
					0,
					0,
					( SCSICmdField8Byte ) startBlock,
					( SCSICmdField4Byte ) blockCount,
					0,
					0 );
		
	}	
	
	else
	{

		cmdStatus = WRITE_10 ( request,
					buffer,
					fMediumBlockSize,
					0,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 );

	}
	
	if ( cmdStatus == true )
	{
		
		// The command was successfully built, now send it
		SetApplicationLayerReference ( request, clientData );
		
		// Tag the command if requested
		if ( GetCMDQUE ( ) == true )
		{
			
			SetTaskAttribute ( request, kSCSITask_SIMPLE );
			SetTaggedTaskIdentifier ( request, GetUniqueTagID ( ) );
			
		}
		
		SendCommand ( request,
					  fWriteTimeoutDuration,
					  &IOSCSIBlockCommandsDevice::AsyncReadWriteComplete );
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		ReleaseSCSITask ( request );
		request	= NULL;
		status	= kIOReturnBadArgument;
		
	}	
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteCompletion - Completion routine for read/write requests.
//															 		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::AsyncReadWriteCompletion (
								SCSITaskIdentifier completedTask )
{
	
	IOReturn	status		= kIOReturnSuccess;
	UInt64		actCount	= 0;
	void *		clientData	= NULL;
	
	// Extract the client data from the SCSITaskIdentifier
	clientData = GetApplicationLayerReference ( completedTask );
	require_nonzero ( clientData, ErrorExit );
	
	if ( ( GetServiceResponse ( completedTask ) == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( completedTask ) == kSCSITaskStatus_GOOD ) )
	{
		
		// Our status is good, so return a success
		actCount = GetRealizedDataTransferCount ( completedTask );
		
	}
	
	else
	{
		
		// Set a generic IO error for starters
		status = kIOReturnIOError;
		
		if ( GetServiceResponse ( completedTask ) == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			// We have a status other than GOOD, see why.		
			if ( GetTaskStatus ( completedTask ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				SCSI_Sense_Data		senseDataBuffer = { 0 };
				bool				senseIsValid	= false;
				
				senseIsValid = GetAutoSenseData ( completedTask, &senseDataBuffer, sizeof ( senseDataBuffer ) );
				if ( senseIsValid )
				{
					
					// Check if this is a recovered error and the amount of data transferred
					// was the amount requested. If so, don't treat those as hard errors.
					if ( ( ( senseDataBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_RECOVERED_ERROR ) &&
						 ( GetRequestedDataTransferCount ( completedTask ) == GetRealizedDataTransferCount ( completedTask ) ) )
					{
						
						IOLog ( "READ or WRITE succeeded, but recoverable (soft) error occurred, SENSE_KEY = 0x%01x, ASC = 0x%02x, ASCQ = 0x%02x, LBA = 0x%08x\n",
								senseDataBuffer.SENSE_KEY & kSENSE_KEY_Mask,
								senseDataBuffer.ADDITIONAL_SENSE_CODE,
								senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER,
								OSReadBigInt32 ( &senseDataBuffer.INFORMATION_1, 0 ) );
						
						status 		= kIOReturnSuccess;
						actCount 	= GetRealizedDataTransferCount ( completedTask );
						
					}
					
					else
					{
						
						ERROR_LOG ( ( "READ or WRITE failed, ASC = 0x%02x, ASCQ = 0x%02x\n",
						senseDataBuffer.ADDITIONAL_SENSE_CODE,
						senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
						
					}
					
				}
				
			}
	
		}
		
	}
	
	IOBlockStorageServices::AsyncReadWriteComplete ( clientData, status, actCount );
	
	ReleaseSCSITask ( completedTask );	
	
	
ErrorExit:
	
	
	return;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteComplete - 	Static completion routine for
//								read/write requests.		  [STATIC][PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::AsyncReadWriteComplete (
								SCSITaskIdentifier request )
{
	
	IOSCSIBlockCommandsDevice *	taskOwner = NULL;
	
	require_nonzero ( request, ErrorExit );
	
	taskOwner = OSDynamicCast ( IOSCSIBlockCommandsDevice, sGetOwnerForTask ( request ) );
	require_nonzero ( taskOwner, ErrorExit );
	
	taskOwner->AsyncReadWriteCompletion ( request );
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sProcessPoll - Static method called to poll for media.
//															[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::sProcessPoll ( void * pdtDriver, void * refCon )
{
	
	IOSCSIBlockCommandsDevice *	driver = NULL;
	
	driver = ( IOSCSIBlockCommandsDevice * ) pdtDriver;
	require_nonzero ( driver, ErrorExit );
	
	driver->ProcessPoll ( );
	
	if ( driver->fPollingMode != kPollingMode_Suspended )
	{
		
		// schedule the poller again
		driver->EnablePolling ( );
		
	}
	
	// drop the retain associated with this poll
	driver->release ( );
	
	
ErrorExit:
	
	
	return;
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


OSMetaClassDefineReservedUsed ( IOSCSIBlockCommandsDevice, 1 );	/* PowerDownHandler */
OSMetaClassDefineReservedUsed ( IOSCSIBlockCommandsDevice, 2 );	/* SetMediumIcon 	*/
OSMetaClassDefineReservedUsed ( IOSCSIBlockCommandsDevice, 3 );	/* AsyncReadWriteCompletion	*/

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  4 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  5 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  6 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  7 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  8 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  9 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 10 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 11 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 12 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 16 );