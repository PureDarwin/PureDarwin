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

// Libkern includes
#include <libkern/OSByteOrder.h>

// Generic IOKit headers
#include <IOKit/IOKitKeys.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOBlockStorageDriver.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/SCSICommandDefinitions.h>
#include <IOKit/scsi/IOReducedBlockServices.h>

#include "IOSCSIReducedBlockCommandsDevice.h"
#include "SCSIReducedBlockCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"RBC"

#if DEBUG
#define SCSI_RBC_DEVICE_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClass ( IOSCSIReducedBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );
OSDefineAbstractStructors ( IOSCSIReducedBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kMaxRetryCount 						8
#define kCapacityDataBufferSize				8
#define kModeSenseWriteProtectBufferSize	17
#define kWriteProtectMask					0x04
#define kAppleKeySwitchProperty				"AppleKeyswitch"
#define kFibreChannelHDIconKey				"FibreChannleHD.icns"
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
IOSCSIReducedBlockCommandsDevice::SyncReadWrite (
					IOMemoryDescriptor *	buffer,
					UInt64					startBlock,
					UInt64					blockCount )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWrite - 	Translates an asynchronous I/O request into a
//						read or a write.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::AsyncReadWrite (
					IOMemoryDescriptor *	buffer,
					UInt64					startBlock,
					UInt64					blockCount,
					void *					clientData )
{
	
	IODirection		direction;
	IOReturn		status = kIOReturnBadArgument;
	
	direction = buffer->getDirection ( );
	
	if ( direction == kIODirectionIn )
	{
		
		status = IssueRead ( buffer, startBlock, blockCount, clientData );
		
	}
	
	else if ( direction == kIODirectionOut )
	{
		
		status = IssueWrite ( buffer, startBlock, blockCount, clientData );
		
	}
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EjectTheMedia - 	Unlocks and ejects the media if it is removable. If it
//						is not removable, it synchronizes the write cache.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::EjectTheMedia ( void )
{
	
	IOReturn				status				= kIOReturnNoResources;
	SCSIServiceResponse		serviceResponse		= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request				= NULL;
	bool					doPollForRemoval	= false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
	
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
		
	if ( fMediaIsRemovable == false )
	{
		
		if ( getProperty ( "Power Off" ) != NULL )
		{
			
			// Spin down the media now. We use to queue up a power change but we found
			// that since the PM stuff happens at a deferred point, the machine might shutdown
			// before we finish the PM change. So, we do the spindown now, then sync with PM
			// so it knows the state.
			if ( fCurrentPowerState > kRBCPowerStateSleep )
			{
				
				request = GetSCSITask ( );
				require_nonzero ( request, ErrorExit );
				
				// At a minimum, make sure the drive is spun down
				if ( START_STOP_UNIT ( request, 1, 0, 0, 0 ) == true )
				{
					
					serviceResponse = SendCommand ( request, 0 );
					
				}
				
				// Give the drive some time to park the heads.
				IOSleep ( 500 );
				
				ReleaseSCSITask ( request );
				request = NULL;
				
			}
			
			fCurrentPowerState = kRBCPowerStateSleep;
			
		}
		
		// Sync ourselves with PM.
		changePowerStateToPriv ( kRBCPowerStateSleep );
		
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
			if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateUnlocked ) == true )
			{
				
				// The command was successfully built, now send it
				( void ) SendCommand ( request, kTenSecondTimeoutInMS );
				
			}
			
			// Eject it.
			if ( START_STOP_UNIT ( request, 0, 0, 1, 0 ) == true )
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
		
		// Reset the media characteristics to known defaults and enable polling
		ResetMediaCharacteristics ( );
		fMediaIsWriteProtected = true;
	
		if ( ( doPollForRemoval == true ) || ( fMediumRemovalPrevented == false ) )
		{
			
			//¥¥¥ Add code here to put up the dialog "It is now safe to remove
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
//	¥ FormatMedia - Unsupported.									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::FormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn	status = kIOReturnUnsupported;
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetFormatCapacities - Unsupported.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIReducedBlockCommandsDevice::GetFormatCapacities (
							UInt64 * capacities,
							UInt32   capacitiesMaxCount ) const
{
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LockUnlockMedia - Unsupported.								   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::LockUnlockMedia ( bool doLock )
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
IOSCSIReducedBlockCommandsDevice::SynchronizeCache ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	IOReturn				status 			= kIOReturnNoResources;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( SYNCHRONIZE_CACHE ( request ) == true )
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
		
		status = kIOReturnInternalError;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportBlockSize - Reports the medium block size.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportBlockSize ( UInt64 * blockSize )
{
	
	*blockSize = fMediaBlockSize;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportEjectability - Reports the medium ejectability characteristic.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportEjectability ( bool * isEjectable )
{
	
	*isEjectable = fMediaIsRemovable;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportLockability - Reports the medium lockability characteristic.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportLockability ( bool * isLockable )
{
	
	*isLockable = true;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportPollRequirements - Reports polling requirements (none).
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportPollRequirements (
									bool * pollIsRequired,
									bool * pollIsExpensive )
{
	
	// Since we have our own polling code, we do not need to
	// have the Storage Family poll us for media changes. We use
	// asynchronous media notifications instead. We have custom
	// polling code because there are several manual eject devices
	// which fail the PREVENT_ALLOW_MEDIUM_REMOVAL command and
	// we must continue to poll them for unexpected media removal.
	*pollIsRequired 	= false;
	*pollIsExpensive 	= false;
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMaxReadTransfer - Reports maximum read transfer in bytes.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportMaxReadTransfer (
									UInt64		blockSize,
									UInt64 * 	max )
{
	
	UInt32	maxBlockCount 	= kDefaultMaxBlocksPerIO;
	UInt64	maxByteCount	= 0;
	bool	supported		= false;
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::ReportMaxReadTransfer.\n" ) );
	
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
		
		if ( fMediaBlockSize > 0 )
			maxBlockCount = min ( maxBlockCount, ( maxByteCount / fMediaBlockSize ) );
		
	}
	
	setProperty ( kIOMaximumBlockCountReadKey, maxBlockCount, 64 );
	
	*max = maxBlockCount * blockSize;
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMaxWriteTransfer - Reports maximum write transfer in bytes.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportMaxWriteTransfer (
									UInt64		blockSize,
									UInt64 *	max )
{
	
	UInt32	maxBlockCount 	= kDefaultMaxBlocksPerIO;
	UInt64	maxByteCount	= 0;
	bool	supported		= false;
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::ReportMaxWriteTransfer.\n" ) );
	
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
		
		if ( fMediaBlockSize > 0 )
			maxBlockCount = min ( maxBlockCount, ( maxByteCount / fMediaBlockSize ) );
		
	}
	
	setProperty ( kIOMaximumBlockCountWriteKey, maxBlockCount, 64 );
	
	*max = maxBlockCount * blockSize;
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMaxValidBlock - Reports maximum valid block on the media.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportMaxValidBlock ( UInt64 * maxBlock )
{
	
	*maxBlock = fMediaBlockCount - 1;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediaState - Reports state of media in the device		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportMediaState (
									bool *	mediaPresent,
									bool *	changed )
{
	
	*mediaPresent 	= fMediaPresent;
	*changed 		= fMediaChanged;
	
	if ( fMediaChanged )
	{
		
		fMediaChanged = !fMediaChanged;
		
	}
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportRemovability - Reports removability characteristic of media
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportRemovability ( bool * isRemovable )
{
	
	*isRemovable = fMediaIsRemovable;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportWriteProtection - Reports write protection characteristic of media
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::ReportWriteProtection (
										bool * isWriteProtected )
{
	
	*isWriteProtected = fMediaIsWriteProtected;
	return kIOReturnSuccess;
	
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
IOSCSIReducedBlockCommandsDevice::InitializeDeviceSupport ( void )
{
	
	bool	setupSuccessful = false;
	
	// Initialize the medium characteristics
	fMediaChanged			= false;
	fMediaPresent			= false;
	fMediaIsRemovable 		= false;
	fMediaIsWriteProtected 	= true;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	fIOSCSIReducedBlockCommandsDeviceReserved =
			IONew ( IOSCSIReducedBlockCommandsDeviceExpansionData, 1 );
	require_nonzero ( fIOSCSIReducedBlockCommandsDeviceReserved, ErrorExit );
	
	// Initialize these after we have allocated fIOSCSIReducedBlockCommandsDeviceReserved
	fMediumRemovalPrevented	= false;
	fKnownManualEject		= false;
	
	// Grab any device information from the IORegistry
	if ( getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) != NULL )
	{
		
		// There is a characteristics property for this device, check for known entries.
		OSDictionary * characterDict = NULL;
		
		characterDict = OSDynamicCast ( OSDictionary,
						getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) );
		
		// Check if the personality for device specifies it is known to be manual ejectable.
		if ( characterDict->getObject ( kIOPropertySCSIManualEjectKey ) != NULL )
		{
			
			STATUS_LOG ( ( "%s: found a Manual Eject property.\n", getName ( ) ) );
			fKnownManualEject = true;
			
		}
		
	}
	
	// Make sure the drive is ready for us!
	require ( ClearNotReadyStatus ( ), ReleaseReservedMemory );
	
	setupSuccessful = DetermineDeviceCharacteristics ( );
	
	if ( setupSuccessful == true ) 
	{		
		
		fPollingMode = kPollingMode_NewMedia;
		fPollingThread = thread_call_allocate (
				( thread_call_func_t ) IOSCSIReducedBlockCommandsDevice::sPollForMedia,
				( thread_call_param_t ) this );
		
		require_nonzero_action_string ( fPollingThread,
										ErrorExit,
										setupSuccessful = false,
										"fPollingThread allocation failed.\n" );
		
		InitializePowerManagement ( GetProtocolDriver ( ) );
		
	}
	
	STATUS_LOG ( ( "%s::%s setupSuccessful = %d\n", getName ( ),
					__FUNCTION__, setupSuccessful ) );
	
	SetMediaCharacteristics ( 0, 0 );
	
	return setupSuccessful;
	
	
ReleaseReservedMemory:
	
	
	require_nonzero_quiet ( fIOSCSIReducedBlockCommandsDeviceReserved, ErrorExit );
	IODelete ( fIOSCSIReducedBlockCommandsDeviceReserved, IOSCSIReducedBlockCommandsDeviceExpansionData, 1 );
	fIOSCSIReducedBlockCommandsDeviceReserved = NULL;
	
	
ErrorExit:
	
	
	return setupSuccessful;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StartDeviceSupport - Starts device support					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOSCSIReducedBlockCommandsDevice::StartDeviceSupport ( void )
{
	
	// Start polling
	EnablePolling ( );
	
	CreateStorageServiceNub ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SuspendDeviceSupport - Suspends device support				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOSCSIReducedBlockCommandsDevice::SuspendDeviceSupport ( void )
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
IOSCSIReducedBlockCommandsDevice::ResumeDeviceSupport ( void )
{
	
	if ( fMediaPresent == false )
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
IOSCSIReducedBlockCommandsDevice::StopDeviceSupport ( void )
{
	DisablePolling ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TerminateDeviceSupport - Terminates device support			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::TerminateDeviceSupport ( void )
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
IOSCSIReducedBlockCommandsDevice::CreateCommandSetObjects ( void )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	fSCSIReducedBlockCommandObject =
		SCSIReducedBlockCommands::CreateSCSIReducedBlockCommandObject ( );
	require_nonzero ( fSCSIReducedBlockCommandObject, ErrorExit );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FreeCommandSetObjects - Releases command set objects			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::FreeCommandSetObjects ( void )
{
	
	if ( fSCSIReducedBlockCommandObject != NULL ) 
	{
		
		fSCSIReducedBlockCommandObject->release ( );
		fSCSIReducedBlockCommandObject = NULL;
  		
	}
	
	// Release the reserved structure.
	if ( fIOSCSIReducedBlockCommandsDeviceReserved != NULL )
	{
		
		IODelete ( fIOSCSIReducedBlockCommandsDeviceReserved, IOSCSIReducedBlockCommandsDeviceExpansionData, 1 );
		fIOSCSIReducedBlockCommandsDeviceReserved = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIReducedBlockCommandObject - Accessor method			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIReducedBlockCommands *
IOSCSIReducedBlockCommandsDevice::GetSCSIReducedBlockCommandObject ( void )
{
	
	check ( fSCSIReducedBlockCommandObject );
	return fSCSIReducedBlockCommandObject;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIPrimaryCommandObject - Accessor method					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIPrimaryCommands	*
IOSCSIReducedBlockCommandsDevice::GetSCSIPrimaryCommandObject ( void )
{
	
	check ( fSCSIReducedBlockCommandObject );
	return OSDynamicCast ( SCSIPrimaryCommands,
						   GetSCSIReducedBlockCommandObject ( ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearNotReadyStatus - Clears any NOT_READY status on device	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::ClearNotReadyStatus ( void )
{
	
	SCSI_Sense_Data				senseBuffer 	= { 0 };
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
		
		if ( TEST_UNIT_READY ( request ) == true )
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
					
					if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize ) == true )
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
					
					if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) && 
						    ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
					{
						
						STATUS_LOG ( ( "%s::logical unit not ready\n", getName ( ) ) );
						driveReady = false;
						IOSleep ( 200 );
						
					}
					
					else if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY ) &&
								( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
						   		( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x01 ) )
					{
						
						STATUS_LOG ( ( "%s::drive not ready\n", getName ( ) ) );
						driveReady = false;
						IOSleep ( 200 );
						
					}
					
					else if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) && 
								( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
								( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) ) 
					{
						
						// The drive needs to be spun up. Issue a START_STOP_UNIT to it.
						if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01 ) == true )
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
								( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ),
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
			
			// Command failed. Wait and try again.
			IOSleep ( 200 );
			
		}
	
	// Check isInactive in case device was hot unplugged during sleep
	// and we are in a possible infinite loop here.
	} while ( ( driveReady == false ) && ( isInactive ( ) == false ) );
	
	bufferDesc->release ( );
	ReleaseSCSITask ( request );
	
	result = isInactive ( ) ? false : true;
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EnablePolling - Schedules the polling thread to run			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::EnablePolling ( void )
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
IOSCSIReducedBlockCommandsDevice::DisablePolling ( void )
{		
	
	fPollingMode = kPollingMode_Suspended;
	
	// Cancel the thread if it is scheduled to run
	require ( thread_call_cancel ( fPollingThread ), Exit );
	
	// It was scheduled to run, so we balance out the retain()
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
IOSCSIReducedBlockCommandsDevice::DetermineDeviceCharacteristics ( void )
{
	
	SCSIServiceResponse				serviceResponse		= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8							inquiryBufferCount	= 0;
	SCSICmd_INQUIRY_StandardData * 	inquiryBuffer		= NULL;
	IOMemoryDescriptor *			bufferDesc			= NULL;
	SCSITaskIdentifier				request				= NULL;
	bool							succeeded			= false;
	UInt8							loop				= 0;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	if ( fDefaultInquiryCount == 0 )
	{
		
		// There is no default Inquiry count for this device, use the standard
		// structure size.
		STATUS_LOG ( ( "%s: use sizeof(SCSICmd_INQUIRY_StandardData) for Inquiry.\n", getName ( ) ) );
		inquiryBufferCount = sizeof ( SCSICmd_INQUIRY_StandardData );
		
	}
	
	else
	{
		
		// This device has a default inquiry count, use it.
		STATUS_LOG ( ( "%s: use fDefaultInquiryCount for Inquiry.\n", getName ( ) ) );
		inquiryBufferCount = fDefaultInquiryCount;
		
	}
	
	inquiryBuffer = ( SCSICmd_INQUIRY_StandardData * ) IOMalloc ( inquiryBufferCount );
	require_nonzero_string ( inquiryBuffer, ErrorExit,
							 "Couldn't allocate INQUIRY buffer" );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	inquiryBuffer,
													inquiryBufferCount,
													kIODirectionIn );
	
	require_nonzero_string ( bufferDesc, ReleaseBuffer,
							 "Couldn't allocate INQUIRY memory descriptor" );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	// Loop for a given number of retries in order to make sure the INQUIRY command succeeds.
	for ( loop = 0; ( ( loop < kMaxRetryCount ) && ( isInactive ( ) == false ) ); loop++ )
	{
		
		if ( INQUIRY ( 	request,
						bufferDesc,
						0,
						0,
						0x00,
						inquiryBufferCount ) == true )
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
		
	}
	
	require ( succeeded, ReleaseTask );
	
	// Save ANSI version of the device
	SetANSIVersion ( inquiryBuffer->VERSION & kINQUIRY_ANSI_VERSION_Mask );
	
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
	
	
ReleaseTask:
	
	
	require_nonzero ( request, ReleaseDescriptor );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero ( bufferDesc, ReleaseBuffer );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseBuffer:
	
		
	require_nonzero ( inquiryBuffer, ErrorExit );
	IOFree ( ( void * ) inquiryBuffer, inquiryBufferCount );
	inquiryBuffer = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "%s::%s succeeded = %d\n", getName ( ), __FUNCTION__, succeeded ) );
	
	return succeeded;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediaCharacteristics - Sets media characteristics			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::SetMediaCharacteristics (
									UInt32	blockSize,
									UInt32	blockCount )
{
	
	UInt64		maxBytesRead	= 0;
	UInt64		maxBytesWrite	= 0;
	
	STATUS_LOG ( ( "mediaBlockSize = %ld, blockCount = %ld\n",
					blockSize, blockCount ) );
	
	fMediaBlockSize		= blockSize;
	fMediaBlockCount	= blockCount;
	
	ReportMaxReadTransfer  ( fMediaBlockSize, &maxBytesRead );
	ReportMaxWriteTransfer ( fMediaBlockSize, &maxBytesWrite );
		
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResetMediaCharacteristics - Resets media characteristics to known values
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::ResetMediaCharacteristics ( void )
{
	
	fMediaBlockSize			= 0;
	fMediaBlockCount		= 0;
	fMediaPresent			= false;
	fMediaIsWriteProtected 	= true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateStorageServiceNub - Creates the linkage object for IOStorageFamily
//								to use.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::CreateStorageServiceNub ( void )
{
	
	IOService * 	nub = NULL;
	
	nub = OSTypeAlloc ( IOReducedBlockServices );
	require_nonzero ( nub, ErrorExit );
	
	nub->init ( );
	require ( nub->attach ( this ), ErrorExit );
	
	nub->registerService ( );
	nub->release ( );
	
	return;
	
	
ErrorExit:
	
	
	PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::CreateStorageServiceNub failed" ) );
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PollForMedia - Polls for media insertion.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::PollForMedia ( void )
{
	
	IOMemoryDescriptor *	bufferDesc		= NULL;
	SCSITaskIdentifier		request			= NULL;
	
	switch ( fPollingMode )
	{
		
		case kPollingMode_NewMedia:
		{
			
			SCSI_Sense_Data			senseBuffer		= { 0 };
			UInt32					capacityData[2]	= { 0 };
			bool					mediaFound 		= false;
			bool					validSense		= false;
			SCSIServiceResponse		serviceResponse;
			
			OSBoolean *				keySwitchLocked = NULL;
			
			keySwitchLocked = OSDynamicCast (
									OSBoolean,
									getProperty ( kAppleKeySwitchProperty ) );
			
			if ( keySwitchLocked != NULL )
			{
				
				// See if we should poll for media.
				if ( keySwitchLocked->isTrue ( ) )
				{
					break;
				}
				
			}
			
			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
			
			bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
															kSenseDefaultSize,
															kIODirectionIn );
			
			require_nonzero ( bufferDesc, ErrorExit );
			request = GetSCSITask ( );
			require_nonzero ( request, ReleaseDescriptor );
			
			// Do a TEST_UNIT_READY to generate sense data
			if ( TEST_UNIT_READY ( request ) == true )
			{
				
				// The command was successfully built, now send it
				serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
				
			}
			
			require ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE, ReleaseTask );
			
			if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				validSense = GetAutoSenseData ( request, &senseBuffer );
				if ( validSense == true )
				{
					
					if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x00 ) && 
						 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
					{
						
						mediaFound = true;
						
					}
					
				}
				
			}
				
			else
			{
				
				mediaFound = true;
				
			}	
			
			bufferDesc->release ( );
			bufferDesc = NULL;
			
			require_quiet ( mediaFound, ReleaseTask );
			
			// We have found media. If it is removable, lock it down.
			if ( fMediaIsRemovable == true )
			{
				
				fMediumRemovalPrevented = false;
				
				// Lock removable media
				if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateLocked ) == true )
				{
					
					// The command was successfully built, now send it
					serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
					
					if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
						 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
					{
						
						fMediumRemovalPrevented = true;
						
					}
					
				}
				
			}
			
			else
			{
				
				fMediumRemovalPrevented = true;
				
			}
			
			bufferDesc = IOMemoryDescriptor::withAddress ( 	capacityData,
															kCapacityDataBufferSize,
															kIODirectionIn );
			
			require_nonzero ( bufferDesc, ReleaseTask );
			
			// We found media, Get its capacity
			if ( READ_CAPACITY ( request, bufferDesc ) == true )
			{
				
				// The command was successfully built, now send it
				serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
				
			}
			
			require_string ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
				 			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ),
				 			 ReleaseTask,
				 			 "Read Capacity failed\n" );
			
			SetMediaCharacteristics (
					OSSwapBigToHostInt32 ( capacityData[1] ),
					OSSwapBigToHostInt32 ( capacityData[0] ) + 1 );
			
			STATUS_LOG ( ( "%s: Media capacity: %x and block size: %x\n",
							getName ( ), fMediaBlockCount, fMediaBlockSize ) );
			
			CheckWriteProtection ( );
			
			fMediaPresent	= true;
			fMediaChanged	= true;
			fPollingMode	= kPollingMode_Suspended;
			
			SetMediaIcon ( );
			
			// Message up the chain that we have media
			messageClients ( kIOMessageMediaStateHasChanged,
							 ( void * ) kIOMediaStateOnline );
			
			if ( fMediumRemovalPrevented == false )
			{
				
				// Media is not locked into the drive, so this is most likely
				// a manually ejectable device, start polling for media removal.
				fPollingMode = kPollingMode_MediaRemoval;
				
			}
			
		}
		break;
		
		case kPollingMode_MediaRemoval:
		{

			SCSIServiceResponse		serviceResponse;
			
			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
			
			request = GetSCSITask ( );
			require_nonzero ( request, ErrorExit );
			
			// Generate some sense data
			if ( TEST_UNIT_READY ( request ) == true )
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
					
					validSense = GetAutoSenseData ( request, &senseBuffer );
					if ( validSense == false )
					{
						
						bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																		kSenseDefaultSize,
																		kIODirectionIn );
						require_nonzero ( bufferDesc, ReleaseTask );
						
						if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize ) == true )
						{
							
							// The command was successfully built, now send it
							serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
							
						}
						
						bufferDesc->release ( );
						bufferDesc = NULL;
						
						require ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ), 
									ReleaseTask );
						
						require ( ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ),
									ReleaseTask );
						
					}
					
					// Check the sense data to see if media is no longer present ( ASC == 0x3A )
					// or if media has changed ( ASC==0x28, ASCQ==0x00 )
					if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) ||
					   ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) &&
						 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ) )
					{
						
						ERROR_LOG ( ( "Media was removed. Tearing down the media object." ) );
						
						// Media was removed, set the polling to determine when new
						// media has been inserted
						fPollingMode = kPollingMode_NewMedia;
						
						// Message up the chain that we do not have media
						messageClients ( kIOMessageMediaStateHasChanged,
										( void * ) kIOMediaStateOffline );
						
						ResetMediaCharacteristics ( );
						EnablePolling ( );
						
					}
					
				}
				
			}
			
		}
		break;
		
		default:
		{
			
			// This is an unknown polling mode -- do nothing.
			ERROR_LOG ( ( "%s:ProcessPoll Unknown polling mode.\n", getName ( ) ) );
			
		}
		break;
	}


ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseDescriptor );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CheckWriteProtection - Checks media write protect state.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::CheckWriteProtection ( void )
{
	
	UInt8					modeSenseBuffer[kModeSenseWriteProtectBufferSize] = { 0 };
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *	bufferDesc		= NULL;
	SCSITaskIdentifier		request			= NULL;
	SCSICmdField1Bit		DBD				= 0;
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	modeSenseBuffer,
													kModeSenseWriteProtectBufferSize,
													kIODirectionIn );
	
	require_nonzero ( bufferDesc, ErrorExit );
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	
	DBD = 1;	/* Disable block descriptors */
	
	
