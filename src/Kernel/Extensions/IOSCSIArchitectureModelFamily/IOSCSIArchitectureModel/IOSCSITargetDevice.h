/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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


#ifndef _IOKIT_IO_SCSI_TARGET_DEVICE_H_
#define _IOKIT_IO_SCSI_TARGET_DEVICE_H_

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Libkern headers
#include <libkern/c++/OSData.h>

// General IOKit headers
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/IOSCSIPrimaryCommandsDevice.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICmds_REPORT_LUNS_Definitions.h>
#include <IOKit/scsi/IOSCSIPeripheralDeviceNub.h>


#if defined(KERNEL) && defined(__cplusplus)


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ¥¥¥¥¥ The current implementation of IOSCSITargetDevice is not designed to be
// ¥¥¥¥¥ subclassed and any object subclassed from this version will definitely
// ¥¥¥¥¥ not be compatible in future system releases.

// Forward declaration of path manager base class
class SCSITargetDevicePathManager;
class IOSCSITargetDeviceHashTable;

class IOSCSITargetDevice : public IOSCSIPrimaryCommandsDevice
{
	
	OSDeclareDefaultStructors ( IOSCSITargetDevice )
	
public:
	
	static bool			Create ( IOSCSIProtocolServices * provider );
	virtual IOReturn 	message ( UInt32 type, IOService * nub, void * arg );
	
protected:
	
	friend class SCSITargetDevicePathManager;
	friend class IOSCSITargetDeviceHashTable;
	
	// Methods used by IOSCSITargetDeviceHashTable
	void		SetHashEntry ( void * newEntry );
	OSObject *	GetNodeUniqueIdentifier ( void );
	void		SetNodeUniqueIdentifier ( OSObject * uniqueID );
	void		AddPath ( IOSCSIProtocolServices * provider );
	
	// Methods used by SCSITargetDevicePathManager
	void			TargetTaskCompletion ( SCSITaskIdentifier request );
	static bool		SetTargetLayerReference ( SCSITaskIdentifier request, void * target );
	static void *	GetTargetLayerReference ( SCSITaskIdentifier request );
	
	bool	CreateTargetDeviceOrPath (
				IOSCSIProtocolServices * 	provider );
	
	bool	IsProviderAnotherPathToTarget (
				IOSCSIProtocolServices * 	provider );
	
	bool	DoesLUNObjectExist ( SCSILogicalUnitNumber logicalUnitNumber );
	
	bool		InitializeDeviceSupport ( void );
	void		StartDeviceSupport ( void );
	void		SuspendDeviceSupport ( void );
	void 		ResumeDeviceSupport ( void );
	void		StopDeviceSupport ( void );
	void 		TerminateDeviceSupport ( void );
	UInt32		GetNumberOfPowerStateTransitions ( void );
	bool		ClearNotReadyStatus ( void );
	
	void	ScanForLogicalUnits ( void );
	void 	RetrieveCharacteristicsFromProvider ( void );

	bool	DetermineTargetCharacteristics ( void );
	bool	VerifyTargetPresence ( void );
	bool	SetCharacteristicsFromINQUIRY ( SCSICmd_INQUIRY_StandardDataAll * inquiryBuffer );
	bool	PerformREPORTLUNS ( void );
	void	ParseReportLUNsInformation ( SCSICmd_REPORT_LUNS_Header * buffer );

	// DEPRECATED, use version with 32-bit dataSize
	bool	RetrieveReportLUNsData (
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								dataBuffer,  
						UInt8									dataSize );
						
	bool	RetrieveReportLUNsData (
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								dataBuffer,  
						UInt32									dataSize );

	UInt64	DetermineMaximumLogicalUnitNumber ( void );
	bool	VerifyLogicalUnitPresence ( SCSILogicalUnitNumber theLogicalUnit );
	bool	CreateLogicalUnit ( SCSILogicalUnitNumber theLogicalUnit );
	
	// INQUIRY utility member routines
	bool 	RetrieveDefaultINQUIRYData ( 
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								inquiryBuffer,  
						UInt8									inquirySize );
	
	bool	RetrieveINQUIRYDataPage ( 
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								inquiryBuffer,  
						UInt8									inquiryPage,
						UInt8									inquirySize );
	
	bool	PublishDefaultINQUIRYInformation ( void );
	void	PublishINQUIRYVitalProductDataInformation ( IOService * object, SCSILogicalUnitNumber logicalUnit );
	void	PublishDeviceIdentification ( IOService * object, SCSILogicalUnitNumber logicalUnit );
	void	PublishUnitSerialNumber ( IOService * object, SCSILogicalUnitNumber logicalUnit );
	
	void	SetLogicalUnitNumber ( SCSITaskIdentifier request, SCSILogicalUnitNumber logicalUnit );
	
	// Power management overrides
	virtual UInt32		GetInitialPowerState ( void );
	virtual void		HandlePowerChange ( void );
	virtual void		HandleCheckPowerState ( void );
	virtual void		TicklePowerManager ( void );
	
	virtual void					ExecuteCommand ( SCSITaskIdentifier request );
	virtual SCSIServiceResponse		AbortTask ( UInt8 theLogicalUnit, SCSITaggedTaskIdentifier theTag );
	virtual SCSIServiceResponse		AbortTaskSet ( UInt8 theLogicalUnit );
	virtual SCSIServiceResponse		ClearACA ( UInt8 theLogicalUnit );
	virtual SCSIServiceResponse		ClearTaskSet ( UInt8 theLogicalUnit );
	virtual SCSIServiceResponse		LogicalUnitReset ( UInt8 theLogicalUnit );
	virtual SCSIServiceResponse		TargetReset ( void );
	
	virtual void		detach ( IOService * provider );
	virtual void		free ( void );
	
private:
	
	// Reserve space for future expansion.
	struct IOSCSITargetDeviceExpansionData { };
	IOSCSITargetDeviceExpansionData * fIOSCSITargetDeviceReserved;
	
	OSSet *							fClients;
	OSObject *						fNodeUniqueIdentifier;
	void *							fTargetHashEntry;
	SCSITargetDevicePathManager *	fPathManager;
	
	// Target Characteristics determined from LUN 0 INQUIRY data.
	UInt8							fTargetPeripheralDeviceType;
	UInt8							fTargetANSIVersion;
	bool							fTargetHasHiSup;
	bool							fTargetHasSCCS;
	bool							fTargetHasEncServs;
	bool							fTargetHasMultiPorts;
	bool							fTargetHasMChanger;
	
	bool		handleOpen ( IOService *		client,
							 IOOptionBits		options,
							 void *				arg );
	
	void		handleClose ( IOService *		client,
							  IOOptionBits		options );
							
	bool		handleIsOpen ( const IOService * client ) const;
	
	// Space reserved for future expansion.
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  1 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  2 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  3 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  4 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  5 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  6 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  7 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  8 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice,  9 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice, 10 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice, 11 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice, 12 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice, 13 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice, 14 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice, 15 );
	OSMetaClassDeclareReservedUnused ( IOSCSITargetDevice, 16 );
	
};

#endif /* defined(KERNEL) && defined(__cplusplus) */

#endif /* _IOKIT_IO_SCSI_TARGET_DEVICE_H_ */