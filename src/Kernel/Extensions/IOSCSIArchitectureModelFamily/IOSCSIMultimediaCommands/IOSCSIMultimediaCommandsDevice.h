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


#ifndef _IOKIT_IO_SCSI_MULTIMEDIA_COMMANDS_DEVICE_H_
#define _IOKIT_IO_SCSI_MULTIMEDIA_COMMANDS_DEVICE_H_

#if KERNEL
#include <IOKit/IOTypes.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <IOKit/storage/IOStorageDeviceCharacteristics.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


// Message constants
#define kIOMessageTrayStateChange			0x69000035
#define kIOMessageTrayStateHasChanged		kIOMessageTrayStateChange // DEPRECATED, use kIOMessageTrayStateChange instead

// Message values for kIOMessageTrayStateChange
enum
{
	kMessageTrayStateChangeRequestAccepted	= 0,
	kMessageTrayStateChangeRequestRejected	= 1
};

#define kIOMessageMediaAccessChange			0x69000036

// Message values for kIOMessageMediaAccessChange
enum
{
	kMessageDeterminingMediaPresence	= 0,
	kMessageFoundMedia					= 1,
	kMessageMediaTypeDetermined			= 2
};

// IOKit property keys and constants
#define	kIOPropertySupportedCDFeatures		kIOPropertySupportedCDFeaturesKey
#define	kIOPropertySupportedDVDFeatures		kIOPropertySupportedDVDFeaturesKey
#define kIOPropertyLowPowerPolling			"Low Power Polling"

typedef UInt32 CDFeatures;
enum
{
	kCDFeaturesAnalogAudioBit			= 0,	// Analog Audio playback
	kCDFeaturesReadStructuresBit		= 1,	// CD-ROM
	kCDFeaturesWriteOnceBit				= 2,	// CD-R
	kCDFeaturesReWriteableBit			= 3,	// CD-R/W
	kCDFeaturesCDDAStreamAccurateBit	= 4,	// CD-DA stream accurate
	kCDFeaturesPacketWriteBit			= 5,	// Packet Writing
	kCDFeaturesTAOWriteBit				= 6,	// CD Track At Once
	kCDFeaturesSAOWriteBit				= 7,	// CD Mastering - Session At Once
	kCDFeaturesRawWriteBit				= 8,	// CD Mastering - Raw
	kCDFeaturesTestWriteBit				= 9,	// CD Mastering/TAO - Test Write
	kCDFeaturesBUFWriteBit				= 10	// CD Mastering/TAO - Buffer Underrun Free
};

enum
{
	kCDFeaturesAnalogAudioMask 			= (1 << kCDFeaturesAnalogAudioBit),
	kCDFeaturesReadStructuresMask		= (1 << kCDFeaturesReadStructuresBit),
	kCDFeaturesWriteOnceMask			= (1 << kCDFeaturesWriteOnceBit),
	kCDFeaturesReWriteableMask			= (1 << kCDFeaturesReWriteableBit),
	kCDFeaturesCDDAStreamAccurateMask	= (1 << kCDFeaturesCDDAStreamAccurateBit),
	kCDFeaturesPacketWriteMask			= (1 << kCDFeaturesPacketWriteBit),
	kCDFeaturesTAOWriteMask				= (1 << kCDFeaturesTAOWriteBit),
	kCDFeaturesSAOWriteMask				= (1 << kCDFeaturesSAOWriteBit),
	kCDFeaturesRawWriteMask				= (1 << kCDFeaturesRawWriteBit),
	kCDFeaturesTestWriteMask			= (1 << kCDFeaturesTestWriteBit),
	kCDFeaturesBUFWriteMask				= (1 << kCDFeaturesBUFWriteBit)
};

