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
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSString.h>

// Generic IOKit related headers
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOService.h>

// IOKit storage headers
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>

// SCSI Architecture Model Family includes
#include "SCSITaskDefinition.h"
#include "SCSILibraryRoutines.h"
#include "SCSIPrimaryCommands.h"
#include "SCSICmds_INQUIRY_Definitions.h"
#include "SCSICmds_REPORT_LUNS_Definitions.h"
#include "SCSICmds_REQUEST_SENSE_Defs.h"
#include "IOSCSITargetDevice.h"
#include "IOSCSITargetDeviceHashTable.h"
#include "SCSITargetDevicePathManager.h"
#include "SCSIPathManagers.h"

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSI Target Device"

#if DEBUG
#define SCSI_TARGET_DEVICE_DEBUGGING_LEVEL					0
#endif

#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_TARGET_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_TARGET_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x; IOSleep (1)
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_TARGET_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x; IOSleep (1)
#else
#define STATUS_LOG(x)
#endif

#define kSCSICmd_INQUIRY_StandardDataAllString	"SCSICmd_INQUIRY_StandardDataAll"

#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClassAndStructors ( IOSCSITargetDevice, IOSCSIPrimaryCommandsDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kTURMaxRetries							1
#define kMaxInquiryAttempts						2
#define kSCSILogicalUnitZero					0					

