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
#include <libkern/c++/OSNumber.h>

// Generic IOKit related headers
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>

// IOKit storage related headers
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>

// SCSI Architecture Model Family includes
#include "SCSITask.h"
#include "SCSILibraryRoutines.h"
#include "SCSICmds_INQUIRY_Definitions.h"
#include "IOSCSIPeripheralDeviceNub.h"
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "SCSIPrimaryCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSI Peripheral Device Nub"

#if DEBUG
#define SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIProtocolServices
OSDefineMetaClassAndStructors ( IOSCSIPeripheralDeviceNub, IOSCSIProtocolServices );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kMaxInquiryAttempts 					8


#if 0
#pragma mark -
#pragma mark ¥ Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ init - Called by IOKit to initialize us.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::init ( OSDictionary * propTable )
{
	
	bool	result = false;
	
	require ( super::init ( propTable ), ErrorExit );
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Called by IOKit to start our services.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::start ( IOService * provider )
{
	
	OSDictionary *	characterDict	= NULL;
	OSObject *		obj				= NULL;
	bool			result			= false;
	
	// Check if our provider is one of us. If it is, we don't want to
	// load again.
	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceNub, provider );
	require_quiet ( ( fProvider == NULL ), ErrorExit );
	
	// Make sure our provider is a protocol services driver.
	fProvider = OSDynamicCast ( IOSCSIProtocolInterface, provider );
	require_nonzero_quiet ( fProvider, ErrorExit );
	
	// Call our super class' start routine so that all inherited
	// behavior is initialized.
	require ( super::start ( provider ), ErrorExit );
	
	fSCSIPrimaryCommandObject = OSTypeAlloc ( SCSIPrimaryCommands );
	require_nonzero ( fSCSIPrimaryCommandObject, ErrorExit );
	
	require ( fProvider->open ( this ), ReleaseCommandObject );
	
	STATUS_LOG ( ( "%s: Check for the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
	
	// Check if the personality for this device specifies a preferred protocol
	if ( ( fProvider->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) == NULL )
	{
		
		STATUS_LOG ( ( "%s: No SCSI Device Characteristics property, set defaults.\n", getName ( ) ) );
		fDefaultInquiryCount = 0;
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "%s: Get the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
		characterDict = OSDynamicCast ( OSDictionary, ( fProvider->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) );
		
		STATUS_LOG ( ( "%s: set this SCSI Device Characteristics property.\n", getName ( ) ) );
		setProperty ( kIOPropertySCSIDeviceCharacteristicsKey, characterDict );
		
		// Check if the personality for this device specifies a preferred protocol
		STATUS_LOG ( ( "%s: check for the Inquiry Length property.\n", getName ( ) ) );
		if ( characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) == NULL )
		{
			
			STATUS_LOG ( ( "%s: No Inquiry Length property, use default.\n", getName ( ) ) );
			fDefaultInquiryCount = 0;
			
		}
		
		else
		{
			
			OSNumber *	defaultInquiry = NULL;
			
			STATUS_LOG ( ( "%s: Get Inquiry Length property.\n", getName ( ) ) );
			defaultInquiry = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) );
			
			// This device has a preferred protocol, use that.
			fDefaultInquiryCount = defaultInquiry->unsigned32BitValue ( );
			
		}
		
	}
	
	STATUS_LOG ( ( "%s: default inquiry count is: %d\n", getName ( ), fDefaultInquiryCount ) );
	
	require ( InterrogateDevice ( ), CloseProvider );
	
	setProperty ( kIOMatchCategoryKey, kSCSITaskUserClientIniterKey );
	
	characterDict = OSDynamicCast ( OSDictionary, fProvider->getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( characterDict == NULL )
	{
		characterDict = OSDictionary::withCapacity ( 1 );
	}
	
	else
	{
		characterDict = OSDictionary::withDictionary ( characterDict );
	}
	
	obj = fProvider->getProperty ( kIOPropertyPhysicalInterconnectTypeKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, obj );
	}
	
	obj = fProvider->getProperty ( kIOPropertyPhysicalInterconnectLocationKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, obj );
	}
	
	obj = fProvider->getProperty ( kIOPropertyReadTimeOutDurationKey ); 
	if ( obj != NULL );
	{
		characterDict->setObject ( kIOPropertyReadTimeOutDurationKey, obj );
	}
	
	obj = fProvider->getProperty ( kIOPropertyWriteTimeOutDurationKey );	
	if ( obj != NULL );
	{
		characterDict->setObject ( kIOPropertyWriteTimeOutDurationKey, obj );
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
	
	characterDict->release ( );
	
	// Register this object as a nub for the Logical Unit Driver.
	registerService ( );
	
	STATUS_LOG ( ( "%s: Registered and setup is complete\n", getName ( ) ) );
	
	// Setup was successful, return true.
	result = true;
	
	return result;
	
	
CloseProvider:
	
	
	fProvider->close ( this );
	
	
ReleaseCommandObject:
	
	
	require_nonzero_quiet ( fSCSIPrimaryCommandObject, ErrorExit );
	fSCSIPrimaryCommandObject->release ( );
	fSCSIPrimaryCommandObject = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ stop - Called by IOKit to stop our services.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIPeripheralDeviceNub::stop ( IOService * provider )
{
	
	STATUS_LOG ( ("%s: stop called\n", getName( ) ) );
	
	require_nonzero ( fProvider, ErrorExit );
	require ( ( fProvider == provider ), ErrorExit );
	
	require_nonzero ( fSCSIPrimaryCommandObject, ErrorExit );
	
	fSCSIPrimaryCommandObject->release ( );
	fSCSIPrimaryCommandObject = NULL;
	
	super::stop ( provider );
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Called by IOKit to deliver messages.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIPeripheralDeviceNub::message ( UInt32			type,
									 IOService *	nub,
									 void *			arg )
{
	
	IOReturn status = kIOReturnSuccess;
	
	switch ( type )
	{
		
		// The device is no longer attached to the bus
		// close provider so message will continue
		// to propagate up the driver stack.
		case kIOMessageServiceIsRequestingClose:
		{
			
			STATUS_LOG ( ( "%s: kIOMessageServiceIsRequestingClose called\n", getName ( ) ) );
			if ( fProvider != NULL )
			{
				
				STATUS_LOG ( ( "%s: closing provider\n", getName ( ) ) );
				fProvider->close ( this );
				
			}
			
		}
		break;
		
		case kSCSIProtocolNotification_VerifyDeviceState:
		{
			
			messageClients ( kSCSIProtocolNotification_VerifyDeviceState, NULL, NULL );
			status = kIOReturnSuccess;
			
		}
		break;
		
		default:
		{
			
			STATUS_LOG ( ( "%s: some message = %ld called\n", getName ( ), type ) );
			status = super::message ( type, nub, arg );
			
		}
		break;
		
	}
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Power Management Utility Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ InitializePowerManagement - Does nothing.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIPeripheralDeviceNub::InitializePowerManagement ( IOService * provider )
{
	
	// This piece does not get any power management
	// since it is just a relay class. Do NOT call registerPowerDriver
	// and do not call super::InitializePowerManagement
	// which does that.
	STATUS_LOG ( ( "%s%s::%s not doing anything%s\n", "\033[33m",
					getName ( ), __FUNCTION__, "\033[0m" ) );
	
}


#if 0
#pragma mark -
#pragma mark ¥ Property Table Utility Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ matchPropertyTable - Handles passive matching.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::matchPropertyTable ( OSDictionary *	table,
												SInt32 *		score )
{
	
	bool	returnValue		= true;
	bool	isMatch			= false;
	SInt32	propertyScore	= *score;
	
	// Clamp the copy of the score to 4000 to prevent us from promoting
	// a device driver a full ranking order. Our increments for ranking
	// order increase by 5000 and we add propertyScore to *score on exit
	// if the exiting value of *score is non-zero.
	if ( propertyScore >= 5000 )
	{
		propertyScore = 4000;
	}
	
	if ( sCompareIOProperty ( this, table, kIOPropertySCSIPeripheralDeviceType, &isMatch ) )
	{
		
		if ( isMatch )
		{
			*score = kDefaultProbeRanking;
		}
		
		else
		{
			
			*score = kPeripheralDeviceTypeNoMatch;
			returnValue = false;
		
		}

		if ( sCompareIOProperty ( this, table, kIOPropertySCSIVendorIdentification, &isMatch ) )
		{
			
			if ( isMatch )
			{
				
				*score = kFirstOrderRanking;

				if ( sCompareIOProperty ( this, table, 
						kIOPropertySCSIProductIdentification, &isMatch ) )
				{
					
					if ( isMatch )
					{
						
						*score = kSecondOrderRanking;

						if ( sCompareIOProperty ( this, table,
							kIOPropertySCSIProductRevisionLevel, &isMatch ) )
						{
							
							if ( isMatch ) 
							{
								*score = kThirdOrderRanking;
							}
							
							else
							{
							
								*score = kPeripheralDeviceTypeNoMatch;
								returnValue = false;							

							}
							
						}
					
					}
					else
					{
						
						*score = kPeripheralDeviceTypeNoMatch;
						returnValue = false;
						
					}
				
				}
			
			}
			
			else
			{
				
				*score = kPeripheralDeviceTypeNoMatch;
				returnValue = false;
			
			}
		
		}
	
	}
	
	else
	{
		
		// We need to special case the SCSITaskUserClient "driver"
		// because if no driver matches, then we want it to match on the device
		
		OSString *	string;
		
		string = OSDynamicCast ( OSString, table->getObject ( kIOMatchCategoryKey ) );
		if ( string != NULL )
		{
			
			if ( strcmp ( string->getCStringNoCopy ( ), kIOPropertySCSITaskUserClientDevice ) )
			{
				*score = kDefaultProbeRanking - 100;
			}
		
		}
	
		else
		{
			
			*score = kPeripheralDeviceTypeNoMatch;
			returnValue = false;
		
		}
		
	}
	
	if ( *score != 0 )
		*score += propertyScore;
	
	return returnValue;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sCompareIOProperty - Compares properties.				  [STATIC][PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::sCompareIOProperty (
								IOService *			object,
								OSDictionary *		table,
								char *				propertyKeyName,
								bool *				matches )
{
	
	OSObject *	tableObject		= NULL;
	OSObject *	deviceObject	= NULL;
	bool		returnValue		= false;
	
	*matches = false;
	
	tableObject	 = table->getObject ( propertyKeyName );
	deviceObject = object->getProperty ( propertyKeyName );
	
	require_nonzero_quiet ( deviceObject, ErrorExit );
	require_nonzero_quiet ( tableObject, ErrorExit );
	
	returnValue = true;
	*matches = deviceObject->isEqualTo ( tableObject );
	
	
ErrorExit:
	
	
	return returnValue;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Provided Services Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SendSCSICommand - Implements required method from IOSCSIProtocolServices.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::SendSCSICommand (
							SCSITaskIdentifier		request,
							SCSIServiceResponse *	serviceResponse,
							SCSITaskStatus *		taskStatus )
{
	return false;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortSCSICommand - Implements required method from IOSCSIProtocolServices.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::AbortSCSICommand ( SCSITaskIdentifier request )
{
	return kSCSIServiceResponse_FUNCTION_REJECTED;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ExecuteCommand - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIPeripheralDeviceNub::ExecuteCommand ( SCSITaskIdentifier request )
{
	fProvider->ExecuteCommand ( request );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortTask - Relays command to protocol services driver.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::AbortTask ( UInt8 theLogicalUnit, SCSITaggedTaskIdentifier theTag )
{
	return	fProvider->AbortTask ( theLogicalUnit, theTag );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortTaskSet - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse		
IOSCSIPeripheralDeviceNub::AbortTaskSet ( UInt8 theLogicalUnit )
{
	return	fProvider->AbortTaskSet ( theLogicalUnit );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearACA - Relays command to protocol services driver.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::ClearACA ( UInt8 theLogicalUnit )
{
	return	fProvider->ClearACA ( theLogicalUnit );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearTaskSet - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::ClearTaskSet ( UInt8 theLogicalUnit )
{
	return	fProvider->ClearTaskSet ( theLogicalUnit );
}
	

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LogicalUnitReset - Relays command to protocol services driver.[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::LogicalUnitReset ( UInt8 theLogicalUnit )
{
	return	fProvider->LogicalUnitReset ( theLogicalUnit );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TargetReset - Relays command to protocol services driver.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::TargetReset ( void )
{
	return	fProvider->TargetReset ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortCommand - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::AbortCommand ( SCSITaskIdentifier abortTask )
{
	return fProvider->AbortCommand ( abortTask );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsProtocolServiceSupported - Relays command to protocol services driver.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::IsProtocolServiceSupported (
								SCSIProtocolFeature		feature,
								void *					serviceValue )
{
	return fProvider->IsProtocolServiceSupported ( feature, serviceValue );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleProtocolServiceFeature - Relays command to protocol services driver.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::HandleProtocolServiceFeature (
								SCSIProtocolFeature		feature,
								void *					serviceValue )
{
	return fProvider->HandleProtocolServiceFeature ( feature, serviceValue );
}


#if 0
#pragma mark -
#pragma mark ¥ Private Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TaskCallback - Callback method for synchronous commands.
//															  [STATIC][PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOSCSIPeripheralDeviceNub::TaskCallback ( SCSITaskIdentifier completedTask )
{
	
	SCSITask *						scsiRequest = NULL;
	IOSCSIPeripheralDeviceNub *		owner		= NULL;
	
	scsiRequest = OSDynamicCast ( SCSITask, completedTask );
	require_nonzero ( scsiRequest, ErrorExit );
	
	owner = ( IOSCSIPeripheralDeviceNub * ) scsiRequest->GetTaskOwner ( );
	require_nonzero ( owner, ErrorExit );
	
	owner->TaskCompletion ( completedTask );
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TaskCompletion - Callback method for synchronous commands.	  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOSCSIPeripheralDeviceNub::TaskCompletion ( SCSITaskIdentifier completedTask )
{
	
	fCommandGate->commandWakeup ( completedTask, true );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ sWaitForTask - Called to wait for a task until it completes.
//															  [STATIC][PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIPeripheralDeviceNub::sWaitForTask (
									void *		object,
									SCSITask *	request )
{
	
	IOSCSIPeripheralDeviceNub * device = NULL;
	
	device = OSDynamicCast ( IOSCSIPeripheralDeviceNub, ( OSObject * ) object );
	
	return device->GatedWaitForTask ( request );
	
}




//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ GatedWaitForTask - Called to wait for a task to complete.		  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIPeripheralDeviceNub::GatedWaitForTask ( SCSITask * request )
{
	
	IOReturn	result	= kIOReturnBadArgument;
	
	if ( request != NULL )
	{
		
		while ( request->GetTaskState ( ) != kSCSITaskState_ENDED )
		{
			
			result = fCommandGate->commandSleep ( request, THREAD_UNINT );
			
		}
		
	}
	
	if ( result == THREAD_AWAKENED )
		result = kIOReturnSuccess;
	
	return result;
	
}




#if 0
#pragma mark -
#pragma mark ¥ Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ InterrogateDevice - Method to interrogate device.				  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::InterrogateDevice ( void )
{
	
	OSString *						string			= NULL;
	SCSICmd_INQUIRY_StandardData *	inqData			= NULL;
	SCSIServiceResponse				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSI_Sense_Data					senseBuffer		= { 0 };
	IOMemoryDescriptor *			bufferDesc		= NULL;
	SCSITask *						request			= NULL;
	UInt8							inqDataCount	= 0;
	int								index			= 0;
	bool							result			= false;
	char							tempString[17]	= { 0 }; // Maximum + 1 for null char
	
	// Before we register ourself as a nub, we need to find out what 
	// type of device we want to connect to us
	// Do an Inquiry command and parse the data to determine the peripheral
	// device type.
	if ( fDefaultInquiryCount == 0 )
	{
		
		// There is no default Inquiry count for this device, use the standard
		// structure size.
		STATUS_LOG ( ( "%s: use sizeof(SCSICmd_INQUIRY_StandardData) for Inquiry.\n", getName ( ) ) );
		inqDataCount = sizeof ( SCSICmd_INQUIRY_StandardData );
		
	}
	
	else
	{
		
		// This device has a default inquiry count, use it.
		STATUS_LOG ( ( "%s: use fDefaultInquiryCount for Inquiry.\n", getName ( ) ) );
		inqDataCount = fDefaultInquiryCount;
		check ( inqDataCount >= sizeof ( SCSICmd_INQUIRY_StandardData ) );
		
	}
	
	inqData = ( SCSICmd_INQUIRY_StandardData * ) IOMalloc ( inqDataCount );
	require_nonzero ( inqData, ErrorExit );
	bzero ( inqData, inqDataCount );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inqData,
												   inqDataCount,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseInquiryBuffer );
	
	request = OSTypeAlloc ( SCSITask );	
	require_nonzero ( request, ReleaseBuffer );
	request->ResetForNewTask ( );
	fSCSIPrimaryCommandObject->TEST_UNIT_READY ( request, 0x00 );
	request->SetTimeoutDuration ( 10000 );
	
	// The command was successfully built, now send it
	serviceResponse = SendTask ( request );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		bool validSense = false;
		
		if ( request->GetTaskStatus ( ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			validSense = request->GetAutoSenseData ( &senseBuffer, sizeof ( senseBuffer ) );
			if ( validSense == false )
			{
				
				request->ResetForNewTask ( );
				fSCSIPrimaryCommandObject->REQUEST_SENSE ( request,
														   bufferDesc,
														   kSenseDefaultSize,
														   0 );
				request->SetTimeoutDuration ( 10000 );
				serviceResponse = SendTask ( request );
				
				if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
					 ( request->GetTaskStatus ( ) == kSCSITaskStatus_GOOD ) )
				{
					// Check that the REQUEST SENSE command completed successfully.
					validSense = true;
				}
				
			}
			
			// If valid sense data was obtained, parse it now to see if it was considered
			// an ILLEGAL REQUEST and if so it was most likely due to accessing an invalid
			// Logical unit on the device.
			if ( validSense == true )
			{
				
				#if VERIFY_SENSE_DATA_VALID
				
				// Check that the SENSE DATA is valid
				if ( ( senseBuffer.VALID_RESPONSE_CODE & kSENSE_DATA_VALID_Mask ) == kSENSE_DATA_VALID )
				
				// We obtained traces showing that some devices don't actually set the valid sense data
				// bit when sense data is definitely valid! Because of this, we don't actually check the
				// valid bit and just look at the data which is returned. Too bad...
				
				#endif /* VERIFY_SENSE_DATA_VALID */
				
				{
					
					// Check the sense data to see if the TUR was sent to an invalid LUN and if so,
					// abort trying to access this Logical Unit. We used to check the sense key for
					// ILLEGAL_REQUEST, but some devices which aren't spun up yet will set NOT_READY
					// for the SENSE_KEY. Might as well not use it...
					if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x25 ) &&
						 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
					{
						
						ERROR_LOG ( ( "ASC/ASCQ = 0x25/0x00 - Invalid LUN\n" ) );
						goto ReleaseTask;
						
					}
					
				}
				
			}
			
		}
		
	}
	
	else if ( ( serviceResponse == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE ) &&
			  ( request->GetTaskStatus ( ) == kSCSITaskStatus_DeviceNotResponding ) )
	{
		
		ERROR_LOG ( ( "taskStatus = DeviceNotResponding\n" ) );
		goto ReleaseTask;
		
	}

	else if ( ( serviceResponse == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE ) &&
			  ( request->GetTaskStatus ( ) == kSCSITaskStatus_DeviceNotPresent ) )
	{
		
		ERROR_LOG ( ( "taskStatus = DeviceNotPresent\n" ) );
		goto ReleaseTask;
		
	}
	
	IOSleep ( 100 );
	
	// Check if we got terminated. If so, bail early.
	require ( ( isInactive ( ) == false ), ReleaseTask );
	
	for ( index = 0; ( index < kMaxInquiryAttempts ) && ( isInactive ( ) == false ); index++ )
	{
		
		request->ResetForNewTask ( );
		fSCSIPrimaryCommandObject->INQUIRY (
									request,
									bufferDesc,
									0,
									0,
									0,
									inqDataCount,
									0 );
		request->SetTimeoutDuration ( 10000 );
		
		serviceResponse = SendTask ( request );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( request->GetTaskStatus ( ) == kSCSITaskStatus_GOOD ) )
		{
			break;	
		}
		
		else if ( ( serviceResponse == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE ) &&
				  ( request->GetTaskStatus ( ) == kSCSITaskStatus_DeviceNotResponding ) )
		{
			
			ERROR_LOG ( ( "taskStatus = DeviceNotResponding\n" ) );
			goto ReleaseTask;
			
		}
		
		else if ( ( serviceResponse == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE ) &&
				  ( request->GetTaskStatus ( ) == kSCSITaskStatus_DeviceNotPresent ) )
		{
			
			ERROR_LOG ( ( "taskStatus = DeviceNotPresent\n" ) );
			goto ReleaseTask;
			
		}
		
		else if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
				  ( request->GetTaskStatus ( ) == kSCSITaskStatus_CHECK_CONDITION ) )
		{
			
			ERROR_LOG ( ( "CHECK_CONDITION on INQUIRY command. THIS SHOULD NEVER HAPPEN! BAD DEVICE, NO COOKIE!!\n" ) );
			
		}
		
		IOSleep ( 1000 );
		
	}
	
	// Check if we have exhausted our max number of attempts. If so, bail early.
	require ( ( index != kMaxInquiryAttempts ), ReleaseTask );
	
	// Check if we got terminated. If so, bail early.
	require ( isInactive ( ) == false, ReleaseTask );
	
	require ( ( request->GetRealizedDataTransferCount ( ) >= ( sizeof ( SCSICmd_INQUIRY_StandardData ) ) ), ReleaseTask );
	
	{
		
		UInt8	qualifier = inqData->PERIPHERAL_DEVICE_TYPE & kINQUIRY_PERIPHERAL_QUALIFIER_Mask;
		
		switch ( qualifier )
		{
			
			case kINQUIRY_PERIPHERAL_QUALIFIER_NotSupported:
				// According to the SPC-2 spec, if a device responds with this
				// qualifier type, it should set the PDT to 0x1F for backward
				// compatibility...
				ERROR_LOG ( ( "Device not supported at this Logical Unit\n" ) );
				check ( ( inqData->PERIPHERAL_DEVICE_TYPE & kINQUIRY_PERIPHERAL_TYPE_Mask ) == kINQUIRY_PERIPHERAL_TYPE_UnknownOrNoDeviceType );
				goto ReleaseTask;
				break;
			
			case kINQUIRY_PERIPHERAL_QUALIFIER_SupportedButNotConnected:
				// Why do some devices respond with this qualifier type when they actually have
				// a Logical Unit present at this Logical Unit Number? So many arrays
				// do this, so we have to allow for it and not reject the LUN if it responds
				// this way...
				STATUS_LOG ( ( "Device supported but not connected\n" ) );
				break;
			
			case kINQUIRY_PERIPHERAL_QUALIFIER_Connected:
				// A device is definitely connected.
				STATUS_LOG ( ( "Device supported and connected\n" ) );
				break;
				
			default:
				ERROR_LOG ( ( "Some reserved qualifier = 0x%02x\n", qualifier ) );
				break;
			
		}
		
	}
	
	// Set the Peripheral Device Type property for the device.
	setProperty ( kIOPropertySCSIPeripheralDeviceType,
				( UInt64 ) ( inqData->PERIPHERAL_DEVICE_TYPE & kINQUIRY_PERIPHERAL_TYPE_Mask ),
				kIOPropertySCSIPeripheralDeviceTypeSize );
	
	// Set the Vendor Identification property for the device.
	bcopy ( inqData->VENDOR_IDENTIFICATION, tempString, kINQUIRY_VENDOR_IDENTIFICATION_Length );
	tempString[kINQUIRY_VENDOR_IDENTIFICATION_Length] = 0;
	StripWhiteSpace ( tempString, kINQUIRY_VENDOR_IDENTIFICATION_Length );
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIVendorIdentification, string );
		string->release ( );
		string = NULL;
		
	}
	
	// Set the Product Indentification property for the device.
	bcopy ( inqData->PRODUCT_IDENTIFICATION, tempString, kINQUIRY_PRODUCT_IDENTIFICATION_Length );
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
	bcopy ( inqData->PRODUCT_REVISION_LEVEL, tempString, kINQUIRY_PRODUCT_REVISION_LEVEL_Length );
	tempString[kINQUIRY_PRODUCT_REVISION_LEVEL_Length] = 0;
	StripWhiteSpace ( tempString, kINQUIRY_PRODUCT_REVISION_LEVEL_Length );
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIProductRevisionLevel, string );
		string->release ( );
		string = NULL;
		
	}
	
	result = true;
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseBuffer );
	request->release ( );
	request = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( bufferDesc, ReleaseInquiryBuffer );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseInquiryBuffer:
	
	
	require_nonzero_quiet ( inqData, ErrorExit );
	require_nonzero_quiet ( inqDataCount, ErrorExit );
	IOFree ( ( void * ) inqData, inqDataCount );
	inqData = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SendTask - Method to send synchronous commands.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::SendTask ( SCSITask * request )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	request->SetTaskCompletionCallback ( &IOSCSIPeripheralDeviceNub::TaskCallback );
	request->SetTaskOwner ( this );
	
	ExecuteCommand ( request );
	
	// Wait for the completion routine to get called
	fCommandGate->runAction ( ( IOCommandGate::Action )
							  &IOSCSIPeripheralDeviceNub::sWaitForTask,
							  request );
	
	serviceResponse = request->GetServiceResponse ( );
	
	return serviceResponse;
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  1 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  2 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  3 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  4 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  5 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  6 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  7 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  8 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  9 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 10 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 11 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 12 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 16 );


#if 0
#pragma mark -
#pragma mark ¥ IOSCSILogicalUnitNub
#pragma mark -
#endif


OSDefineMetaClassAndStructors ( IOSCSILogicalUnitNub, IOSCSIPeripheralDeviceNub );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Called by IOKit to start our services.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSILogicalUnitNub::start ( IOService * provider )
{
	
	OSDictionary *	characterDict	= NULL;
	OSNumber *		number			= NULL;
	OSObject *		obj				= NULL;
	bool			result			= false;
	
	// Check if our provider is one of us. If it is, we don't want to
	// load again.
	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceNub, provider );
	require_quiet ( ( fProvider == NULL ), ErrorExit );
	
	// Make sure our provider is a protocol services driver.
	fProvider = OSDynamicCast ( IOSCSIProtocolInterface, provider );
	require_nonzero_quiet ( fProvider, ErrorExit );
	
	// Call our super class' start routine so that all inherited
	// behavior is initialized.
	require ( super::start ( provider ), ErrorExit );
	
	fSCSIPrimaryCommandObject = OSTypeAlloc ( SCSIPrimaryCommands );
	require_nonzero ( fSCSIPrimaryCommandObject, ErrorExit );
	
	require ( fProvider->open ( this ), ReleaseCommandObject );
	
	STATUS_LOG ( ( "%s: Check for the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
	
	// Check if the personality for this device specifies a preferred protocol
	if ( ( fProvider->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) == NULL )
	{
		
		STATUS_LOG ( ( "%s: No SCSI Device Characteristics property, set defaults.\n", getName ( ) ) );
		fDefaultInquiryCount = 0;
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "%s: Get the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
		characterDict = OSDynamicCast ( OSDictionary, ( fProvider->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) );
		
		STATUS_LOG ( ( "%s: set this SCSI Device Characteristics property.\n", getName ( ) ) );
		setProperty ( kIOPropertySCSIDeviceCharacteristicsKey, characterDict );
		
		// Check if the personality for this device specifies a preferred protocol
		STATUS_LOG ( ( "%s: check for the Inquiry Length property.\n", getName ( ) ) );
		if ( characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) == NULL )
		{
			
			STATUS_LOG ( ( "%s: No Inquiry Length property, use default.\n", getName ( ) ) );
			fDefaultInquiryCount = 0;
			
		}
		
		else
		{
			
			OSNumber *	defaultInquiry = NULL;
			
			STATUS_LOG ( ( "%s: Get Inquiry Length property.\n", getName ( ) ) );
			defaultInquiry = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) );
			
			// This device has a preferred protocol, use that.
			fDefaultInquiryCount = defaultInquiry->unsigned32BitValue ( );
			
		}
		
	}
	
	STATUS_LOG ( ( "%s: default inquiry count is: %d\n", getName ( ), fDefaultInquiryCount ) );
	
	require ( InterrogateDevice ( ), CloseProvider );
	
	setProperty ( kIOMatchCategoryKey, kSCSITaskUserClientIniterKey );
	
	characterDict = OSDynamicCast ( OSDictionary, fProvider->getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( characterDict == NULL )
	{
		characterDict = OSDictionary::withCapacity ( 1 );
	}
	
	else
	{
		characterDict = OSDictionary::withDictionary ( characterDict );
	}
	
	obj = fProvider->getProperty ( kIOPropertyPhysicalInterconnectTypeKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, obj );
	}

	obj = fProvider->getProperty ( kIOPropertyPhysicalInterconnectLocationKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, obj );
	}
	
	// Create an OSNumber object with the SCSI Logical Unit Identifier
	number = OSNumber::withNumber ( fLogicalUnitNumber, 64 );
	if ( number != NULL )
	{
		
		// Set the SCSI Logical Unit Number key
		characterDict->setObject ( kIOPropertySCSILogicalUnitNumberKey, number );
		
		number->release ( );
		number = NULL;
		
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
	
	characterDict->release ( );
	
	// Register this object as a nub for the Logical Unit Driver.
	registerService ( );
	
	STATUS_LOG ( ( "%s: Registered and setup is complete\n", getName ( ) ) );
	// Setup was successful, return true.
	result = true;
	
	return result;
	
	
CloseProvider:
	
	
	fProvider->close ( this );
	
	
ReleaseCommandObject:
	
	
	require_nonzero_quiet ( fSCSIPrimaryCommandObject, ErrorExit );
	fSCSIPrimaryCommandObject->release ( );
	fSCSIPrimaryCommandObject = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetLogicalUnitNumber - Sets the logical unit number for this device.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSILogicalUnitNub::SetLogicalUnitNumber ( UInt8 newLUN )
{
	
	OSNumber *		logicalUnitNumber = NULL;
	char			unit[10];
	
	STATUS_LOG ( ( "%s: SetLogicalUnitNumber to %d\n", getName ( ), newLUN ) );
	
	// Set the location and the IOUnit values in the IORegistry
	fLogicalUnitNumber = newLUN;	
	
	// Set the location to allow booting.
	sprintf ( unit, "%x", ( int ) fLogicalUnitNumber );
	setLocation ( unit );
	
	// Create an OSNumber object with the SCSI Logical Unit Identifier
	logicalUnitNumber = OSNumber::withNumber ( fLogicalUnitNumber, 64 );
	if ( logicalUnitNumber != NULL )
	{
		
		// Backwards compatibility with 10.3. Set the LUN here as well...
		setProperty ( kIOPropertySCSILogicalUnitNumberKey, logicalUnitNumber );
		
		// Set the Unit number used to build the device tree path
		setProperty ( "IOUnitLUN", logicalUnitNumber );
		
		logicalUnitNumber->release ( );
		logicalUnitNumber = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetLogicalUnitNumber - Gets the logical unit number for this device.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
IOSCSILogicalUnitNub::GetLogicalUnitNumber ( void )
{
	return fLogicalUnitNumber;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ExecuteCommand - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSILogicalUnitNub::ExecuteCommand ( SCSITaskIdentifier request )
{
	
	STATUS_LOG ( ( "%s: ExecuteCommand for %d\n", getName ( ),
				 fLogicalUnitNumber ) );
	
	if ( fLogicalUnitNumber != 0 )
	{
		
		SCSITask *	scsiRequest;
		
		scsiRequest = OSDynamicCast ( SCSITask, request );
		if ( scsiRequest != NULL )
		{
			
			scsiRequest->SetLogicalUnitNumber ( fLogicalUnitNumber );
			
		}
		
	}
	
	IOSCSIPeripheralDeviceNub::ExecuteCommand ( request );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortCommand - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSILogicalUnitNub::AbortCommand ( SCSITaskIdentifier abortTask )
{
	return IOSCSIPeripheralDeviceNub::AbortCommand ( abortTask );
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 1 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 2 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 3 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 4 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 5 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 6 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 7 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 8 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,	 9 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 10 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 11 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 12 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 13 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 14 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 15 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 16 );