typedef	UInt32 DVDFeatures;
enum
{
	kDVDFeaturesCSSBit 					= 0,	// DVD-CSS
	kDVDFeaturesReadStructuresBit 		= 1,	// DVD-ROM
	kDVDFeaturesWriteOnceBit			= 2,	// DVD-R
	kDVDFeaturesRandomWriteableBit		= 3,	// DVD-RAM
	kDVDFeaturesReWriteableBit			= 4,	// DVD-RW
	kDVDFeaturesTestWriteBit			= 5,	// DVD-R Write - Test Write
	kDVDFeaturesBUFWriteBit				= 6,	// DVD-R Write - Buffer Underrun Free
	kDVDFeaturesPlusRBit				= 7,	// DVD+R
	kDVDFeaturesPlusRWBit				= 8		// DVD+RW (implies backgound format support)
};

enum
{
	kDVDFeaturesCSSMask 				= (1 << kDVDFeaturesCSSBit),
	kDVDFeaturesReadStructuresMask 		= (1 << kDVDFeaturesReadStructuresBit),
	kDVDFeaturesWriteOnceMask			= (1 << kDVDFeaturesWriteOnceBit),
	kDVDFeaturesRandomWriteableMask		= (1 << kDVDFeaturesRandomWriteableBit),
	kDVDFeaturesReWriteableMask			= (1 << kDVDFeaturesReWriteableBit),
	kDVDFeaturesTestWriteMask			= (1 << kDVDFeaturesTestWriteBit),
	kDVDFeaturesBUFWriteMask			= (1 << kDVDFeaturesBUFWriteBit),
	kDVDFeaturesPlusRMask				= (1 << kDVDFeaturesPlusRBit),
	kDVDFeaturesPlusRWMask				= (1 << kDVDFeaturesPlusRWBit)
};

enum
{
	kDiscStatusEmpty 					= 0,
	kDiscStatusIncomplete				= 1,
	kDiscStatusComplete					= 2,
	kDiscStatusOther					= 3,
	kDiscStatusMask						= 0x03,
	kDiscStatusErasableMask				= 0x10
};


#if defined(KERNEL) && defined(__cplusplus)

// MMC power states as defined in T10:1228D SCSI Multimedia Commands - 2 (MMC-2)
// Revision 11a, August 30, 1999, page 312 (Annex F).
enum
{
	kMMCPowerStateSystemSleep	= 0,
	kMMCPowerStateSleep 		= 1,
	kMMCPowerStateStandby 		= 2,
	kMMCPowerStateIdle			= 3,
	kMMCPowerStateActive		= 4,
	kMMCNumPowerStates			= 5
};

enum
{
	kMediaStateUnlocked	= 0,
	kMediaStateLocked 	= 1
};


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// General IOKit headers
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOStorage.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IODVDTypes.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/IOSCSIPrimaryCommandsDevice.h>

// Forward definitions for internal use only classes
class SCSIMultimediaCommands;
class SCSIBlockCommands;

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class IOSCSIMultimediaCommandsDevice : public IOSCSIPrimaryCommandsDevice
{
	
	OSDeclareAbstractStructors ( IOSCSIMultimediaCommandsDevice )
	
private:

    SCSIMultimediaCommands *	fSCSIMultimediaCommandObject;
    SCSIMultimediaCommands *	GetSCSIMultimediaCommandObject ( void );
	
    SCSIBlockCommands *			fSCSIBlockCommandObject;
    SCSIBlockCommands *			GetSCSIBlockCommandObject ( void );

	static void		AsyncReadWriteComplete ( SCSITaskIdentifier completedTask );
	
protected:
	
    // Reserve space for future expansion.
    struct IOSCSIMultimediaCommandsDeviceExpansionData
	{
		IONotifier *		fPowerDownNotifier;
	};
    IOSCSIMultimediaCommandsDeviceExpansionData * fIOSCSIMultimediaCommandsDeviceReserved;
	
	#define fPowerDownNotifier 	fIOSCSIMultimediaCommandsDeviceReserved->fPowerDownNotifier
	
	// This method will retreive the SCSI Primary Command Set object for
	// the class.  For subclasses, this will be overridden using a
	// dynamic cast on the subclasses base command set object.
	virtual SCSIPrimaryCommands *	GetSCSIPrimaryCommandObject ( void );

	CDFeatures						fSupportedCDFeatures;
	DVDFeatures						fSupportedDVDFeatures;

	UInt16							fCurrentDiscSpeed;	
	bool							fMediaChanged; 
	bool							fMediaPresent;
	
	// The byte count of each physical block on the media.
	UInt32							fMediaBlockSize;
	
	// The total number of blocks of fMediaBlockSize on the media.
	UInt32							fMediaBlockCount;
	
	// Flags used to indicate device feature
	bool							fMediaIsRemovable;
	bool							fMediaIsWriteProtected;
	UInt32							fMediaType;
	
	thread_call_t					fPollingThread;
	bool							fDeviceSupportsLowPowerPolling;
	bool							fLowPowerPollingEnabled;
	UInt32							fPollingMode;
	
	enum
	{
		kPollingMode_Suspended		= 0,
		kPollingMode_NewMedia 		= 1,
		kPollingMode_MediaRemoval	= 2
	};
	
	virtual void 		CreateStorageServiceNub ( void );
	virtual bool		DetermineDeviceCharacteristics ( void );
	virtual IOReturn	DetermineIfMediaIsRemovable ( void );
	virtual IOReturn	DetermineDeviceFeatures ( void );
	virtual void		DetermineMediaType ( void );
	virtual bool		CheckForDVDMediaType ( void );
	virtual bool		CheckForCDMediaType ( void );
	virtual void		PollForMedia ( void );
	virtual void		EnablePolling ( void );
	virtual void		DisablePolling ( void );
	virtual void		CheckWriteProtection ( void );
	virtual bool		ClearNotReadyStatus ( void );
		
	virtual IOReturn	GetDeviceConfiguration ( void );
	virtual IOReturn	GetDeviceConfigurationSize ( UInt32 * size );
	virtual IOReturn	ParseFeatureList ( UInt32 numProfiles, UInt8 * firstFeaturePtr );
	
	virtual IOReturn	GetMechanicalCapabilities ( void );
	virtual IOReturn	GetMechanicalCapabilitiesSize ( UInt32 * size );
	virtual IOReturn	ParseMechanicalCapabilities ( UInt8 * mechanicalCapabilitiesPtr );

	virtual IOReturn	CheckForLowPowerPollingSupport ( void );
	virtual IOReturn	GetCurrentPowerStateOfDrive ( UInt32 * powerState );

	virtual IOReturn	IssueRead ( IOMemoryDescriptor *	buffer,
									UInt64					startBlock,
									UInt64					blockCount );

	virtual IOReturn	IssueRead ( IOMemoryDescriptor *	buffer,
									void *					clientData,
									UInt64					startBlock,
									UInt64					blockCount );
	
	virtual IOReturn	IssueWrite ( IOMemoryDescriptor *	buffer,
									UInt64					startBlock,
									UInt64					blockCount );
	
	virtual IOReturn	IssueWrite ( IOMemoryDescriptor *	buffer,
									void *					clientData,
									UInt64					startBlock,
									UInt64					blockCount );

    virtual void		SetMediaCharacteristics ( UInt32 blockSize, UInt32 blockCount );
 	virtual void		ResetMediaCharacteristics ( void );

	UInt8				ConvertBCDToHex ( UInt8 binaryCodedDigit );

	// ------ User Client Support ------
	
	virtual IOReturn 	HandleSetUserClientExclusivityState ( IOService * userClient, bool state );

	// ----- Power Management Support ------
	
	// We override this method to set our power states and register ourselves
	// as a power policy maker.
	virtual void 		InitializePowerManagement ( IOService * provider );
	
	// We override this method so that when we register for power management,
	// we go to our active power state (which the drive is definitely in
	// at startup time).
	virtual UInt32		GetInitialPowerState ( void );

	// We override this method in order to provide the number of transitions
	// from Fully active to Sleep state so that the idle timer can be adjusted
	// to the appropriate time period based on the disk spindown time set in
	// the Energy Saver prefs panel.
	virtual UInt32		GetNumberOfPowerStateTransitions ( void );
	
	// The TicklePowerManager method is called to tell the power manager that the
	// device needs to be in a certain power state to handle requests.
	virtual void		TicklePowerManager ( void );

	// The HandlePowerChange method is the state machine for power management.
	// It is guaranteed to be on its own thread of execution (different from
	// the power manager thread AND the workloop thread. This routine can
	// send sync or async calls to the drive without worrying about threading
	// issues.
	virtual void		HandlePowerChange ( void );

	// The HandleCheckPowerState (void) method is on the serialized side of the command
	// gate and can change member variables safely without multi-threading issues.
	// It's main purpose is to call the superclass' HandleCheckPowerState ( UInt32 maxPowerState )
	// with the max power state the class registered with.
	virtual void		HandleCheckPowerState ( void );

	// The CheckMediaPresence method is called to see if the media which we
	// anticipated being there is still there.
	virtual bool		CheckMediaPresence ( void );
	
	virtual bool		InitializeDeviceSupport ( void );
	virtual void		StartDeviceSupport ( void );
	virtual void		SuspendDeviceSupport ( void );
	virtual void		ResumeDeviceSupport ( void );
	virtual void		StopDeviceSupport ( void );
	virtual void		TerminateDeviceSupport ( void );
	virtual bool 		CreateCommandSetObjects ( void );
	virtual void 		FreeCommandSetObjects ( void );
	virtual IOReturn	VerifyDeviceState ( void );


public:
	
	virtual IOReturn	SyncReadWrite ( 	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount );
	
	virtual IOReturn	AsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt64					block,
											UInt64					nblks,
											void *					clientData );

	virtual IOReturn	EjectTheMedia ( void );
	virtual IOReturn	GetTrayState ( UInt8 * trayState );
	virtual IOReturn	SetTrayState ( UInt8 trayState );
	virtual IOReturn	FormatMedia ( UInt64 byteCapacity );
	virtual UInt32		GetFormatCapacities ( 	UInt64 * capacities,
												UInt32   capacitiesMaxCount ) const;
	virtual IOReturn	LockUnlockMedia ( bool doLock );
	virtual IOReturn	SynchronizeCache ( void );
	virtual IOReturn	ReportBlockSize ( UInt64 * blockSize );
	virtual IOReturn	ReportEjectability ( bool * isEjectable );
	virtual IOReturn	ReportLockability ( bool * isLockable );
	virtual IOReturn	ReportPollRequirements ( bool * pollIsRequired,
												 bool * pollIsExpensive );
	virtual IOReturn	ReportMaxReadTransfer (  UInt64 	blockSize,
												 UInt64 * 	max );
	virtual IOReturn	ReportMaxValidBlock ( UInt64 * maxBlock );
	virtual IOReturn	ReportMaxWriteTransfer ( UInt64	blockSize,
												UInt64 * max );
	virtual IOReturn	ReportMediaState ( 	bool * mediaPresent,
											bool * changed );
	virtual IOReturn	ReportRemovability ( bool * isRemovable );
	virtual IOReturn	ReportWriteProtection ( bool * isWriteProtected );
	
	static 	void		sPollForMedia ( void * pdtDriver, void * refCon );

	/* CD Specific */
	virtual IOReturn	SetMediaAccessSpeed ( UInt16 kilobytesPerSecond );
	
	virtual IOReturn	GetMediaAccessSpeed ( UInt16 * kilobytesPerSecond );

	virtual IOReturn	AsyncReadCD ( 	IOMemoryDescriptor * buffer,
										UInt32 block,
										UInt32 nblks,
										CDSectorArea sectorArea,
										CDSectorType sectorType,
										void *	clientData );
	
	virtual IOReturn	ReadISRC ( UInt8 track, CDISRC isrc );
	
	virtual IOReturn	ReadMCN ( CDMCN mcn);
	
	virtual IOReturn	ReadTOC ( IOMemoryDescriptor * buffer );
	
	virtual IOReturn	AudioPause ( bool pause );
	
	virtual IOReturn	AudioPlay ( CDMSF timeStart, CDMSF timeStop );
	
	virtual IOReturn	AudioScan ( CDMSF timeStart, bool reverse );
	
	virtual IOReturn	AudioStop ( void );
	
	virtual IOReturn	GetAudioStatus ( CDAudioStatus * status );
	
	virtual IOReturn	GetAudioVolume ( UInt8 * leftVolume, UInt8 * rightVolume );
	
	virtual IOReturn	SetAudioVolume ( UInt8 leftVolume, UInt8 rightVolume );
	
	/* DVD Specific */
	virtual UInt32			GetMediaType		( void );
    
	virtual IOReturn		ReportKey			( IOMemoryDescriptor * buffer,
												  const DVDKeyClass keyClass,
												  const UInt32 lba,
												  const UInt8 agid,
												  const DVDKeyFormat keyFormat );
												  
	virtual IOReturn		SendKey				( IOMemoryDescriptor * buffer,
												  const DVDKeyClass keyClass,
												  const UInt8 agid,
												  const DVDKeyFormat keyFormat );
												  
	virtual IOReturn		ReadDVDStructure 	( 	IOMemoryDescriptor * 		buffer,
													const UInt32 				length,
													const UInt8					structureFormat,
													const UInt32				logicalBlockAddress,
													const UInt8					layer,
													const UInt8 				agid );

	// The block size decoding for Read CD and Read CD MSF  as defined in table 255
    bool	GetBlockSize (
						UInt32 *					requestedByteCount,
						SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
						SCSICmdField1Bit 			SYNC,
						SCSICmdField2Bit 			HEADER_CODES,
						SCSICmdField1Bit 			USER_DATA,
						SCSICmdField1Bit 			EDC_ECC,
						SCSICmdField2Bit 			ERROR_FIELD,
						SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS );
	
	SCSICmdField4Byte ConvertMSFToLBA ( SCSICmdField3Byte MSF );

	// Command methods to access all commands available to MMC based devices.
	// The BLANK command as defined in section 6.1.1
	virtual bool BLANK (
						SCSITaskIdentifier			request,
		    			SCSICmdField1Bit 			IMMED, 
						SCSICmdField3Bit 			BLANKING_TYPE, 
						SCSICmdField4Byte 			START_ADDRESS_TRACK_NUMBER, 
						SCSICmdField1Byte 			CONTROL );

	// The CLOSE TRACK/SESSION command as defined in section 6.1.2
    virtual bool CLOSE_TRACK_SESSION (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField1Bit 			SESSION, 
						SCSICmdField1Bit 			TRACK, 
						SCSICmdField2Byte 			TRACK_NUMBER, 
						SCSICmdField1Byte 			CONTROL );

	// The FORMAT UNIT command as defined in section 6.1.3
    virtual bool FORMAT_UNIT (
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor *		dataBuffer,
		    			IOByteCount					parameterListSize,
						SCSICmdField1Bit 			FMT_DATA, 
						SCSICmdField1Bit 			CMP_LIST, 
						SCSICmdField3Bit 			FORMAT_CODE, 
						SCSICmdField2Byte 			INTERLEAVE_VALUE, 
						SCSICmdField1Byte 			CONTROL );

	// The GET CONFIGURATION command as defined in section 6.1.4
    virtual bool GET_CONFIGURATION (
						SCSITaskIdentifier			request,
   						IOMemoryDescriptor *		dataBuffer, 
						SCSICmdField2Bit 			RT, 
						SCSICmdField2Byte 			STARTING_FEATURE_NUMBER, 
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The GET EVENT/STATUS NOTIFICATION command as defined in section 6.1.5
    virtual bool GET_EVENT_STATUS_NOTIFICATION (
						SCSITaskIdentifier			request,
	    				IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField1Byte 			NOTIFICATION_CLASS_REQUEST, 
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The GET PERFORMANCE command as defined in section 6.1.6
    virtual bool GET_PERFORMANCE (
						SCSITaskIdentifier			request,
	   					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			TOLERANCE, 
						SCSICmdField1Bit 			WRITE, 
						SCSICmdField2Bit 			EXCEPT, 
						SCSICmdField4Byte 			STARTING_LBA, 
						SCSICmdField2Byte 			MAXIMUM_NUMBER_OF_DESCRIPTORS, 
						SCSICmdField1Byte 			CONTROL );

	// The LOAD/UNLOAD MEDIUM command as defined in section 6.1.7
    virtual bool LOAD_UNLOAD_MEDIUM (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField1Bit 			LO_UNLO, 
						SCSICmdField1Bit 			START, 
						SCSICmdField1Byte 			SLOT, 
						SCSICmdField1Byte 			CONTROL );

	// The MECHANISM STATUS command as defined in section 6.1.8
    virtual bool MECHANISM_STATUS (
						SCSITaskIdentifier			request,
	   					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The PAUSE/RESUME command as defined in section 6.1.9
    virtual bool PAUSE_RESUME (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RESUME, 
						SCSICmdField1Byte 			CONTROL );

	// The PLAY AUDIO (10) command as defined in section 6.1.10
    virtual bool PLAY_AUDIO_10 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			PLAY_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The PLAY AUDIO (12) command as defined in section 6.1.11
    virtual bool PLAY_AUDIO_12 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField4Byte 			PLAY_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The PLAY AUDIO MSF command as defined in section 6.1.12
    virtual bool PLAY_AUDIO_MSF (
						SCSITaskIdentifier			request,
						SCSICmdField3Byte 			STARTING_MSF, 
						SCSICmdField3Byte 			ENDING_MSF, 
						SCSICmdField1Byte 			CONTROL );
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The PLAY CD command as defined in section 6.1.13
    virtual bool PLAY_CD (
						SCSITaskIdentifier			request,
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
	
	virtual bool READ_10 (
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 			*dataBuffer, 
		    			UInt32						blockSize,
		    			SCSICmdField1Bit 			DPO, 
		    			SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			TRANSFER_LENGTH, 
						SCSICmdField1Byte 			CONTROL );
	
	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The READ BUFFER CAPACITY command as defined in section 6.1.14
    virtual bool READ_BUFFER_CAPACITY (
						SCSITaskIdentifier			request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT *******************/

	// The READ CD command as defined in section 6.1.15
    virtual bool READ_CD (
						SCSITaskIdentifier			request,
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
    virtual bool READ_CD_MSF (
						SCSITaskIdentifier			request,
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
    virtual bool READ_CAPACITY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer, 
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Bit 			PMI,
						SCSICmdField1Byte 			CONTROL );

	// The READ DISC INFORMATION command as defined in section 6.1.18
    virtual bool READ_DISC_INFORMATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer, 
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The READ DVD STRUCTURE command as defined in section 6.1.19
    virtual bool READ_DVD_STRUCTURE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte 			ADDRESS,
						SCSICmdField1Byte 			LAYER_NUMBER, 
						SCSICmdField1Byte 			FORMAT, 
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField2Bit 			AGID,
						SCSICmdField1Byte 			CONTROL );

	// The READ FORMAT CAPACITIES command as defined in section 6.1.20
    virtual bool READ_FORMAT_CAPACITIES (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The READ HEADER command as defined in section 6.1.21
    virtual bool READ_HEADER (
						SCSITaskIdentifier			request,
    					IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT ***********************/

	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The READ MASTER CUE command as defined in section 6.1.22
    virtual bool READ_MASTER_CUE (
						SCSITaskIdentifier			request,
   						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte 			SHEET_NUMBER, 
						SCSICmdField3Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT ***********************/

	// The READ SUB-CHANNEL command as defined in section 6.1.23
    virtual bool READ_SUB_CHANNEL (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField1Bit 			SUBQ,
						SCSICmdField1Byte 			SUB_CHANNEL_PARAMETER_LIST, 
						SCSICmdField1Byte 			TRACK_NUMBER, 
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The READ TOC/PMA/ATIP command as defined in section 6.1.24/25
    virtual bool READ_TOC_PMA_ATIP (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer, 
						SCSICmdField1Bit 			MSF,
						SCSICmdField4Bit 			FORMAT,
						SCSICmdField1Byte			TRACK_SESSION_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The READ TRACK INFORMATION command as defined in section 6.1.26
    virtual bool READ_TRACK_INFORMATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			ADDRESS_NUMBER_TYPE,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The REPAIR TRACK command as defined in section 6.1.27
    virtual bool REPAIR_TRACK (
						SCSITaskIdentifier			request,
						SCSICmdField2Byte 			TRACK_NUMBER, 
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT ***********************/

	// The REPORT KEY command as defined in section 6.1.28
    virtual bool REPORT_KEY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			ALLOCATION_LENGTH, 
						SCSICmdField2Bit 			AGID,
						SCSICmdField6Bit 			KEY_FORMAT,
						SCSICmdField1Byte 			CONTROL );

	// The RESERVE TRACK command as defined in section 6.1.29
    virtual bool RESERVE_TRACK (
						SCSITaskIdentifier			request,
						SCSICmdField4Byte			RESERVATION_SIZE,
						SCSICmdField1Byte 			CONTROL );

	// The SCAN command as defined in section 6.1.30
    virtual bool SCAN (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			DIRECT,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte			SCAN_STARTING_ADDRESS_FIELD,
						SCSICmdField2Bit 			TYPE,
						SCSICmdField1Byte 			CONTROL );

	// The SEND CUE SHEET command as defined in section 6.1.31
    virtual bool SEND_CUE_SHEET (
						SCSITaskIdentifier			request,
	    				IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Byte			CUE_SHEET_SIZE,
						SCSICmdField1Byte 			CONTROL );

	// The SEND DVD STRUCTURE command as defined in section 6.1.32
    virtual bool SEND_DVD_STRUCTURE (
						SCSITaskIdentifier			request,
	   	 				IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte			FORMAT,
						SCSICmdField2Byte 			STRUCTURE_DATA_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The SEND EVENT command as defined in section 6.1.33
    virtual bool SEND_EVENT (
						SCSITaskIdentifier			request,
	   		 			IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The SEND KEY command as defined in section 6.1.34
    virtual bool SEND_KEY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
						SCSICmdField2Bit 			AGID,
						SCSICmdField6Bit 			KEY_FORMAT,
						SCSICmdField1Byte 			CONTROL );

	// The SEND OPC INFORMATION command as defined in section 6.1.35
    virtual bool SEND_OPC_INFORMATION (
						SCSITaskIdentifier			request,
	    				IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DO_OPC, 
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	/*********************** LEGACY COMMAND SUPPORT ***********************/
	// The SET CD SPEED command as defined in section 6.1.36
    virtual bool SET_CD_SPEED (
						SCSITaskIdentifier			request,
						SCSICmdField2Byte 			LOGICAL_UNIT_READ_SPEED, 
						SCSICmdField2Byte 			LOGICAL_UNIT_WRITE_SPEED, 
						SCSICmdField1Byte 			CONTROL );
	/*********************** END LEGACY COMMAND SUPPORT ***********************/

	// The SET READ AHEAD command as defined in section 6.1.37
    virtual bool SET_READ_AHEAD (
						SCSITaskIdentifier			request,
						SCSICmdField4Byte 			TRIGGER_LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField4Byte 			READ_AHEAD_LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField1Byte 			CONTROL );

	// The SET STREAMING command as defined in section 6.1.38
    virtual bool SET_STREAMING (
						SCSITaskIdentifier			request,
	 		   			IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
						SCSICmdField1Byte 			CONTROL );
	
	virtual bool START_STOP_UNIT ( 
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField4Bit 			POWER_CONDITIONS, 
						SCSICmdField1Bit 			LOEJ, 
						SCSICmdField1Bit 			START, 
						SCSICmdField1Byte 			CONTROL );
	
	// The STOP PLAY/SCAN command as defined in section 6.1.39
    virtual bool STOP_PLAY_SCAN (
						SCSITaskIdentifier			request,
						SCSICmdField1Byte 			CONTROL );

	// The SYNCHRONIZE CACHE command as defined in section 6.1.40
    virtual bool SYNCHRONIZE_CACHE (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS, 
						SCSICmdField1Byte 			CONTROL );

	// The WRITE (10) command as defined in section 6.1.41
    virtual bool WRITE_10 (
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor *		dataBuffer, 
		    			UInt32						blockSize,
						SCSICmdField1Bit 			DPO, 
						SCSICmdField1Bit 			FUA, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			TRANSFER_LENGTH, 
						SCSICmdField1Byte 			CONTROL );

	// The WRITE AND VERIFY (10) command as defined in section 6.1.42
    virtual bool WRITE_AND_VERIFY_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
		    			UInt32						blockSize,
						SCSICmdField1Bit 			DPO, 
						SCSICmdField1Bit 			BYT_CHK, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField4Byte 			TRANSFER_LENGTH, 
						SCSICmdField1Byte 			CONTROL );
	
	/* Added with 10.1.3 */
	OSMetaClassDeclareReservedUsed ( IOSCSIMultimediaCommandsDevice, 1 );
	
	virtual IOReturn	ReadTOC (	IOMemoryDescriptor *	buffer,
									CDTOCFormat				format,
									UInt8					msf,
									UInt32					trackSessionNumber,
									UInt16 *				actualByteCount );
	
	/* Added with 10.1.3 */
	OSMetaClassDeclareReservedUsed ( IOSCSIMultimediaCommandsDevice, 2 );
    
	virtual IOReturn	ReadDiscInfo (	IOMemoryDescriptor *	buffer,
										UInt16 *				actualByteCount );
	
	/* Added with 10.1.3 */
	OSMetaClassDeclareReservedUsed ( IOSCSIMultimediaCommandsDevice, 3 );
	
	virtual IOReturn	ReadTrackInfo (	IOMemoryDescriptor *	buffer,
										UInt32					address,
										CDTrackInfoAddressType	addressType,
										UInt16 *				actualByteCount );
	
	/* Added with 10.2 */
	OSMetaClassDeclareReservedUsed ( IOSCSIMultimediaCommandsDevice, 4 );
	
	virtual IOReturn	PowerDownHandler ( void * 		refCon,
										   UInt32 		messageType,
										   IOService * 	provider,
										   void * 		messageArgument,
										   vm_size_t 	argSize );
	
	/* Added with 10.3.3 */
	OSMetaClassDeclareReservedUsed ( IOSCSIMultimediaCommandsDevice, 5 );
	
protected:
	
	virtual void AsyncReadWriteCompletion ( SCSITaskIdentifier completedTask );
	
	
private:
	
	// Space reserved for future expansion.
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 	6 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 	7 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 	8 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 	9 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 10 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 11 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 12 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 13 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 14 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 15 );
    OSMetaClassDeclareReservedUnused ( IOSCSIMultimediaCommandsDevice, 16 );
    
};

#endif /* defined(KERNEL) && defined(__cplusplus) */

#endif /* _IOKIT_IO_SCSI_MULTIMEDIA_COMMANDS_DEVICE_H_ */