#if 0
#pragma mark -
#pragma mark ¥ Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Create - Creates a new target device.					   [PUBLIC][STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::Create (
				IOSCSIProtocolServices * provider )
{
	
	bool					result 		= false;
	IOSCSITargetDevice *	newDevice 	= NULL;
	
	// Allocate the object.
	newDevice = OSTypeAlloc ( IOSCSITargetDevice );
	require_nonzero ( newDevice, ErrorExit );
	
	// Init the object.
	result = newDevice->init ( 0 );
	require ( result, ReleaseTargetExit );
	
	// Attach the object.
	result = newDevice->attach ( provider );
	require ( result, DetachTargetExit );
	
	// Start the object.
	result = newDevice->start ( provider );
	require ( result, DetachTargetExit );
	
	// Create the target device or path to a previously existing
	// target (in the case of multipathing).
	result = newDevice->CreateTargetDeviceOrPath ( provider );
	require ( result, DetachTargetExit );
	
	newDevice->release ( );
	newDevice = NULL;
	
	return result;
	
	
DetachTargetExit:
	
	
	newDevice->stop ( provider );
	newDevice->detach ( provider );
	
	
ReleaseTargetExit:
	
	
	require_nonzero_quiet ( newDevice, ErrorExit );
	newDevice->release ( );
	newDevice = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateTargetDeviceOrPath - Creates a new target device or path to an
//								 existing target device.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::CreateTargetDeviceOrPath (
					IOSCSIProtocolServices * provider )
{
	
	bool	result = true;
	
	// Verify that a target is indeed connected for this instantiation.	
	result = VerifyTargetPresence ( );
	require ( result, ErrorExit );
	
	// Set all appropriate Registry Properties so that they are available if needed.
	RetrieveCharacteristicsFromProvider ( );
	
	// Determine the SCSI Target Device characteristics for the target
	// that is represented by this object.
	result = DetermineTargetCharacteristics ( );
	require ( result, ErrorExit );
	
	result = IsProviderAnotherPathToTarget ( provider );
	if ( result == false )
	{
		
		// Create a path manager.
		fPathManager = SCSIPressurePathManager::Create ( this, provider );
		check ( fPathManager );
		
		// Finally, perform a LUN scan.
		ScanForLogicalUnits ( );
		
	}
	
	else
	{
		
		// Our provider is a path to an existing target, so we must
		// detach ourselves from our provider here (our provider has
		// already been attach'ed to the existing target).
		detach ( provider );
		
	}
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsProviderAnotherPathToTarget - 	Checks if we have this target device
//										in global hash table. If so, it simply
//										adds a new path and returns false,
//										otherwise, returns true.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::IsProviderAnotherPathToTarget (
						IOSCSIProtocolServices * 	provider )
{
	
	bool		result 				 = false;
	OSObject *	nodeUniqueIdentifier = NULL;
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::IsProviderAnotherPathToTarget\n" ) );
	
	nodeUniqueIdentifier = GetNodeUniqueIdentifier ( );
	if ( nodeUniqueIdentifier != NULL )
	{
		
		IOSCSITargetDeviceHashTable *	ht	= NULL;
		
		ht = IOSCSITargetDeviceHashTable::GetSharedInstance ( );
		
		if ( OSDynamicCast ( OSData, nodeUniqueIdentifier ) )
			result = ht->IsProviderPathToExistingTarget ( this, provider, ht->Hash ( ( OSData * ) nodeUniqueIdentifier ) );
		else
			result = ht->IsProviderPathToExistingTarget ( this, provider, ht->Hash ( ( OSString * ) nodeUniqueIdentifier ) );
		
	}
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::IsProviderAnotherPathToTarget\n" ) );
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetHashEntry - Set the hash entry corresponding to this target device.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::SetHashEntry ( void * newEntry )
{
	fTargetHashEntry = newEntry;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetNodeUniqueIdentifier - Gets the node's unique identifier (unit
//								serial number, EUI-64, or FCNameIdentifier)
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

OSObject *
IOSCSITargetDevice::GetNodeUniqueIdentifier ( void )
{
	return fNodeUniqueIdentifier;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetNodeUniqueIdentifier - Sets the node's unique identifier (unit
//								serial number, EUI-64, or FCNameIdentifier)
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::SetNodeUniqueIdentifier ( OSObject * uniqueID )
{
	
	require_nonzero ( uniqueID, ErrorExit );
	
	uniqueID->retain ( );
	
	if ( fNodeUniqueIdentifier != NULL )
		fNodeUniqueIdentifier->release ( );
	
	fNodeUniqueIdentifier = uniqueID;
	
#if DEBUG
	
	setProperty ( "Node Unique ID", fNodeUniqueIdentifier );
	
#endif	/* DEBUG */
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AddPath - Adds a path to the target device					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::AddPath ( IOSCSIProtocolServices * provider )
{
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::AddPath\n" ) );
	
	attach ( provider );
	provider->open ( this );
	if ( fPathManager != NULL )
	{
		
		fPathManager->AddPath ( provider );
		messageClients ( kIOMessageServicePropertyChange );
		
	}
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::AddPath\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleOpen - Handles opens on the object.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::handleOpen ( IOService *		client,
								 IOOptionBits		options,
								 void *				arg )
{
	
	bool	result = false;
	
	// It's an open from a multi-LUN client
	require_nonzero ( fClients, ErrorExit );
	require_nonzero ( OSDynamicCast ( IOSCSILogicalUnitNub, client ), ErrorExit );
	result = fClients->setObject ( client );
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleClose - Handles closes on the object.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::handleClose ( IOService *	client,
								  IOOptionBits	options )
{
	
	require_nonzero ( fClients, Exit );
	
	if ( fClients->containsObject ( client ) )
	{
		
		fClients->removeObject ( client );
		
		if ( ( fClients->getCount ( ) == 0 ) && isInactive ( ) )
		{
			message ( kIOMessageServiceIsRequestingClose, getProvider ( ), 0 );
		}
		
	}
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleIsOpen - Figures out if there are any opens on this object.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::handleIsOpen ( const IOService * client ) const
{
	
	bool	result = false;
		
	require_nonzero ( fClients, CallSuperClassError );
	
	// General case (is anybody open)
	if ( ( client == NULL ) && ( fClients->getCount ( ) != 0 ) )
	{
		result = true;
	}
	
	else
	{
		// specific case (is this client open)
		result = fClients->containsObject ( client );
	}
	
	return result;
	
	
CallSuperClassError:
	
	
	result = super::handleIsOpen ( client );
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Called for broadcasted messages.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSITargetDevice::message ( UInt32 type, IOService * nub, void * arg )
{
	
	IOReturn	result = kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kSCSIPort_NotificationStatusChange:
		{
			
			IOSCSIProtocolServices *	path = NULL;
			
			path = OSDynamicCast ( IOSCSIProtocolServices, nub );
						
			// Port status is changing, let path manager object know
			// about it.
			if ( ( fPathManager != NULL ) && ( path != NULL ) )
			{
				
				fPathManager->PathStatusChanged ( path, ( UInt32 ) arg );
				messageClients ( kIOMessageServicePropertyChange );
				
			}
			
		}
		break;
		
		default:
		{
			
			STATUS_LOG ( ( "IOSCSITargetDevice::Unknown message type = 0x%08x\n", type ) );
			result = super::message ( type, nub, arg );
			
		}
		break;
		
	}
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ detach - Detaches a path from the target device.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::detach ( IOService * provider )
{
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::detach\n" ) );
	
	if ( fPathManager != NULL )
	{
		
		fPathManager->RemovePath ( ( IOSCSIProtocolServices * ) provider );
		messageClients ( kIOMessageServicePropertyChange );
		
	}
	
	super::detach ( provider );
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::detach\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ free - Called to free any resource before object destruction.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::free ( void )
{
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::free\n" ) );
	
	if ( fTargetHashEntry != NULL )
	{
		
		IOSCSITargetDeviceHashTable *	ht = NULL;
		
		ht = IOSCSITargetDeviceHashTable::GetSharedInstance ( );
		
		ht->DestroyHashReference ( fTargetHashEntry );
		fTargetHashEntry = NULL;
		
	}

	if ( fPathManager != NULL )
	{
		
		fPathManager->release ( );
		fPathManager = NULL;
		
	}
	
	if ( fNodeUniqueIdentifier != NULL )
	{
		
		fNodeUniqueIdentifier->release ( );
		fNodeUniqueIdentifier = NULL;
		
	}
	
	super::free ( );
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::free\n" ) );
	
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
IOSCSITargetDevice::InitializeDeviceSupport ( void )
{
	
	bool	result = false;
	
	// Allocate space for our set that will keep track of the LUNs.
	fClients = OSSet::withCapacity ( 8 );
	require_nonzero ( fClients, ErrorExit );
	
	fClients->setCapacityIncrement ( 8 );
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StartDeviceSupport - Starts device support.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::StartDeviceSupport ( void )
{
	return;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ScanForLogicalUnits - Scans for SCSI Logical Units in the Target Device.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::ScanForLogicalUnits ( void )
{
	
	UInt64			countLU				= 0;
	UInt64			loopLU 				= 0;
	OSData *		data 				= NULL;
	bool			supportsREPORTLUNS 	= false;
	bool			result				= false;
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::ScanForLogicalUnits\n" ) ); 
	
	// Try to determine the available Logical Units by using the REPORT_LUNS
	// command.
	
	// Create the Logical Unit nodes for each valid LUN.  If the number
	// of valid LUNs could not be determined, should we create a Logical Unit
	// node for each possible LUN and have it determine if there is a valid
	// Logical Unit for its LUN?
	
	// Determine the maximum number of Logical Units supported by the device
	// and protocol.
	countLU = DetermineMaximumLogicalUnitNumber ( );
	
	// Try to determine the available Logical Units by issuing the
	// REPORT_LUNS command to the target device.
	supportsREPORTLUNS = PerformREPORTLUNS ( );
	
	if ( supportsREPORTLUNS == false )
	{
		
		ERROR_LOG ( ( "Device does not support REPORT_LUNS command, creating LUNs by brute-force\n" ) );
		
		// No, the device doesn't support REPORT_LUNS. Do a brute force
		// LUN scan starting at Logical Unit zero and going up to the highest
		// supported LUN the physical interconnect reported that it
		// supports.
		
		// Optimization: We don't have to verify Logical Unit zero exists. We've already done
		// that by verifying this target device exists.
		CreateLogicalUnit ( kSCSILogicalUnitZero );
		
		for ( loopLU = 1; loopLU <= countLU; loopLU++ )
		{
			
			bool	LUNPresent 	= false;
			
			// Verify the LUN exists.
			LUNPresent = VerifyLogicalUnitPresence ( loopLU );
			if ( LUNPresent == true )
			{
				
				// It exists, create an object to represent it.
				CreateLogicalUnit ( loopLU );
				
			}
			
		}
		
	}
	
	// Get the standard INQUIRY data.
	data = OSDynamicCast ( OSData, getProperty ( kSCSICmd_INQUIRY_StandardDataAllString ) );
	
	// Check if we removed the standard INQUIRY data already. If so,
	// then don't submit the data to the lower layer driver for requesting
	// physical interconnect specific features.
	if ( data != NULL )
	{
		
		// Check if the protocol layer driver needs the inquiry data for
		// any reason. SCSI Parallel uses this to determine Wide,
		// Sync, DT, QAS, IU, etc.
		result = IsProtocolServiceSupported ( kSCSIProtocolFeature_SubmitDefaultInquiryData, NULL );
		if ( result == true )
		{
			
			HandleProtocolServiceFeature ( kSCSIProtocolFeature_SubmitDefaultInquiryData,
										   ( void * ) data->getBytesNoCopy ( ) );
			
		}
		
		// Remove the property to free up the memory.
		removeProperty ( kSCSICmd_INQUIRY_StandardDataAllString );
		
		// Make target device visible in the IORegistry.
		registerService ( );
		
	}
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::ScanForLogicalUnits\n" ) ); 
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PerformREPORTLUNS - Issues the REPORT_LUNS command. Returns true if the
//						  device supports the command and valid data is
//						  returned.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::PerformREPORTLUNS ( void )
{
	
	bool							result				= false;
	bool							supportsREPORTLUNS	= false;
	SCSICmd_REPORT_LUNS_Header *	header				= NULL;
	SCSICmd_REPORT_LUNS_Header *	buffer				= NULL;
	UInt32							length				= 0;
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::PerformREPORTLUNS\n" ) ); 
	
	// Check to see the specification that this device claims compliance with
	// and if it is after SPC (SCSI-3), see if it supports the REPORT_LUNS
	// command.
	require ( ( fTargetANSIVersion >= kINQUIRY_ANSI_VERSION_SCSI_SPC_Compliant ), ErrorExit );
	
	// Allocate the header so we can see how much data we really need to
	// retrieve from the device.
	header = IONew ( SCSICmd_REPORT_LUNS_Header, 1 );
	require_nonzero ( header, ErrorExit );
	
	// Zero it.
	bzero ( header, sizeof ( SCSICmd_REPORT_LUNS_Header )  );			
	
	// Retrieve REPORT_LUNS data.
	result = RetrieveReportLUNsData ( kSCSILogicalUnitZero, ( UInt8 * ) header, ( UInt32 ) sizeof ( SCSICmd_REPORT_LUNS_Header ) );
	require ( result, ReleaseHeader );
	
	// Get the full length.
	length = OSSwapBigToHostInt32 ( header->LUN_LIST_LENGTH ) + kREPORT_LUNS_HeaderSize;
	
	STATUS_LOG ( ( "length = %ld\n", length ) ); 
	require ( ( length >= sizeof ( SCSICmd_REPORT_LUNS_Header ) ), ReleaseHeader ); 
	
	// Allocate the buffer for the full LUN data.
	buffer = ( SCSICmd_REPORT_LUNS_Header * ) IOMalloc ( length );
	require_nonzero ( buffer, ReleaseHeader );
	
	result = RetrieveReportLUNsData ( kSCSILogicalUnitZero, ( UInt8 * ) buffer, length );
	require ( result, ReleaseBuffer );
	
	// Sanity checks on buffer passed back. The device should respond with the same data,
	// but devices aren't always trustworthy...
	require ( ( header->LUN_LIST_LENGTH == buffer->LUN_LIST_LENGTH ), ReleaseBuffer );
	
	// Parse the REPORT_LUNS information and build the LUNs reported.
	ParseReportLUNsInformation ( buffer );
	
	supportsREPORTLUNS = true;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( buffer, ReleaseHeader );
	IOFree ( buffer, length );
	buffer = NULL;
	
	
ReleaseHeader:
	
	
	require_nonzero_quiet ( header, ErrorExit );
	IODelete ( header, SCSICmd_REPORT_LUNS_Header, 1 );
	header = NULL;
	
	
ErrorExit:
	
	
	return supportsREPORTLUNS;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ParseReportLUNsInformation -  Parses REPORT_LUNS data and creates logical
//									units.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::ParseReportLUNsInformation (
						SCSICmd_REPORT_LUNS_Header * buffer )
{
	
	UInt32							count		= 0;
	UInt32							index		= 0;
	SCSICmd_REPORT_LUNS_LUN_ENTRY *	LUN			= NULL;
	bool							LUNPresent 	= false;
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::ParseReportLUNsInformation\n" ) );
	
	require_nonzero ( buffer, ErrorExit );
	
	count = OSSwapBigToHostInt32 ( buffer->LUN_LIST_LENGTH ) / ( sizeof ( SCSICmd_REPORT_LUNS_LUN_ENTRY ) );
	require ( count, ErrorExit );
	
	STATUS_LOG ( ( "count = %ld\n", count ) );
	
	for ( index = 0; index < count; index++ )
	{
		
		UInt8	addressMethod		= 0;
		UInt8	logicalUnitNumber	= 0;
		
		STATUS_LOG ( ( "Processing item %ld\n", index ) );
		
		LUN = &buffer->LUN[index];
		LUN->FIRST_LEVEL_ADDRESSING = OSSwapBigToHostInt16 ( LUN->FIRST_LEVEL_ADDRESSING );
		addressMethod = LUN->FIRST_LEVEL_ADDRESSING >> kREPORT_LUNS_ADDRESS_METHOD_OFFSET;

		STATUS_LOG ( ( "Data: 0x%04x : 0x%04x : 0x%04x : 0x%04x\n",
						OSSwapBigToHostInt16 ( LUN->FIRST_LEVEL_ADDRESSING ),
						OSSwapBigToHostInt16 ( LUN->SECOND_LEVEL_ADDRESSING ),
						OSSwapBigToHostInt16 ( LUN->THIRD_LEVEL_ADDRESSING ),
						OSSwapBigToHostInt16 ( LUN->FOURTH_LEVEL_ADDRESSING ) ) );
		
		STATUS_LOG ( ( "addressMethod = %d\n", addressMethod ) );
		
		// For now, we only support single level LUN addressing using the PERIPHERAL_DEVICE
		// method of address and with BUS_IDENTIFIER field set to zero (meaning all LUNs are
		// relative to this device and non-hierarchical)
		if ( addressMethod == kREPORT_LUNS_ADDRESS_METHOD_PERIPHERAL_DEVICE )
		{
			
			REPORT_LUNS_PERIPHERAL_DEVICE_ADDRESSING *	p;
			
			p = ( REPORT_LUNS_PERIPHERAL_DEVICE_ADDRESSING * ) &LUN->FIRST_LEVEL_ADDRESSING;
			
			// Make sure the BUS_IDENTIFIER field is zero. We don't support hierarchical LUNs
			// yet...
			if ( p->BUS_IDENTIFIER == 0 )
			{
				
				logicalUnitNumber = p->TARGET_LUN;
				STATUS_LOG ( ( "logicalUnitNumber = %d\n", logicalUnitNumber ) );
				
				// Don't create more than one logical unit object to
				// represent the same LUN.
				LUNPresent = DoesLUNObjectExist ( logicalUnitNumber );
				
				if ( LUNPresent == false )
				{
					
					LUNPresent = VerifyLogicalUnitPresence ( logicalUnitNumber );
					if ( LUNPresent == true )
					{
						CreateLogicalUnit ( logicalUnitNumber );
					}
					
				}
				
			}
			
			else
			{
				ERROR_LOG ( ( "BUS_IDENTIFIER is non-zero, not creating LUN\n" ) );
			}
			
		}
		
		else
		{
			ERROR_LOG ( ( "Not kREPORT_LUNS_ADDRESS_METHOD_PERIPHERAL_DEVICE, not creating LUN\n" ) );
		}
		
	}
	
	// According to SPC-2, Logical Unit zero must always be present. Logical Unit zero has the
	// option of presenting itself in the REPORT_LUNS LUN list. Some RAID controllers omit
	// Logical Unit zero from this list since they claim HiSup and have a PERIPHERAL_QUALIFIER
	// field of 001b or 011b.
	// 
	// So, create Logical Unit zero if the LUN object doesn't currently exist for it.
	LUNPresent = DoesLUNObjectExist ( kSCSILogicalUnitZero );
	if ( LUNPresent == false )
	{
		
		CreateLogicalUnit ( kSCSILogicalUnitZero );
		
	}
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::ParseReportLUNsInformation\n" ) );
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ExecuteCommand - Executes a command using path manager (if one exists).
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 					
IOSCSITargetDevice::ExecuteCommand ( SCSITaskIdentifier request )
{
	
	SetTargetLayerReference ( request, ( void * ) this );
	
	if ( fPathManager != NULL )
	{
		fPathManager->ExecuteCommand ( request );
	}
	
	else
	{
		super::ExecuteCommand ( request );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortTask - Aborts a task.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse 					
IOSCSITargetDevice::AbortTask ( UInt8				 		logicalUnit,
								SCSITaggedTaskIdentifier	theTag )
{
	
	if ( fPathManager != NULL )
	{
		return fPathManager->AbortTask ( logicalUnit, theTag );
	}
	
	else
	{
		return super::AbortTask ( logicalUnit, theTag );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortTaskSet - Aborts a task set.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse 					
IOSCSITargetDevice::AbortTaskSet ( UInt8 logicalUnit )
{
	
	if ( fPathManager != NULL )
	{
		return fPathManager->AbortTaskSet ( logicalUnit );
	}
	
	else
	{
		return super::AbortTaskSet ( logicalUnit );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearACA - Clears an ACA condition.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse 					
IOSCSITargetDevice::ClearACA ( UInt8 logicalUnit )
{
	
	if ( fPathManager != NULL )
	{
		return fPathManager->ClearACA ( logicalUnit );
	}
	
	else
	{
		return super::ClearACA ( logicalUnit );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearTaskSet - Clears a task set.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse 					
IOSCSITargetDevice::ClearTaskSet ( UInt8 logicalUnit )
{
	
	if ( fPathManager != NULL )
	{
		return fPathManager->ClearTaskSet ( logicalUnit );
	}
	
	else
	{
		return super::ClearTaskSet ( logicalUnit );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LogicalUnitReset - Resets a particular logical unit.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse 					
IOSCSITargetDevice::LogicalUnitReset ( UInt8 logicalUnit )
{
	
	if ( fPathManager != NULL )
	{
		return fPathManager->LogicalUnitReset ( logicalUnit );
	}
	
	else
	{
		return super::LogicalUnitReset ( logicalUnit );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TargetReset - Resets the target device.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse 					
IOSCSITargetDevice::TargetReset ( void )
{
	
	if ( fPathManager != NULL )
	{
		return fPathManager->TargetReset ( );
	}
	
	else
	{
		return super::TargetReset ( );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SuspendDeviceSupport - Suspends device support.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 					
IOSCSITargetDevice::SuspendDeviceSupport ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResumeDeviceSupport - Resumes device support.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 					
IOSCSITargetDevice::ResumeDeviceSupport ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StopDeviceSupport - Stops device support.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::StopDeviceSupport ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TerminateDeviceSupport - Terminates device support.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 					
IOSCSITargetDevice::TerminateDeviceSupport ( void )
{	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetNumberOfPowerStateTransitions - Unused.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSITargetDevice::GetNumberOfPowerStateTransitions ( void )
{
	return 0;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearNotReadyStatus - Unused.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::ClearNotReadyStatus ( void )
{
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTargetLayerReference - Sets target layer reference value in task.
//															[PROTECTED][STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::SetTargetLayerReference ( SCSITaskIdentifier 	request,
											  void *				value )
{
	
	SCSITask *	scsiRequest = NULL;
	bool		result		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest != NULL )
	{
		result = scsiRequest->SetTargetLayerReference ( value );
	}
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTargetLayerReference - Gets target layer reference value in task.
//															[PROTECTED][STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void *
IOSCSITargetDevice::GetTargetLayerReference ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest = NULL;
	void *		result		= NULL;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest != NULL )
	{
		result = scsiRequest->GetTargetLayerReference ( );
	}
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Power Management Utility Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetInitialPowerState - Unused.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32		
IOSCSITargetDevice::GetInitialPowerState ( void )
{
	return 0;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandlePowerChange - Unused.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void		
IOSCSITargetDevice::HandlePowerChange ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleCheckPowerState - Unused.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void		
IOSCSITargetDevice::HandleCheckPowerState ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TicklePowerManager - Unused.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void		
IOSCSITargetDevice::TicklePowerManager ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveCharacteristicsFromProvider - Gets characteristics from the
//											provider.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::RetrieveCharacteristicsFromProvider ( void )
{
	
	OSDictionary * 	characterDict 	= NULL;
	OSObject *		obj				= NULL;
	
	fDefaultInquiryCount = 0;
	
	// Check if the personality for this device specifies a preferred protocol
	obj = GetProtocolDriver ( )->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey );
	if ( obj != NULL )
	{
		
		characterDict = OSDynamicCast ( OSDictionary, obj );
		if ( characterDict != NULL )
		{
			
			setProperty ( kIOPropertySCSIDeviceCharacteristicsKey, characterDict );
			
			// Check if the personality for this device specifies a preferred INQUIRY count.
			if ( characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) != NULL )
			{
				
				OSNumber *	defaultInquiry = NULL;
				
				STATUS_LOG ( ( "%s: Get Inquiry Length property.\n", getName ( ) ) );
				
				obj	= characterDict->getObject ( kIOPropertySCSIInquiryLengthKey );
				
				defaultInquiry = OSDynamicCast ( OSNumber, obj );
				if ( defaultInquiry != NULL )
				{
					
					// This device has a preferred INQUIRY count, use that.
					fDefaultInquiryCount = defaultInquiry->unsigned32BitValue ( );
					
				}
				
			}
			
		}
		
	}
	
	STATUS_LOG ( ( "%s: default inquiry count is: %d\n", getName ( ), fDefaultInquiryCount ) );
	
	obj = GetProtocolDriver ( )->getProperty ( kIOPropertyProtocolCharacteristicsKey );
	characterDict = OSDynamicCast ( OSDictionary, obj );
	if ( characterDict == NULL )
	{
		characterDict = OSDictionary::withCapacity ( 1 );
	}
	
	else
	{
		characterDict->retain ( );
	}
	
	if ( characterDict != NULL )
	{
		
		obj = GetProtocolDriver ( )->getProperty ( kIOPropertyPhysicalInterconnectTypeKey );
		if ( obj != NULL )
		{
			characterDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, obj );
		}
		
		obj = GetProtocolDriver ( )->getProperty ( kIOPropertyPhysicalInterconnectLocationKey );
		if ( obj != NULL )
		{
			characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, obj );
		}
		
		setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
		
	#if 0
		// Set default device usage based on protocol characteristics
		obj = GetProtocolDriver ( )->getProperty ( kIOPropertySCSIProtocolMultiInitKey );
		if ( obj != NULL )
		{
			
			// The protocol allows for multiple initiators in the SCSI Domain, set device
			// usage to "shared" to prevent power down a drive that appears idle to this system
			// but may be in use by others.
			
		}
	#endif
		
		characterDict->release ( );
		
	}
	
}


#if 0
#pragma mark -
#pragma mark ¥ Target Related Member Routines
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineTargetCharacteristics - Method to publish INQUIRY VPD data.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::DetermineTargetCharacteristics ( void )
{
	
	bool			result		 = false;
	OSObject *		obj 		 = NULL;
	OSDictionary *	protocolDict = NULL;
	
	result = PublishDefaultINQUIRYInformation ( );
	require ( result, ErrorExit );
	
	PublishINQUIRYVitalProductDataInformation ( this, kSCSILogicalUnitZero );
	
	//PublishReportDeviceIdentifierData ( this, kSCSILogicalUnitZero );
	
	// Check to see if the HBA inserted a property by calling SetTargetProperty().
	protocolDict = OSDynamicCast ( OSDictionary, getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	require_nonzero ( protocolDict, ErrorExit );
	
	// Check if the Node WWN property is there.
	obj = protocolDict->getObject ( kIOPropertyFibreChannelNodeWorldWideNameKey );
	require_nonzero ( obj, ErrorExit );
	
	// Set the Node Unique Identifier.
	SetNodeUniqueIdentifier ( obj );
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PublishDefaultINQUIRYInformation - Publishes default INQUIRY information.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::PublishDefaultINQUIRYInformation ( void )
{
	
	bool								result		= false;
	UInt8								length		= 0;
	OSData *							data		= NULL;
	SCSICmd_INQUIRY_StandardDataAll *	inqData		= NULL;
	
	inqData = IONew ( SCSICmd_INQUIRY_StandardDataAll, 1 );
	require_nonzero ( inqData, ErrorExit );
	
	// Determine the total amount of data that this target has available for
	// the INQUIRY command.
	if ( fDefaultInquiryCount != 0 )
	{
		
		// There is a default INQUIRY size for this device, just report that
		// value back.
		length = fDefaultInquiryCount;
		
	}
	
	else
	{
		
		length = kINQUIRY_StandardDataHeaderSize + 1;
		
		// Since there is not a default INQUIRY size for this device, determine what 
		// the INQUIRY data size should be by sending an INQUIRY command for 6 bytes
		// (In actuality, only 5 bytes are needed, but asking for 6 alleviates the 
		// problem that some FireWire and USB to ATAPI bridges exhibit).
		result = RetrieveDefaultINQUIRYData ( kSCSILogicalUnitZero, ( UInt8 * ) inqData, length );	
		require ( result, ReleaseBuffer );
		
		length = inqData->ADDITIONAL_LENGTH + kINQUIRY_StandardDataHeaderSize;
		
	}
	
   	// Before we register ourself as a nub, we need to find out what 
   	// type of device we want to connect to us.
	// Do an Inquiry command and parse the data to determine the peripheral
	// device type.
	result = RetrieveDefaultINQUIRYData ( kSCSILogicalUnitZero, ( UInt8 * ) inqData, length );	
	require ( result, ReleaseBuffer );
	
	SetCharacteristicsFromINQUIRY ( inqData );
	
	data = OSData::withBytes ( ( const void * ) inqData, sizeof ( SCSICmd_INQUIRY_StandardDataAll ) );
	require_nonzero ( data, ReleaseBuffer );
	
	setProperty ( kSCSICmd_INQUIRY_StandardDataAllString, data );
	data->release ( );
	data = NULL;
	
	IODelete ( inqData, SCSICmd_INQUIRY_StandardDataAll, 1 );
	inqData = NULL;
	
	result = true;
	
	return result;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( inqData, ErrorExit );
	IODelete ( inqData, SCSICmd_INQUIRY_StandardDataAll, 1 );
	inqData = NULL;
	
	
ErrorExit:
	
	
	result = false;
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PublishINQUIRYVitalProductDataInformation - Check Page 00h for VPD pages
//	  supported by this target. Then, publish interesting pages (80h, 83h)
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::PublishINQUIRYVitalProductDataInformation (
										IOService * 			object,
										SCSILogicalUnitNumber	logicalUnit )
{
	
	bool							result		= false;
	UInt8							length		= 0;
	UInt8 							index		= 0;
	UInt8							pageLength	= 0;
	SCSICmd_INQUIRY_Page00_Header *	data		= NULL;
	UInt8 *							bytes		= NULL;
	IOBufferMemoryDescriptor *		buffer		= NULL;
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::PublishINQUIRYVitalProductDataInformation\n" ) );
	
	buffer = IOBufferMemoryDescriptor::withCapacity ( kINQUIRY_MaximumDataSize, kIODirectionIn );
	require_nonzero ( buffer, ErrorExit );
	
	length 	= sizeof ( SCSICmd_INQUIRY_Page00_Header );
	data 	= ( SCSICmd_INQUIRY_Page00_Header * ) buffer->getBytesNoCopy ( );
	bytes	= ( UInt8 * ) data;
	
	require_nonzero ( data, ReleaseBuffer );
	bzero ( data, kINQUIRY_MaximumDataSize );
	
	result = RetrieveINQUIRYDataPage ( logicalUnit,
									   bytes,
									   kINQUIRY_Page00_PageCode,
									   length );
	require ( result, ReleaseBuffer );
	
	STATUS_LOG ( ( "PERIPHERAL_DEVICE_TYPE = 0x%02x\n", data->PERIPHERAL_DEVICE_TYPE ) );
	STATUS_LOG ( ( "PAGE_CODE = 0x%02x\n", data->PAGE_CODE ) );
	STATUS_LOG ( ( "PAGE_LENGTH = %d\n", data->PAGE_LENGTH ) );
	
	pageLength = data->PAGE_LENGTH;
	
	length = data->PAGE_LENGTH + sizeof ( SCSICmd_INQUIRY_Page00_Header );
	
	STATUS_LOG ( ( "length = %d\n", length ) );
	
	result = RetrieveINQUIRYDataPage ( logicalUnit,
									   bytes,
									   kINQUIRY_Page00_PageCode,
									   length );
	require ( result, ReleaseBuffer );
	require ( ( data->PAGE_CODE == kINQUIRY_Page00_PageCode ), ReleaseBuffer );
	require ( ( data->PAGE_LENGTH == pageLength ), ReleaseBuffer );
	
	// If Vital Data Page zero was successfully retrieved, check to see if page 80h or 83h
	// the Device Identification page is supported and if so, retrieve that data.
	for ( index = sizeof ( SCSICmd_INQUIRY_Page00_Header ); index < length; index++ )
	{
		
		STATUS_LOG ( ( "PAGE_CODE_ENTRY[%d] = 0x%02x\n", ( int ) ( index - sizeof ( SCSICmd_INQUIRY_Page00_Header ) ), bytes[index] ) );
		
		if ( bytes[index] == kINQUIRY_Page80_PageCode )
		{
			
			PublishUnitSerialNumber ( object, logicalUnit );
			
		}
		
		if ( bytes[index] == kINQUIRY_Page83_PageCode )
		{
			
			PublishDeviceIdentification ( object, logicalUnit );
			
		}
		
	}
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( buffer, ErrorExit );
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::PublishINQUIRYVitalProductDataInformation\n" ) );
	
	return;
	
}
	


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VerifyTargetPresence - Verifies target presence.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::VerifyTargetPresence ( void )
{
	
	bool	presenceVerified = false;	
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::VerifyTargetPresence\n" ) );
	
	presenceVerified = VerifyLogicalUnitPresence ( kSCSILogicalUnitZero );
	
	return presenceVerified;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetCharacteristicsFromINQUIRY - 	Sets characteristics from INQUIRY
//										data.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::SetCharacteristicsFromINQUIRY (
							SCSICmd_INQUIRY_StandardDataAll * inquiryBuffer )
{
	
	OSString *		string			= NULL;
	char			tempString[17]	= { 0 }; // Maximum + 1 for null char
	
	// Set target characteristics
	// Save the target's Peripheral Device Type
	fTargetPeripheralDeviceType = ( inquiryBuffer->PERIPHERAL_DEVICE_TYPE & kINQUIRY_PERIPHERAL_TYPE_Mask );
	
	// Save the SCSI ANSI version that the device to which the device claims compliance.
	fTargetANSIVersion = ( inquiryBuffer->VERSION & kINQUIRY_ANSI_VERSION_Mask );
	
	// Set the other supported features
	fTargetHasHiSup 		= ( inquiryBuffer->RESPONSE_DATA_FORMAT & kINQUIRY_Byte3_HISUP_Mask );
	fTargetHasSCCS			= ( inquiryBuffer->SCCSReserved & kINQUIRY_Byte5_SCCS_Mask );
	fTargetHasEncServs		= ( inquiryBuffer->flags1 & kINQUIRY_Byte6_ENCSERV_Mask );
	fTargetHasMultiPorts	= ( inquiryBuffer->flags1 & kINQUIRY_Byte6_MULTIP_Mask );
	fTargetHasMChanger		= ( inquiryBuffer->flags1 & kINQUIRY_Byte6_MCHNGR_Mask );
	
#if DEBUG
	
	setProperty ( "ANSI Version", fTargetANSIVersion, 8 );
	setProperty ( "HiSup", fTargetHasHiSup );
	setProperty ( "SCCS", fTargetHasSCCS );
	setProperty ( "EncServs", fTargetHasEncServs );
	setProperty ( "MultiPorts", fTargetHasMultiPorts );
	setProperty ( "MChanger", fTargetHasMChanger );
	
#endif	/* DEBUG */
	
   	// Set the Peripheral Device Type property for the device.
   	setProperty ( kIOPropertySCSIPeripheralDeviceType,
   				( UInt64 ) fTargetPeripheralDeviceType,
   				kIOPropertySCSIPeripheralDeviceTypeSize );
	
   	// Set the Vendor Identification property for the device.
	bcopy ( inquiryBuffer->VENDOR_IDENTIFICATION, tempString, kINQUIRY_VENDOR_IDENTIFICATION_Length );
	tempString[kINQUIRY_VENDOR_IDENTIFICATION_Length] = 0;
	StripWhiteSpace ( tempString, kINQUIRY_VENDOR_IDENTIFICATION_Length );
   	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIVendorIdentification, string );
		string->release ( );
		string = NULL;
		
	}
	
   	// Set the Product Identification property for the device.
	bcopy ( inquiryBuffer->PRODUCT_IDENTIFICATION, tempString, kINQUIRY_PRODUCT_IDENTIFICATION_Length );
	tempString[kINQUIRY_PRODUCT_IDENTIFICATION_Length] = 0;
	StripWhiteSpace ( tempString, kINQUIRY_PRODUCT_IDENTIFICATION_Length );
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIProductIdentification, string );
		string->release ( );
		string = NULL;
		
	}
	
   	// Set the Product Revision Level property for the device.
	bcopy ( inquiryBuffer->PRODUCT_REVISION_LEVEL, tempString, kINQUIRY_PRODUCT_REVISION_LEVEL_Length );
	tempString[kINQUIRY_PRODUCT_REVISION_LEVEL_Length] = 0;
	StripWhiteSpace ( tempString, kINQUIRY_PRODUCT_REVISION_LEVEL_Length );
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIProductRevisionLevel, string );
		string->release ( );
		string = NULL;
		
	}
	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TargetTaskCompletion - Hook to check for Unit Attention conditions.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::TargetTaskCompletion ( SCSITaskIdentifier request )
{
	
	if ( ( GetServiceResponse ( request ) == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION ) )
	{
		
		bool 						validSense	= false;
		SCSI_Sense_Data				senseBuffer = { 0 };
		
		validSense = GetAutoSenseData ( request, &senseBuffer, sizeof ( senseBuffer ) );
		if ( validSense )
		{
			
			ERROR_LOG ( ( "IOSCSITargetDevice::TargetTaskCompletion - SENSE_KEY = %d, ASC = 0x%02x, ASCQ = 0x%02x\n",
						  senseBuffer.SENSE_KEY & kSENSE_KEY_Mask,
						  senseBuffer.ADDITIONAL_SENSE_CODE,
						  senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
			
			// Check for Unit Attention errors.
			if ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_UNIT_ATTENTION )
			{
				
				if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3F ) &&
					 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x0E ) )
				{
					
					// REPORT_LUNS DATA HAS CHANGED
					IOLog ( "REPORT_LUNS DATA HAS CHANGED\n" );
					
				}
				
				else if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3F ) &&
						  ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x03 ) )
				{
					
					// INQUIRY DATA HAS CHANGED
					IOLog ( "INQUIRY DATA HAS CHANGED\n" );
					
				}
				
			}
			
		}
		
	}
	
	TaskCompletedNotification ( request );
	
}


#if 0
#pragma mark -
#pragma mark ¥ Logical Unit Related Member Routines
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveReportLUNsData - 	OBSOLETE.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::RetrieveReportLUNsData (
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								dataBuffer,  
						UInt8									dataSize )
{
	return RetrieveReportLUNsData ( logicalUnit, dataBuffer, ( UInt32 ) dataSize );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveReportLUNsData - 	Gets REPORT_LUNS data from device.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::RetrieveReportLUNsData (
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								dataBuffer,  
						UInt32									dataSize )
{
	
	SCSIServiceResponse 	serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *	bufferDesc 		= NULL;
	SCSITaskIdentifier		request 		= NULL;
	bool					result			= false; 
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) dataBuffer,
												   dataSize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
 	request = GetSCSITask ( );
 	require_nonzero ( request, ReleaseBuffer );
	
	if ( REPORT_LUNS ( request, bufferDesc, dataSize, 0 ) == true )
	{
		
		SetLogicalUnitNumber ( request, logicalUnit );
		
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			result = true;
		}
		
		else
		{
			
			bool 						validSense	= false;
			SCSI_Sense_Data				senseBuffer = { 0 };
			
			validSense = GetAutoSenseData ( request, &senseBuffer, sizeof ( senseBuffer ) );
			if ( validSense )
			{
				
				ERROR_LOG ( ( "REPORT_LUNS failed, serviceResponse = %d, taskStatus = %d\n",
							  serviceResponse, GetTaskStatus ( request ) ) );
				ERROR_LOG ( ( "SENSE_KEY = %d, ASC = 0x%02x, ASCQ = 0x%02x\n",
							  senseBuffer.SENSE_KEY & kSENSE_KEY_Mask,
							  senseBuffer.ADDITIONAL_SENSE_CODE,
							  senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
			}
			
		}
		
	}
	
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMaximumLogicalUnitNumber - Determins Max LUN supported.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64 
IOSCSITargetDevice::DetermineMaximumLogicalUnitNumber ( void )
{
	
	UInt32		protocolLUNCount 	= 0;
	bool		supported			= false;
	
	// If this is a SCSI-3 device or later, issue the REPORT_LUNS command
	// to determine the possible Logical Units supported by the device.
	// else get the maxmium supported value from the Protocol Services
	// driver and scan to that limit.
	supported = IsProtocolServiceSupported (
							kSCSIProtocolFeature_GetMaximumLogicalUnitNumber,
							( void * ) &protocolLUNCount );
	
	if ( supported == false )
	{
		
		// Since the protocol does not report that it supports a maximum
		// Logical Unit Number, it does not support Logical Units at all
		// meaning that only LUN 0 is valid.
		protocolLUNCount = 0;
		
	}
	
	return protocolLUNCount;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VerifyLogicalUnitPresence - Verifies the presence of a logical unit.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::VerifyLogicalUnitPresence (
						SCSILogicalUnitNumber		logicalUnit )
{
	
	bool					presenceVerified 	= false;
	SCSITaskIdentifier		request				= NULL;
	UInt8					TURCount			= 0;
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::VerifyLogicalUnitPresence\n" ) );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	do
	{
		
		SCSIServiceResponse		serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		
		TEST_UNIT_READY ( request, 0x00 );
		
		STATUS_LOG ( ( "Sending TEST_UNIT_READY %d to LUN %d\n", TURCount + 1, ( int ) logicalUnit ) );
		
		SetLogicalUnitNumber ( request, logicalUnit );
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				bool 						validSense	= false;
				SCSI_Sense_Data				senseBuffer = { 0 };
				
				validSense = GetAutoSenseData ( request, &senseBuffer, sizeof ( senseBuffer ) );
				if ( validSense == false )
				{

					IOMemoryDescriptor *		bufferDesc	= NULL;
					
					bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																sizeof ( SCSI_Sense_Data ),
																kIODirectionIn );
					
					if ( bufferDesc != NULL )
					{
						
						REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 );
						serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
						
						bufferDesc->release ( );
						
					}
					
				}
				
				// Check the sense data to see if the TUR was sent to an invalid LUN and if so,
				// abort trying to access this Logical Unit. We used to check the sense key for
				// ILLEGAL_REQUEST, but some devices which aren't spun up yet will set NOT_READY
				// for the SENSE_KEY. Might as well not use it...
				if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x25 ) &&
					 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
				{
					
					ERROR_LOG ( ( "Logical unit = %lld not valid\n", logicalUnit ) );
				 	break;
				 	
				}
				
				ERROR_LOG ( ( "SENSE_KEY = %d, ASC/ASCQ = 0x%02x/0x%02x",
							  senseBuffer.SENSE_KEY & kSENSE_KEY_Mask,
							  senseBuffer.ADDITIONAL_SENSE_CODE,
							  senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
			}
			
			// The SCSI Task completed with status meaning that a target was found
			// set that the presence was verified.
			presenceVerified = true;
			
		}
		
		else if ( ( serviceResponse == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE ) &&
				  ( GetTaskStatus ( request ) == kSCSITaskStatus_DeviceNotResponding ) )
		{
			
			ERROR_LOG ( ( "taskStatus = DeviceNotResponding\n" ) );
			
		}
	
		else if ( ( serviceResponse == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE ) &&
				  ( GetTaskStatus ( request ) == kSCSITaskStatus_DeviceNotPresent ) )
		{
			
			ERROR_LOG ( ( "taskStatus = DeviceNotPresent\n" ) );
			
		}
		
		TURCount++;
		
	} while ( ( presenceVerified == false ) && ( TURCount < kTURMaxRetries ) );
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::VerifyLogicalUnitPresence, LUN %lld present = %s\n", logicalUnit, presenceVerified ? "yes" : "no" ) );
	
	return presenceVerified;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateLogicalUnit - Creates a logical unit.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::CreateLogicalUnit ( SCSILogicalUnitNumber logicalUnit )
{
	
	bool						result	= false;
	IOSCSILogicalUnitNub * 		nub		= NULL;
	
	nub = OSTypeAlloc ( IOSCSILogicalUnitNub );
	require_nonzero ( nub, ErrorExit );
	
	result = nub->init ( 0 );
	require ( result, ReleaseNub );
	
	nub->SetLogicalUnitNumber ( ( UInt8 ) logicalUnit );
	
	result = nub->attach ( this );
	require ( result, ReleaseNub );
	
	PublishINQUIRYVitalProductDataInformation ( nub, logicalUnit );
	
	result = nub->start ( this );
	require_action_quiet ( result, ReleaseNub, nub->detach ( this ) );
	
	
ReleaseNub:
	
	
	require_nonzero_quiet ( nub, ErrorExit );
	nub->release ( );
	nub = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetLogicalUnitNumber - Sets the LUN for a request.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::SetLogicalUnitNumber (
						SCSITaskIdentifier 		request,
						SCSILogicalUnitNumber	logicalUnit )
{
	
	if ( logicalUnit != 0 )
	{
		
		SCSITask *	scsiRequest = NULL;
		
	    scsiRequest = OSDynamicCast ( SCSITask, request );
	    if ( scsiRequest != NULL )
	    {
			
			scsiRequest->SetLogicalUnitNumber ( logicalUnit );
			
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DoesLUNObjectExist - Check if a LUN object exists.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::DoesLUNObjectExist ( SCSILogicalUnitNumber logicalUnit )
{
	
	bool					result	= false;
	OSCollectionIterator *	iter	= NULL;
	IOSCSILogicalUnitNub *	lun		= NULL;
	OSObject *				obj		= NULL;
	
	iter = OSCollectionIterator::withCollection ( fClients );
	if ( iter != NULL )
	{
		
		
BeginLoop:
		
		
		obj = iter->getNextObject ( );
		while ( obj != NULL )
		{
			
			lun = OSDynamicCast ( IOSCSILogicalUnitNub, obj );
			
			if ( ( lun != NULL ) && ( lun->GetLogicalUnitNumber ( ) == logicalUnit ) )
			{
				
				result = true;
				break;
				
			}
			
			if ( iter->isValid ( ) )
			{
				obj = iter->getNextObject ( );
			}
			
			else
			{
				
				iter->reset ( );
				ERROR_LOG ( ( "IOSCSITargetDevice::DoesLUNObjectExist collection changed from underneath iterator\n" ) );
				goto BeginLoop;
				
			}
			
		}
		
		iter->release ( );
		
	}
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Utility Member Routines
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveDefaultINQUIRYData - 	Retrieves as much INQUIRY data as
//									requested.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSITargetDevice::RetrieveDefaultINQUIRYData ( 
							SCSILogicalUnitNumber	logicalUnit,
							UInt8 * 				inquiryBuffer,  
							UInt8					inquirySize )
{
	
	SCSIServiceResponse 		serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *		bufferDesc 		= NULL;
	SCSITaskIdentifier			request 		= NULL;
 	int 						index			= 0;
	bool						result			= false;
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inquiryBuffer,
												   inquirySize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
 	request = GetSCSITask ( );
 	require_nonzero ( request, ReleaseBuffer );
	
	for ( index = 0; index < kMaxInquiryAttempts; index++ )
	{
		
		result = INQUIRY ( request, bufferDesc, 0, 0, 0, inquirySize, 0 );
		require ( result, ReleaseTask );
		
		SetLogicalUnitNumber ( request, logicalUnit );
		
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{	
			
			result = true;
			break;	
			
		}
		
		else
		{
			
			result = false;
			IOSleep ( 1000 );
			
		}
		
	}
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseBuffer );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveINQUIRYDataPage - Retrieves as much INQUIRY data for a given
//								page as requested.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::RetrieveINQUIRYDataPage (
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								inquiryBuffer,  
						UInt8									inquiryPage,
						UInt8									inquirySize )
{
	
	SCSIServiceResponse 	serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request 		= NULL;
	IOMemoryDescriptor *	bufferDesc 		= NULL;
	bool					result			= false; 
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inquiryBuffer,
												   inquirySize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
 	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseBuffer );
	
	if ( INQUIRY (
			request,
			bufferDesc,
			0,
		  	1,
			inquiryPage,
			inquirySize,
			0 ) == true )
	{
		
		SetLogicalUnitNumber ( request, logicalUnit );
		
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			result = true;	
		}
		
		else
		{
			
			bool 						validSense	= false;
			SCSI_Sense_Data				senseBuffer = { 0 };

			ERROR_LOG ( ( "RetrieveINQUIRYDataPage failed, page = 0x%02x, serviceResponse = %d, taskStatus = %d\n",
						  inquiryPage, serviceResponse, GetTaskStatus ( request ) ) );
			
			validSense = GetAutoSenseData ( request, &senseBuffer, sizeof ( senseBuffer ) );
			if ( validSense )
			{
				
				ERROR_LOG ( ( "SENSE_KEY = %d, ASC = 0x%02x, ASCQ = 0x%02x\n",
							  senseBuffer.SENSE_KEY & kSENSE_KEY_Mask,
							  senseBuffer.ADDITIONAL_SENSE_CODE,
							  senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
			}
			
		}
		
	}
	
	ReleaseSCSITask ( request );
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PublishUnitSerialNumber - Publishes unit serial number
//								(page 0x80) data.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::PublishUnitSerialNumber ( IOService * 			object,
											  SCSILogicalUnitNumber	logicalUnit )
{
	
	bool							result 									= false;
	SCSICmd_INQUIRY_Page80_Header *	data									= NULL;
	IOBufferMemoryDescriptor *		buffer									= NULL;
	OSString *						string									= NULL;
	char							serialNumber[kINQUIRY_MaximumDataSize]	= { 0 };
	UInt8							length									= 0;
	UInt8							pageLength								= 0;
	SInt16							serialLength							= 0;
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::PublishUnitSerialNumber\n" ) );
	
	buffer = IOBufferMemoryDescriptor::withCapacity ( kINQUIRY_MaximumDataSize, kIODirectionIn );
	require_nonzero ( buffer, ErrorExit );
	
	// Reading header should not include PRODUCT_SERIAL_NUMBER field. Adjust
	// length accordingly.
	length 	= sizeof ( SCSICmd_INQUIRY_Page80_Header ) - sizeof ( UInt8 );
	data 	= ( SCSICmd_INQUIRY_Page80_Header * ) buffer->getBytesNoCopy ( );
	
	require_nonzero ( data, ReleaseBuffer );
	bzero ( data, kINQUIRY_MaximumDataSize );
	
	// This device reports that it supports Inquiry Page 80
	// determine the length of the unit serial number.
	result = RetrieveINQUIRYDataPage ( logicalUnit,
									   ( UInt8 * ) data,
									   kINQUIRY_Page80_PageCode,
									   length );
	require ( result, ReleaseBuffer );
	
	// Verify that the device does indeed have Device Identification information, by checking
	// that the additional length field is not zero.
	require_nonzero ( data->PAGE_LENGTH, ReleaseBuffer );

	STATUS_LOG ( ( "PERIPHERAL_DEVICE_TYPE = 0x%02x\n", data->PERIPHERAL_DEVICE_TYPE ) );
	STATUS_LOG ( ( "PAGE_CODE = 0x%02x\n", data->PAGE_CODE ) );
	STATUS_LOG ( ( "PAGE_LENGTH = %d\n", data->PAGE_LENGTH ) );
	
	pageLength = data->PAGE_LENGTH;
	
	length = sizeof ( SCSICmd_INQUIRY_Page80_Header ) - sizeof ( UInt8 ) + data->PAGE_LENGTH;
	
	STATUS_LOG ( ( "length = %d\n", length ) );
	
	result = RetrieveINQUIRYDataPage ( logicalUnit,
									   ( UInt8 * ) data,
									   kINQUIRY_Page80_PageCode,
									   length );
	require ( result, ReleaseBuffer );
	require ( ( data->PAGE_CODE == kINQUIRY_Page80_PageCode ), ReleaseBuffer );
	require ( ( data->PAGE_LENGTH == pageLength ), ReleaseBuffer );
	
	bcopy ( &data->PRODUCT_SERIAL_NUMBER, serialNumber, data->PAGE_LENGTH );
	
	// ¤8.4.6 of SPC-2 indicates that if the product serial number is not available,
	// the device server shall return ASCII spaces in the field.
	
	// We treat this as though the device has no unit serial number. Get rid of
	// all spaces at the end and turn them into NULL characters.
	StripWhiteSpace ( serialNumber, data->PAGE_LENGTH );
	
	serialLength = strlen ( serialNumber );
	
	// Was the serial number field totally blank?
	if ( serialLength > 0 )
	{
		
		// No it wasn't blank, so set the property in the registry.
		string = OSString::withCString ( serialNumber );
		if ( string != NULL )
		{
			
			object->setProperty ( kIOPropertySCSIINQUIRYUnitSerialNumber, string );
			string->release ( );
			string = NULL;
			
		}
		
	}
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( buffer, ErrorExit );
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::PublishUnitSerialNumber\n" ) );
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PublishDeviceIdentification - Publishes device ID page (page 0x83) data.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::PublishDeviceIdentification (
							IOService * 			object,
							SCSILogicalUnitNumber	logicalUnit )
{
	
	OSArray *											deviceIDs 		= NULL;
	SCSICmd_INQUIRY_Page83_Identification_Descriptor *	descriptor		= NULL;
	SCSICmd_INQUIRY_Page83_Header *						data			= NULL;
	IOBufferMemoryDescriptor *							buffer			= NULL;
	UInt8 *												bytes			= NULL;
	UInt8												length			= 0;
	UInt8												offset			= 0;
	UInt8												pageLength		= 0;
	bool												result			= false;
	
	STATUS_LOG ( ( "+IOSCSITargetDevice::PublishDeviceIdentification\n" ) );
	
	buffer = IOBufferMemoryDescriptor::withCapacity ( kINQUIRY_MaximumDataSize, kIODirectionIn );
	require_nonzero ( buffer, ErrorExit );
	
	length 	= sizeof ( SCSICmd_INQUIRY_Page83_Header );
	data 	= ( SCSICmd_INQUIRY_Page83_Header * ) buffer->getBytesNoCopy ( );
	bytes	= ( UInt8 * ) data;

	require_nonzero ( data, ReleaseBuffer );
	
	bzero ( data, kINQUIRY_MaximumDataSize );
	
	// This device reports that it supports Inquiry Page 83h
	// determine the length of all descriptors
	result = RetrieveINQUIRYDataPage ( logicalUnit,
									   bytes,
									   kINQUIRY_Page83_PageCode,
									   length );
	require ( result, ReleaseBuffer );
	
	// Verify that the device does indeed have Device Identification information, by checking
	// that the additional length field is not zero.
	require_nonzero ( data->PAGE_LENGTH, ReleaseBuffer );

	STATUS_LOG ( ( "PAGE_LENGTH = %d\n", data->PAGE_LENGTH ) );
	
	pageLength = data->PAGE_LENGTH;
	
	length = data->PAGE_LENGTH + sizeof ( SCSICmd_INQUIRY_Page83_Header );
	
	result = RetrieveINQUIRYDataPage ( logicalUnit,
									   bytes,
									   kINQUIRY_Page83_PageCode,
									   length );
	
	require ( result, ReleaseBuffer );
	require ( ( data->PAGE_CODE == kINQUIRY_Page83_PageCode ), ReleaseBuffer );
	require ( ( pageLength == data->PAGE_LENGTH ), ReleaseBuffer );
	
	// Create the array to hold the ID dictionaries
	deviceIDs = OSArray::withCapacity ( 1 );
	require_nonzero ( deviceIDs, ReleaseBuffer );
	
	// Set the index to the first ID byte
	offset = sizeof ( SCSICmd_INQUIRY_Page83_Header );
	
	STATUS_LOG ( ( "PERIPHERAL_DEVICE_TYPE = 0x%02x\n", data->PERIPHERAL_DEVICE_TYPE ) );
	STATUS_LOG ( ( "PAGE_CODE = 0x%02x\n", data->PAGE_CODE ) );
	STATUS_LOG ( ( "PAGE_LENGTH = %d\n", data->PAGE_LENGTH ) );
	
	STATUS_LOG ( ( "length = %d\n", length ) );
	STATUS_LOG ( ( "offset = %d\n", offset ) );
	
	while ( offset < length )
	{
		
		UInt8				codeSet			= 0;
		UInt8				idType			= 0;
		UInt8				association		= 0;
		OSDictionary * 		idDictionary	= NULL;
		OSNumber *			number	 		= NULL;
		
		STATUS_LOG ( ( "Processing SCSICmd_INQUIRY_Page83_Identification_Descriptor\n" ) );
		
		// Create the dictionary for the current device ID
		idDictionary = OSDictionary::withCapacity ( 1 );
		require_nonzero ( idDictionary, ReleaseDeviceIDs );
		
		descriptor = ( SCSICmd_INQUIRY_Page83_Identification_Descriptor * ) &bytes[offset];
		
		STATUS_LOG ( ( "CODE_SET = 0x%02x\n", descriptor->CODE_SET ) );
		STATUS_LOG ( ( "IDENTIFIER_TYPE = 0x%02x\n", descriptor->IDENTIFIER_TYPE ) );
		STATUS_LOG ( ( "IDENTIFIER_LENGTH = %d\n", descriptor->IDENTIFIER_LENGTH ) );
		
		// Move offset.
		offset += descriptor->IDENTIFIER_LENGTH +
			offsetof ( SCSICmd_INQUIRY_Page83_Identification_Descriptor, IDENTIFIER );
		
		// Sanity check the length to make sure it is within bounds of the page data.
		require_action ( ( offset <= length ), ReleaseDeviceIDs, idDictionary->release ( ) );
		
	#if DEBUG_MORE
		
		UInt8	index = 0;
		
		for ( index = 0; index < descriptor->IDENTIFIER_LENGTH; index++ )
		{
			
			UInt8 *		identifier = ( UInt8 * ) &descriptor->IDENTIFIER;
			
			STATUS_LOG ( ( "identifier[%d] = 0x%02x : ", index, identifier[index] ) );
			
		}
		
		STATUS_LOG ( ( "\n" ) );
		
	#endif	/* DEBUG_MORE */
		
		// Process identification header
		codeSet = descriptor->CODE_SET & kINQUIRY_Page83_CodeSetMask;
		number = OSNumber::withNumber ( codeSet, 8 );
		if ( number != NULL )
		{
			
			idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdCodeSet, number );
			number->release ( );
			number = NULL;
			
		}
		
		idType = descriptor->IDENTIFIER_TYPE & kINQUIRY_Page83_IdentifierTypeMask;
		number = OSNumber::withNumber ( idType, 8 );
		if ( number != NULL )
		{
			
			idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdType, number );
			number->release ( );
			number = NULL;
			
		}
		
		association = ( ( descriptor->IDENTIFIER_TYPE & kINQUIRY_Page83_AssociationMask ) >> 4 );
		number = OSNumber::withNumber ( association, 8 );
		if ( number != NULL )
		{
			
			idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdAssociation, number );
			number->release ( );
			number = NULL;
			
		}
		
		if ( codeSet == kINQUIRY_Page83_CodeSetASCIIData )
		{
			
			OSString *		string 							 		= NULL;
			char			identifier[kINQUIRY_MaximumDataSize] 	= { 0 };
			
			bcopy ( &descriptor->IDENTIFIER, identifier, descriptor->IDENTIFIER_LENGTH );
						
			string = OSString::withCString ( identifier );
			if ( string != NULL )
			{
				
				idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdentifier, string );
				string->release ( );
				string = NULL;
				
			}
			
		}
		
		else
		{
			
			OSData *	idData = NULL;
			
			idData = OSData::withBytes ( &descriptor->IDENTIFIER, descriptor->IDENTIFIER_LENGTH );
			if ( idData != NULL )
			{
				
				idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdentifier, idData );
				
				// Are we getting the data for the target device?
				if ( object == this )
				{
					
					// Yes, this is the target device, so set the node unique
					// identifier if this identifier is the EUI-64 or FCNameIdentifier.
					if ( ( codeSet == kINQUIRY_Page83_CodeSetBinaryData ) &&
						 ( ( idType == kINQUIRY_Page83_IdentifierTypeIEEE_EUI64 ) ||
						   ( idType == kINQUIRY_Page83_IdentifierTypeFCNameIdentifier ) ) &&
						 ( association == ( kINQUIRY_Page83_AssociationDevice >> 4 ) ) )
					{
						SetNodeUniqueIdentifier ( idData );
					}
					
				}
				
				idData->release ( );
				idData = NULL;
				
			}
			
		}
		
		STATUS_LOG ( ( "offset = %d\n", offset ) );
		
		deviceIDs->setObject ( idDictionary );
		
		idDictionary->release ( );
		idDictionary = NULL;
		
	}
	
	object->setProperty ( kIOPropertySCSIINQUIRYDeviceIdentification, deviceIDs );
	
	
ReleaseDeviceIDs:
	
	
	require_nonzero_quiet ( deviceIDs, ErrorExit );
	deviceIDs->release ( );
	deviceIDs = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( buffer, ErrorExit );
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-IOSCSITargetDevice::PublishDeviceIdentification\n" ) );
	
	return;
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  1 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  2 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  3 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  4 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  5 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  6 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  7 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  8 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  9 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 10 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 11 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 12 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 13 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 14 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 15 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 16 );