Loop:


	if ( MODE_SENSE_6 ( request,
						bufferDesc,
						DBD,
						0x00,
						0x06,
						kModeSenseWriteProtectBufferSize ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "%s: Returned Mode sense data: ", getName ( ) ) );
		
		#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >=3 )
			for ( UInt32 i = 0; i < kModeSenseWriteProtectBufferSize; i++ )
			{
				STATUS_LOG ( ( "%x: ", modeSenseBuffer[i] ) );
			}
	
			STATUS_LOG ( ( "\n" ) );
		#endif // DEBUG
		
		if ( ( modeSenseBuffer[15] & kWriteProtectMask ) != 0 )
		{
			
			fMediaIsWriteProtected = true;
			
		}
		
		else
		{
			
			fMediaIsWriteProtected = false;
			
		}
		
	}
	
	else
	{
		
		if ( DBD == 1 )
		{
			
			// Retry with DBD=0
			DBD = 0;
			goto Loop;
			
		}
		
		STATUS_LOG ( ( "%s: Mode Sense failed with service response = %x\n", getName ( ), serviceResponse ) );
		
		// The mode sense failed, mark as write protected to be safe.
	 	fMediaIsWriteProtected = true;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ReleaseDescriptor:
	
	require_nonzero ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediaIcon - Sets an icon key in the registry if desired.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::SetMediaIcon ( void )
{
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::SetMediaIcon called\n" ) );
	
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
	//	¥ FibreChannel HD
	//	¥ USB HD
	//	¥ SuperDisk
	//	¥ MagnetoOptical
	//	¥ SmartMedia
	//	¥ MemoryStick
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
//	¥ IssueRead - DEPRECATED.										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::IssueRead (
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
IOSCSIReducedBlockCommandsDevice::IssueRead (
									IOMemoryDescriptor *	buffer,
									UInt64					startBlock,
									UInt64					blockCount,
									void *					clientData )
{
	
	IOReturn 				status 	= kIOReturnNoResources;
	SCSITaskIdentifier		request	= NULL;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( READ_10 (	request,
					buffer,
					fMediaBlockSize,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
	{
		
		SetApplicationLayerReference ( request, clientData );
		SendCommand ( request,
					  fReadTimeoutDuration,
					  &IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete );
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		ReleaseSCSITask ( request );
		request = NULL;
		status = kIOReturnBadArgument;
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueWrite - DEPRECATED.										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::IssueWrite (
						IOMemoryDescriptor *	buffer,
						UInt64					startBlock,
						UInt64					blockCount )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueWrite - Issues an asynchronous write command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::IssueWrite (
							IOMemoryDescriptor *	buffer,
							UInt64					startBlock,
							UInt64					blockCount,
							void *					clientData )
{
	
	IOReturn				status	= kIOReturnNoResources;
	SCSITaskIdentifier		request	= NULL;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( WRITE_10 ( request, 
					buffer,
	 				fMediaBlockSize,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
	{
		
		SetApplicationLayerReference ( request, clientData );
		SendCommand ( request,
					  fWriteTimeoutDuration,
					  &IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete );
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		ReleaseSCSITask ( request );
		request = NULL;
		status = kIOReturnBadArgument;
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteCompletion - Completion routine for read/write requests.
//															 		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::AsyncReadWriteCompletion (
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
						
						status		= kIOReturnSuccess;
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
	
	IOReducedBlockServices::AsyncReadWriteComplete ( clientData, status, actCount );
	
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
IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete (
										SCSITaskIdentifier request )
{
	
	IOSCSIReducedBlockCommandsDevice *	taskOwner = NULL;
	
	require_nonzero ( request, ErrorExit );
	
	taskOwner = OSDynamicCast ( IOSCSIReducedBlockCommandsDevice, sGetOwnerForTask ( request ) );
	require_nonzero ( taskOwner, ErrorExit );
	
	taskOwner->AsyncReadWriteCompletion ( request );
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sPollForMedia - Static method called to poll for media.
//															[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::sPollForMedia (
									void *	pdtDriver,
									void *	refCon )
{
	
	IOSCSIReducedBlockCommandsDevice *	driver = NULL;
	
	driver = ( IOSCSIReducedBlockCommandsDevice * ) pdtDriver;
	require_nonzero ( driver, ErrorExit );
	
	driver->PollForMedia ( );
	
	if ( driver->fPollingMode != kPollingMode_Suspended )
	{
		
		// schedule the poller again since we didn't find media
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


OSMetaClassDefineReservedUsed ( IOSCSIReducedBlockCommandsDevice, 1 );	/* PowerDownHandler	*/
OSMetaClassDefineReservedUsed ( IOSCSIReducedBlockCommandsDevice, 2 );	/* SetMediaIcon		*/
OSMetaClassDefineReservedUsed ( IOSCSIReducedBlockCommandsDevice, 3 );	/* AsyncReadWriteCompletion	*/

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice,  4 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice,  5 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice,  6 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice,  7 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice,  8 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice,  9 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice, 10 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice, 11 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice, 12 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIReducedBlockCommandsDevice, 16 );