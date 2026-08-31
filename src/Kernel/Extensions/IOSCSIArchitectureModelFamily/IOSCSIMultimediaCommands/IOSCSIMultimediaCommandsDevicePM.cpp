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
#include "IOSCSIMultimediaCommandsDevice.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"MMCPower"

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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kPowerStatusBufferSize		8
#define kPowerStatusByte			5


// We have 4 default power states. ACTIVE, IDLE, STANDBY, and SLEEP in order
// of most power to least power. In SLEEP, the Vcc can be removed, so we must
// trap for that in our power management code.

static IOPMPowerState sPowerStates[kMMCNumPowerStates] =
{
	{ kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, /* never entered voluntarily */
	{ kIOPMPowerStateVersion1, 0, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | IOPMMaxPerformance | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};

enum
{
	kMMCPowerConditionsActive	= 0x01,
	kMMCPowerConditionsIdle		= 0x02,
	kMMCPowerConditionsStandby	= 0x03,
	kMMCPowerConditionsSleep 	= 0x05
};


// Static prototypes
static IOReturn
IOSCSIMultimediaCommandsDevicePowerDownHandler ( void * 		target,
												 void * 		refCon,
												 UInt32 		messageType,
												 IOService * 	provider,
												 void * 		messageArgument,
												 vm_size_t 		argSize );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ IOSCSIMultimediaCommandsDevicePowerDownHandler -	C->C++ Glue code for
// 													 	Power Down
//														Notifications. [STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


static IOReturn
IOSCSIMultimediaCommandsDevicePowerDownHandler ( void * 		target,
												 void * 		refCon,
												 UInt32 		messageType,
												 IOService * 	provider,
												 void * 		messageArgument,
												 vm_size_t 		argSize )
{
	
	return ( ( IOSCSIMultimediaCommandsDevice * ) target )->PowerDownHandler (
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
IOSCSIMultimediaCommandsDevice::PowerDownHandler ( void * 		refCon,
												   UInt32 		messageType,
												   IOService * 	provider,
												   void * 		messageArgument,
												   vm_size_t 	argSize )
{
	
	IOReturn				status 		= kIOReturnUnsupported;
	SCSITaskIdentifier		request		= NULL;
	
	// Make sure it is a power off message first.
	// We don't do anything at sleep or restart time. That is
	// handled via standard power management functions. See
	// HandlePowerChange() for more information.
	
	require ( messageType == kIOMessageSystemWillPowerOff, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	// This is our chance to bring the drive tray in (if the drive supports
	// power inject).
	if ( fMediaPresent == false )
	{
		
		UInt8	trayState = 0;	// 0 = close, 1 = open
		
		// Bring the tray closed if the power state is not in some sleep mode.			
		if ( fCurrentPowerState > kMMCPowerStateSleep )
		{
			
			status = GetTrayState ( &trayState );
			if ( ( ( status == kIOReturnSuccess ) && ( trayState == 1 ) ) ||
				 ( status != kIOReturnSuccess ) )
			{
				
				// Set the tray state to closed						
				if ( START_STOP_UNIT ( request, 0, 0, 1, 1, 0 ) == true )
				{
					( void ) SendCommand ( request, kTenSecondTimeoutInMS );
				}
				
				status = kIOReturnSuccess;
				
			}
			
		}
		
	}
	
	else
	{
		
		// Media is present (but may be spinning). Make sure
		// the drive is spun down.
		if ( fCurrentPowerState > kMMCPowerStateSleep )
		{
			
			// Make sure the drive is spun down
			if ( START_STOP_UNIT ( request, 1, 0, 0, 0, 0 ) == true )
			{
				
				( void ) SendCommand ( request, 0 );
				
			}
			
		}
		
		status = kIOReturnSuccess;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ GetInitialPowerState - Asks the driver which power state the device is
//							in at startup time. This function is only called
//							once, right after InitializePowerManagement().
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIMultimediaCommandsDevice::GetInitialPowerState ( void )
{
	
	return kMMCPowerStateActive;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ GetNumberOfPowerStateTransitions - Asks the driver for the number of state
//										transitions between sleep state and
//										the highest power state.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIMultimediaCommandsDevice::GetNumberOfPowerStateTransitions ( void )
{
	
	UInt32		numTransitions 	= 0;
	bool		state 			= false;
	
	// If someone has us opened exclusively, we do not want to change
	// power states, so we just return 0 power state changes. Otherwise,
	// we return the normal number of power transitions from active state
	// to sleep state.
	state = HandleGetUserClientExclusivityState ( );
	if ( state == false )
	{
		numTransitions = kMMCPowerStateActive - kMMCPowerStateSleep;
	}
	
    return numTransitions;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ InitializePowerManagement - 	Register the driver with our policy-maker
//									(also in the same class).		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::InitializePowerManagement (
										IOService * provider )
{
	
	fCurrentPowerState = kMMCPowerStateActive;
	
	// Call our super to get us into the power management tree
	super::InitializePowerManagement ( provider );
	
	// Register ourselves as a "policy maker" for this device. We use
	// the number of default power states defined by MMC-2.
	registerPowerDriver ( this, sPowerStates, kMMCNumPowerStates );
	
	// Install handler for shutdown notifications
	fPowerDownNotifier = registerPrioritySleepWakeInterest (
		( IOServiceInterestHandler ) IOSCSIMultimediaCommandsDevicePowerDownHandler,
		this );
	
	// Make sure we clamp the lowest power setting that we voluntarily go
	// into is state kMMCPowerStateSleep. We only enter kMMCPowerStateSystemSleep
	// if told by the power manager during a system sleep.
    changePowerStateTo ( kMMCPowerStateSleep );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ HandleCheckPowerState - 	Checks to see if the power state is
//								kMMCPowerStateActive				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::HandleCheckPowerState ( void )
{
	
	if ( IsDeviceAccessEnabled ( ) )
	{
		
		super::HandleCheckPowerState ( kMMCPowerStateActive );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ TicklePowerManager - 	Calls activityTickle to tell the power manager we
//							need to be in a certain state to fulfill I/O
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::TicklePowerManager ( void )
{
	
	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::TicklePowerManager called\n" ) );
	( void ) super::TicklePowerManager ( kMMCPowerStateActive );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ HandlePowerChange - Handles the state machine for power management
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIMultimediaCommandsDevice::HandlePowerChange ( void )
{
	
	SCSIServiceResponse		serviceResponse;
	SCSITaskIdentifier		request = NULL;
	UInt32					features;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::HandlePowerChange called\n" ) );
	
	request = GetSCSITask ( );
	
	while ( ( fProposedPowerState != fCurrentPowerState ) && ( isInactive ( ) == false ) )
	{
	 	
	 	STATUS_LOG ( ( "fProposedPowerState = %ld, fCurrentPowerState = %ld\n",
	 					fProposedPowerState, fCurrentPowerState ) );
	 	
		// are we currently asleep and going to wake up?
	 	if ( ( fCurrentPowerState <= kMMCPowerStateSleep ) &&
	 		 ( fProposedPowerState > kMMCPowerStateSleep ) )
		{
			
			STATUS_LOG ( ( "We think we're in sleep\n" ) );
			
			// Test to see if the drive is asleep. Send a TEST_UNIT_READY with
			// a timeout of 1 second. If it is asleep, it will be reset and
			// that will wake it up.
			if ( TEST_UNIT_READY ( request, 0 ) == true )
			{
				( void ) SendCommand ( request, kOneSecondTimeoutInMS );
			}
			
			// First, if we were in sleep mode ( fCurrentPowerState <= kMMCPowerStateSleep )
			// then we need to do some stuff to recover the device. If power
			// was pulled, we need to clear the power-on reset sense data.
			STATUS_LOG ( ( "calling ClearPowerOnReset\n" ) );
			
			// /!\ WARNING! we may bail out of ClearPowerOnReset because
			// the device was hot unplugged while we were sleeping - do
			// sanity check here and break out of loop if we have been
			// terminated.
			if ( ClearPowerOnReset ( ) == false )
				break;
			
			STATUS_LOG ( ( "calling ClearNotReadyStatus\n" ) );
			
			// /!\ WARNING! we may bail out of ClearNotReadyStatus because
			// the device was hot unplugged while we were sleeping - do sanity
			// check here and break out of loop if we have been terminated.
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
						
						STATUS_LOG ( ( "Calling PREVENT_ALLOW_MEDIUM_REMOVAL\n" ) );
						
						// Media is present, so lock it down
						if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateLocked, 0 ) == true )
						{
							
							// The command was successfully built, now send it
					    	serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
							
						}
						
					}
					
				}
				
				else
				{
					
					// Check to see if someone had us open exclusively before going to
					// sleep. If so, don't start polling!
					if ( HandleGetUserClientExclusivityState ( ) == false )
					{
						
						STATUS_LOG ( ( "Calling EnablePolling\n" ) );
						fPollingMode = kPollingMode_NewMedia;
						EnablePolling ( );
						
					}
					
				}
				
			}
			
		}
		
		switch ( fProposedPowerState )
		{
			
			case kMMCPowerStateSystemSleep:
			{
				
				bool		shouldSleepDrive	= true;
				bool		driveIsAsleep 		= false;
				OSString *	prop				= NULL;
							
				STATUS_LOG ( ( "case kMMCPowerStateSystemSleep\n" ) );
				
				// are we already asleep?
				if ( fCurrentPowerState == kMMCPowerStateSleep )
				{
					
					STATUS_LOG ( ( "We are already asleep\n" ) );
					
					// is there media in the drive?
					if ( fMediaPresent == true )
					{
						
						STATUS_LOG ( ( "Media is present, bailing out\n" ) );
						
						fCurrentPowerState = kMMCPowerStateSystemSleep;
						break;
						
					}
					
					// are we using low power polling?
					else if ( fLowPowerPollingEnabled )
					{
						
						STATUS_LOG ( ( "Low power polling is enabled, let's disable it now.\n" ) );
						features = fLowPowerPollingEnabled = false;
						HandleProtocolServiceFeature ( kSCSIProtocolFeature_ProtocolSpecificPolling, ( void * ) &features );
						fCurrentPowerState = kMMCPowerStateSystemSleep;
						STATUS_LOG ( ( "Done disabling low power polling.\n" ) );
						break;
						
					}
										
				}
				
				if ( fMediaPresent == false )
				{
					
					DisablePolling ( );
					IOSleep ( 1000 );
					
				}
				
				STATUS_LOG ( ( "We are NOT already asleep\n" ) );

				// let outstanding commands complete
				fCurrentPowerState = fProposedPowerState;

				while ( fNumCommandsOutstanding > 1 )
				{
					
					// Sleep this thread for 1ms until all outstanding commands
					// have completed. This prevents a sleep command from entering
					// the queue before an I/O and causing a problem on wakeup.
					IOSleep ( 1 );
					
				}
				
				// Set the tray state to closed.
				if ( START_STOP_UNIT ( request, 0, 0, 1, 1, 0 ) == true )
				{
					( void ) SendCommand ( request, kTenSecondTimeoutInMS );
					check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
				}
				
				prop = OSDynamicCast ( OSString, getProperty ( "No Sleep" ) );
				if ( prop != NULL )
				{
					shouldSleepDrive = false;
				}
				
				if ( shouldSleepDrive == true )
				{
				
					// If the device supports the power conditions mode page, and we haven't already
					// put it to sleep using the START_STOP_UNIT command, issue one to the drive.
					if ( fDeviceSupportsPowerConditions )
					{
						
						STATUS_LOG ( ( "Sending DVD sleep command\n" ) );
						
						if ( START_STOP_UNIT ( request, 0, kMMCPowerConditionsSleep, 0, 0, 0 ) == true )
						{
							
							( void ) SendCommand ( request, 0 );
							check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
							driveIsAsleep = true;
							
						}
												
					}
						
					else if ( IsProtocolServiceSupported ( kSCSIProtocolFeature_ProtocolSpecificSleepCommand, NULL ) )
					{
						
						STATUS_LOG ( ( "Sending ATA sleep command\n" ) );
						
						features = true;
						( void ) HandleProtocolServiceFeature (
									kSCSIProtocolFeature_ProtocolSpecificSleepCommand,
									( void * ) &features );
						driveIsAsleep = true;
						
					}
					
				}				
				
				if ( driveIsAsleep == false )
				{
					
					STATUS_LOG ( ( "Device does NOT have protocol specific sleep command.\n" ) );
					STATUS_LOG ( ( "Spinning down drive.\n" ) );
					
					// At a minimum, make sure the drive is spun down
					if ( START_STOP_UNIT ( request, 0, 0, 0, 0, 0 ) == true )
					{
						
						( void ) SendCommand ( request, 0 );
						check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
						
					}
					
				}
				
				break;
				
			}
				
			case kMMCPowerStateSleep:
			{
				
				bool		shouldSleepDrive	= true;
				bool		driveIsAsleep 		= false;
				OSString *	prop				= NULL;
				
				STATUS_LOG ( ( "case kMMCPowerStateSleep\n" ) );

				// are we waking up from system sleep?
				if ( fCurrentPowerState < kMMCPowerStateSleep )
				{
					
					// If we have woken up from system sleep, the drive
					// should get a power on reset or a reset which will cause it
					// to truly be in active mode. We can simply tickle the power
					// manager here to bring us into active state and just pretend
					// we went back to sleep here.
					STATUS_LOG ( ( "TicklePowerManager.\n" ) );
					TicklePowerManager ( );
					STATUS_LOG ( ( "Wakeup path completed.\n" ) );

					fCurrentPowerState = fProposedPowerState;
					break;
					
				}
				
				// device is now going to sleep

				STATUS_LOG ( ( "Device is going to sleep now.\n" ) );

				// is there media in the drive?
				if ( fMediaPresent == false )
				{
					
					STATUS_LOG ( ( "Media is NOT present.\n" ) );
					
					// Set the tray state to closed.
					if ( START_STOP_UNIT ( request, 0, 0, 1, 1, 0 ) == true )
					{
						( void ) SendCommand ( request, 0 );
						check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
					}
					
					// does the device support low power polling?
					if ( fDeviceSupportsLowPowerPolling == false )
					{
						
						STATUS_LOG ( ( "Device does NOT support low power polling.\n" ) );
						STATUS_LOG ( ( "Spinning down drive.\n" ) );

						// At a minimum, make sure the drive is spun down
						if ( START_STOP_UNIT ( request, 0, 0, 0, 0, 0 ) == true )
						{
							
							( void ) SendCommand ( request, 0 );
							check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
							
						}
						
						fCurrentPowerState = fProposedPowerState;
						STATUS_LOG ( ( "Sleep path completed for non-low power polling drive.\n" ) );
						break;
						
					}
					
					else
					{
						
						// disable polling
						STATUS_LOG ( ( "Disabling polling\n" ) );
						DisablePolling ( );
						
					}
					
				}
				
				STATUS_LOG ( ( "fDeviceSupportsLowPowerPolling  | fMediaPresent\n" ) );
				
				prop = OSDynamicCast ( OSString, getProperty ( "No Sleep" ) );
				if ( prop != NULL )
				{
					shouldSleepDrive = false;
				}
				
				if ( shouldSleepDrive == true )
				{
					
					// If the device supports the power conditions mode page, and we haven't already
					// put it to sleep using the START_STOP_UNIT command, issue one to the drive.
					if ( fDeviceSupportsPowerConditions == true )
					{
						
						STATUS_LOG ( ( "Sending START_STOP_UNIT to drive to turn it off\n" ) );
						
						if ( START_STOP_UNIT ( request, 0, kMMCPowerConditionsSleep, 0, 0, 0 ) == true )
						{
							
							( void ) SendCommand ( request, 0 );
							check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
							driveIsAsleep = true;
							
						}
						
					}
					
					else
					{
						
						STATUS_LOG ( ( "Calling protocol layer to sleep drive\n" ) );
						
						features = true;
						( void ) HandleProtocolServiceFeature (
									kSCSIProtocolFeature_ProtocolSpecificSleepCommand,
									( void * ) &features );
						driveIsAsleep = true;
						
					}
					
				}
				
				if ( driveIsAsleep == false )
				{
					
					STATUS_LOG ( ( "Spinning down drive, since it is broken with sleep/wake.\n" ) );
					
					// At a minimum, make sure the drive is spun down
					if ( START_STOP_UNIT ( request, 0, 0, 0, 0, 0 ) == true )
					{
						
						( void ) SendCommand ( request, 0 );
						check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
						
					}
					
				}
				
				// is there media in the drive?
				if ( fMediaPresent == false )
				{
					
					STATUS_LOG ( ( "Enabling low power polling.\n" ) );
					
					features = fLowPowerPollingEnabled = true;
					( void ) HandleProtocolServiceFeature (
								kSCSIProtocolFeature_ProtocolSpecificPolling,
								( void * ) &features );
					
				}
				
				fCurrentPowerState = fProposedPowerState;
				
			}
			break;
						
			case kMMCPowerStateStandby:
			{
				
				UInt32		deviceState = 0;
				IOReturn	status;
				
				STATUS_LOG ( ( "case kMMCPowerStateStandby\n" ) );
				
				if ( fDeviceSupportsPowerConditions )
				{
					
					STATUS_LOG ( ( "case kMMCPowerStateStandby & fDeviceSupportsPowerConditions\n" ) );
					// Drive might have put itself in a different mode already, check to see
					// which state the drive is currently in.
					status = GetCurrentPowerStateOfDrive ( &deviceState );
					if ( ( status != kIOReturnSuccess ) || ( deviceState > kMMCPowerStateStandby ) )
					{
						
						STATUS_LOG ( ( "Sending START_STOP_UNIT to put drive in standby mode\n" ) );
						
						if ( START_STOP_UNIT ( request, 0, kMMCPowerConditionsStandby, 0, 0, 0 ) == true )
						{
							
							( void ) SendCommand ( request, 0 );
							check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
							
						}
						
					}
								
				}
				
				fCurrentPowerState = kMMCPowerStateStandby;
				
			}
			break;
			
			case kMMCPowerStateIdle:
			{
				
				UInt32		deviceState = 0;
				IOReturn	status;
				
				STATUS_LOG ( ( "case kMMCPowerStateIdle\n" ) );
				
				if ( fDeviceSupportsPowerConditions )
				{
					
					STATUS_LOG ( ( "case kMMCPowerStateIdle & fDeviceSupportsPowerConditions\n" ) );
					// Drive might have put itself in a different mode already, check to see
					// which state the drive is currently in.
					status = GetCurrentPowerStateOfDrive ( &deviceState );
					if ( ( status != kIOReturnSuccess ) || ( deviceState > kMMCPowerStateIdle ) )
					{
						
						STATUS_LOG ( ( "Sending START_STOP_UNIT to drive to put it in idle mode\n" ) );
						
						if ( START_STOP_UNIT ( request, 0, kMMCPowerConditionsIdle, 0, 0, 0 ) == true )
						{
							
							( void ) SendCommand ( request, 0 );
							check ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD );
							
						}
						
					}
					
				}
				
				fCurrentPowerState = kMMCPowerStateIdle;
				
			}
			break;
			
			case kMMCPowerStateActive:
			{
				
				STATUS_LOG ( ( "case kMMCPowerStateActive\n" ) );
				
				if ( fMediaPresent == true )
				{
					
					if ( ( fMediaBlockSize * fMediaBlockCount ) != 0 )
					{
						
						if ( fCurrentDiscSpeed != 0 )
						{
							
							// Since the device is being returned to active state, make sure that 
							// the drive speed is restored to what it was.				
							SetMediaAccessSpeed ( fCurrentDiscSpeed );
							
						}
						
					}
					
				}
				
				fCurrentPowerState = kMMCPowerStateActive;
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
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::HandlePowerChange done\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ CheckMediaPresence - 	Checks if media is present. If so, it locks it down
//							again, else it requests a dialog to be displayed
//							and tears the stack down				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::CheckMediaPresence ( void )
{
	
	SCSI_Sense_Data				senseBuffer		= { 0 };
	IOMemoryDescriptor *		bufferDesc		= NULL;
	SCSITaskIdentifier			request			= NULL;
	bool						mediaPresent	= false;
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
					
					STATUS_LOG ( ( "sense data: %01x, %02x, %02x\n",
								( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ),
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
							if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01, 0x00 ) == true )
							{
								
								( void ) SendCommand ( request, 0 );
								
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
					
					else if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_UNIT_ATTENTION ) &&
								( senseBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) &&
								( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
					{
						
						STATUS_LOG ( ( "%s::drive NOT READY\n", getName ( ) ) );
						IOSleep ( 200 );
						continue;
						
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
            
            // The command failed. Let's wait a bit and try again.
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ GetCurrentPowerStateOfDrive - 	Gets the drive's current power state
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


IOReturn
IOSCSIMultimediaCommandsDevice::GetCurrentPowerStateOfDrive ( UInt32 * powerState )
{
	
	UInt8						powerStatus[kPowerStatusBufferSize];
	IOMemoryDescriptor *		bufferDesc		= NULL;
	SCSITaskIdentifier			request			= NULL;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOReturn					status			= kIOReturnError;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	require ( fDeviceSupportsPowerConditions, Exit );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) powerStatus,
													kPowerStatusBufferSize,
													kIODirectionIn );
	require_nonzero ( bufferDesc, Exit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	if ( GET_EVENT_STATUS_NOTIFICATION ( request,
										 bufferDesc,
										 1,
										 0x02,
										 kPowerStatusBufferSize,
										 0 ) == true )
	{
		
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	require ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ), ReleaseTask );
	require ( ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ), ReleaseTask );
	
	// According to MMC, the powerStatus byte (6th byte) goes from
	// high to low, whereas our states go from low to high. Compensate
	// by subtracting the returned state from the number of power states.
	*powerState = kMMCNumPowerStates - powerStatus[kPowerStatusByte];
	status 		= kIOReturnSuccess;
	
	
ReleaseTask:
	
	
	require_nonzero ( request, ReleaseDescriptor );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero ( bufferDesc, Exit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
Exit:
	
	
	return status;
	
}