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

// SCSI Architecture Model Family includes
#include <IOKit/scsi/SCSITask.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOSCSIReducedBlockCommandsDevice.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"RBCPower"

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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// We have 4 default power states. ACTIVE, IDLE, STANDBY, and SLEEP in order
// of most power to least power. In SLEEP, the Vcc can be removed, so we must
// trap for that in our power management code.

static IOPMPowerState sPowerStates[kRBCNumPowerStates] =
{
	{ kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },		/* never entered voluntarily */
	{ kIOPMPowerStateVersion1, 0, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | IOPMMaxPerformance | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};

enum
{
	kRBCPowerConditionsActive	= 0x01,
	kRBCPowerConditionsIdle		= 0x02,
	kRBCPowerConditionsStandby	= 0x03,
	kRBCPowerConditionsSleep 	= 0x05
};


// Static prototypes
static IOReturn
IOSCSIReducedBlockCommandsDevicePowerDownHandler ( void * 			target,
												   void * 			refCon,
												   UInt32 			messageType,
												   IOService *		provider,
												   void * 			messageArgument,
												   vm_size_t 		argSize );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ IOSCSIReducedBlockCommandsDevicePowerDownHandler - C->C++ Glue code for
//														Power Down
//														Notifications.
//																	   [STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


static IOReturn
IOSCSIReducedBlockCommandsDevicePowerDownHandler ( void * 			target,
												   void * 			refCon,
												   UInt32 			messageType,
												   IOService *		provider,
												   void * 			messageArgument,
												   vm_size_t 		argSize )
{
	
	return ( ( IOSCSIReducedBlockCommandsDevice * ) target )->PowerDownHandler (
														refCon,
														messageType,
														provider,
														messageArgument,
														argSize );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ PowerDownHandler - Method called at sleep/restart/shutdown time.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIReducedBlockCommandsDevice::PowerDownHandler ( void * 		refCon,
													 UInt32 		messageType,
													IOService * 	provider,
													void * 			messageArgument,
													vm_size_t 		argSize )
{
	
	if ( messageType == kIOMessageSystemWillPowerOff )
		setProperty ( "Power Off", true );
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ GetInitialPowerState - Asks the driver which power state the device is
//							in at startup time. This function is only called
//							once, right after InitializePowerManagement().
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIReducedBlockCommandsDevice::GetInitialPowerState ( void )
{
	
	return kRBCPowerStateActive;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ GetNumberOfPowerStateTransitions - Asks the driver for the number of state
//										transitions between sleep state and
//										the highest power state.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIReducedBlockCommandsDevice::GetNumberOfPowerStateTransitions ( void )
{
	return ( kRBCPowerStateActive - kRBCPowerStateSleep );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ InitializePowerManagement - 	Register the driver with our policy-maker
//									(also in the same class).		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::InitializePowerManagement (
											IOService * provider )
{
	
	fCurrentPowerState = kRBCPowerStateActive;
	
	// Call our super to get us into the power management tree
	super::InitializePowerManagement ( provider );
	
	// Register ourselves as a "policy maker" for this device. We use
	// the number of default power states defined by RBC.
	registerPowerDriver ( this, sPowerStates, kRBCNumPowerStates );
	
	// Install handler for shutdown notifications
	fPowerDownNotifier = registerPrioritySleepWakeInterest (
					( IOServiceInterestHandler ) IOSCSIReducedBlockCommandsDevicePowerDownHandler,
					this );
	
	// Make sure we clamp the lowest power setting that we voluntarily go
	// into is state kRBCPowerStateSleep. We only enter kRBCPowerStateSystemSleep
	// if told by the power manager during a system sleep.
    changePowerStateTo ( kRBCPowerStateSleep );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ HandleCheckPowerState - 	Checks to see if the power state is
//								kRBCPowerStateActive				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::HandleCheckPowerState ( void )
{
	
	if ( IsDeviceAccessEnabled ( ) )
	{
		
		super::HandleCheckPowerState ( kRBCPowerStateActive );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ TicklePowerManager - 	Calls activityTickle to tell the power manager we
//							need to be in a certain state to fulfill I/O
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::TicklePowerManager ( void )
{
	
	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	( void ) super::TicklePowerManager ( kRBCPowerStateActive );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ HandlePowerChange - Handles the state machine for power management
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIReducedBlockCommandsDevice::HandlePowerChange ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::HandlePowerChange called\n" ) );
	
	request = GetSCSITask ( );
	
	while ( ( fProposedPowerState != fCurrentPowerState ) &&
			( isInactive ( ) == false ) )
	{
		
		STATUS_LOG ( ( "fProposedPowerState = %ld, fCurrentPowerState = %ld\n",
						fProposedPowerState, fCurrentPowerState ) );
		
		if ( ( fCurrentPowerState <= kRBCPowerStateSleep ) &&
			 ( fProposedPowerState > kRBCPowerStateSleep ) )
		{
			
			STATUS_LOG ( ( "We think we're in sleep\n" ) );
			
			// First, if we were in sleep mode ( fCurrentPowerState <= kRBCPowerStateSleep )
			// then we need to do some stuff to recover the device. If power was pulled, we
			// need to clear the power-on reset sense data.
			STATUS_LOG ( ( "calling ClearPowerOnReset\n" ) );
			
			// /!\ WARNING! we may bail out of ClearPowerOnReset because
			// the device was hot unplugged while we were sleeping - do sanity check here
			// and break out of loop if we have been terminated.
			if ( ClearPowerOnReset ( ) == false )
				break;
			
			STATUS_LOG ( ( "calling ClearNotReadyStatus\n" ) );
			
			// /!\ WARNING! we may bail out of ClearNotReadyStatus because
			// the device was hot unplugged while we were sleeping - do sanity check here
			// and break out of loop if we have been terminated.
			if ( ClearNotReadyStatus ( ) == false )
				break;
            
			if ( fMediaIsRemovable == true )
			{
				
				if ( fMediaPresent == true )
				{
					
					bool	mediaPresent = false;
					
					// Give the driver some lee-way in waking up because we think
					// media is actually there!
					for ( UInt32 index = 0; index < 100; index++ )
					{
						
						STATUS_LOG ( ( "Calling CheckMediaPresence\n" ) );
						
						mediaPresent = CheckMediaPresence ( );
						if ( mediaPresent )
							break;
						
						IOSleep ( 200 );
						
					}
					
					if ( mediaPresent != fMediaPresent )
					{
						PANIC_NOW ( ( "Unexpected Media Removal when waking up from sleep\n" ) );
					}
					
					else
					{
						
						// Media is present, so lock it down
						if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateLocked ) == true )
					    {
							
							// The command was successfully built, now send it
					    	serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
							
						}
						
						if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
							 ( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
						{
							
							// Media is not locked into the drive, so this is most likely
							// a manually ejectable device, start polling for media removal.
							fPollingMode = kPollingMode_MediaRemoval;
							EnablePolling ( );
							
						}
						
					}
					
				}
				
				else
				{
					
					fPollingMode = kPollingMode_NewMedia;
					EnablePolling ( );
					
				}
			
			}
			
		}
		
		switch ( fProposedPowerState )
		{
			
			case kRBCPowerStateSystemSleep:
			{
				
				UInt32 previousPowerState;
				
				STATUS_LOG ( ( "case kRBCPowerStateSystemSleep\n" ) );
				
				if ( fMediaPresent == false )
				{
					
					STATUS_LOG ( ( "Disabling polling\n" ) );
					DisablePolling ( );
					
				}
		
				// let outstanding commands complete
				previousPowerState = fCurrentPowerState;
				fCurrentPowerState = fProposedPowerState;

				while ( fNumCommandsOutstanding > 1 )
				{
					
					// Sleep this thread for 1ms until all outstanding commands
					// have completed. This prevents a sleep command from entering
					// the queue before an I/O and causing a problem on wakeup.
					IOSleep ( 1 );
					
				}
				
				// If the device supports the power conditions mode page, and we haven't already
				// put it to sleep using the START_STOP_UNIT command, issue one to the drive.
				if ( previousPowerState != kRBCPowerStateSleep )
				{
					
					if ( fDeviceSupportsPowerConditions )
					{
												
						if ( START_STOP_UNIT ( request, 1, kRBCPowerConditionsSleep, 0, 0 ) == true )
						{
							
							( void ) SendCommand ( request, 0 );
							
						}
						
					}
					
					else
					{
												
						// At a minimum, make sure the drive is spun down
						if ( START_STOP_UNIT ( request, 1, 0, 0, 0 ) == true )
						{
							
							serviceResponse = SendCommand ( request, 0 );
							
						}
						
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateSystemSleep;
				
			}
			break;
			
			case kRBCPowerStateSleep:
			{
				
				STATUS_LOG ( ( "case kRBCPowerStateSleep\n" ) );
				
				if ( fCurrentPowerState > kRBCPowerStateSleep )
				{
					
					// At a minimum, make sure the drive is spun down
					if ( START_STOP_UNIT ( request, 1, 0, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateSleep;
				
			}
			break;
			
			case kRBCPowerStateStandby:
			{
				
				STATUS_LOG ( ( "case kRBCPowerStateStandby\n" ) );
				
				if ( fDeviceSupportsPowerConditions )
				{
					
					if ( START_STOP_UNIT ( request, 1, kRBCPowerConditionsStandby, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
												
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateStandby;
				
			}
			break;
			
			case kRBCPowerStateIdle:
			{
				
				STATUS_LOG ( ( "case kRBCPowerStateIdle\n" ) );
				
				if ( fDeviceSupportsPowerConditions )
				{
					
					if ( START_STOP_UNIT ( request, 1, kRBCPowerConditionsIdle, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateIdle;
				
			}
			break;
			
			case kRBCPowerStateActive:
			{
				
				STATUS_LOG ( ( "case kRBCPowerStateActive\n" ) );
				
				if ( fDeviceSupportsPowerConditions )
				{
					
					if ( START_STOP_UNIT ( request, 1, kRBCPowerConditionsActive, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
				
				}
				
				if ( fMediaPresent )
				{
					
					// Don't forget to start the media
					if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01 ) == true )
					{
						
						STATUS_LOG ( ( "Sending START_STOP_UNIT.\n" ) );
						serviceResponse = SendCommand ( request, 0 );
						
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateActive;
				fCommandGate->commandWakeup ( &fCurrentPowerState, false );
				
			}
			break;
			
			default:
				PANIC_NOW ( ( "Undefined power state issued\n" ) );
				break;
			
		}
		
	}
	
	if ( isInactive ( ) )
	{
		
		fCurrentPowerState = fProposedPowerState;
		fCommandGate->commandWakeup ( &fCurrentPowerState, false );
		
	}
	
	ReleaseSCSITask ( request );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ CheckMediaPresence - 	Checks if media is present. If so, it locks it down
//							again, else it requests a dialog to be displayed
//							and tears the stack down				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIReducedBlockCommandsDevice::CheckMediaPresence ( void )
{
	
	SCSI_Sense_Data				senseBuffer		= { 0 };
	IOMemoryDescriptor *		bufferDesc		= NULL;
	SCSITaskIdentifier			request			= NULL;
	bool						mediaPresent 	= false;
	bool						driveReady 		= false;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
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
					
					STATUS_LOG ( ( "sense data: %01x, %02x, %02x\n",
								( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ),
								  senseBuffer.ADDITIONAL_SENSE_CODE,
								  senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );

					// Check the sense key to see if it is an error group we know how to handle
					if  ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY ) || 
						  ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_MEDIUM_ERROR ) )
					{
						
						// The SenseKey is an 02 ( Not Ready ) or 03 ( Medium Error ). Check to see
						// if we can do something about this
						
						if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) && 
							 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) )
						{
							
							// Device requires a start command before we can tell if media is there
							if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01 ) == true )
							{
								
								STATUS_LOG ( ( "Sending START_STOP_UNIT.\n" ) );
								( void ) SendCommand ( request, kTenSecondTimeoutInMS );
								
							}
							
							STATUS_LOG ( ( "%s::drive NOT READY\n", getName ( ) ) );
							
							IOSleep ( 200 );
							continue;
							
						}
						
						else if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) && 
							 	  ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
						{
							
							STATUS_LOG ( ( "No Media.\n" ) );
							// No media is present, return false
							driveReady = true;
							mediaPresent = false;
							
						}
						
						else
						{
							
							STATUS_LOG ( ( "%s::drive NOT READY\n", getName ( ) ) );
							IOSleep ( 200 );
							continue;
							
						}
										
					}
										
					else
					{
						
						STATUS_LOG ( ( "%s::drive READY, media present\n", getName ( ) ) );
						// Media is present, return true
						driveReady = true;
						mediaPresent = true;
						
					}
					
				}
				
			}
			
			else
			{
				
				STATUS_LOG ( ( "%s::drive READY, media present\n", getName ( ) ) );
				// Media is present, return true
				driveReady = true;
				mediaPresent = true;
				
			}
			
		}
        
		else
		{
			
			// The command failed, so let's try again...
			IOSleep ( 200 );
			
		}
	
	// check isInactive in case device was hot unplugged during sleep
	// and we are in an infinite loop here
	} while ( ( driveReady == false ) && ( isInactive ( ) == false ) );
	
	ReleaseSCSITask ( request );
	
	
ReleaseDescriptor:
	
	
	require_nonzero ( bufferDesc, ErrorExit );
	bufferDesc->release ( );	
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return mediaPresent;
	
}