/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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

// Generic IOKit related headers
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOKitKeys.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IODVDTypes.h>
#include <IOKit/storage/IOCDTypes.h>

// User notification includes
#include <UserNotification/KUNCUserNotifications.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/SCSICommandDefinitions.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include "IOSCSIProtocolInterface.h"
#include "IOSCSIMultimediaCommandsDevice.h"
#include "IODVDServices.h"
#include "IOCompactDiscServices.h"
#include "SCSIBlockCommands.h"
#include "SCSIMultimediaCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Flag to turn on compiling of APIs marked as obsolete
#define INCLUDE_OBSOLETE_APIS					0

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"MMC"

#if DEBUG
#define SCSI_MMC_DEVICE_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_MMC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_MMC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_MMC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClass ( IOSCSIMultimediaCommandsDevice, IOSCSIPrimaryCommandsDevice );
OSDefineAbstractStructors ( IOSCSIMultimediaCommandsDevice, IOSCSIPrimaryCommandsDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define	kMaxProfileSize							56
#define kDiscInformationSize					32
#define kATIPBufferSize							16
#define kTrackInfoBufferSize					8
#define kDVDPhysicalFormatInfoBufferSize		8
#define	kProfileDataLengthFieldSize				4
#define kProfileFeatureHeaderSize				8
#define kProfileDescriptorSize					4
#define kModeSense6ParameterHeaderSize			4
#define kModeSense10ParameterHeaderSize			8
#define kMechanicalCapabilitiesMinBufferSize	4
#define kSubChannelDataBufferSize				24
#define kCDAudioModePageBufferSize				24
#define kDiscStatusOffset						2
#define kDVDBookTypePlusRDoubleLayer			0xE	// Until it gets to IODVDTypes.h

// Offsets into the CD Mechanical Capabilities mode page
#define kMediaAccessSpeedOffset					14

#define kMaxRetryCount							8
#define	kDefaultMaxBlocksPerIO					65535
#define kMechanicalCapabilitiesModePageCode		0x2A
#define kCDAudioModePageCode					0x0E

#define kAppleKeySwitchProperty					"AppleKeyswitch"
#define kAppleLowPowerPollingKey				"Low Power Polling"

#define kLocalizationPath						"/System/Library/Extensions/IOSCSIArchitectureModelFamily.kext/Contents/PlugIns/IOSCSIMultimediaCommandsDevice.kext"
#define kMediaEjectHeaderString					"Media Eject Header"
#define kMediaEjectNoticeString					"Media Eject Notice"
#define kOKButtonString							"OK"
#define kEmptyString							""

// Get Configuration Profiles
enum
{
	kGetConfigurationProfile_ProfileList				= 0x0000, /* Profile List */
	kGetConfigurationProfile_RandomWrite				= 0x0020, /* Random Write Profile */
	kGetConfigurationProfile_IncrementalStreamedWrite	= 0x0021, /* Incremental Streamed Writing Profile */
	kGetConfigurationProfile_DVDPlusRW					= 0x002A, /* DVD+RW Profile */
	kGetConfigurationProfile_DVDPlusR					= 0x002B, /* DVD+R Profile */
	kGetConfigurationProfile_CDTAO						= 0x002D, /* CD Track At Once Profile */
	kGetConfigurationProfile_CDMastering				= 0x002E, /* CD Mastering Profile */
	kGetConfigurationProfile_DVDWrite					= 0x002F, /* DVD-R Write Profile */
	kGetConfigurationProfile_DVDPlusRDoubleLayer		= 0x003B, /* DVD+R Double Layer Profile */
	kGetConfigurationProfile_AnalogAudio				= 0x0103, /* Analog Audio Profile */
	kGetConfigurationProfile_DVDCSS						= 0x0106  /* DVD-CSS Profile */
};

// Get Configuration Feature Numbers in Profile List
enum
{
	kGetConfigurationFeature_CDROM		= 0x0008,
	kGetConfigurationFeature_CDR		= 0x0009,
	kGetConfigurationFeature_CDRW		= 0x000A,
	kGetConfigurationFeature_DVDROM		= 0x0010,
	kGetConfigurationFeature_DVDR		= 0x0011,
	kGetConfigurationFeature_DVDRAM		= 0x0012,
	kGetConfigurationFeature_DVDRW		= 0x0014,
	kGetConfigurationFeature_DVDPlusRW	= 0x001A,
	kGetConfigurationFeature_DVDPlusR	= 0x001B
};

// Mechanical Capabilities flags
enum
{
	kMechanicalCapabilitiesCDRMask				= 0x01,
	kMechanicalCapabilitiesCDRWMask				= 0x02,
	kMechanicalCapabilitiesTestWriteMask		= 0x04,
	kMechanicalCapabilitiesDVDROMMask			= 0x08,
	kMechanicalCapabilitiesDVDRMask				= 0x10,
	kMechanicalCapabilitiesDVDRAMMask 			= 0x20,
};

enum
{
	kMechanicalCapabilitiesAnalogAudioMask			= 0x01,
	kMechanicalCapabilitiesCDDAStreamAccurateMask	= 0x02,
	kMechanicalCapabilitiesBUFMask					= 0x80,
};


// Random Writable Protection (DVD-RAM DVD+RW protection mask)
enum
{
	kRandomWritableProtectionMask		= 0x01
};

// DiscType mask
enum
{
	kDiscTypeCDRWMask					= 0x40
};

// Media Catalog Number and ISRC masks
enum
{
	kMediaCatalogValueFieldValidMask	= 0x80,
	kTrackCatalogValueFieldValidMask	= 0x80
};

// Spindle speed settings
enum
{
	kCDSpeed1x		= 176,
	kDVDSpeed1x		= 1350
};

enum
{
	kThirtySeconds	= 30
};

enum
{
	kCDTAOTestWriteBit 	= 2,
	kCDTAOBUFWriteBit	= 6,
	
	kCDTAOTestWriteMask = (1 << kCDTAOTestWriteBit),
	kCDTAOBUFWriteMask	= (1 << kCDTAOBUFWriteBit)
};

enum
{
	kCDMasteringCDRWBit			= 1,
	kCDMasteringTestWriteBit 	= 2,
	kCDMasteringRawWriteBit		= 3,
	kCDMasteringSAOWriteBit		= 5,
	kCDMasteringBUFWriteBit		= 6,
	
	kCDMasteringCDRWMask		= (1 << kCDMasteringCDRWBit),
	kCDMasteringTestWriteMask	= (1 << kCDMasteringTestWriteBit),
	kCDMasteringRawWriteMask	= (1 << kCDMasteringRawWriteBit),
	kCDMasteringSAOWriteMask	= (1 << kCDMasteringSAOWriteBit),
	kCDMasteringBUFWriteMask	= (1 << kCDMasteringBUFWriteBit)
};

enum
{
	kDVDRWBit			= 1,
	kDVDTestWriteBit 	= 2,
	kDVDBUFWriteBit		= 6,
	
	kDVDRWMask			= (1 << kDVDRWBit),
	kDVDTestWriteMask	= (1 << kDVDTestWriteBit),
	kDVDBUFWriteMask	= (1 << kDVDBUFWriteBit)
};


// Apple Features mode page code
#define kAppleFeaturesModePageCode		0x31

// Apple Features mode page struct
struct AppleFeatures
{
	UInt16		dataLength;					// should always be 0x10
	UInt8		mediumType;					// should always be 0
	UInt8		reserved[5];				// reserved, should always be 0
	UInt8		pageCode;					// should always be kAppleFeaturesModePageCode
	UInt8		pageLength;					// should always be 0x06
	UInt8		signature[4];				// should always be '.App'
#if defined(__LITTLE_ENDIAN__)
	UInt8		supportsLowPowerPoll:1; 	// flag - supports sleep shortcut
	UInt8		reservedBits:7;				// reserved, should always be 0
#else /* ! defined(__LITTLE_ENDIAN__) */
	UInt8		reservedBits:7;				// reserved, should always be 0
	UInt8		supportsLowPowerPoll:1; 	// flag - supports sleep shortcut
#endif /* defined(__LITTLE_ENDIAN__) */
	UInt8		reserved2;					// reserved, should always be 0
};
typedef struct AppleFeatures AppleFeatures;


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
IOSCSIMultimediaCommandsDevice::SyncReadWrite (
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
IOSCSIMultimediaCommandsDevice::AsyncReadWrite (
						IOMemoryDescriptor *	buffer,
						UInt64					startBlock,
						UInt64					blockCount,
						void *					clientData )
{
	
	IODirection		direction;
	IOReturn		status = kIOReturnBadArgument;
	
	require ( IsProtocolAccessEnabled ( ), ErrorExit );
	require ( IsDeviceAccessEnabled ( ), ErrorExit );
	
	direction = buffer->getDirection ( );
	if ( direction == kIODirectionIn )
	{
		
		status = IssueRead ( buffer, clientData, startBlock, blockCount );
		
	}
	
	else if ( direction == kIODirectionOut )
	{
		
		status = IssueWrite ( buffer, clientData, startBlock, blockCount );
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EjectTheMedia - 	Unlocks and ejects the media if it is removable. If it
//						is not removable, it synchronizes the write cache.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::EjectTheMedia ( void )
{
	
	SCSITaskIdentifier		request		= NULL;
	IOReturn				status		= kIOReturnNoResources;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( fMediaIsRemovable == false )
	{
		
		if ( SYNCHRONIZE_CACHE ( request, 0, 0, 0, 0, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			( void ) SendCommand ( request, kThirtySecondTimeoutInMS ); 
			
		}
		
	}
	
	else
	{
		
		if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateUnlocked, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			( void ) SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		if ( START_STOP_UNIT ( request, 0, 0, 1, 0, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			( void ) SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		ResetMediaCharacteristics ( );
		messageClients ( kIOMessageTrayStateChange,
						 ( void * ) kMessageTrayStateChangeRequestAccepted );
		
		if ( fLowPowerPollingEnabled == false )
		{
			
			// Set the polling to determine when new media has been inserted
			fPollingMode = kPollingMode_NewMedia;
			TicklePowerManager ( );
			EnablePolling ( );
			
		}
		
	}
	
	ReleaseSCSITask ( request );
	status = kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FormatMedia - Unsupported.									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::FormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn	status = kIOReturnUnsupported;
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetFormatCapacities - Unsupported.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIMultimediaCommandsDevice::GetFormatCapacities (
									UInt64 * capacities,
									UInt32   capacitiesMaxCount ) const
{
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LockUnlockMedia - Unsupported.								   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::LockUnlockMedia ( bool doLock )
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
IOSCSIMultimediaCommandsDevice::SynchronizeCache ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	IOReturn				status 			= kIOReturnNoResources;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
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
IOSCSIMultimediaCommandsDevice::ReportBlockSize ( UInt64 * blockSize )
{
	
	*blockSize = fMediaBlockSize;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportEjectability - Reports the medium ejectability characteristic.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReportEjectability ( bool * isEjectable )
{
	
	*isEjectable = fMediaIsRemovable;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportLockability - Reports the medium lockability characteristic.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReportLockability ( bool * isLockable )
{
	
	*isLockable = true;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportPollRequirements - Reports polling requirements (none).
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReportPollRequirements (
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
//	¥ ReportMaxReadTransfer - Reports maximum read transfer in bytes.  [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReportMaxReadTransfer (
										UInt64 		blockSize,
										UInt64 * 	max )
{
	
	UInt32	maxBlockCount 	= kDefaultMaxBlocksPerIO;
	UInt64	maxByteCount	= 0;
	bool	supported		= false;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReportMaxReadTransfer.\n" ) );
	
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
IOSCSIMultimediaCommandsDevice::ReportMaxWriteTransfer ( UInt64 	blockSize,
														 UInt64 * 	max )
{
	
	UInt32	maxBlockCount 	= kDefaultMaxBlocksPerIO;
	UInt64	maxByteCount	= 0;
	bool	supported		= false;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReportMaxWriteTransfer.\n" ) );
	
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
IOSCSIMultimediaCommandsDevice::ReportMaxValidBlock ( UInt64 * maxBlock )
{
	
	if ( fMediaBlockCount == 0 )
	{
		
		// If the capacity is zero, return that for
		// the max valid block.
		*maxBlock = 0;
		
	}
	
	else
	{
		
		// Since the driver stores the number of blocks, and
		// blocks are addressed starting at zero, subtract one
		// to get the maximum valid block.
		*maxBlock = fMediaBlockCount - 1;
		
	}
	
	STATUS_LOG ( ( "%s::%s maxBlockHi = 0x%x, maxBlockLo = 0x%x\n", getName ( ),
					__FUNCTION__, ( *maxBlock ), ( *maxBlock ) >> 32 ) );
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediaState - Reports state of media in the device		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReportMediaState ( 
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
IOSCSIMultimediaCommandsDevice::ReportRemovability ( bool * isRemovable )
{
	
	*isRemovable = fMediaIsRemovable;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportWriteProtection - Reports write protection characteristic of media
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReportWriteProtection (
										bool * isWriteProtected )
{
	
	*isWriteProtected = fMediaIsWriteProtected;
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTrayState - Reports the current tray state of the device (if possible)
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::GetTrayState ( UInt8 * trayState )
{
	
	IOReturn				status 			= kIOReturnNoResources;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					statusBuffer[8] = { 0 };
	IOMemoryDescriptor *	buffer			= NULL;
	
	buffer = IOMemoryDescriptor::withAddress ( 	statusBuffer,
												8,
												kIODirectionIn );
	require_nonzero ( buffer, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );	
	
	if ( GET_EVENT_STATUS_NOTIFICATION ( request,
										 buffer,
										 1,
										 1 << 4, /* media status notification event */
										 8,
										 0x00 ) == true )
	{
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "GET_EVENT_STATUS_NOTIFICATION succeeded.\n" ) );
		*trayState = statusBuffer[5] & 0x01;
		STATUS_LOG ( ( "trayState = %d.\n", *trayState ) );
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		// The device doesn't support the GET_EVENT_STATUS_NOTIFICATION.
		// Assume the tray is shut.
		ERROR_LOG ( ( "GET_EVENT_STATUS_NOTIFICATION failed.\n" ) );
		*trayState = 0;
		STATUS_LOG ( ( "trayState = %d.\n", *trayState ) );
		status = kIOReturnSuccess;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( buffer, ErrorExit );
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:
	
		
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTrayState - Sets the tray state of the device (if possible)
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::SetTrayState ( UInt8 trayState )
{
	
	IOReturn				status 			= kIOReturnNotPermitted;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	require ( ( fMediaPresent == false ), ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ErrorExit, status = kIOReturnNoResources );
	
	// Set to desired tray state.
	if ( START_STOP_UNIT ( request, 1, 0, 1, !trayState, 0 ) == true )
	{
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "START_STOP_UNIT succeeded.\n" ) );
		status = kIOReturnSuccess;
		
	}
	
	ReleaseSCSITask ( request );
	messageClients ( kIOMessageTrayStateChange,
					 ( void * ) kMessageTrayStateChangeRequestAccepted );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadCD - Issues an asynchronous READ_CD request.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::AsyncReadCD (
							IOMemoryDescriptor *	buffer,
							UInt32					startBlock,
							UInt32					blockCount,
							CDSectorArea			sectorArea,
							CDSectorType			sectorType,
							void *					clientData )
{
	
	IOReturn				status 	= kIOReturnUnsupported;
	SCSITaskIdentifier		request	= NULL;
	
	STATUS_LOG ( ( "%s::%s Attempted\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ErrorExit, status = kIOReturnNoResources );
	
	if ( READ_CD (	request,
					buffer,
					sectorType,
					0,
					startBlock, 
					blockCount, 
					( ( sectorArea & kCDSectorAreaSync        ) ? 0x1 : 0x0 ),
					( ( sectorArea & kCDSectorAreaHeader      ) ? 0x1 : 0x0 ) | ( ( sectorArea & kCDSectorAreaSubHeader   ) ? 0x2 : 0x0 ),
					( ( sectorArea & kCDSectorAreaUser        ) ? 0x1 : 0x0 ),
					( ( sectorArea & kCDSectorAreaAuxiliary   ) ? 0x1 : 0x0 ),
					( ( sectorArea & kCDSectorAreaErrorFlags  ) ? 0x1 : 0x0 ),
					( ( sectorArea & kCDSectorAreaSubChannel  ) ? 0x1 : 0x0 ) | ( ( sectorArea & kCDSectorAreaSubChannelQ ) ? 0x2 : 0x0 ),
					0 ) == true )
	{
		
		SetApplicationLayerReference ( request, clientData );
		
		// The command was successfully built, now send it
		SendCommand ( request, fReadTimeoutDuration, &IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete );
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		status = kIOReturnBadArgument;
		ReleaseSCSITask ( request );
		request = NULL;
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadISRC - 	Reads the ISRC (International Standard Recording Code)
//					from the media.									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReadISRC ( UInt8 track, CDISRC isrc )
{
	
	IOReturn				status 			= kIOReturnNoResources;
	IOMemoryDescriptor *	desc			= NULL;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					isrcData[kSubChannelDataBufferSize];
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadISRC called\n" ) );
	
	desc = IOMemoryDescriptor::withAddress ( isrcData, kSubChannelDataBufferSize, kIODirectionIn );
	require_nonzero ( desc, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	if ( READ_SUB_CHANNEL ( request,
							desc,
							1,
							1,
							0x03,
							track,
							kSubChannelDataBufferSize,
							0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		// Check if we found good data.
		if ( isrcData[8] & kTrackCatalogValueFieldValidMask )
		{
			
			bcopy ( &isrcData[9], isrc, kCDISRCMaxLength );
			isrc[kCDISRCMaxLength] = 0;
			
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			status = kIOReturnNotFound;
		}
		
	}
	
	else
	{
		status = kIOReturnNotFound;
	}
	
	ReleaseSCSITask ( request );
	
	
ReleaseDescriptor:
	
	require_nonzero_quiet ( desc, ErrorExit );
	desc->release ( );
	desc = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadMCN - Reads the MCN (Media Catalogue Number) from the media. [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReadMCN ( CDMCN mcn )
{
	
	IOReturn				status 			= kIOReturnNoResources;
	IOMemoryDescriptor *	desc			= NULL;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					mcnData[kSubChannelDataBufferSize];
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadMCN called\n" ) );
	
	desc = IOMemoryDescriptor::withAddress ( mcnData, kSubChannelDataBufferSize, kIODirectionIn );
	require_nonzero ( desc, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	if ( READ_SUB_CHANNEL (	request,
							desc,
							1,
							1,
							0x02,
							0,
							kSubChannelDataBufferSize,
							0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		// Check if we found good data.
		if ( mcnData[8] & kMediaCatalogValueFieldValidMask )
		{
			
			bcopy ( &mcnData[9], mcn, kCDMCNMaxLength );
			mcn[kCDMCNMaxLength] = 0;
			
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			status = kIOReturnNotFound;
		}
		
	}
	
	else
	{
		status = kIOReturnNotFound;
	}
	
	ReleaseSCSITask ( request );
	
	
ReleaseDescriptor:
	
	require_nonzero_quiet ( desc, ErrorExit );
	desc->release ( );
	desc = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadTOC - 	Reads the Table Of Contents, Format 0x02, from the media.
//					*OBSOLETE* Callers should use the other ReadTOC	API.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReadTOC ( IOMemoryDescriptor * buffer )
{
	return ReadTOC ( buffer, kCDTOCFormatTOC, 0x01, 0x00, NULL );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadTOC - 	Reads the specified format of the Table Of Contents from
//					the media.										   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReadTOC (
							IOMemoryDescriptor *	buffer,
							CDTOCFormat				format,
							UInt8					msf,
							UInt32					trackSessionNumber,
							UInt16 *				actualByteCount )
{
	
	IOBufferMemoryDescriptor *	doubleBuffer	= NULL;
	SCSITaskIdentifier			request			= NULL;
	SCSIServiceResponse			serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOReturn					status			= kIOReturnError;
	IOMemoryDescriptor *		bufferToUse		= NULL;
	
	if ( ( format == kCDTOCFormatTOC ) && ( msf == 1 ) )
	{
		
		// If they ask for 4 bytes, use 0xFFFE to make sure we get the whole TOC
		if ( buffer->getLength ( ) == sizeof ( CDTOC ) )
		{
			
			UInt8 *	zeroPtr = 0;
			
			STATUS_LOG ( ( "2ble buffer using 0xFFFE as size\n" ) );
			
			doubleBuffer = IOBufferMemoryDescriptor::withCapacity ( 0xFFFE, kIODirectionIn );
			require_nonzero_action ( doubleBuffer, ErrorExit, status = kIOReturnNoResources );
			
			bufferToUse = doubleBuffer;
			zeroPtr = ( UInt8 * ) doubleBuffer->getBytesNoCopy ( );
			bzero ( zeroPtr, doubleBuffer->getLength ( ) );
			
		}
		
		// If they ask for an odd number of bytes, pad it to make it even
		else if ( ( buffer->getLength ( ) & 1 ) == 1 )
		{
			
			UInt8 *	zeroPtr = 0;
			
			STATUS_LOG ( ( "2ble buffer using %ld as size\n", buffer->getLength ( ) + 1 ) );
			
			doubleBuffer = IOBufferMemoryDescriptor::withCapacity ( buffer->getLength ( ) + 1, kIODirectionIn );
			require_nonzero_action ( doubleBuffer, ErrorExit, status = kIOReturnNoResources );
			
			bufferToUse = doubleBuffer;
			zeroPtr = ( UInt8 * ) doubleBuffer->getBytesNoCopy ( );
			bzero ( zeroPtr, doubleBuffer->getLength ( ) );
			
		}
		
		// Else, it's all good...
		else
		{
			
			STATUS_LOG ( ( "No 2ble buffer\n" ) );
			bufferToUse = buffer;
			
		}
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "No 2ble buffer\n" ) );
		bufferToUse = buffer;
		
	}
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ReleaseDescriptor, status = kIOReturnNoResources );
	
	if ( READ_TOC_PMA_ATIP (	request,
								bufferToUse,
								msf,			// MSF bit set
								format,
								0,
								bufferToUse->getLength ( ),
								0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( actualByteCount != NULL )
		{
			*actualByteCount = GetRealizedDataTransferCount ( request );
		}
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD )
		{
			
			status = kIOReturnSuccess;
			
			if ( ( format == kCDTOCFormatTOC ) && ( msf == 1 ) )
			{
				
				UInt16			sizeOfTOC 	= 0;
				UInt8 *			ptr			= NULL;
				IOMemoryMap *	map			= NULL;
				
				map = bufferToUse->map ( kIOMapAnywhere );
				require_nonzero_action ( map, ReleaseTask, status = kIOReturnError );
				
				ptr = ( UInt8 * ) map->getVirtualAddress ( );
				
				// Get the size of the TOC data returned
				sizeOfTOC = OSReadBigInt16 ( ptr, 0 ) + sizeof ( UInt16 );
				require_action ( ( sizeOfTOC > sizeof ( CDTOC ) ), ReleaseTask, map->release ( ); status = kIOReturnError );
				
				// We have successfully gotten a TOC, check if the driver was able to 
				// determine a block count from READ CAPACITY.  If the media has a reported
				// block count of zero, then this is most likely one of the devices that does
				// not correctly update the capacity information after writing a disc.  Since the
				// BCD conversion check cannot be successfully done without valid capacity
				// data, do neither the check nor the conversion. 
				if ( fMediaBlockCount != 0 )
				{
					
					UInt8			lastSessionNum		= 0;
					UInt32			numLBAfromMSF		= 0;
					UInt32 			index				= 0;
					UInt8 *			beginPtr			= NULL;
					bool			needsBCDtoHexConv	= false;
					
					beginPtr = ptr;
					
				#if ( SCSI_MMC_DEVICE_DEBUGGING_LEVEL >= 2 )
					if ( sizeOfTOC > GetRealizedDataTransferCount ( request ) )
						ERROR_LOG ( ( "sizeOfTOC != Realized Data Transfer Count\n size = %d, xfer count = %ld\n",
										sizeOfTOC, ( UInt32 ) GetRealizedDataTransferCount ( request ) ) );
				#endif
					
					// Get the number of the last session on the disc.
					// Since this number will be match to the appropriate
					// data in the returned TOC, the format of this (Hex or BCD)
					// has no impact on its use.		
					lastSessionNum = ptr[3];
					
					// Find the A2 point for the last session
					for ( index = 4; index < ( UInt32 ) ( sizeOfTOC - 4 ); index += 11 )
					{
						
						// Check if this Track Descriptor is for the last session
						if ( ptr[index] != lastSessionNum )
						{
							// Not for the last session, go on to the next descriptor.
							continue;
						}
						
						// If we got here, then this Track Descriptor is for the last session,
						// now check to see if it is the A2 point.
						if ( ptr[index + 3] != 0xA2 )
						{
							continue;
						}
						
						// If we got here, then this Track Descriptor is for the last session,
						// and is the A2 point. Now check if the beginning of the lead out is greater
						// than the disc capacity (plus the 2 second leadin or 150 blocks) plus 75
						// sector tolerance.
						
						// ¥¥¥ The spec says the tolerance should only be considered if the last
						// track is audio. Fix this when we get time
						numLBAfromMSF = ( ( ( ptr[index + 8] * 60 ) + ( ptr[index + 9] ) ) * 75 ) + ( ptr[index + 10] );
						
						if ( numLBAfromMSF > ( ( fMediaBlockCount + 150 ) + 75 ) )
						{
							
							needsBCDtoHexConv = true;
							break;
							
						}
						
					}
					
					if ( needsBCDtoHexConv == true )
					{
						
						ERROR_LOG ( ( "Drive needs BCD->HEX conversion\n" ) );
						
						// Convert First/Last session info
						ptr[2] 	= ConvertBCDToHex ( ptr[2] );
						ptr[3] 	= ConvertBCDToHex ( ptr[3] );
						ptr 	= &ptr[4];
						
						// Loop over track descriptors finding the BCD values and change them to hex.
						for ( index = 0; index < ( UInt32 ) ( sizeOfTOC - 4 ); index += 11 )
						{
							
							if ( ( ptr[index + 3] == 0xA0 ) || ( ptr[index + 3] == 0xA1 ) )
							{
								
								// Fix the A0 and A1 PMIN values.
								ptr[index + 8] = ConvertBCDToHex ( ptr[index + 8] );
								
							}
							
							else
							{
								
								// Fix the Point value field
								ptr[index + 3] = ConvertBCDToHex ( ptr[index + 3] );
								
								// Fix the Minutes value field
								ptr[index + 8] = ConvertBCDToHex ( ptr[index + 8] );
								
								// Fix the Seconds value field
								ptr[index + 9] = ConvertBCDToHex ( ptr[index + 9] );
								
								// Fix the Frames value field
								ptr[index + 10] = ConvertBCDToHex ( ptr[index + 10] );
								
							}
							
						}
						
					}
					
					if ( bufferToUse != buffer )
					{
						
						STATUS_LOG ( ( "Writing Bytes\n" ) );
						buffer->writeBytes ( 0, beginPtr, min ( buffer->getLength ( ), sizeOfTOC ) );
						
					}
					
				}
				
				map->release ( );
				map = NULL;
				
			}
			
		}
		
	}
	
	else
	{
		
		if ( actualByteCount != NULL )
		{
			*actualByteCount = 0;
		}
		
		// We got an error on the READ_TOC_PMA_ATIP. We shouldn't get one unless the media
		// is blank, so return an error.
		status = kIOReturnIOError;
		
	}
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseDescriptor );		
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( doubleBuffer, ErrorExit );
	doubleBuffer->release ( );
	doubleBuffer = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadTOC status = %d\n", status ) );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadDiscInfo - 	Performs a READ_DISC_INFO command to get information
//						about the media.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReadDiscInfo (	IOMemoryDescriptor *	buffer,
												UInt16 *				actualByteCount )
{
	
	IOReturn				status 			= kIOReturnIOError;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadDiscInfo called\n" ) );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ErrorExit, status = kIOReturnNoResources );
	
	if ( READ_DISC_INFORMATION (	request,
									buffer,
									buffer->getLength ( ),
									0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( actualByteCount != NULL )
		{
			*actualByteCount = GetRealizedDataTransferCount ( request );
		}
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD )
		{
			status = kIOReturnSuccess;
		}
		
	}
	
	else if ( actualByteCount != NULL )
	{
		*actualByteCount = 0;
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadDiscInfo status = %d\n", status ) );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadTrackInfo - 	Performs a READ_TRACK_INFO command to get information
//						about the media.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReadTrackInfo (
							IOMemoryDescriptor *	buffer,
							UInt32					address,
							CDTrackInfoAddressType	addressType,
							UInt16 *				actualByteCount )
{

	IOReturn				status 			= kIOReturnIOError;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadTrackInfo called\n" ) );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ErrorExit, status = kIOReturnNoResources );
	
	if ( READ_TRACK_INFORMATION (	request,
									buffer,
									addressType,
									address,
									buffer->getLength ( ),
									0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( actualByteCount != NULL )
		{
			*actualByteCount = GetRealizedDataTransferCount ( request );
		}
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD )
		{
			status = kIOReturnSuccess;
		}
		
	}
	
	else if ( actualByteCount != NULL )
	{
		
		*actualByteCount = 0;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadTrackInfo status = %d\n", status ) );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AudioPause - 	Pauses analog audio playback.
//					*OBSOLETE* All CD playback is digital now.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::AudioPause ( bool pause )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AudioPlay - 	Starts analog audio playback.
//					*OBSOLETE* All CD playback is digital now.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::AudioPlay ( CDMSF timeStart, CDMSF timeStop )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AudioScan - 	Starts analog audio scanning.
//					*OBSOLETE* All CD playback is digital now.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::AudioScan ( CDMSF timeStart, bool reverse )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AudioStop - 	Stops analog audio playback.
//					*OBSOLETE* All CD playback is digital now.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::AudioStop ( void )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAudioStatus - 	Gets the audio status.
//						*OBSOLETE* All CD playback is digital now.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::GetAudioStatus ( CDAudioStatus * status )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAudioVolume - 	Gets the analog audio volume.
//						*OBSOLETE* All CD playback is digital now.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::GetAudioVolume ( UInt8 * leftVolume,
												 UInt8 * rightVolume )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetAudioVolume - 	Sets the analog audio volume.
//						*OBSOLETE* All CD playback is digital now.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::SetAudioVolume ( UInt8 leftVolume,
												 UInt8 rightVolume )
{
	return kIOReturnUnsupported;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetMediaType - Returns the media type.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIMultimediaCommandsDevice::GetMediaType ( void )
{
	
	return fMediaType;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportKey - Issues a REPORT_KEY to the device.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReportKey (
							IOMemoryDescriptor * 	buffer,
							const DVDKeyClass		keyClass,
							const UInt32			lba,
							const UInt8				agid,
							const DVDKeyFormat		keyFormat )
{
	
	IOReturn				status			= kIOReturnUnsupported;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	require_nonzero ( ( fSupportedDVDFeatures & kDVDFeaturesCSSMask ), Exit );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ErrorExit, status = kIOReturnNoResources );
	
	if ( REPORT_KEY ( 	request,
						buffer,
						( keyFormat == 0x04 ) ? lba : 0,
						( buffer != NULL ) ? buffer->getLength ( ) : 0,
						agid,
						keyFormat,
						0x00 ) == true )
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
		
		SCSI_Sense_Data		senseDataBuffer;
		bool				senseIsValid;
		
		senseIsValid = GetAutoSenseData ( request, &senseDataBuffer );
		if ( senseIsValid )
		{
			
			IOLog ( "REPORT_KEY failed : ASC = 0x%02x, ASCQ = 0x%02x\n", 
			senseDataBuffer.ADDITIONAL_SENSE_CODE,
			senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER );
			
		}
		
		status = kIOReturnIOError;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
Exit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SendKey - Issues a SEND_KEY to the device.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::SendKey (
							IOMemoryDescriptor * 	buffer,
							const DVDKeyClass		keyClass,
							const UInt8				agid,
							const DVDKeyFormat		keyFormat )
{
	
	IOReturn				status 			= kIOReturnUnsupported;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	require_nonzero ( ( fSupportedDVDFeatures & kDVDFeaturesCSSMask ), Exit );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request, ErrorExit, status = kIOReturnNoResources );
	
	if ( SEND_KEY (	request,
					buffer,
					( buffer != NULL ) ? buffer->getLength ( ) : 0,
					agid,
					keyFormat,
					0x00 ) == true )
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
		
		SCSI_Sense_Data		senseDataBuffer;
		bool				senseIsValid;
		
		senseIsValid = GetAutoSenseData ( request, &senseDataBuffer );
		if ( senseIsValid )
		{
			
			IOLog ( "SEND_KEY failed : ASC = 0x%02x, ASCQ = 0x%02x\n", 
			senseDataBuffer.ADDITIONAL_SENSE_CODE,
			senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER );
			
		}
		
		status = kIOReturnIOError;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
Exit:	
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadDVDStructure - Issues a READ_DVD_STRUCTURE to the device.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ReadDVDStructure (
							IOMemoryDescriptor * 		buffer,
							const UInt32 				length,
							const UInt8					structureFormat,
							const UInt32				logicalBlockAddress,
							const UInt8					layer,
							const UInt8 				agid )
{
	
	IOReturn				status			= kIOReturnNoResources;
	SCSITaskIdentifier		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( READ_DVD_STRUCTURE (	request,
								buffer,
								logicalBlockAddress,
								layer,
								structureFormat,
								length,
								agid,
								0x00 ) == true )
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
		
		SCSI_Sense_Data		senseDataBuffer;
		bool				senseIsValid;
		
		senseIsValid = GetAutoSenseData ( request, &senseDataBuffer );
		if ( senseIsValid )
		{
			
			IOLog ( "READ_DVD_STRUCTURE failed : ASC = 0x%02x, ASCQ = 0x%02x\n", 
			senseDataBuffer.ADDITIONAL_SENSE_CODE,
			senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER );
			
		}
		
		status = kIOReturnIOError;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetMediaAccessSpeed - Obtains the media access speed.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::GetMediaAccessSpeed (
								UInt16 * kilobytesPerSecond )
{
	
	IOReturn					status					= kIOReturnSuccess;
	UInt32						actualSize				= 0;
	UInt32						minBufferSize			= 0;
	UInt16						blockDescriptorLength	= 0;
	UInt8						pageCode				= 0;
	UInt8 *						mechanicalCapabilities	= NULL;
	UInt8						headerSize				= 0;
	IOBufferMemoryDescriptor *	bufferDesc				= NULL;
	bool						use10Byte				= true;
	
	status = GetMechanicalCapabilitiesSize ( &actualSize );
	require_success ( status, ErrorExit );
	
	bufferDesc = IOBufferMemoryDescriptor::withCapacity (
												actualSize,
												kIODirectionIn,
												true );
	require_nonzero_action ( bufferDesc,
							 ErrorExit,
							 status = kIOReturnNoResources );
	
	mechanicalCapabilities = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( mechanicalCapabilities, actualSize );
	
	status = GetModeSense ( bufferDesc,
							kMechanicalCapabilitiesModePageCode,
							actualSize,
							&use10Byte );
	require_success ( status, ReleaseDescriptor );
	
	if ( use10Byte )
	{
		
		// We check to make sure there aren't any block descriptors. If there
		// are, we skip over them.
		blockDescriptorLength	= OSReadBigInt16 ( mechanicalCapabilities, 6 );
		headerSize				= kModeSense10ParameterHeaderSize;
		
	}
	
	else
	{
		
		blockDescriptorLength	= mechanicalCapabilities[3];
		headerSize				= kModeSense6ParameterHeaderSize;
		
	}
	
	// Ensure that our buffer is of the minimum correct size.
	// Also, check that the first byte of the page is the
	// correct PAGE_CODE (kMechanicalCapabilitiesModePageCode).
	minBufferSize = headerSize + blockDescriptorLength + kMediaAccessSpeedOffset + sizeof ( UInt16 );
	
	require_action ( ( actualSize >= minBufferSize ),
					 ReleaseDescriptor,
					 status = kIOReturnInternalError );
	
	pageCode = mechanicalCapabilities[headerSize + blockDescriptorLength] & 0x3F;
	
	require_action ( ( pageCode == kMechanicalCapabilitiesModePageCode ),
					 ReleaseDescriptor,
					 status = kIOReturnInternalError );
		
	*kilobytesPerSecond = OSReadBigInt16 (
								mechanicalCapabilities,
								headerSize + blockDescriptorLength + kMediaAccessSpeedOffset );
	status = kIOReturnSuccess;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediaAccessSpeed - Sets the media access speed.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::SetMediaAccessSpeed (
									UInt16 kilobytesPerSecond )
{
	
	IOReturn				status 			= kIOReturnNoResources;
	SCSITaskIdentifier 		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	switch ( fMediaType )
	{
		
		case kCDMediaTypeROM:
		case kCDMediaTypeR:
		case kCDMediaTypeRW:
			if ( SET_CD_SPEED ( request, kilobytesPerSecond, 0, 0 ) == true )
			{
				serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
			}
			break;
		
		case kDVDMediaTypeROM:
		case kDVDMediaTypeRAM:
		case kDVDMediaTypeR:
		case kDVDMediaTypeRW:
		case kDVDMediaTypePlusR:
		case kDVDMediaTypePlusRW:
			if ( SET_CD_SPEED ( request, kilobytesPerSecond, 0, 0x40 ) == true )
			{
				serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
			}
			break;
		
		default:
			break;
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		fCurrentDiscSpeed = kilobytesPerSecond;
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		status = kIOReturnIOError;
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	return status;
	
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
IOSCSIMultimediaCommandsDevice::InitializeDeviceSupport ( void )
{
	
	bool 	setupSuccessful = false;
	
	// Initialize the device characteristics flags
	fSupportedCDFeatures 			= 0;
	fSupportedDVDFeatures 			= 0;
	fDeviceSupportsLowPowerPolling 	= false;
	fMediaChanged					= false;
	fMediaPresent					= false;
	fMediaIsRemovable 				= false;
	fMediaType						= kCDMediaTypeUnknown;
	fMediaIsWriteProtected 			= true;
	fCurrentDiscSpeed				= 0;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::InitializeDeviceSupport called\n" ) );
	
	fIOSCSIMultimediaCommandsDeviceReserved =
			IONew ( IOSCSIMultimediaCommandsDeviceExpansionData, 1 );
	require_nonzero ( fIOSCSIMultimediaCommandsDeviceReserved, ErrorExit );
	
	bzero ( fIOSCSIMultimediaCommandsDeviceReserved,
			sizeof ( IOSCSIMultimediaCommandsDeviceExpansionData ) );
	
	// Make sure the drive is ready for us!
	require ( ClearNotReadyStatus ( ), ReleaseExpansionData );
	
	setupSuccessful = DetermineDeviceCharacteristics ( );
	
	if ( setupSuccessful == true )
	{		
		
		fPollingMode 	= kPollingMode_NewMedia;
		fPollingThread 	= thread_call_allocate (
						( thread_call_func_t ) IOSCSIMultimediaCommandsDevice::sPollForMedia,
						( thread_call_param_t ) this );
		
		require_nonzero_action ( fPollingThread, ReleaseExpansionData, setupSuccessful = false );
		
		InitializePowerManagement ( GetProtocolDriver ( ) );
		
	}
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::InitializeDeviceSupport setupSuccessful = %d\n", setupSuccessful ) );
	
	SetMediaCharacteristics ( 0, 0 );
	
	return setupSuccessful;
	
	
ReleaseExpansionData:
	
	
	require_nonzero_quiet ( fIOSCSIMultimediaCommandsDeviceReserved, ErrorExit );
	IODelete ( fIOSCSIMultimediaCommandsDeviceReserved, IOSCSIMultimediaCommandsDeviceExpansionData, 1 );
	fIOSCSIMultimediaCommandsDeviceReserved = NULL;
	
	
ErrorExit:
	
	
	return setupSuccessful;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StartDeviceSupport - Starts device support.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::StartDeviceSupport ( void )
{
	
	EnablePolling ( );
	
	CreateStorageServiceNub ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SuspendDeviceSupport - Suspends device support.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::SuspendDeviceSupport ( void )
{
	
	if ( fPollingMode != kPollingMode_Suspended )
	{
		
		DisablePolling ( );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResumeDeviceSupport - Resumes device support.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::ResumeDeviceSupport ( void )
{
	
	if ( fMediaPresent == false )
	{
		
		fPollingMode = kPollingMode_NewMedia;
		EnablePolling ( );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StopDeviceSupport - Stops device support.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::StopDeviceSupport ( void )
{
	
	DisablePolling ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TerminateDeviceSupport - Terminates device support.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::TerminateDeviceSupport ( void )
{
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::cleanUp called.\n" ) );
	
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
IOSCSIMultimediaCommandsDevice::CreateCommandSetObjects ( void )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	fSCSIMultimediaCommandObject =
		SCSIMultimediaCommands::CreateSCSIMultimediaCommandObject ( );
	require_nonzero ( fSCSIMultimediaCommandObject, ErrorExit );
	
	fSCSIBlockCommandObject =
		SCSIBlockCommands::CreateSCSIBlockCommandObject( );
	require_nonzero_action ( fSCSIBlockCommandObject,
							 ErrorExit,
							 fSCSIMultimediaCommandObject->release ( ) );
	
	// We're ready to go now.
	result = true;
	
	
ErrorExit:
	
	
	return result;	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FreeCommandSetObjects - Releases command set objects			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::FreeCommandSetObjects ( void )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	if ( fSCSIMultimediaCommandObject != NULL )
	{
		
		fSCSIMultimediaCommandObject->release ( );
		fSCSIMultimediaCommandObject = NULL;
		
	}
	
	if ( fSCSIBlockCommandObject != NULL )
	{
		
		fSCSIBlockCommandObject->release ( );
		fSCSIBlockCommandObject = NULL;
		
	}
	
	// Release the reserved structure. Since this function is called from
	// free(), we just get rid of the stuff here.
	if ( fIOSCSIMultimediaCommandsDeviceReserved != NULL )
	{
		
		IODelete ( fIOSCSIMultimediaCommandsDeviceReserved, IOSCSIMultimediaCommandsDeviceExpansionData, 1 );
		fIOSCSIMultimediaCommandsDeviceReserved = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIMultimediaCommandObject - Accessor method				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIMultimediaCommands *
IOSCSIMultimediaCommandsDevice::GetSCSIMultimediaCommandObject ( void )
{
	
	check ( fSCSIMultimediaCommandObject );
	return fSCSIMultimediaCommandObject;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIBlockCommandObject - Accessor method					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIBlockCommands *
IOSCSIMultimediaCommandsDevice::GetSCSIBlockCommandObject ( void )
{
	
	check ( fSCSIBlockCommandObject );
	return fSCSIBlockCommandObject;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIPrimaryCommandObject - Accessor method					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIPrimaryCommands	*
IOSCSIMultimediaCommandsDevice::GetSCSIPrimaryCommandObject ( void )
{
	
	check ( fSCSIMultimediaCommandObject );
	return OSDynamicCast ( SCSIPrimaryCommands,
						   GetSCSIMultimediaCommandObject ( ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VerifyDeviceState - Releases command set objects				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::VerifyDeviceState ( void )
{
	
	if ( fLowPowerPollingEnabled == true )
	{
		
		STATUS_LOG ( ( "Low power polling turned off\n" ) );
		fLowPowerPollingEnabled = false;
		
	}
	
	if ( IsPowerManagementIntialized ( ) == true )
	{
		
		STATUS_LOG ( ( "TicklePowerManager\n" ) );
		TicklePowerManager ( );
		
	}
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearNotReadyStatus - Clears any NOT_READY status on device	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::ClearNotReadyStatus ( void )
{
	
	SCSI_Sense_Data				senseBuffer		= { 0 };
	IOMemoryDescriptor *		bufferDesc		= NULL;
	SCSITaskIdentifier			request			= NULL;
	bool						driveReady 		= false;
	bool						result 			= true;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
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
					
					if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
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
					
					else if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY ) && 
								( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
								( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) ) 
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
	
	// check isInactive in case device was hot unplugged during sleep
	// and we are in an infinite loop here
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
IOSCSIMultimediaCommandsDevice::EnablePolling ( void )
{		
	
	AbsoluteTime	time;
	
	// No reason to start a thread if we've been termintated
	require ( ( isInactive ( ) == false ) && fPollingThread, Exit );
	require ( ( fPollingMode != kPollingMode_Suspended ), Exit );
	require_nonzero ( fPollingThread, Exit );
	
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
IOSCSIMultimediaCommandsDevice::DisablePolling ( void )
{		
	
	// Change the polling mode
	fPollingMode = kPollingMode_Suspended;
	
	// Cancel the thread if it is scheduled to run
	require ( thread_call_cancel ( fPollingThread ), Exit );
	
	// It was running, so we balance out the retain ( )
	// with a release ( )
	release ( );
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleSetUserClientExclusivityState - Overrides the default function in
//											order to handle non-exclusive
//											user client connections.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::HandleSetUserClientExclusivityState (
									IOService *		userClient,
									bool			state )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	status = super::HandleSetUserClientExclusivityState ( userClient, state );
	require_success ( status, ErrorExit );
	
	status = kIOReturnExclusiveAccess;
	
	if ( state == false )
	{
		
		status = message ( kSCSIServicesNotification_Resume, NULL, NULL );
		messageClients ( kSCSIServicesNotification_ExclusivityChanged );
		
	}
	
	else
	{
		
		if ( fMediaPresent )
		{
			
			OSIterator *				childList;
			IOService *					childService;
			OSObject *					childObject;
			IOService *					parent;
			
			STATUS_LOG ( ( "Media is present\n" ) );

			childList = getChildIterator ( gIOServicePlane );
			if ( childList != NULL )
			{

				STATUS_LOG ( ( "childList != NULL\n" ) );
				
				while ( ( childObject = childList->getNextObject ( ) ) != NULL )
				{
					
					childService = OSDynamicCast ( IOService, childObject );
					if ( childService == NULL )
						continue;
					
					STATUS_LOG ( ( "childService = %s\n", childService->getName ( ) ) );
					
					childService = OSDynamicCast ( IOBlockStorageDevice, childService );
					if ( childService != NULL )
					{
						
						// Keep a pointer to the parent of the block storage driver for
						// the call to messageClient().
						parent = childService;
						parent->retain ( );
						
						childList->release ( );
						childList = childService->getChildIterator ( gIOServicePlane );
						
						while ( ( childObject = childList->getNextObject ( ) ) != NULL )
						{
							
							childService = OSDynamicCast ( IOService, childObject );
							if ( childService == NULL )
								continue;
							
							STATUS_LOG ( ( "childService = %s\n", childService->getName ( ) ) );
							
							childService = OSDynamicCast ( IOBlockStorageDriver, childService );
							if ( childService == NULL )
								continue;
							
							// Ask the child nicely if it can close.  This allows it to say no
							// (if it's busy, has media mounted, etc.) without being destructive
							// to the state of the device.
							status = parent->messageClient ( kIOMessageServiceIsRequestingClose, ( IOBlockStorageDriver * ) childService );
							if ( status == kIOReturnSuccess )
							{
								
								message ( kSCSIServicesNotification_Suspend, NULL, NULL );
								ResetMediaCharacteristics ( );
								messageClients ( kSCSIServicesNotification_ExclusivityChanged );
								
							}
							
							else
							{
								
								ERROR_LOG ( ( "BlockStorageDriver wouldn't close, status = %d\n", status ) );
								super::HandleSetUserClientExclusivityState ( userClient, !state );
								
							}
							
							break;
							
						}
						
						// Make sure to drop the retain() from above
						parent->release ( );
						
					}
					
				}
				
				if ( childList != NULL )
					childList->release ( );
				
			}
			
		}
		
		else
		{
			
			// No media is present, so clear the status
			message ( kSCSIServicesNotification_Suspend, NULL, NULL );
			status = kIOReturnSuccess;
			
		}
		
	}
	
	
ErrorExit:
	
	
	ERROR_LOG ( ( "IOSCSIMultimediaCommandsDevice::HandleSetUserClientExclusivityState status = %d\n", status ) );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateStorageServiceNub - Creates the linkage object for IOStorageFamily
//								to use.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::CreateStorageServiceNub ( void )
{
	
	IOService * 	nub = NULL;
	
	if ( fSupportedDVDFeatures & kDVDFeaturesReadStructuresMask )
	{
		
		// We support DVD structure reads, so create the DVD nub
		nub = OSTypeAlloc ( IODVDServices );
	
	}
	
	else
	{
		
		// Create a CD nub instead
		nub = OSTypeAlloc ( IOCompactDiscServices );
	
	}
	require_nonzero ( nub, ErrorExit );
	
	nub->init ( );
	require ( nub->attach ( this ), ErrorExit );
	
	nub->start ( this );
	nub->release ( );
	
	return;
	
	
ErrorExit:
	
	
	PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::CreateStorageServiceNub failed" ) );
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineDeviceCharacteristics - Determines device characteristics
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::DetermineDeviceCharacteristics ( void )
{
	
	IOReturn	status	= kIOReturnSuccess;
	bool		result	= false;
	
	STATUS_LOG ( ( "%s::%s called.\n", getName ( ), __FUNCTION__ ) );
	
	status = DetermineIfMediaIsRemovable ( );
	require_success ( status, ErrorExit );
		
	status = DetermineDeviceFeatures ( );
	require_success ( status, ErrorExit );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineIfMediaIsRemovable - Determines if media is removable
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::DetermineIfMediaIsRemovable ( void )
{
	
	SCSIServiceResponse				serviceResponse 	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8							loop				= 0;
	UInt8							inquiryBufferCount 	= sizeof ( SCSICmd_INQUIRY_StandardData );
    SCSICmd_INQUIRY_StandardData * 	inquiryBuffer 		= NULL;
	IOMemoryDescriptor *			bufferDesc 			= NULL;
	SCSITaskIdentifier				request 			= NULL;
	IOReturn						status				= kIOReturnNoResources;
	
	inquiryBuffer = ( SCSICmd_INQUIRY_StandardData * ) IOMalloc ( inquiryBufferCount );
	require_nonzero ( inquiryBuffer, ErrorExit );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	inquiryBuffer,
													inquiryBufferCount,
													kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseBuffer );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	for ( loop = 0; ( loop < kMaxRetryCount ) && ( isInactive ( ) == false ); loop++ )
	{
		
		if ( INQUIRY ( 	request,
						bufferDesc,
						0,
						0,
						0x00,
						inquiryBufferCount,
						0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			status = kIOReturnSuccess;
			break;
			
		}
		
	}
	
	require_success ( status, ReleaseTask );
	
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
	
	// Save ANSI version of the device
	SetANSIVersion ( inquiryBuffer->VERSION & kINQUIRY_ANSI_VERSION_Mask );
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseDescriptor );	
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, ReleaseBuffer );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( inquiryBuffer, ErrorExit );
	require_nonzero_quiet ( inquiryBufferCount, ErrorExit );
	IOFree ( ( void * ) inquiryBuffer, inquiryBufferCount );
	inquiryBuffer = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineDeviceFeatures - Determines device features			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::DetermineDeviceFeatures ( void )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	status = GetDeviceConfiguration ( );
	check ( status == kIOReturnSuccess );
	
	status = GetMechanicalCapabilities ( );
	check ( status == kIOReturnSuccess );
	
	if ( status != kIOReturnSuccess )
	{
		
		// Since it responded as a SCSI Peripheral Device Type 05
		// it must at least be a CD-ROM...
		STATUS_LOG ( ( "Device supports CD-ROM\n" ) );
		status = kIOReturnSuccess;
		fSupportedCDFeatures |= kCDFeaturesReadStructuresMask;
		
	}
	
	// Check to see if the device supports power conditions.
	CheckPowerConditionsModePage ( );
	
	( void ) CheckForLowPowerPollingSupport ( );
	
	// Set Supported CD & DVD features flags
	setProperty ( kIOPropertySupportedCDFeatures, fSupportedCDFeatures, 32 );
	setProperty ( kIOPropertySupportedDVDFeatures, fSupportedDVDFeatures, 32 );
	
	fDeviceCharacteristicsDictionary->setObject (
								kIOPropertySupportedCDFeatures,
								getProperty ( kIOPropertySupportedCDFeatures ) );
	fDeviceCharacteristicsDictionary->setObject (
								kIOPropertySupportedDVDFeatures,
								getProperty ( kIOPropertySupportedDVDFeatures ) );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetDeviceConfigurationSize - 	Gets the size of the profile list
//									returned by GET_CONFIGURATION command
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::GetDeviceConfigurationSize ( UInt32 * size )
{
	
	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request			= NULL;
	IOMemoryDescriptor *		bufferDesc		= NULL;
	IOReturn					status			= kIOReturnNoResources;
	UInt8						featureHeader[kProfileDataLengthFieldSize] = { 0 };
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	featureHeader,
													kProfileDataLengthFieldSize,
													kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
		
	if ( GET_CONFIGURATION ( 	request,
								bufferDesc,
								0x02, /* Only one feature descriptor */
								kGetConfigurationProfile_ProfileList,
								kProfileDataLengthFieldSize,
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		// Swap to proper endian-ness since we are reading a multiple-byte field
		*size 	= OSReadBigInt32 ( &featureHeader[0], 0 ) + kProfileDataLengthFieldSize;
		status 	= kIOReturnSuccess;
		
		STATUS_LOG ( ( "size = %ld\n", *size ) );
		
		require_action ( ( *size > kProfileDataLengthFieldSize ),
						 ReleaseTask,
						 *size = 0; status = kIOReturnIOError );
		
	}
	
	else
	{
		
		*size = 0;
		status = kIOReturnIOError;
		
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
	
		
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetDeviceConfiguration - 	Gets the profile list returned by
//								GET_CONFIGURATION command and parses it.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::GetDeviceConfiguration ( void )
{
	
	IOMemoryDescriptor *	bufferDesc			= NULL;
	SCSIServiceResponse		serviceResponse 	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					numProfiles			= 0;
	UInt8 *					profilePtr			= NULL;
	SCSITaskIdentifier		request				= NULL;
	IOReturn				status				= kIOReturnSuccess;
	UInt32					actualProfileSize	= 0;
	
	status = GetDeviceConfigurationSize ( &actualProfileSize );
	require_success ( status, ErrorExit );
	
	// The number of profiles is the actual size minus the feature header
	// size minus the current profile size divided by the size of a profile
	// descriptor
	numProfiles = ( actualProfileSize - kProfileFeatureHeaderSize -
					kProfileDescriptorSize ) / kProfileDescriptorSize;
	
	require_nonzero_action ( numProfiles,
							 ErrorExit,
							 status = kIOReturnIOError );
	
	STATUS_LOG ( ( "numProfiles = %d\n", numProfiles ) );
	
	// If we have at least one profile, then we have at least a 16 byte
	// buffer. As an optimization, since the other GET_CONFIGURATION commands
	// after the one to get the profile list only need a max of 8 bytes
	// we use the same buffer and memory descriptor.
	
	profilePtr = ( UInt8 * ) IOMalloc ( actualProfileSize );
	require_nonzero_action ( profilePtr,
							 ErrorExit,
							 status = kIOReturnNoResources );
	bzero ( profilePtr, actualProfileSize );
	
	bufferDesc = IOMemoryDescriptor::withAddress (	profilePtr,
													actualProfileSize,
													kIODirectionIn );
	require_nonzero_action ( bufferDesc,
							 ReleaseBuffer,
							 status = kIOReturnNoResources );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request,
							 ReleaseDescriptor,
							 status = kIOReturnNoResources );
	
	if ( GET_CONFIGURATION ( 	request,
								bufferDesc,
								0x02, /* Only one feature descriptor */
								kGetConfigurationProfile_ProfileList,
								actualProfileSize,
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		// Adjust the pointer to be beyond the header and the current profile
		// to avoid duplicates
		status = ParseFeatureList ( numProfiles,
									profilePtr +
									kProfileFeatureHeaderSize +
									kProfileDescriptorSize );
		require_success ( status, ReleaseTask );
		
	}
	
	// Check for Analog Audio Play Support	
	if ( GET_CONFIGURATION ( 	request,
								bufferDesc,
								0x02, /* Only one feature descriptor */
								kGetConfigurationProfile_AnalogAudio,
								kProfileFeatureHeaderSize + 4,
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "Device supports Analog Audio\n" ) );
		fSupportedCDFeatures |= kCDFeaturesAnalogAudioMask;
		
	}
	
	if ( GET_CONFIGURATION ( request,
							 bufferDesc,
							 0x02, /* Only one feature descriptor */
							 kGetConfigurationProfile_IncrementalStreamedWrite,
							 kProfileFeatureHeaderSize + 4,
							 0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "Device supports Packet Writing\n" ) );
		fSupportedCDFeatures |= kCDFeaturesPacketWriteMask;
		
	}
	
	// Check for CD TAO support
	if ( GET_CONFIGURATION ( 	request,
								bufferDesc,
								0x02, /* Only one feature descriptor */
								kGetConfigurationProfile_CDTAO,
								kProfileFeatureHeaderSize + 8,
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "Device supports TAO Write\n" ) );
		fSupportedCDFeatures |= kCDFeaturesTAOWriteMask;
		
		if ( profilePtr[kProfileFeatureHeaderSize + 4] & kCDTAOTestWriteMask )
		{
			
			STATUS_LOG ( ( "Device supports TAO Test Write\n" ) );
			fSupportedCDFeatures |= kCDFeaturesTestWriteMask;
			
		}
		
		if ( profilePtr[kProfileFeatureHeaderSize + 4] & kCDTAOBUFWriteMask )
		{
			
			STATUS_LOG ( ( "Device supports TAO BUF\n" ) );
			fSupportedCDFeatures |= kCDFeaturesBUFWriteMask;
			
		}
		
	}
	
	// Check for CD Mastering support
	if ( GET_CONFIGURATION ( 	request,
								bufferDesc,
								0x02, /* Only one feature descriptor */
								kGetConfigurationProfile_CDMastering,
								kProfileFeatureHeaderSize + 8,
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
		
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		if ( profilePtr[kProfileFeatureHeaderSize + 4] & kCDMasteringCDRWMask )
		{
			
			fSupportedCDFeatures |= kCDFeaturesReWriteableMask;
			
		}
		
		else
		{
			
			// Check if device claimed support for CD-RW profile
			// but isn't really CD-RW
			if ( fSupportedCDFeatures & kCDFeaturesReWriteableMask )
			{
				
				ERROR_LOG ( ( "Device claimed support for CD-RW, but doesn't do CD-RW\n" ) );
				
				// Not really CD-RW. Get rid of the feature in the
				// feature list.
				fSupportedCDFeatures &= ~kCDFeaturesReWriteableMask;
				
			}
			
		}
		
		if ( profilePtr[kProfileFeatureHeaderSize + 4] & kCDMasteringTestWriteMask )
		{
			
			STATUS_LOG ( ( "Device supports CD-Mastering Test Write\n" ) );
			fSupportedCDFeatures |= kCDFeaturesTestWriteMask;
			
		}
		
		if ( profilePtr[kProfileFeatureHeaderSize + 4] & kCDMasteringRawWriteMask )
		{
			
			STATUS_LOG ( ( "Device supports CD-Mastering Raw Write\n" ) );
			fSupportedCDFeatures |= kCDFeaturesRawWriteMask;
			
		}
		
		if ( profilePtr[kProfileFeatureHeaderSize + 4] & kCDMasteringSAOWriteMask )
		{
			
			STATUS_LOG ( ( "Device supports CD-Mastering SAO Write\n" ) );
			fSupportedCDFeatures |= kCDFeaturesSAOWriteMask;
			
		}

		if ( profilePtr[kProfileFeatureHeaderSize + 4] & kCDMasteringBUFWriteMask )
		{
			
			STATUS_LOG ( ( "Device supports CD-Mastering BUF\n" ) );
			fSupportedCDFeatures |= kCDFeaturesBUFWriteMask;
			
		}
		
	}
		
	// Check for DVD-R Write support (on DVD-R, DVD-RW, or DVD-RAM drives only)
	if ( ( fSupportedDVDFeatures & ( kDVDFeaturesWriteOnceMask |
									 kDVDFeaturesRandomWriteableMask |
									 kDVDFeaturesReWriteableMask ) ) )
	{
		
		if ( GET_CONFIGURATION ( 	request,
									bufferDesc,
									0x02, /* Only one feature descriptor */
									kGetConfigurationProfile_DVDWrite,
									kProfileFeatureHeaderSize + 8,
									0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			if ( profilePtr[kProfileFeatureHeaderSize + 4] & kDVDRWMask )
			{
				
				STATUS_LOG ( ( "Device supports DVD-RW Write\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesReWriteableMask;
				
			}
			
			else
			{
				
				// Check if device claimed support for DVD-RW profile
				// but isn't really DVD-RW
				if ( fSupportedDVDFeatures & kDVDFeaturesReWriteableMask )
				{
					
					ERROR_LOG ( ( "Device claimed support for DVD-RW, but doesn't do DVD-RW\n" ) );
					
					// Not really DVD-RW. Get rid of the feature in the
					// feature list.
					fSupportedDVDFeatures &= ~kDVDFeaturesReWriteableMask;
					
				}
				
			}
			
			if ( profilePtr[kProfileFeatureHeaderSize + 4] & kDVDTestWriteMask )
			{
				
				STATUS_LOG ( ( "Device supports DVD-R Write Test Write\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesTestWriteMask;
				
			}
			
			if ( profilePtr[kProfileFeatureHeaderSize + 4] & kDVDBUFWriteMask )
			{
				
				STATUS_LOG ( ( "Device supports DVD-R Write BUF\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesBUFWriteMask;
				
			}
			
		}
		
	}
	
	// Check for DVD-CSS Support (on DVD-ROM drives only)
	if ( fSupportedDVDFeatures & kDVDFeaturesReadStructuresMask )
	{
		
		if ( GET_CONFIGURATION ( 	request,
									bufferDesc,
									0x02, /* Only one feature descriptor */
									kGetConfigurationProfile_DVDCSS,
									kProfileFeatureHeaderSize + 4,
									0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			STATUS_LOG ( ( "Device supports DVD-CSS\n" ) );
			fSupportedDVDFeatures |= kDVDFeaturesCSSMask;
			
		}
		
	}
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseDescriptor );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, ReleaseBuffer );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( profilePtr, ErrorExit );
	require_nonzero_quiet ( actualProfileSize, ErrorExit );
	IOFree ( profilePtr, actualProfileSize );
	profilePtr = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ParseFeatureList - 	Parses the profile list from the GET_CONFIGURATION
//							command.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ParseFeatureList (
								UInt32		numProfiles,
								UInt8 *		buffer )
{
	
	UInt16		profileNumber	= 0;
	UInt8 *		profilePtr		= NULL;
	IOReturn	status			= kIOReturnBadArgument;
	
	require_nonzero ( numProfiles, ErrorExit );
	require_nonzero ( buffer, ErrorExit );
	
	profilePtr = buffer;
	
	while ( numProfiles-- )
	{
		
		profileNumber = OSReadBigInt16 ( profilePtr, 0 );
		
		switch ( profileNumber )
		{
			
			case kGetConfigurationFeature_CDROM:
				STATUS_LOG ( ( "device supports CD-ROM\n" ) );
				fSupportedCDFeatures |= kCDFeaturesReadStructuresMask;
				break;
			
			case kGetConfigurationFeature_CDR:
				STATUS_LOG ( ( "device supports CD-R\n" ) );
				fSupportedCDFeatures |= kCDFeaturesWriteOnceMask;
				break;
			
			case kGetConfigurationFeature_CDRW:
				STATUS_LOG ( ( "device supports CD-RW\n" ) );
				fSupportedCDFeatures |= kCDFeaturesReWriteableMask;
				break;
			
			case kGetConfigurationFeature_DVDROM:
				STATUS_LOG ( ( "device supports DVD-ROM\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesReadStructuresMask;
				break;
			
			case kGetConfigurationFeature_DVDR:
				STATUS_LOG ( ( "device supports DVD-R\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesWriteOnceMask;
				break;
			
			case kGetConfigurationFeature_DVDRAM:
				STATUS_LOG ( ( "device supports DVD-RAM/DVD+RW\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesRandomWriteableMask;
				break;
			
			case kGetConfigurationFeature_DVDRW:
				STATUS_LOG ( ( "device supports DVD-RW\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesReWriteableMask;
				break;
			
			case kGetConfigurationFeature_DVDPlusRW:
				STATUS_LOG ( ( "device supports DVD+RW\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesPlusRWMask;
				break;
			
			case kGetConfigurationFeature_DVDPlusR:
				STATUS_LOG ( ( "device supports DVD+R\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesPlusRMask;
				break;
			
			default:
				STATUS_LOG ( ( "unknown drive type\n" ) );
				break;
			
		}
		
		profilePtr += kProfileDescriptorSize;
		
	}
	
	status = kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetMechanicalCapabilitiesSize - 	Obtains the size of the mechanical
//										capabilities buffer.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::GetMechanicalCapabilitiesSize ( UInt32 * size )
{
	
	UInt8					header[kModeSense10ParameterHeaderSize] = { 0 };
	IOMemoryDescriptor *	bufferDesc		= NULL;
	IOReturn				status			= kIOReturnNoResources;
	bool					use10Byte		= true;
	UInt8					bufferSize		= 0;
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	header,
													kModeSense10ParameterHeaderSize,
													kIODirectionIn );
	
	require_nonzero ( bufferDesc, ErrorExit );
	*size = 0;
		
	// Issue the mode sense commmand for the mechanical capabilities mode page.
	status = GetModeSense ( bufferDesc,
							kMechanicalCapabilitiesModePageCode,
							use10Byte ? kModeSense10ParameterHeaderSize : kModeSense6ParameterHeaderSize,
							&use10Byte );
	
	require_success ( status, ReleaseDescriptor );
	
	if ( use10Byte )
	{
		
		*size = OSReadBigInt16 ( header, 0 ) + sizeof ( UInt16 );
		bufferSize = kModeSense10ParameterHeaderSize;
		
	}
	
	else
	{
		
		*size = header[0] + sizeof ( UInt8 );
		bufferSize = kModeSense6ParameterHeaderSize;
		
	}
	
	require_action ( ( *size > bufferSize ),
					 ReleaseDescriptor,
					 status = kIOReturnInternalError );	
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetMechanicalCapabilities - 	Obtains the mechanical capabilities
//									of the device.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::GetMechanicalCapabilities ( void )
{
	
	UInt8 *							mechanicalCapabilities 	= NULL;
	IOBufferMemoryDescriptor *		bufferDesc 				= NULL;
	UInt32							actualSize				= 0;
	IOReturn						status					= kIOReturnError;
	UInt16							blockDescriptorLength 	= 0;
	UInt8							headerSize				= 0;
	UInt32							minBufferSize			= 0;
	UInt8							pageCode				= 0;
	bool							use10Byte				= true;
	
	status = GetMechanicalCapabilitiesSize ( &actualSize );
	require_success ( status, ErrorExit );
	
	bufferDesc = IOBufferMemoryDescriptor::withCapacity (
												actualSize,
												kIODirectionIn,
												true );
	require_nonzero ( bufferDesc, ErrorExit );
	
	mechanicalCapabilities = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( mechanicalCapabilities, actualSize );
	
	status = GetModeSense ( bufferDesc,
							kMechanicalCapabilitiesModePageCode,
							actualSize,
							&use10Byte );
	require_nonzero ( bufferDesc, ReleaseDescriptor );
	
	if ( use10Byte )
	{
		
		// We check to make sure there aren't any block descriptors. If there
		// are, we skip over them.
		blockDescriptorLength	= OSReadBigInt16 ( mechanicalCapabilities, 6 );
		headerSize				= kModeSense10ParameterHeaderSize;
		
	}
	
	else
	{
		
		blockDescriptorLength	= mechanicalCapabilities[3];
		headerSize				= kModeSense6ParameterHeaderSize;
		
	}
	
	// Ensure that our buffer is of the minimum correct size. We
	// know we will inspect at least kMechanicalCapabilitiesMinBufferSize
	// bytes of data in the page. We add this to the mode sense header,
	// any block descriptors, and the size of the first two bytes
	// (which include the PAGE CODE and PAGE LENGTH).
	minBufferSize = headerSize +
					blockDescriptorLength +
					2 +
					kMechanicalCapabilitiesMinBufferSize;
	
	require_action ( ( actualSize >= minBufferSize ),
					 ReleaseDescriptor,
					 status = kIOReturnInternalError );
	
	// Also, check that the first byte of the page is the
	// correct PAGE_CODE (kMechanicalCapabilitiesModePageCode).	
	pageCode = mechanicalCapabilities[headerSize + blockDescriptorLength] & 0x3F;
	
	require_action ( ( pageCode == kMechanicalCapabilitiesModePageCode ),
					 ReleaseDescriptor,
					 status = kIOReturnInternalError );
	
	// We're ok. Parse the capabilities now.
	status = ParseMechanicalCapabilities ( 	mechanicalCapabilities +
											headerSize +
											blockDescriptorLength +
											2 );
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ParseMechanicalCapabilities - Parses the mechanical capabilities
//									of the device.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::ParseMechanicalCapabilities (
										UInt8 * mechanicalCapabilities )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	require_nonzero_action ( mechanicalCapabilities,
							 ErrorExit,
							 status = kIOReturnBadArgument );
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesDVDROMMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports DVD-ROM\n" ) );
		fSupportedDVDFeatures |=
			( kDVDFeaturesReadStructuresMask | kDVDFeaturesCSSMask );
		
	}
	
	// Hop to the next byte so we can check more capabilities
	mechanicalCapabilities++;		
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesDVDRAMMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports DVD-RAM\n" ) );
		fSupportedDVDFeatures |= kDVDFeaturesRandomWriteableMask;
	
	}
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesDVDRMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports DVD-R\n" ) );
		fSupportedDVDFeatures |= kDVDFeaturesWriteOnceMask;
	
	}
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesTestWriteMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports Test Write\n" ) );
		fSupportedCDFeatures |= kCDFeaturesTestWriteMask;
		
		// Check to see if the drive supports DVD-R writing or DVD-RW writing.
		// If so, we should claim it supports TestWrite for DVD too since we
		// can't tell for sure...
		if ( ( fSupportedDVDFeatures & kDVDFeaturesWriteOnceMask ) ||
			 ( fSupportedDVDFeatures & kDVDFeaturesReWriteableMask ) )
		{
			
			fSupportedDVDFeatures |= kDVDFeaturesTestWriteMask;
			
		}
		
	}
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesCDRWMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports CD-RW\n" ) );
		fSupportedCDFeatures |= kCDFeaturesReWriteableMask;
	
	}
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesCDRMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports CD-R\n" ) );
		fSupportedCDFeatures |= kCDFeaturesWriteOnceMask;
	
	}
	
	// Hop to the next byte so we can check more capabilities
	mechanicalCapabilities++;		
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesBUFMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports BUF\n" ) );
		fSupportedCDFeatures |= kCDFeaturesBUFWriteMask;
		if ( fSupportedDVDFeatures != 0 )
		{
			
			fSupportedDVDFeatures |= kDVDFeaturesBUFWriteMask;
			
		}
		
	}
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesAnalogAudioMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports Analog Audio\n" ) );
		fSupportedCDFeatures |= kCDFeaturesAnalogAudioMask;
	
	}
	
	// Hop to the next byte so we can check more capabilities
	mechanicalCapabilities++;		
	
	if ( ( *mechanicalCapabilities & kMechanicalCapabilitiesCDDAStreamAccurateMask ) != 0 )
	{
		
		STATUS_LOG ( ( "Device supports CD-DA stream accurate reads\n" ) );
		fSupportedCDFeatures |= kCDFeaturesCDDAStreamAccurateMask;
	
	}
	
	// Since it responded to the CD Mechanical Capabilities Mode Page, it must at
	// least be a CD-ROM...
	STATUS_LOG ( ( "Device supports CD-ROM\n" ) );
	fSupportedCDFeatures |= kCDFeaturesReadStructuresMask;
	
	
ErrorExit:
	
		
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediaCharacteristics - Sets the media characteristics.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::SetMediaCharacteristics (
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
//	¥ ResetMediaCharacteristics - Resets the media characteristics.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::ResetMediaCharacteristics ( void )
{
	
	fMediaBlockSize			= 0;
	fMediaBlockCount		= 0;
	fCurrentDiscSpeed		= 0;
	fMediaPresent			= false;
	fMediaType				= kCDMediaTypeUnknown;
	fMediaIsWriteProtected	= true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PollForMedia - Tests for media presence.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::PollForMedia ( void )
{
	
	SCSIServiceResponse			serviceResponse		= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSI_Sense_Data				senseBuffer			= { 0 };
	UInt32						capacityData[2]		= { 0 };
	IOMemoryDescriptor *		bufferDesc			= NULL;
	SCSITaskIdentifier			request				= NULL;
	bool						mediaFound 			= false;
	bool						validSense			= false;
	bool						shouldEjectMedia	= false;
	
	OSBoolean *					keySwitchLocked 	= NULL;
	
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
			
			validSense = GetAutoSenseData ( request, &senseBuffer );
			if ( validSense == false )
			{
				
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				
				require_nonzero ( bufferDesc, ReleaseTask );
				
				// Get the sense data to determine if media is present.
				// This will eventually use the autosense data if the
				// Transport Protocol supports it else issue the REQUEST_SENSE.		  
				if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
				{
					
					// The command was successfully built, now send it
					serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
					
				}
				
				bufferDesc->release ( );
				bufferDesc = NULL;
				
			}
			
			if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x00 ) && 
				( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
			{
				
				mediaFound = true;
				
			}
			
			switch ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) )
			{
				
				case kSENSE_KEY_NOT_READY:
					
					// Check to make sure we shouldn't eject the disc.
					
					// Check for "Not Ready. Logical unit not ready, cause not reportable." and
					// "Not Ready. Medium not present."
					if ( ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
						   ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ) ||
						   ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) )
					{
						
						// Don't eject the disc
						break;
						
					}
					
					// Check for "Not Ready. Logical unit is in process of becoming ready."
					if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
						 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x01 ) )
					{
						
						// Message clients that the unit is attempting find out if media is really there.
						messageClients ( kIOMessageMediaAccessChange,
										 ( void * ) kMessageDeterminingMediaPresence );
						
						// Don't eject the disc
						break;
						
					}
					
					// Check for "Not Ready. Unable to recover table-of-contents."
					if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x57 ) &&
						 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
					{
						
						// Don't eject the disc
						break;
						
					}
					
					// Check for "Not Ready. Logical unit  not ready, format in progress."
					if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
						 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x04 ) )
					{
						
						// Don't eject the disc
						break;
						
					}
					
					// Everything else we should eject the disc
					shouldEjectMedia = true;
					break;
					
				case kSENSE_KEY_MEDIUM_ERROR:
				case kSENSE_KEY_HARDWARE_ERROR:
					// Should eject the disc
					shouldEjectMedia = true;
					break;	
				
				case kSENSE_KEY_BLANK_CHECK:
					
					if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x64 ) &&
						 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
					{
						
						// Don't eject the disc
						break;
						
					}
					
					// Should eject the disc
					shouldEjectMedia = true;
					break;
					
				default:
					break;
				
			}
			
		}
		
		else
		{
			
			mediaFound = true;
			
		}
		
	}
	
	if ( mediaFound == true )
	{
		
		keySwitchLocked = OSDynamicCast (
								OSBoolean,
								getProperty ( kAppleKeySwitchProperty ) );
		
		if ( keySwitchLocked != NULL )
		{
			
			// See if we should eject the media.
			if ( keySwitchLocked->isTrue ( ) )
			{
				shouldEjectMedia = true;
			}
			
		}
		
	}
	
	if ( shouldEjectMedia == true )
	{
		
		ERROR_LOG ( ( "IOSCSIMultimediaCommandsDevice::PollForMedia error occurred, ASC = 0x%02x, ASCQ = 0x%02x\n",
					   senseBuffer.ADDITIONAL_SENSE_CODE, senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
		
		if ( START_STOP_UNIT ( request, 0, 0, 1, 0, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			( void ) SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		// Bail out after ejecting the disc and notifying the user and anyone listening
		// for tray state change message. Also, make sure that mediaFound is set to
		// false so that we clean up properly.
		mediaFound = false;
		messageClients ( kIOMessageTrayStateChange,
						 ( void * ) kMessageTrayStateChangeRequestAccepted );
		
		#if NOT_READY_FOR_PRIMETIME
		
		kern_return_t	result = KERN_SUCCESS;
		
	    // NOTE! This call below depends on the hard coded path of this KEXT. Make sure
	    // that if the KEXT moves, this path is changed!
	    result = KUNCUserNotificationDisplayNotice ( 0,							// Timeout in seconds
	    											 0,							// Flags (for later usage)
	    											 kEmptyString,				// IconPath (not supported yet)
	    											 kEmptyString,				// SoundPath (not supported yet)
	    											 kLocalizationPath,			// LocalizationPath
	    											 kMediaEjectHeaderString,	// The header
	    											 kMediaEjectNoticeString,	// The notice - look in Localizable.strings
	    											 kOKButtonString );
		
		#endif
		
	}
	
	require_quiet ( mediaFound, ReleaseTask );
	
	messageClients ( kIOMessageMediaAccessChange,
					 ( void * ) kMessageFoundMedia );
	
	// If we got here, then we have found media
	if ( fMediaIsRemovable == true )
	{
		
		// Lock removable media
		if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, 1, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
	}
	
	bufferDesc = IOMemoryDescriptor::withAddress (	capacityData,
													8,
													kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseTask );
	
	// We found media, get its capacity
	if ( READ_CAPACITY ( 	request,
							bufferDesc,
							0,
							0x00,
							0,
							0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
				
		if ( capacityData[0] == 0 )
		{
			
			// If the last block address is zero, set the characteristics with
			// the returned data.
			SetMediaCharacteristics ( capacityData[1], capacityData[0] );
			
		}
		
		else
		{
			
			// If the last block address is not zero, increment it by one to 
			// get the total number of blocks on the media.
			SetMediaCharacteristics ( OSSwapBigToHostInt32 ( capacityData[1] ),
									  OSSwapBigToHostInt32 ( capacityData[0] ) + 1 );
			
		}
		
		STATUS_LOG ( ( "%s: Media blocks: 0x%x and block size: 0x%x\n",
						getName ( ), fMediaBlockCount, fMediaBlockSize ) );
		
	}
	
	else
	{
		
		ERROR_LOG ( ( "%s: Read Capacity failed\n", getName ( ) ) );
		if ( fMediaIsRemovable == true )
		{
			
			// Unlock removable media
			if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, 0, 0 ) == true )
			{
				
				// The command was successfully built, now send it
				serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
				
			}
			
		}
		
		goto ReleaseTask;
		
	}
	
	DetermineMediaType ( );
		
	CheckWriteProtection ( );
	
	fMediaPresent	= true;
	fMediaChanged	= true;
	fPollingMode 	= kPollingMode_Suspended;
	
	// Message up the chain that we have media
	messageClients ( kIOMessageMediaStateHasChanged,
					 ( void * ) kIOMediaStateOnline );
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseDescriptor );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, Exit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CheckForLowPowerPollingSupport - 	Checks for low power polling support
//										available on some ATAPI drives.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::CheckForLowPowerPollingSupport ( void )
{
	
	IOReturn					status 				= kIOReturnNoResources;
	OSBoolean *					boolValue 			= NULL;
	const OSSymbol * 			key					= NULL;
	OSDictionary *				dict				= NULL;
	OSString *					internalString 		= NULL;
	AppleFeatures				appleFeaturesBuffer	= { 0 };
	IOMemoryDescriptor *		bufferDesc			= NULL;
	bool						use10Byte			= true;
	
	bufferDesc = IOMemoryDescriptor::withAddress (  &appleFeaturesBuffer,
													sizeof ( appleFeaturesBuffer ),
													kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit ); 
	
	status = GetModeSense ( bufferDesc,
							kAppleFeaturesModePageCode,
							sizeof ( appleFeaturesBuffer ),
							&use10Byte );
	require_success_quiet ( status, ReleaseDescriptor );
	
	// Swap to proper endian-ness since we are reading a multiple-byte field		
	if ( ( OSReadBigInt32 ( &appleFeaturesBuffer.signature, 0 ) == '.App' ) &&
		 ( appleFeaturesBuffer.supportsLowPowerPoll ) )
	{
		
		// Device supports the Apple specific low-power polling. Make sure
		// to flag that attribute.
		STATUS_LOG ( ( "Device Supports Low Power Polling\n" ) );
		fDeviceSupportsLowPowerPolling = true;
		
	}
	
	
ReleaseDescriptor:
	
	
	require_nonzero ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	if ( fDeviceSupportsLowPowerPolling )
	{
		
		fDeviceSupportsLowPowerPolling = IsProtocolServiceSupported ( 
												kSCSIProtocolFeature_ProtocolSpecificPolling,
												NULL );
		
	}
	
	dict = GetProtocolCharacteristicsDictionary ( );
	if ( dict != NULL )
	{
		
		key = OSSymbol::withCString ( kIOPropertyPhysicalInterconnectLocationKey );
		if ( key != NULL )
		{
			
			internalString = OSDynamicCast ( OSString, dict->getObject ( key ) );
			key->release ( );
			key = NULL;
			
		}
		
	}
	
    if ( ( internalString == NULL ) || ( !internalString->isEqualTo ( kIOPropertyInternalKey ) ) )
	{
		
		// Not an internal drive, let's not use the power conditions mode page
		// info or low power polling
		fDeviceSupportsPowerConditions = false;
		fDeviceSupportsLowPowerPolling = false;
		
	}
	
	// If the drive is not a DVD drive, we won't use power conditions,
	// we'll either use ATA style sleep commands for ATAPI drives or just
	// spin down the drives if they are external.
	if ( fSupportedDVDFeatures == 0 )
	{
		fDeviceSupportsPowerConditions = false;
	}
	
	boolValue = OSBoolean::withBoolean ( fDeviceSupportsLowPowerPolling );
	if ( boolValue != NULL )
	{
		
		fDeviceCharacteristicsDictionary->setObject ( kAppleLowPowerPollingKey, boolValue );
		boolValue->release ( );
		boolValue = NULL;
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediaType - 	Determines the type of media in the device.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::DetermineMediaType ( void )
{
	
	bool	mediaTypeFound = false;
	
	mediaTypeFound = CheckForDVDMediaType ( );	
	if ( mediaTypeFound == false )
	{
		
		mediaTypeFound = CheckForCDMediaType ( );
		
	}
	
	messageClients ( kIOMessageMediaAccessChange,
					( void * ) kMessageMediaTypeDetermined );
	
	// Set to maximum speed
	SetMediaAccessSpeed ( 0xFFFF );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CheckForDVDMediaType - 	Determines if the type of media is a DVD media
//								type.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::CheckForDVDMediaType ( void )
{
	
	SCSITaskIdentifier			request				= NULL;
	SCSIServiceResponse			serviceResponse		= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOBufferMemoryDescriptor *	bufferDesc			= NULL;
	bool						mediaTypeFound		= false;
	UInt8 *						buffer				= NULL;
	UInt32						bufferSize			= 0;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::CheckForDVDMediaType ENTER\n" ) );
	
	// If device supports READ_DVD_STRUCTURE, issue one to find
	// out if the media is a DVD media type
	require_quiet ( ( fSupportedDVDFeatures & kDVDFeaturesReadStructuresMask ), Exit );
	
	bufferSize = max ( kDVDPhysicalFormatInfoBufferSize, kProfileFeatureHeaderSize + kProfileDescriptorSize );
	bufferDesc = IOBufferMemoryDescriptor::withCapacity ( bufferSize, kIODirectionIn );
	require_nonzero ( bufferDesc, Exit );
	
	buffer = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( buffer, bufferSize );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	if ( READ_DVD_STRUCTURE ( 	request,
								bufferDesc,
								0x00,
								0x00,
								kDVDStructureFormatPhysicalFormatInfo,
								kDVDPhysicalFormatInfoBufferSize,
								0x00,
								0x00 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		DVDPhysicalFormatInfo *	physicalFormatInfo = ( DVDPhysicalFormatInfo * ) buffer;
		
		switch ( physicalFormatInfo->bookType )
		{
			
			case kDVDBookTypeROM:
				STATUS_LOG ( ( "fMediaType = DVD-ROM\n" ) );
				fMediaType = kDVDMediaTypeROM;
				break;
				
			case kDVDBookTypeRAM:
				STATUS_LOG ( ( "fMediaType = DVD-RAM\n" ) );
				fMediaType = kDVDMediaTypeRAM;
				break;
				
			case kDVDBookTypeR:
				STATUS_LOG ( ( "fMediaType = DVD-R\n" ) );
				fMediaType = kDVDMediaTypeR;
				break;
				
			case kDVDBookTypeRW:
				STATUS_LOG ( ( "fMediaType = DVD-RW\n" ) );
				fMediaType = kDVDMediaTypeRW;
				break;
				
			case kDVDBookTypePlusRW:
				STATUS_LOG ( ( "fMediaType = DVD+RW\n" ) );
				fMediaType = kDVDMediaTypePlusRW;
				break;
				
			case kDVDBookTypePlusR:
				STATUS_LOG ( ( "fMediaType = DVD+R\n" ) );
				fMediaType = kDVDMediaTypePlusR;
				break;
			
			case kDVDBookTypePlusRDoubleLayer:
				STATUS_LOG ( ( "fMediaType = DVD+R\n" ) );
				fMediaType = kDVDMediaTypePlusR;
				break;
				
			default:
				STATUS_LOG ( ( "physicalFormatInfo->bookType = %d\n", physicalFormatInfo->bookType ) );
				STATUS_LOG ( ( "fMediaType = kCDMediaTypeUnknown\n" ) );
				break;
			
		}
			
	}
	
	if ( ( fMediaType == kCDMediaTypeUnknown ) &&
		 ( fSupportedDVDFeatures & kDVDFeaturesPlusRWMask ) )
	{
		
		STATUS_LOG ( ( "Check for DVD+RW media\n" ) );
		
		bzero ( buffer, bufferSize );
		
		if ( GET_CONFIGURATION ( 	request,
									bufferDesc,
									0x02, // Only one feature descriptor
									kGetConfigurationProfile_DVDPlusRW,
									kProfileFeatureHeaderSize + kProfileDescriptorSize,
									0 ) == true )
		
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			UInt8 *		profilePtr;
			
			profilePtr = buffer + kProfileFeatureHeaderSize;
			
			if ( profilePtr[2] & 0x01 )
			{
				
				STATUS_LOG ( ( "fMediaType = DVD+RW\n" ) );
				fMediaType = kDVDMediaTypePlusRW;
				
			}
			
		}
		
	}
	
	if ( ( fMediaType == kCDMediaTypeUnknown ) &&
		 ( fSupportedDVDFeatures & kDVDFeaturesPlusRMask ) )
	{
		
		STATUS_LOG ( ( "Check for DVD+R media\n" ) );
		
		bzero ( buffer, bufferSize );
		
		if ( GET_CONFIGURATION ( 	request,
									bufferDesc,
									0x02, // Only one feature descriptor
									kGetConfigurationProfile_DVDPlusR,
									kProfileFeatureHeaderSize + kProfileDescriptorSize,
									0 ) == true )
		
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			UInt8 *		profilePtr;
			
			profilePtr = buffer + kProfileFeatureHeaderSize;
			
			if ( profilePtr[2] & 0x01 )
			{
				
				STATUS_LOG ( ( "fMediaType = DVD+R\n" ) );
				fMediaType = kDVDMediaTypePlusR;
				
			}
			
		}
		
	}
	
	if ( ( fMediaType == kCDMediaTypeUnknown ) &&
		 ( fSupportedDVDFeatures & kDVDFeaturesPlusRMask ) )
	{
		
		STATUS_LOG ( ( "Check for DVD+R DL media\n" ) );
		
		bzero ( buffer, bufferSize );
		
		if ( GET_CONFIGURATION ( 	request,
									bufferDesc,
									0x02, // Only one feature descriptor
									kGetConfigurationProfile_DVDPlusRDoubleLayer,
									kProfileFeatureHeaderSize + kProfileDescriptorSize,
									0 ) == true )
		
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			UInt8 *		profilePtr;
			
			profilePtr = buffer + kProfileFeatureHeaderSize;
			
			if ( profilePtr[2] & 0x01 )
			{
				
				STATUS_LOG ( ( "fMediaType = DVD+R\n" ) );
				fMediaType = kDVDMediaTypePlusR;
				
			}
			
		}
		
	}
	
	if ( fMediaType != kCDMediaTypeUnknown )
	{
		mediaTypeFound = true;
	}
	
	ReleaseSCSITask ( request );
	
	
ReleaseDescriptor:
	
	
	require_nonzero ( bufferDesc, Exit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
Exit:
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::CheckForDVDMediaType EXIT\n" ) );
	
	return mediaTypeFound;
	
}
		

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CheckForCDMediaType - Determines if the type of media is a CD media
//							type.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::CheckForCDMediaType ( void )
{
	
	UInt8 *						buffer			= NULL;
	IOBufferMemoryDescriptor * 	bufferDesc		= NULL;
	SCSITaskIdentifier			request			= NULL;
	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	bool						mediaTypeFound	= false;
	
	bufferDesc = IOBufferMemoryDescriptor::withCapacity ( 4, kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	buffer = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	// Is this a writeable drive?
	if ( fSupportedCDFeatures & kCDFeaturesWriteOnceMask )
	{
		
		// A writeable drive should support the READ_DISC_INFORMATION command.
		// We can determine media type using it.
		if ( READ_DISC_INFORMATION ( request,
									 bufferDesc,
									 bufferDesc->getLength ( ),
									 0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			switch ( buffer[kDiscStatusOffset] & kDiscStatusMask )
			{
				
				case kDiscStatusEmpty:
				{
					
					STATUS_LOG ( ( "Blank media\n" ) );
					fMediaBlockCount = 0;
					STATUS_LOG ( ( "fMediaType = CD-R\n" ) );
					fMediaType = kCDMediaTypeR;
					mediaTypeFound = true;
					break;
					
				}
				
				case kDiscStatusIncomplete:
				{
					
					STATUS_LOG ( ( "fMediaType = CD-R\n" ) );
					fMediaType = kCDMediaTypeR;
					mediaTypeFound = true;
					break;
					
				}
				
				case kDiscStatusComplete:					
				{
					
					STATUS_LOG ( ( "fMediaType = CD-ROM\n" ) );
					fMediaType = kCDMediaTypeROM;
					mediaTypeFound = true;
					break;
					
				}
				
				case kDiscStatusOther:
				default:
				{
					break;
				}
				
			}
			
			// Check to see if it is erasable. If it is, it's CD-RW media
			if ( buffer[kDiscStatusOffset] & kDiscStatusErasableMask )
			{
				
				STATUS_LOG ( ( "fMediaType = CD-RW\n" ) );
				fMediaType = kCDMediaTypeRW;
				mediaTypeFound = true;
				
			}
			
		}
		
	}
	
	// Were we able to determine the media type yet?
	require_quiet ( ( mediaTypeFound == false ), ReleaseTask );
	
	// Media type not yet determined, check for CD-ROM media.
	// Issue a READ_TOC_PMA_ATIP to find out if the media is
	// finalized or not.
	if ( READ_TOC_PMA_ATIP ( 	request,
								bufferDesc,
								0x00,
								0x00,
								0x00,
								bufferDesc->getLength ( ),
								0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "fMediaType = CD-ROM\n" ) );
		fMediaType = kCDMediaTypeROM;		
		mediaTypeFound = true;
		
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
	
	
	return mediaTypeFound;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CheckWriteProtection - Determines write protect state of media.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::CheckWriteProtection ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	IOMemoryDescriptor *	bufferDesc		= NULL;
	UInt8					buffer[16]		= { 0 };
	
	// Assume it is write protected
	fMediaIsWriteProtected = true;
	
	require_quiet ( ( fMediaType == kDVDMediaTypeRAM ), Exit );
	require_quiet ( ( fSupportedDVDFeatures & kDVDFeaturesRandomWriteableMask ), Exit );
	
	bufferDesc = IOMemoryDescriptor::withAddress (	buffer,
													sizeof ( buffer ),
													kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	if ( GET_CONFIGURATION ( request,
							 bufferDesc,
							 0x02, /* Only one feature descriptor */
							 kGetConfigurationProfile_RandomWrite,
							 sizeof ( buffer ),
							 0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		// The current bit in the Random Writable Descriptor
		// tells us whether the disc is write protected. It is located
		// at byte 2 of the Random Writable Descriptor Feature page
		if ( buffer[kProfileFeatureHeaderSize + 2] & kRandomWritableProtectionMask )
		{
			
			fMediaIsWriteProtected = false;
			
		}
		
	}
	
	ReleaseSCSITask ( request );
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( bufferDesc, Exit );
	bufferDesc->release ( );	
	bufferDesc = NULL;
	
	
ErrorExit:
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - DEPRECATED.										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::IssueRead (
							IOMemoryDescriptor *	buffer,
							UInt64					startBlock,
							UInt64					blockCount )
{
	return kIOReturnUnsupported;
}



//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueWrite - DEPRECATED.										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::IssueWrite (
							IOMemoryDescriptor *	buffer,
							UInt64					startBlock,
							UInt64					blockCount )
{
	return kIOReturnUnsupported;
}



//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - Issues an asynchronous read request.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::IssueRead (
							IOMemoryDescriptor *	buffer,
							void *					clientData,
							UInt64					startBlock,
							UInt64					blockCount )
{
	
	IOReturn 				status	= kIOReturnNoResources;
	SCSITaskIdentifier		request	= NULL;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( READ_10 ( 	request,
					buffer,
					fMediaBlockSize,
					0,
					0,
					0,
					startBlock,
					blockCount,
					0 ) == true )
	{
		
		SetApplicationLayerReference ( request, clientData );
		
		// The command was successfully built, now send it
		SendCommand ( request,
					  fReadTimeoutDuration,
					  &IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete );
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		status = kIOReturnBadArgument;
		ReleaseSCSITask ( request );
		request = NULL;
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueWrite - Issues an asynchronous write request.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIMultimediaCommandsDevice::IssueWrite (
							IOMemoryDescriptor *	buffer,
							void *					clientData,
							UInt64					startBlock,
							UInt64					blockCount )
{
	
	IOReturn 				status	= kIOReturnNoResources;
	SCSITaskIdentifier		request	= NULL;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( WRITE_10 ( request, 
					buffer,
					fMediaBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
	{
		
		SetApplicationLayerReference ( request, clientData );
		
		// The command was successfully built, now send it
		SendCommand ( request,
					  fWriteTimeoutDuration,
					  &IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete );
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		status = kIOReturnBadArgument;
		ReleaseSCSITask ( request );
		request = NULL;
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ConvertBCDToHex - Converts BCD values to Hex					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
IOSCSIMultimediaCommandsDevice::ConvertBCDToHex ( UInt8 binaryCodedDigit )
{
	
	UInt8	accumulator = 0;
	UInt8	x			= 0;
	
	// Divide by 16 (equivalent to >> 4)
	x = ( binaryCodedDigit >> 4 ) & 0x0F;
	if ( x > 9 )
	{
		
		return binaryCodedDigit;
		
	}
	
	accumulator = 10 * x;
	x = binaryCodedDigit & 0x0F;
	if ( x > 9 )
	{
		
		return binaryCodedDigit;
		
	}
	
	accumulator += x;
	
	return accumulator;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteCompletion - Completion routine for read/write requests.
//															 		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::AsyncReadWriteCompletion (
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
		
		// Either the task never completed or we have a status other than GOOD,
		// return an error.		
		if ( GetTaskStatus ( completedTask ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			SCSI_Sense_Data		senseDataBuffer;
			bool				senseIsValid;
			
			senseIsValid = GetAutoSenseData ( completedTask, &senseDataBuffer, sizeof ( senseDataBuffer ) );
			if ( senseIsValid )
			{
				
				IOLog ( "SAM Multimedia: READ or WRITE failed, ASC = 0x%02x, ASCQ = 0x%02x\n", 
				senseDataBuffer.ADDITIONAL_SENSE_CODE,
				senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER );
				
				if ( ( senseDataBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) ||
					 ( ( senseDataBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) &&
					   ( senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ) )
				{
					
					// Message up the chain that we do not have media
					messageClients ( kIOMessageMediaStateHasChanged,
									( void * ) kIOMediaStateOffline );
					
					ResetMediaCharacteristics ( );
					fPollingMode = kPollingMode_NewMedia;
					EnablePolling ( );
					
				}
				
				if ( ( senseDataBuffer.ADDITIONAL_SENSE_CODE == 0x64 ) &&
					 ( senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
				{
					
					// The caller is trying to read blocks for which the block type
					// doesn't match.
					status = kIOReturnUnsupportedMode;
					
				}
				
				if ( ( senseDataBuffer.ADDITIONAL_SENSE_CODE == 0x6F ) &&
					 ( ( senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x01 ) ||
					   ( senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) ||
					   ( senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x03 ) ) )
				{	
					
					// The key is no longer present for reading these
					// blocks->privileges error.
					status = kIOReturnNotPrivileged;
					
				}
				
			}
			
		}
		
	}
	
	if ( fSupportedDVDFeatures & kDVDFeaturesReadStructuresMask )
	{
		IODVDServices::AsyncReadWriteComplete ( clientData, status, actCount );
	}
	
	else
	{	
		IOCompactDiscServices::AsyncReadWriteComplete ( clientData, status, actCount );
	}
	
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
IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete (
										SCSITaskIdentifier request )
{
	
	IOSCSIMultimediaCommandsDevice	*	taskOwner = NULL;
	
	require_nonzero ( request, ErrorExit );
	
	taskOwner = OSDynamicCast ( IOSCSIMultimediaCommandsDevice, sGetOwnerForTask ( request ) );
	require_nonzero ( taskOwner, ErrorExit );
	
	taskOwner->AsyncReadWriteCompletion ( request );
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sPollForMedia - 	Static routine to poll for media.	[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::sPollForMedia (
						void *	pdtDriver,
						void *	refCon )
{
	
	IOSCSIMultimediaCommandsDevice *	driver;
	
	driver = ( IOSCSIMultimediaCommandsDevice * ) pdtDriver;
	
	driver->PollForMedia ( );
	
	if ( driver->fPollingMode != kPollingMode_Suspended )
	{
		
		// schedule the poller again
		driver->EnablePolling ( );
		
	}
	
	// drop the retain associated with this poll
	driver->release ( );
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUsed ( IOSCSIMultimediaCommandsDevice,  1 ); 	/* ReadTOC */
OSMetaClassDefineReservedUsed ( IOSCSIMultimediaCommandsDevice,  2 );	/* ReadDiscInfo */
OSMetaClassDefineReservedUsed ( IOSCSIMultimediaCommandsDevice,  3 );	/* ReadTrackInfo */
OSMetaClassDefineReservedUsed ( IOSCSIMultimediaCommandsDevice,  4 );	/* PowerDownHandler */
OSMetaClassDefineReservedUsed ( IOSCSIMultimediaCommandsDevice,  5 );	/* AsyncReadWriteCompletion */

OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice,  6 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice,  7 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice,  8 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice,  9 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice, 10 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice, 11 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice, 12 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIMultimediaCommandsDevice, 16 );