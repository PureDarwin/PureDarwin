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


#ifndef _IOKIT_IO_SCSI_REDUCED_BLOCK_COMMANDS_DEVICE_H_
#define _IOKIT_IO_SCSI_REDUCED_BLOCK_COMMANDS_DEVICE_H_

#if defined(KERNEL) && defined(__cplusplus)


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// General IOKit headers
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOStorage.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/IOSCSIPrimaryCommandsDevice.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


// RBC power states as defined in T10:1240D SCSI Reduced Block Commands (RBC)
// Revision 10a, August 18, 1999, page 13.
enum
{
	kRBCPowerStateSystemSleep	= 0,
	kRBCPowerStateSleep 		= 1,
	kRBCPowerStateStandby 		= 2,
	kRBCPowerStateIdle			= 3,
	kRBCPowerStateActive		= 4,
	kRBCNumPowerStates			= 5
};

enum
{
	kMediaStateUnlocked	= 0,
	kMediaStateLocked 	= 1
};

// Forward declaration for the SCSIReducedBlockCommands that is used internally by the
// IOSCSIReducedBlockCommandsDevice class.
class SCSIReducedBlockCommands;

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class IOSCSIReducedBlockCommandsDevice : public IOSCSIPrimaryCommandsDevice
{
	
	OSDeclareAbstractStructors ( IOSCSIReducedBlockCommandsDevice );
	
private:
	
    SCSIReducedBlockCommands *		fSCSIReducedBlockCommandObject;
    SCSIReducedBlockCommands *		GetSCSIReducedBlockCommandObject ( void );
	
	static void			AsyncReadWriteComplete ( SCSITaskIdentifier completedTask );
	
protected:
    
    // Reserve space for future expansion.
    struct IOSCSIReducedBlockCommandsDeviceExpansionData
	{
		IONotifier *	fPowerDownNotifier;
        bool			fMediumRemovalPrevented;
        bool			fKnownManualEject;
        UInt32			fPollingMode;
	};
    IOSCSIReducedBlockCommandsDeviceExpansionData * fIOSCSIReducedBlockCommandsDeviceReserved;
	
	#define fPowerDownNotifier		fIOSCSIReducedBlockCommandsDeviceReserved->fPowerDownNotifier
	#define fMediumRemovalPrevented	fIOSCSIReducedBlockCommandsDeviceReserved->fMediumRemovalPrevented
	#define fKnownManualEject		fIOSCSIReducedBlockCommandsDeviceReserved->fKnownManualEject
	#define fPollingMode			fIOSCSIReducedBlockCommandsDeviceReserved->fPollingMode
	
	bool				fMediaChanged;
	bool				fMediaPresent;
	
	// The byte count of each physical block on the media.
	UInt32				fMediaBlockSize;
	
	// The total number of blocks of mediaBlockSize on the media.
	UInt32				fMediaBlockCount;
	
	// Flags used to indicate device feature
	bool				fMediaIsRemovable;
	bool				fMediaIsWriteProtected;
	
	thread_call_t		fPollingThread;
	
	enum
	{
		kPollingMode_Suspended			= 0,
		kPollingMode_NewMedia 			= 1,
		kPollingMode_MediaRemoval		= 2
	};
	
	virtual void 		CreateStorageServiceNub ( void );
	virtual bool		DetermineDeviceCharacteristics ( void );	
	virtual void		PollForMedia ( void );
	virtual void		EnablePolling ( void );
	virtual void		DisablePolling ( void );
	virtual void		CheckWriteProtection ( void );
	virtual void		SetMediaCharacteristics ( UInt32 blockSize, UInt32 blockCount );
 	virtual void		ResetMediaCharacteristics ( void );
	virtual bool		ClearNotReadyStatus ( void );
	
	virtual IOReturn	IssueRead ( IOMemoryDescriptor *	buffer,
									UInt64					startBlock,
									UInt64					blockCount );
	
	virtual IOReturn	IssueWrite ( IOMemoryDescriptor*	buffer,
									 UInt64					startBlock,
									 UInt64					blockCount );
	
	virtual IOReturn	IssueRead ( IOMemoryDescriptor *	buffer,
									UInt64					startBlock,
									UInt64					blockCount,
									void *					clientData );
	
	virtual IOReturn	IssueWrite ( IOMemoryDescriptor *	buffer,
									 UInt64					startBlock,
									 UInt64					blockCount,
									 void *					clientData );
	
	// This method will retreive the SCSI Primary Command Set object for
	// the class.  For subclasses, this will be overridden using a
	// dynamic cast on the subclasses base command set object.
	virtual SCSIPrimaryCommands *	GetSCSIPrimaryCommandObject ( void );
	
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
	virtual bool		CreateCommandSetObjects ( void );
	virtual void		FreeCommandSetObjects ( void );
	
public:
	
	virtual IOReturn	SyncReadWrite ( 	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount );
	
	virtual IOReturn	AsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt64					block,
											UInt64					nblks,
											void *					clientData );
	
	
	virtual IOReturn	EjectTheMedia ( void );	
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
	
	
protected:
	
	
	// The FORMAT_UNIT command as defined in section 5.1
	virtual bool FORMAT_UNIT (
							SCSITaskIdentifier			request,
							SCSICmdField1Bit			IMMED,
	   						SCSICmdField1Bit			PROGRESS,
	   						SCSICmdField1Bit			PERCENT_TIME,
	   						SCSICmdField1Bit			INCREMENT );
	
	// The INQUIRY command as defined in SPC-2 w/o CONTROL byte
	virtual bool INQUIRY (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit			CMDDT,
							SCSICmdField1Bit			EVPD,
							SCSICmdField1Byte			PAGE_OR_OPERATION_CODE,
							SCSICmdField1Byte			ALLOCATION_LENGTH );
	
	// The MODE_SELECT(6) command as defined in SPC-2 w/o CONTROL byte
	virtual bool MODE_SELECT_6 (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			PF,
							SCSICmdField1Bit 			SP,
							SCSICmdField1Byte 			PARAMETER_LIST_LENGTH );
	
	// The MODE_SENSE(6) command as defined in SPC-2 w/o CONTROL byte
	virtual bool MODE_SENSE_6 (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Bit 			DBD,
		   					SCSICmdField2Bit 			PC,
		   					SCSICmdField6Bit 			PAGE_CODE,
		   					SCSICmdField1Byte 			ALLOCATION_LENGTH );
	
	// The PERSISTENT_RESERVE_IN command as defined in SPC-2 w/o CONTROL byte
	virtual bool PERSISTENT_RESERVE_IN (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
		   					SCSICmdField5Bit 			SERVICE_ACTION,
		   					SCSICmdField2Byte 			ALLOCATION_LENGTH );
	
	// The PERSISTENT_RESERVE_OUT command as defined in SPC-2 w/o CONTROL byte
	virtual bool PERSISTENT_RESERVE_OUT (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
		   					SCSICmdField5Bit			SERVICE_ACTION,
		   					SCSICmdField4Bit			SCOPE,
		   					SCSICmdField4Bit			TYPE );
	
	// The PREVENT_ALLOW_MEDIUM_REMOVAL command as defined in SPC-2 w/o CONTROL byte
	virtual bool PREVENT_ALLOW_MEDIUM_REMOVAL ( 
							SCSITaskIdentifier			request,
	 						SCSICmdField2Bit			PREVENT );
	
   	// The READ_10 command as defined in section 5.2
	virtual bool READ_10 (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
							UInt32						blockSize,
							SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
							SCSICmdField2Byte			TRANSFER_LENGTH );
	
	// The READ_CAPACITY command as defined in section 5.3
	virtual bool READ_CAPACITY (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer );
	
	// The RELEASE(6) command as defined in SPC-2 w/o CONTROL byte
	virtual bool RELEASE_6 (								
							SCSITaskIdentifier			request );
	
	// The REQUEST_SENSE command as defined in SPC-2 w/o CONTROL byte
	virtual bool REQUEST_SENSE (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField1Byte 			ALLOCATION_LENGTH );
	
	// The RESERVE(6) command as defined in SPC-2 w/o CONTROL byte
	virtual bool RESERVE_6 (	
							SCSITaskIdentifier			request );
	
	// The START_STOP_UNIT command as defined in section 5.4
	virtual bool START_STOP_UNIT (
							SCSITaskIdentifier			request,
							SCSICmdField1Bit			IMMED,
							SCSICmdField4Bit			POWER_CONDITIONS,
							SCSICmdField1Bit			LEOJ,
							SCSICmdField1Bit			START );
	
	// The SYNCRONIZE_CACHE command as defined in section 5.5
	virtual bool SYNCHRONIZE_CACHE (
							SCSITaskIdentifier			request );
	
	// The TEST_UNIT_READY command as defined in SPC-2 w/o CONTROL byte
	virtual bool TEST_UNIT_READY (
							SCSITaskIdentifier			request );
	
	// The VERIFY command as defined in section 5.7
	virtual bool VERIFY (
							SCSITaskIdentifier			request,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
							SCSICmdField2Byte 			VERIFICATION_LENGTH );
	
	// The WRITE_10 command	as defined in section 5.6
	virtual bool WRITE_10 (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
							UInt32						blockSize,
							SCSICmdField1Bit			FUA,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
							SCSICmdField2Byte 			TRANSFER_LENGTH );
	
	// The WRITE_BUFFER command as defined in SPC-2 w/o CONTROL byte
	virtual bool WRITE_BUFFER ( 
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer,
							SCSICmdField4Bit 			MODE,
							SCSICmdField1Byte 			BUFFER_ID,
							SCSICmdField3Byte 			BUFFER_OFFSET,
							SCSICmdField3Byte 			PARAMETER_LIST_LENGTH );
	
	

	/* Added with 10.2 */
	OSMetaClassDeclareReservedUsed ( IOSCSIReducedBlockCommandsDevice, 1 );
	
public:
	
	virtual IOReturn	PowerDownHandler (	void * 			refCon,
											UInt32 			messageType,
											IOService * 	provider,
											void * 			messageArgument,
											vm_size_t 		argSize );
	
	
	/* Added with 10.2 */	
	OSMetaClassDeclareReservedUsed ( IOSCSIReducedBlockCommandsDevice, 2 );
	
protected:

	virtual	void		SetMediaIcon ( void );
	

	/* Added with 10.3.3 */		
	OSMetaClassDeclareReservedUsed ( IOSCSIReducedBlockCommandsDevice, 3 );
	
protected:

	virtual	void AsyncReadWriteCompletion ( SCSITaskIdentifier completedTask );
	
	
private:
	
	// Space reserved for future expansion.
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice,  4 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice,  5 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice,  6 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice,  7 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice,  8 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice,  9 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice, 10 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice, 11 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice, 12 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice, 13 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice, 14 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice, 15 );
	OSMetaClassDeclareReservedUnused ( IOSCSIReducedBlockCommandsDevice, 16 );
	
};

#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif /* _IOKIT_IO_SCSI_REDUCED_BLOCK_COMMANDS_DEVICE_H_ */