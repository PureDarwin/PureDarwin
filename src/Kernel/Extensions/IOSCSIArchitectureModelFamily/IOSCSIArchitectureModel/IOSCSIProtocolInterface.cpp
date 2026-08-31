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
#include "IOSCSIProtocolInterface.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSIProtocolInterface"

#if DEBUG
#define SCSI_PROTOCOL_INTERFACE_DEBUGGING_LEVEL				0
#endif


// This module needs SAM_MODULE defined in order to pick up the
// static debugging function.
#define SAM_MODULE	1

#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_PROTOCOL_INTERFACE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PROTOCOL_INTERFACE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PROTOCOL_INTERFACE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#if ( SCSI_PROTOCOL_INTERFACE_POWER_DEBUGGING )
#define SERIAL_STATUS_LOG(x)	kprintf(x)
#else
#define SERIAL_STATUS_LOG(x)
#endif


#define super IOService
OSDefineMetaClass ( IOSCSIProtocolInterface, IOService );
OSDefineAbstractStructors ( IOSCSIProtocolInterface, IOService );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

enum
{
	kThreadDelayInterval		= 500, //( 0.5 second in ms )
	k100SecondsInMicroSeconds 	= 100 * 1000 * 1000
};


/*
 *	Since most, if not all, IOSCSIArchitectureModelFamily drivers
 *	must support power management in some way or another, the support
 *	has been moved up into the highest layers of the object hierarchy.
 *	This mechanism allows for many different power management schemes,
 *	depending on how the specification defines power management. Each
 *	protocol layer and application layer driver is responsible for
 *	implementing power management as the governing specification defines
 *	it. From there, developers can subclass and add any vendor-specific
 *	workarounds necessary for power management or they can make the
 *	power management scheme more elaborate or more simplistic.
 *
 */


#if 0
#pragma mark -
#pragma mark ¥ Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Called by IOKit to start our services.		  		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIProtocolInterface::start ( IOService * provider )
{
	
	bool			result		= false;
	IOWorkLoop *	workLoop	= NULL;
	
	workLoop = getWorkLoop ( );
	require_nonzero ( workLoop, ErrorExit );
	
	fCommandGate = IOCommandGate::commandGate ( this );
	require_nonzero ( fCommandGate, ErrorExit );
	
	workLoop->addEventSource ( fCommandGate );
	
	fPowerAckInProgress				= false;
	fPowerTransitionInProgress 		= false;
	fPowerManagementInitialized 	= false;
	fUserClientExclusiveControlled	= false;
	
	// Allocate the thread on which to do power management
	fPowerManagementThread = thread_call_allocate (
			( thread_call_func_t ) IOSCSIProtocolInterface::sPowerManagement,
			( thread_call_param_t ) this );
	require_nonzero ( fPowerManagementThread, ReleaseCommandGate );
	
	result = true;
	
	return result;
	

ReleaseCommandGate:
	
	
	workLoop->removeEventSource ( fCommandGate );
	fCommandGate->release ( );
	fCommandGate = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ free - Called by IOKit to deallocate any resources.	  		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::free ( void )
{
	
	IOWorkLoop *	workLoop = NULL;
	
	if ( fCommandGate != NULL )
	{
		
		workLoop = getWorkLoop ( );
		if ( workLoop != NULL )
		{
			
			// Remove our command gate as an event source on
			// the workloop
			workLoop->removeEventSource ( fCommandGate );
			
		}
		
		fCommandGate->release ( );
		fCommandGate = NULL;
		
	}
	
	if ( fPowerManagementThread != NULL )
	{
		
		// Free the thread we allocated for power management
		thread_call_free ( fPowerManagementThread );
		fPowerManagementThread = NULL;
		
	}
	
	super::free ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ GetUserClientExclusivityState - Call to see if user client is in
//									 exclusive mode.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIProtocolInterface::GetUserClientExclusivityState ( void )
{
	
	bool	state;
	
	fCommandGate->runAction ( ( IOCommandGate::Action )
					&IOSCSIProtocolInterface::sGetUserClientExclusivityState,
					( void * ) &state );
	return state;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ SetUserClientExclusivityState - Call to see if user client is in
//									 exclusive mode.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIProtocolInterface::SetUserClientExclusivityState (
										IOService *		userClient,
										bool			state )
{
	
	IOReturn	status = kIOReturnExclusiveAccess;
	
	fCommandGate->runAction ( ( IOCommandGate::Action )
					&IOSCSIProtocolInterface::sSetUserClientExclusivityState,
					( void * ) &status,
					( void * ) userClient,
					( void * ) state );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ initialPowerStateForDomainState - 	Returns to the power manager what
//										initial state the device should be in.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIProtocolInterface::initialPowerStateForDomainState (
											IOPMPowerFlags	flags )
{
	
	// We ignore the flags since we are always active at startup time and we
	// just report what our initial power state is.
	return GetInitialPowerState ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ setPowerState - Set/Change the power state of the device		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIProtocolInterface::setPowerState (
							UInt32	  powerStateOrdinal,
							IOService * whichDevice )
{
	
	return fCommandGate->runAction ( ( IOCommandGate::Action )
							&IOSCSIProtocolInterface::sHandleSetPowerState,
							( void * ) powerStateOrdinal );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ IsPowerManagementIntialized - Returns fPowerManagementInitialized.[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIProtocolInterface::IsPowerManagementIntialized ( void )
{
	
	return fPowerManagementInitialized;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ CheckPowerState - 	Checks if it is ok for I/O to come through. If it is
//						not ok, then we block the thread.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::CheckPowerState ( void )
{
	
	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	TicklePowerManager ( );
	
	// Now run an action behind a command gate to block the threads if necessary
	fCommandGate->runAction ( ( IOCommandGate::Action )
						  		&IOSCSIProtocolInterface::sHandleCheckPowerState );
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ willTerminate - Called prior to phase2 termination.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIProtocolInterface::willTerminate ( IOService *   provider,
										IOOptionBits   options )
{

	bool	result = false;

	fCommandGate->commandWakeup ( &fCurrentPowerState, false );
	result = super::willTerminate ( provider, options );

	return result;

}
 
 
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ finalize - Terminates all power management.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIProtocolInterface::finalize ( IOOptionBits options )
{
	
	if ( fPowerManagementInitialized == true )
	{
		
		if ( fPowerTransitionInProgress == true )
			fCommandGate->commandSleep ( &fPowerTransitionInProgress, THREAD_UNINT ); 
		
		fCommandGate->commandWakeup ( &fCurrentPowerState, false );
		
		STATUS_LOG ( ( "PMstop about to be called.\n" ) );
		
		PMstop ( );
		
		if ( fPowerManagementThread != NULL )
		{
			
			// If the power management thread is scheduled, unschedule it.
			thread_call_cancel ( fPowerManagementThread );
			
		}
		
		fPowerManagementInitialized = false;
		
	}
	
	return super::finalize ( options );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ InitializePowerManagement - Joins PM tree and initializes pm vars.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::InitializePowerManagement ( IOService * provider )
{
	
	PMinit ( );
	setIdleTimerPeriod ( 5 * 60 ); 	// set 5 minute idle timer until system sends us
									// new values via setAggressiveness()
	provider->joinPMtree ( this );
	
	// Call makeUsable here to tell the power manager to put us in our
	// highest power state when we call registerPowerDriver().
	makeUsable ( );
	fPowerManagementInitialized = true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ HandleSetPowerState - Synchronized form of changing a power state.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::HandleSetPowerState ( UInt32 powerStateOrdinal )
{
	
	AbsoluteTime	time;
	
	fProposedPowerState = powerStateOrdinal;
	
	if ( ( fPowerTransitionInProgress == false ) || fPowerAckInProgress )
	{
		
		// mark us as being in progress, then call the thread which is
		// the power management state machine
		fPowerTransitionInProgress = true;
		
		// We use a delayed thread call in order to allow the thread we're
		// on (the power manager thread) to get back to business doing
		// whatever it needs to with the rest of the system
		clock_interval_to_deadline ( kThreadDelayInterval, kMillisecondScale, &time );
		( void ) thread_call_enter_delayed ( fPowerManagementThread, time );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ HandleCheckPowerState - 	Called on safe side of command gate to
//								serialize access to the sleep related
//								variables.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::HandleCheckPowerState ( UInt32 maxPowerState )
{
	
	STATUS_LOG ( ( "IOSCSIProtocolInterface::%s called\n", __FUNCTION__ ) );
	
	// A while loop is used here in case the power is changed on us while
	// the power state-machine is running. If the power manager tells us to
	// wake up, we unblock the thread by calling fCommandGate->commandWakeup, but
	// while we do this we may block and inbetween getting rescheduled, the
	// power manager might tell us to change states again, so we guard against
	// this scenario by looping and verifying that the state does indeed go to
	// active.
	while ( ( fCurrentPowerState != maxPowerState ) &&
			( isInactive ( ) == false ) &&
			( getWorkLoop ( )->onThread ( ) == false ) )
	{
		
		fCommandGate->commandSleep ( &fCurrentPowerState, THREAD_UNINT );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ TicklePowerManager - 	Convenience function to call power manager's
//							activity tickle routine.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIProtocolInterface::TicklePowerManager ( UInt32 maxPowerState )
{
	
	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	return ( activityTickle ( kIOPMSuperclassPolicy1, maxPowerState ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ HandleGetUserClientExclusivityState -	Checks to see what state the user
//											client is in.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIProtocolInterface::HandleGetUserClientExclusivityState ( void )
{
	
	return fUserClientExclusiveControlled;
	
}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ HandleSetUserClientExclusivityState - Sets the state the user client is in
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIProtocolInterface::HandleSetUserClientExclusivityState (
							IOService *		userClient,
							bool			state )
{
	
	IOReturn		status = kIOReturnExclusiveAccess;
	
	if ( fUserClient == NULL )
	{
		
		STATUS_LOG ( ( "fUserClient is NULL\n" ) );
		
		if ( state != false )
		{
			
			STATUS_LOG ( ( "state is true\n" ) );
			fUserClient = userClient;
			fUserClientExclusiveControlled = state;
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			
			STATUS_LOG ( ( "state is false\n" ) );
			status = kIOReturnBadArgument;
			
		}
		
	}
	
	else if ( fUserClient == userClient )
	{
		
		STATUS_LOG ( ( "fUserClient is same as userClient\n" ) );
		
		if ( state == false )
		{
			
			STATUS_LOG ( ( "state is false\n" ) );
			fUserClient = NULL;
			fUserClientExclusiveControlled = state;
			status = kIOReturnSuccess;
			
		}
		
		status = kIOReturnSuccess;
		
	}
	
	STATUS_LOG ( ( "HandleSetUserClientExclusivityState status = %d\n", status ) );
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Static Methods (C->C++ glue)
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ sHandleSetPowerState - C->C++ glue.					[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIProtocolInterface::sHandleSetPowerState (
								IOSCSIProtocolInterface * 	self,
								UInt32		 				powerStateOrdinal )
{
	
	IOReturn	result = 0;
	
	if ( self->isInactive ( ) == false )
	{ 
		
		self->HandleSetPowerState ( powerStateOrdinal );
		result = k100SecondsInMicroSeconds;
		
	}
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ sGetPowerTransistionInProgress - Returns power transition status.
//															[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIProtocolInterface::sGetPowerTransistionInProgress (
								IOSCSIProtocolInterface * self )
{
	return ( self->fPowerTransitionInProgress );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ sPowerManagement - C->C++ glue.						[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::sPowerManagement ( thread_call_param_t whichDevice )
{
	
	IOSCSIProtocolInterface *	self;
	
	self = ( IOSCSIProtocolInterface * ) whichDevice;
	if ( self != NULL )
	{
		
		self->retain ( );
		
		if ( self->isInactive ( ) == false )
		{
			
			self->HandlePowerChange ( );
			self->fPowerAckInProgress = true;	
			self->acknowledgeSetPowerState ( );
			self->fPowerAckInProgress = false;
			
		}
		
		self->fPowerTransitionInProgress = false;
		
		// If we are now inactive, wakeup anyone blocked in finalize()
		// since they're waiting for us to finish...
		if ( self->isInactive ( ) == true )
		{
			self->fCommandGate->commandWakeup ( &self->fPowerTransitionInProgress, false );
		}
		
		self->release ( );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ sHandleCheckPowerState - C->C++ glue.					[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::sHandleCheckPowerState (
							IOSCSIProtocolInterface * self )
{
	
	self->HandleCheckPowerState ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ sGetUserClientExclusivityState - C->C++ glue.			[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::sGetUserClientExclusivityState (
								IOSCSIProtocolInterface *	self,
								bool *						state )
{
	
	*state = self->HandleGetUserClientExclusivityState ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ sSetUserClientExclusivityState - C->C++ glue.			[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIProtocolInterface::sSetUserClientExclusivityState (
								IOSCSIProtocolInterface *	self,
								IOReturn *					status,
								IOService *					userClient,
								bool						state )
{
	
	*status = self->HandleSetUserClientExclusivityState ( userClient, state );
	
}


#if 0
#pragma mark -
#pragma mark ¥ Static Debugging Assertion Method
#pragma mark -
#endif


void
IOSCSIArchitectureModelFamilyDebugAssert (	const char * componentNameString,
											const char * assertionString, 
											const char * exceptionLabelString,
											const char * errorString,
											const char * fileName,
											long lineNumber,
											int errorCode )
{
	
	IOLog ( "%s Assert failed: %s ", componentNameString, assertionString );
	
	if ( exceptionLabelString != NULL )
		IOLog ( "%s ", exceptionLabelString );
	
	if ( errorString != NULL )
		IOLog ( "%s ", errorString );
	
	if ( fileName != NULL )
		IOLog ( "file: %s ", fileName );
	
	if ( lineNumber != 0 )
		IOLog ( "line: %ld ", lineNumber );
	
	if ( ( long ) errorCode != 0 )
		IOLog ( "error: %ld ", ( long ) errorCode );
	
	IOLog ( "\n" );
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif

OSMetaClassDefineReservedUsed ( IOSCSIProtocolInterface, 1 );
// Used by the abstract member routine:
// virtual SCSIServiceResponse		AbortTask ( UInt8 theLogicalUnit, SCSITaggedTaskIdentifier theTag ) = 0;

OSMetaClassDefineReservedUsed ( IOSCSIProtocolInterface, 2 );
// Used by the abstract member routine:
// virtual SCSIServiceResponse		AbortTaskSet ( UInt8 theLogicalUnit ) = 0;

OSMetaClassDefineReservedUsed ( IOSCSIProtocolInterface, 3 );
// Used by the abstract member routine:
// virtual SCSIServiceResponse		ClearACA ( UInt8 theLogicalUnit ) = 0;

OSMetaClassDefineReservedUsed ( IOSCSIProtocolInterface, 4 );
// Used by the abstract member routine:
// virtual SCSIServiceResponse		ClearTaskSet ( UInt8 theLogicalUnit ) = 0;

OSMetaClassDefineReservedUsed ( IOSCSIProtocolInterface, 5 );
// Used by the abstract member routine:
// virtual SCSIServiceResponse		LogicalUnitReset ( UInt8 theLogicalUnit ) = 0;

OSMetaClassDefineReservedUsed ( IOSCSIProtocolInterface, 6 );
// Used by the abstract member routine:
// virtual SCSIServiceResponse		TargetReset ( void ) = 0;

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 7 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 8 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 9 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 10 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 11 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 12 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIProtocolInterface, 16 );