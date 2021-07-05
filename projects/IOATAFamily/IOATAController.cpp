/*
 * Copyright (c) 2000-2008 Apple, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *  
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *
 *	IOATAController.cpp
 *
 */
 

#include <IOKit/assert.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include "IOATATypes.h"
#include "IOATAController.h"
#include "IOATACommand.h"
#include "IOATADevice.h"
#include "IOATABusInfo.h"
#include "IOATADevConfig.h"
#include "IOATABusCommand.h"
#include "ATATimerEventSource.h"


#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>


// for wait U8Status, loop time in uS
#define kStatusDelayTime  5
// how many times through the loop for a MS.
#define kStatusDelayLoopMS  1000 / kStatusDelayTime


#define _doubleBufferDesc	reserved->_doubleBufferDesc


#ifdef DLOG
#undef DLOG
#endif

//#define  ATA_DEBUG 1

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


#pragma mark -IOService Overrides -



//---------------------------------------------------------------------------

#define super IOService

OSDefineMetaClass( IOATAController, IOService )
OSDefineAbstractStructors( IOATAController, IOService )
    OSMetaClassDefineReservedUnused(IOATAController, 0);
    OSMetaClassDefineReservedUnused(IOATAController, 1);
    OSMetaClassDefineReservedUnused(IOATAController, 2);
    OSMetaClassDefineReservedUnused(IOATAController, 3);
    OSMetaClassDefineReservedUnused(IOATAController, 4);
    OSMetaClassDefineReservedUnused(IOATAController, 5);
    OSMetaClassDefineReservedUnused(IOATAController, 6);
    OSMetaClassDefineReservedUnused(IOATAController, 7);
    OSMetaClassDefineReservedUnused(IOATAController, 8);
    OSMetaClassDefineReservedUnused(IOATAController, 9);
    OSMetaClassDefineReservedUnused(IOATAController, 10);
    OSMetaClassDefineReservedUnused(IOATAController, 11);
    OSMetaClassDefineReservedUnused(IOATAController, 12);
    OSMetaClassDefineReservedUnused(IOATAController, 13);
    OSMetaClassDefineReservedUnused(IOATAController, 14);
    OSMetaClassDefineReservedUnused(IOATAController, 15);
    OSMetaClassDefineReservedUnused(IOATAController, 16);
    OSMetaClassDefineReservedUnused(IOATAController, 17);
    OSMetaClassDefineReservedUnused(IOATAController, 18);
    OSMetaClassDefineReservedUnused(IOATAController, 19);
    OSMetaClassDefineReservedUnused(IOATAController, 20);

//---------------------------------------------------------------------------



bool 
IOATAController::init(OSDictionary * properties)
{
    DLOG("IOATAController::init() starting\n");


    // Initialize instance variables.
    //
    _workLoop               = 0;
    _cmdGate                = 0;
    _provider				= 0;
	_timer					= 0;
	
	queue_init( &_commandQueue );

    if (super::init(properties) == false)
    {
        DLOG("IOATAController: super::init() failed\n");
        return false;
    }

    DLOG("IOATAController::init() done\n");

    return true;
}


/*---------------------------------------------------------------------------
 *
 *	Check and see if we really match this device.
 * override to accept or reject close matches based on further information
 ---------------------------------------------------------------------------*/

IOService* 
IOATAController::probe(IOService* provider,	SInt32*	score)
{

	return this;

}

/*---------------------------------------------------------------------------
 *
 *	Override IOService start.
 *
 *	Subclasses should override the start method, call the super::start
 *	first then add interrupt sources and probe their busses for devices 
 *	and create device nubs as needed.
 ---------------------------------------------------------------------------*/

bool 
IOATAController::start(IOService *provider)
{

	OSObject * prop;

    DLOG("IOATAController::start() begin\n");

 	_provider = provider;
 	_busState = IOATAController::kBusFree;
 	_currentCommand = 0L;
 	_selectedUnit = kATAInvalidDeviceID;
 	_queueState = IOATAController::kQueueOpen;
	
 	// call start on the superclass
    if (!super::start(_provider))
 	{
        DLOG("IOATAController: super::start() failed\n");
        return false;
	}

	prop = getProperty ( kIOPropertyPhysicalInterconnectTypeKey, gIOServicePlane );
	if ( prop == NULL )
	{
		setProperty ( kIOPropertyPhysicalInterconnectTypeKey, kIOPropertyPhysicalInterconnectTypeATA);
	}

	prop = getProperty ( kIOPropertyPhysicalInterconnectLocationKey, gIOServicePlane );
	if ( prop == NULL )
	{
		setProperty ( kIOPropertyPhysicalInterconnectLocationKey, kIOPropertyInternalKey);
	}

	reserved = ( ExpansionData * ) IOMalloc ( sizeof ( ExpansionData ) );
	if ( !reserved )
		return false;
	
	bzero ( reserved, sizeof ( ExpansionData ) );
	
	if( !configureTFPointers() )
	{
		DLOG("IOATA TF Pointers failed\n");
		return false;
	}
	
	if( !scanForDrives() )
	{
		DLOG("IOATA scan for drives failed\n");
		return false;
	}
	
	// Allocate the double buffer for PIO (and non-aligned DMA if needed) transfers.
	if(! allocateDoubleBuffer() )
	{
		DLOG("IOATA doubleBuffer alloc failed\n");
		return false;
	}

	// a device specific subclass will create the work loop and attach
    _workLoop = getWorkLoop();
    if (!_workLoop)
 	{
        DLOG("IOATA: IOWorkLoop allocation failed\n");
       return false;
	}
	
	_workLoop->retain();
	
	// create a timer event source and attach it to the work loop
    _timer = ATATimerEventSource::ataTimerEventSource(this,
               (ATATimerEventSource::Action) timeoutOccured);
    
    if (!_timer || _workLoop->addEventSource(_timer))
    {
        DLOG("IOATA: ATATImerEventSource allocation failed\n");
        return false;
	}
	
	
	// create a commandGate and attach it to the work loop.
    _cmdGate = IOCommandGate::commandGate(this);
 
    if (!_cmdGate || _workLoop->addEventSource(_cmdGate))
 	{
        DLOG("IOATAController: IOCommandGate failed\n");
        return false;
	}

	//3643376 make it easier for ASP to find disk drives in the system. 
	
	registerService();
    
	DLOG("IOATAController::start() done\n");
    return true;
}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
IOATAController::free()
{
	
	if ( _workLoop )
	{
		
		if ( _cmdGate )
		{
			
			_workLoop->removeEventSource(_cmdGate);
			_cmdGate = NULL;
			
		}
		
		if ( _timer )
		{
			
			_workLoop->removeEventSource(_timer);
			_timer = NULL;
			
		}
		
		_workLoop->release();
		
	}
	
	if ( reserved )
	{
		
		if ( _doubleBufferDesc )
		{
			
			_doubleBufferDesc->complete();
			_doubleBufferDesc->release();
			_doubleBuffer.bufferSize = 0;
			_doubleBuffer.logicalBuffer = 0;
			_doubleBuffer.physicalBuffer = 0;	
			_doubleBufferDesc = NULL;
			
		}

	}
	
	if ( reserved )
	{
		IOFree ( reserved, sizeof ( ExpansionData ) );
		reserved = NULL;
	}
	
	super::free();


}


#pragma mark -initialization-

/*---------------------------------------------------------------------------
 *
	// false if couldn't allocate the per-bus double buffer.
	// controllers should provide implementation where needed 
	// for DMA hardware compatibility. The default method provides
	// a 4K buffer for PIO since MemoryDescriptors do not by default have 
	// logical addresses in the kernel space.
 ---------------------------------------------------------------------------*/
bool 
IOATAController::allocateDoubleBuffer( void )
{

	DLOG("IOATAController::allocateDoubleBuffer() started\n");
	
	_doubleBufferDesc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
		kernel_task,
		kIODirectionInOut | kIOMemoryPhysicallyContiguous,
		(kATADefaultSectorSize * 8),
		0xFFFFF000UL );
	
    if ( !_doubleBufferDesc )
    {
        IOLog("%s: double buffer allocation failed\n", getName());
        return false;
    }
	
	_doubleBufferDesc->prepare();
	_doubleBuffer.logicalBuffer 	= (IOLogicalAddress)_doubleBufferDesc->getBytesNoCopy();
	_doubleBuffer.physicalBuffer	= _doubleBufferDesc->getPhysicalAddress();
	_doubleBuffer.bufferSize		= kATADefaultSectorSize * 8;
 	
	DLOG("IOATAController::allocateDoubleBuffer() done\n");
	
	return true;

}

/*---------------------------------------------------------------------------
 *
 *	Initialize the taskfile pointers to the addresses of the ATA registers
 *	in your hardware. Subclasses must provide implementation.
 *
 ---------------------------------------------------------------------------*/
bool
IOATAController::configureTFPointers(void)
{

	DLOG("IOATA: configureTFPointers. must provide implementation.\n");
	return false;

}


#pragma mark -client interface-

/*---------------------------------------------------------------------------
 *
 *	select the bus timing configuration for a particular device
 *  should be called by device driver after doing an identify device command and working out 
 *	the desired timing configuration.
 *	should be followed by a Set Features comand to the device to set it in a 
 *	matching transfer mode.
 ---------------------------------------------------------------------------*/
IOReturn 
IOATAController::selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber)
{


	DLOG("IOATA: sublcass must implement selectConfig.\n");

	return kATAModeNotSupported;

}

/*---------------------------------------------------------------------------
 *
 *	Find out what the current bus timing configuration is for a particular
 *	device. 
 *	Subclasses must implement
 *
 ---------------------------------------------------------------------------*/
IOReturn 
IOATAController::getConfig( IOATADevConfig* configOut, UInt32 unitNumber)
{


	DLOG("IOATA: sublcass must implement getConfig.\n");

	return kATAModeNotSupported;


} 

/*---------------------------------------------------------------------------
 *
 *	All ata controller subclasses must provide an implementation.
 *
 *
  ---------------------------------------------------------------------------*/

IOReturn
IOATAController::provideBusInfo( IOATABusInfo* infoOut)
{

	DLOG(" IOATA: sublcass must implement provideBusInfo\n");

	return -1;
}



/*---------------------------------------------------------------------------
 *
 *	The main call which puts something on the work loop
 *
 ---------------------------------------------------------------------------*/

IOReturn 
IOATAController::executeCommand(IOATADevice* nub, IOATABusCommand* command)
{
	if( !command || !nub )
		return -1;

	IOReturn resultCode = kATANoErr;

	// flag the command as in use.
	command->setCommandInUse();

	_cmdGate->runAction( (IOCommandGate::Action) 
						&IOATAController::executeCommandAction,
            			(void *) command, 			// arg 0
            			(void *) &resultCode, 		// arg 1
            			0, 0);						// arg2 arg 3
    
    
 
    return resultCode;
}


#pragma mark -workloop entry-




/*---------------------------------------------------------------------------
 *
 * Static function called by the internal IOCommandGate object to
 * handle a runAction() request invoked by executeCommand(). 	
 *
 ---------------------------------------------------------------------------*/
void 
IOATAController::executeCommandAction(OSObject * owner,
                                               void *     arg0,
                                               void *     arg1,
                                               void *  /* arg2 */,
                                               void *  /* arg3 */)
{


	IOATAController* self = (IOATAController*) owner;
	IOATABusCommand* command = (IOATABusCommand*) arg0;
	IOReturn*	result 	  = (IOReturn*)	arg1;

	// do a little reality check.
	assert(command && result);

	*result = self->handleCommand( (void*)command );

}


/*---------------------------------------------------------------------------
 *
 *	Do something with the command
 *
 ---------------------------------------------------------------------------*/
IOReturn 
IOATAController::handleCommand(	void*	param0,     /* the command */
				void*	param1,		/* not used = 0 */
				void*	param2,		/* not used = 0 */
				void*	param3 )	/* not used = 0 */
{
 
 
 	IOATABusCommand* command = (IOATABusCommand*) param0;                                  	

	if( command == 0L )
	{
		DLOG("IOATAController::handleCmd nill ptr\n");
		return -1;
	}

	// set the state flag on the transaction.
	command->state = kATAInitial;

	// Put the command on the queue.
	enqueueCommand( command );		

	// wake up it is time to go to work
	dispatchNext();	

	// No error on async commands
	return kATANoErr;


}


/*---------------------------------------------------------------------------
 *
 * enqueueCommand looks at the command flags and queues it at the tail if 
 * unless it is immediate, then it puts it at the head of the queue.
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::enqueueCommand( IOATABusCommand* command)
{

	// immediate means go to the head of the queue.
	// This could get sticky - if there's already an 
	// immediate command at the head of the queue.
	
	// whether a new immediate should go to the head, or behind the 
	// most recent already queued immediate is unsettled at this time.

	if( command->getFlags() & mATAFlagImmediate )
	{

		queue_enter_first( 	&_commandQueue, 
					 		command,
							IOATABusCommand*,
							queueChain );
	
	
	} else {
		
		// put at the tail.
		
		queue_enter( 	&_commandQueue, 
					 	command,
						IOATABusCommand*,
						queueChain );
	
	}


	return kATANoErr;

}



/*---------------------------------------------------------------------------
 *
 *	dequeueFirstCommand  take the first command off the head of the queue
 *	returns a IOATABusCommand*, or nil if empty.
 *
 ---------------------------------------------------------------------------*/
IOATABusCommand*
IOATAController::dequeueFirstCommand( void )
{

	IOATABusCommand* cmdPtr = 0L;

	if( !queue_empty( &_commandQueue ) )
	{
		queue_remove_first( &_commandQueue, 
							cmdPtr,
							IOATABusCommand*,
							queueChain );

	}
	return cmdPtr;

}
 
/*---------------------------------------------------------------------------
 * true - bus is ready to dispatch commands
 * false - bus is busy with a current command.
 *
 ---------------------------------------------------------------------------*/
bool 
IOATAController::busCanDispatch( void )
{

	// normal case
	if( _busState == IOATAController::kBusFree
		&& _queueState == IOATAController::kQueueOpen )
	{
		return true;
	}
	

	// special case if we are dispatching immediate commands only
	if( _busState == IOATAController::kBusFree
		&& _queueState == IOATAController::kQueueLocked
		&& _immediateGate == kImmediateOK 
		&& !queue_empty( &_commandQueue ) )
	{
	
		DLOG("IOATA Qfrozen check for immediate\n");
	
		// make sure the head of the queue is immediate
		IOATABusCommand* cmdPtr = (IOATABusCommand*) queue_first( & _commandQueue ) ;
		
		if( cmdPtr != 0 
			&& (cmdPtr->getFlags() & mATAFlagImmediate) )
		{
			DLOG("IOTA q-head is immediate\n");
			return true;
		}
	}
	
	
	// otherwise no dispatch.
	return false;

}


/*---------------------------------------------------------------------------
 *
 *  this should only be called on the single-threaded side of the command gate.
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn 
IOATAController::dispatchNext( void )
{
	
	IOReturn result = kATANoErr;

		DLOG("IOATAController::dispatchNext start\n");
	
	// check that the hardware is free and ready to accept commands
	if( !busCanDispatch() )
		return result;
	
	// set the bus state flag
	_busState = IOATAController::kBusBusy;
	
	// take the command at the head of the queue and make current.
	_currentCommand = dequeueFirstCommand();
	
	if( _currentCommand == 0L )
	{
		// if there's nothing in the queue, free the bus and 
		// return.
	
		DLOG("IOATAController::dispatchNext queue empty\n");
		_busState = IOATAController::kBusFree;
		return kATAQueueEmpty;
	
	}

	// set the state flag on the transaction.
	_currentCommand->state = IOATAController::kATAStarted;

	// IF we are in the special case circumstance of running commands 
	// that re-enter the command gate because they were dispatched during 
	// an executeEventCallout(), we MUST poll the device for command completion
	// This is an ugly artifact of the workloop design.
	
	if( _queueState == IOATAController::kQueueLocked
		&& _immediateGate == kImmediateOK
		&& _currentCommand->getFlags() & mATAFlagImmediate)
	{
	
		_currentCommand->setFlags(_currentCommand->getFlags() | mATAFlagUseNoIRQ );
	
	}

	// in case someone tries to slip a reset command through as an exec IO.

	if(	_currentCommand->getTaskFilePtr()->ataTFCommand == 0x08 )
	{
	
		_currentCommand->setOpcode(kATAFnBusReset);
	}


	switch(	_currentCommand->getOpcode() )
	{
	
	
		case kATAFnExecIO:			/* Execute ATA I/O */
		case kATAPIFnExecIO:		/* ATAPI I/O */
			result = handleExecIO();			
		break;

		case kATAFnRegAccess:		/* Register Access */
			result = handleRegAccess();
		break;
		
		case kATAFnBusReset:		/* Reset ATA bus */
			result = handleBusReset();		
		break;

		case kATAFnQFlush:			/* I/O Queue Release */
			result = handleQueueFlush();
		break;
		
		
		default:
			_currentCommand->setResult( kATAUnknownOpcode );
			result = kATAUnknownOpcode;
			_currentCommand->state = IOATAController::kATAComplete;
			completeIO(kATAUnknownOpcode);
		break;	
	
	}

		DLOG("IOATAController::dispatchNext done return = %ld\n", (long int)result);

	return result;

}



/*---------------------------------------------------------------------------
 *	Calls the device driver(s) which are connected to this bus in order to 
 *	inform them that an event occurred that may require their attention. 
 *	Resets would be such an event, as the drivers may need to immediately 
 *	reconfigure their devices BEFORE any other commands may be issued.
 *
 ---------------------------------------------------------------------------*/
void
IOATAController::executeEventCallouts( ataEventCode event, ataUnitID unit )
{

	// make the current command local, in preparation for allowing 
	// immediate commands through.
	IOATABusCommand* currCommand = _currentCommand;
	UInt32 currQueueState = _queueState;
	UInt32 currBusState = _busState;
	UInt32 currImmed = _immediateGate;

	// unbusy the bus, but freeze the queue
	// this allows immediate commands to run
	_queueState = kQueueLocked;
	_busState = kBusFree;
	_immediateGate = kImmediateOK;
	_currentCommand = 0;

	
	for( int i = 0; i < 2; i++)
	{
		// call the device as requested, or both devices if the unit id 
		// isn't 0 or 1.
		if( (_nub[i] != 0L )  
			&& ( (unit == kATAInvalidDeviceID) || (unit == i) ) )
		{
			// during notify event, device drivers will have an opportunity
			// to execute commands to their devices in immediate mode on this thread.
			
			_nub[i]->notifyEvent( event );
			
		}		
	
	}

	// retore the state of the bus, queue, immediate gate and current commands

	_queueState = currQueueState;
	_busState = currBusState;
	_immediateGate = currImmed;
	_currentCommand = currCommand;
	

}	


/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
void 
IOATAController::completeIO( IOReturn commandResult )
{
	DLOG("IOATAController::completeIO start = %ld\n",(long int)commandResult);


	// make the current command local and nil the command in execution.
	IOATABusCommand* finishedCmd = _currentCommand;
	// someone called completeIO with no command executing.
	if( finishedCmd == 0L )
		return;

	// nil the current command
	_currentCommand = 0L;

	finishedCmd->state = IOATAController::kATADone;

	// clear the timer if it is running.
	stopTimer();
	stopDMA();

	// set the bus state to free to allow next dispatch.
	_busState = IOATAController::kBusFree;	

	// set the result code in case someone didn't update it already.	
	finishedCmd->setResult(commandResult);
	
	// complete the IO and execute the callback if any prior to 
	// dispatching next command. Gives device drivers a chance to 
	// recover from error.
	finishedCmd->executeCallback();

	dispatchNext();	
	
	DLOG("IOATAController::completeIO done\n");
	

}

#pragma mark -timers-
/*---------------------------------------------------------------------------
 *	start the transaction timer for a transaction.
 *
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn 
IOATAController::startTimer( UInt32 inMS)
{
	IOReturn err = kATANoErr;
	
	// make sure it is not armed first.
	_timer->disable();
	_timer->cancelTimeout();

	// arm the timer
	_timer->enable();
	err = _timer->setTimeoutMS( inMS );

	if( err )
	{
		DLOG("IOATAController::startTimer failed\n");
	
	}
	
	return err;

}

/*---------------------------------------------------------------------------
 *
 *	Kill a running timer.
 *
 *
 ---------------------------------------------------------------------------*/
void
IOATAController::stopTimer(void)
{
	_timer->disable();
	_timer->cancelTimeout();

}


/*---------------------------------------------------------------------------
 *
 * simple static function that is called by the timerEventSource via the 
 *	command gate.
 *
 ---------------------------------------------------------------------------*/
void
IOATAController::timeoutOccured(OSObject *owner, IOTimerEventSource *sender)
{
	IOATAController* self = (IOATAController*) owner;		
	self->handleTimeout();
}


/*---------------------------------------------------------------------------
 *	Timer event sources can't fire while we're on the safe side of a command gate
 *	so we need a way to check for a timeout happening while we are inside 
 *  the thread-safe part of the code. Unfortunately, TimerEventSource provides 
 *  no means to check if it has expired, so this function always returns false 
 *	for now. Eventually, either Timer will get an subclass that allows this.
 ---------------------------------------------------------------------------*/
bool
IOATAController::checkTimeout( void )
{
	// check to see if the timer has expired while on the 
	// safe side of the command-gate.
	
		return _timer->hasTimedOut();
		
}


/*---------------------------------------------------------------------------
 *
 *	Do what is necessary to fail the current IO command because the HW did not
 *	respond within the alloted time.
 *
 ---------------------------------------------------------------------------*/
void
IOATAController::handleTimeout( void )
{
	// if there's a current command, kill it with timeout error

	if( _currentCommand != 0L )
	{	
	
		if( (_currentCommand->getFlags() & mATAFlagUseDMA ) == mATAFlagUseDMA )
		{
			stopDMA();
		
		}
		
		_currentCommand->setResult( kATATimeoutErr );
		_currentCommand->state = IOATAController::kATAComplete;
		asyncStatus();
		completeIO(kATATimeoutErr);	
	
	} else {
	
	// otherwise, check and see if there's anything to dispatch
	// that has been overlooked somehow.	
		dispatchNext();
	
	}
		
}

#pragma mark -Workloop handlers-
/*---------------------------------------------------------------------------
 *
 *	handleDeviceInterrupt - once the primary part of the device interrupt 
 *	service routine determines that a device interrupt has occured and is 
 *	valid, this routine should be called to handle the post interrupt service
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::handleDeviceInterrupt(void)
{
	// mark volatile to enforce reading the register.
	volatile UInt8 status = 0x00;
	
	// make sure there's a command active
	if( !_currentCommand )
	{
		DLOG("IOATA Device Int no command active\n");
		return kATADevIntNoCmd;
	}
	// read the actual status register to clear the interrupt.
	status = *_tfStatusCmdReg;
	OSSynchronizeIO();
	
	return asyncIO();
	
}

/*---------------------------------------------------------------------------
 *
 *
 * 
 *
 ---------------------------------------------------------------------------*/

IOReturn
IOATAController::handleExecIO( void )
{
	IOReturn err = kATANoErr;
	
	// select the desired device
	// don't start the IOTimer until after selection as there are no
	// generation counts in the IOTimerEventSource. Device Selection will honor 
	// the timeout value in ms on its own.
	err = selectDevice( _currentCommand->getUnit() );
	if( err )
	{	
		IOLog("IOATAController device blocking bus.\n");
		_currentCommand->state = IOATAController::kATAComplete;

		if( _currentCommand->getFlags() & mATAFlagUseNoIRQ )
		{
			completeIO( kIOReturnOffline );	
			return kIOReturnOffline;	
		}
		
		startTimer( 1000 );  // start a 1 second timeout so that we can unwind the stack if the bus is stuck.
		return kATANoErr;  // defer error handling to the timer thread. 
	}

	// start the IO Timer
	startTimer( _currentCommand->getTimeoutMS() );


	// go to asyncIO and start the state machine.
	// indicate the command has been issued
	_currentCommand->state = IOATAController::kATAStarted;		
	if( _currentCommand->getFlags() & mATAFlagUseNoIRQ )
	{
		err = synchronousIO();
	} else {
		err = asyncIO();
	}
		
	// return success and pend IRQ for further operation or completion.
	
	return err;

}

/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::handleRegAccess( void )
{
	IOReturn err = kATANoErr;

	// select the desired device
	err = selectDevice( _currentCommand->getUnit() );
	if( err )
	{
		_currentCommand->state = IOATAController::kATAComplete;
		completeIO( err );
		return err;
	}

	bool isWrite = (_currentCommand->getFlags() & mATAFlagIOWrite) ? true : false;

	err = registerAccess( isWrite );
	_currentCommand->state = IOATAController::kATAComplete;
	completeIO( err );
	return err;

}


/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::handleBusReset(void)
{

	bool		isATAPIReset = ((_currentCommand->getFlags() & mATAFlagProtocolATAPI) != 0);
	bool		doATAPI[2];
	IOReturn	err = kATANoErr;
	UInt8		index;
	UInt8 		statCheck;
	
	DLOG("IOATA bus reset start.\n");

	doATAPI[0] = doATAPI[1] = false;		

	// If this is an ATAPI reset select just the corresponding atapi device (instead of both) 
	if(isATAPIReset)
	{
		
		ataUnitID unitNumber = _currentCommand->getUnit();
		
		if((unitNumber == 0) || (unitNumber == 1))
		{
			
			doATAPI[unitNumber] = true;  // Mark only selected ATAPI as reset victim.
			
		}else{
			
			err = kATAInvalidDevID;
			
		}
		
	}else{
	
		doATAPI[0] = doATAPI[1] = true; // In ATA case, mark both as candidates for reset commands prior to a bus reset.
		
	}											
	
	
	// Issue the needed ATAPI reset commands	
	for(index=0;index<2;index++)
	{
		if( doATAPI[index] && _devInfo[index].type == kATAPIDeviceType)
		{			
			OSSynchronizeIO();		
			*_tfSDHReg = mATASectorSize + (index << 4);

			// read the alt status and disreguard to provide 400ns delay
			OSSynchronizeIO();		
			statCheck = *_tfAltSDevCReg;  

			err = softResetBus(true);			
		}
		
	}
	
	
	// once the ATAPI device has been reset, contact the device driver
	if(isATAPIReset)
	{			
		executeEventCallouts( kATAPIResetEvent, _currentCommand->getUnit() );		
	}		
	

	// Handle the ATA reset case
	if(!isATAPIReset)
	{	
		err = softResetBus(); 	
		executeEventCallouts( kATAResetEvent, kATAInvalidDeviceID );	
	}
	
	_currentCommand->state = IOATAController::kATAComplete;

	DLOG("IOATA bus reset done.\n");


	completeIO( err );

	
	return err;

}


/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::handleQueueFlush( void )
{

	//BUG do something here.


	return kATANoErr;
}


/*---------------------------------------------------------------------------
 *
 *  Use the state-indicator of the current command to figure out what to do
 *	after an interrupt, etc. This is the heart of the ATA state-machine.
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::asyncIO(void)
{
	IOReturn err = kATANoErr;

	if( (_currentCommand->getFlags() & mATAFlagProtocolATAPI) == mATAFlagProtocolATAPI
		&& _currentCommand->getPacketSize() > 0)
	{
	
		_currentCommand->state = determineATAPIState();
	}

	switch( _currentCommand->state )
	{
	
		
		case kATAStarted	:  // taskfile issue
			err = asyncCommand();
			DLOG("ATAController:: command sent: err = %ld state= %lx\n", (long int) err, (int) _currentCommand->state);
			if( err )
			{
				_currentCommand->state = IOATAController::kATAComplete;
				asyncStatus();
				break;
			}

			// if next state isn't write packet, or if the packet device asserts IRQ,
			// return pending the next interrupt.
			if( _currentCommand->state != IOATAController::kATAPICmd
				|| _devInfo[ _currentCommand->getUnit() ].packetSend == kATAPIIRQPacket )
			{
				// pending IRQ
				DLOG("ATAController:: pending IRQ for packet\n");
				break;
			}
							
		// otherwise fall through and send the packet for DRQ devices.
		case kATAPICmd:	 // packet issue
			DLOG("ATAController:: issue packet\n");
			err = writePacket();
			if( err )
			{
				_currentCommand->state = IOATAController::kATAComplete;
				asyncStatus();
				break;
			}

			// if there's data IO, next phase is dataTx, otherwise check status.
			if( (_currentCommand->getFlags() & (mATAFlagIORead |  mATAFlagIOWrite ) )
				&&  ((_currentCommand->getFlags() & mATAFlagUseDMA ) != mATAFlagUseDMA ) )
			{
				_currentCommand->state = IOATAController::kATADataTx;
			}	else {  			
				// this is a non-data command, the next step is to check status.				
				_currentCommand->state = IOATAController::kATAStatus;			
			}

		break;
										
		case kATADataTx:  // PIO data transfer phase
			err = asyncData();
			if( err )
			{
				_currentCommand->state = IOATAController::kATAComplete;
				asyncStatus();
				break;
			}
		
			// if there's more data to transfer, then 
			// break. If ATA protocol PIO write, then break for IRQ
			if(_currentCommand->state == kATADataTx
				|| ( (_currentCommand->getFlags() & (mATAFlagProtocolATAPI | mATAFlagIOWrite | mATAFlagUseDMA) ) == mATAFlagIOWrite ) )
			{
				 break;
			}
			
			if( (_currentCommand->getFlags() & mATAFlagProtocolATAPI) == mATAFlagProtocolATAPI
				&& _currentCommand->getPacketSize() > 0)
			{			
				// atapi devices will go to status after an interrupt.
				break;			
			}	
				
			// else fall through to status state.
		case kATAStatus:  // data tx complete		
			err = asyncStatus();
			_currentCommand->state = IOATAController::kATAComplete;
		break;
	
		// state machine is somehow inconsistent	
		default:
			DLOG("IOATA AsyncIO state broken\n");
			err = kATAErrUnknownType;
			_currentCommand->state = IOATAController::kATAComplete;
		break;
		
	}// end of switch ->state


	// call completeIO if the command is marked for completion.
	
	// BUG consider allowing the timeout to run rather than completing 
	// at this point. It might be safer to simply allow it to execute 
	// rather than move on the state machine at this point, since there's 
	// a possible race-condition if the timer expires while still inside the 
	// command gate.

	if( _currentCommand->state == IOATAController::kATAComplete )
	{
		completeIO(err);
	}
	
	return err;


}

/*---------------------------------------------------------------------------
 * This is a special-case state machine which synchronously polls the 
 *	status of the hardware while completing the command. This is used only in 
 *	special case of IO's which cannot be completed for some reason using the 
 *	normal interrupt system. This may involve vendor-specific commands, or 
 *	certain instances where the interrupts may not be available, such as when 
 *  handling commands issued as a result of messaging the drivers after a reset
 *	event. DMA commands are NOT accepted, only non-data and PIO commands.
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::synchronousIO(void)
{
	IOReturn err = kATANoErr;

	OSSynchronizeIO();		
	*_tfAltSDevCReg = 0x02; // disable interrupts

	
	// start by issuing the command	
	err = asyncCommand();
	DLOG("ATAController::synchronous command sent: err = %ld state= %lx\n", (long int) err, (int) _currentCommand->state);
	if( err )
	{
		_currentCommand->state = IOATAController::kATAComplete;
	
	} else {
	
	// spin on status until the next phase
	
		for( UInt32 i = 0; i< 3000; i++)
		{
			if( waitForU8Status( mATABusy, 0x00	) )
				break;
			IOSleep(10); //allow other threads to run.
		}
	
	}


	// if packet, send packet next
	if( _currentCommand->state == IOATAController::kATAPICmd )
	{						
		DLOG("ATAController::synchronous issue packet\n");
		err = writePacket();
		
		if( err == kATANoErr )
		{
			// if there's data IO, next phase is dataTx, otherwise check status.
			if( (_currentCommand->getFlags() & (mATAFlagIORead |  mATAFlagIOWrite ) )
				&&  ((_currentCommand->getFlags() & mATAFlagUseDMA ) != mATAFlagUseDMA ) )
		
			{
				_currentCommand->state = IOATAController::kATADataTx;
		
			} else {  			
				
				// this is a non-data command, the next step is to check status.				
				_currentCommand->state = IOATAController::kATAStatus;			
			}		

		} else {
		
			// an error occured writing the packet.
			_currentCommand->state = IOATAController::kATAComplete;
		}
	}


	// PIO data transfer phase
									
	if( _currentCommand->state == IOATAController::kATADataTx ) 
	{
		while( _currentCommand->state == IOATAController::kATADataTx  )
		{
			err = asyncData();
			if( err )
			{
				_currentCommand->state = IOATAController::kATAComplete;
				break;
			}
		}		
		
		if( (_currentCommand->getFlags() & mATAFlagProtocolATAPI) == mATAFlagProtocolATAPI
			&& _currentCommand->getPacketSize() > 0)
		{			
			// atapi devices will go to status after an interrupt.
			waitForU8Status( mATABusy, 0x00	);
		}	
	
	}		

	// else fall through to status state.
	if( _currentCommand->state == IOATAController::kATAStatus ) 		
	{
		err = asyncStatus();
		_currentCommand->state = IOATAController::kATAComplete;
	}
		
		
		
	// read the status register to make sure the hardware is in a consistent state.
	volatile UInt8 finalStatus = *_tfStatusCmdReg;
	OSSynchronizeIO();		
	finalStatus++;
	// call completeIO if the command is marked for completion.
	
	if( _currentCommand->state == IOATAController::kATAComplete )
	{
		completeIO(err);
	}

	OSSynchronizeIO();		
	*_tfAltSDevCReg = 0x00; // enable interrupts
	
	return err;


}


//----------------------------------------------------------------------------------------
//	FUNCTION:		asyncStatus
//	Description:	Get the end result of the IO from the device.
//	Input:			none
//	Output:			ATAError code
//----------------------------------------------------------------------------------------

IOReturn	
IOATAController::asyncStatus(void)
{
	IOReturn    err = kATANoErr;
	
	UInt8 status =  *_tfAltSDevCReg;
	OSSynchronizeIO();

	UInt8 error = 0x00;
	
	// if err bit is set, read the error register
	if( status & mATAError )
	{
		error = *_tfFeatureReg;
		OSSynchronizeIO();
		err = kATADeviceError;
		// look for error results in the TF 
		if( _currentCommand->getFlags() & (mATAFlagTFAccess | mATAFlagTFAccessResult) )
		{
			registerAccess( false );
		}


	// if this command returns results in registers on successful completion
	// read them now. 
	} else if( _currentCommand->getFlags() & mATAFlagTFAccessResult ) {
	
		registerAccess( false );
	}

	 _currentCommand->setEndResult( status, error);

	return err;	
}	





/*---------------------------------------------------------------------------
 *
 *	Command phase
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::asyncCommand(void)
{
	IOReturn err = kATANoErr;

	// if DMA, program and activate the DMA channel
	if( (_currentCommand->getFlags() & mATAFlagUseDMA ) == mATAFlagUseDMA )
	{
		err = startDMA();	
	}

	if( err )
	{
		stopDMA();
		return err;
	}

	DLOG("ATAController: command flags = %lx , packet size = %d\n",_currentCommand->getFlags(), _currentCommand->getPacketSize() );

	err = issueCommand();	
	if( err )
		return err;

	// if the command is an atapi command, set state to issue packet and return

	if( (_currentCommand->getFlags() & mATAFlagProtocolATAPI) == mATAFlagProtocolATAPI
		&& _currentCommand->getPacketSize() > 0)

	{
		// set to packet state
		_currentCommand->state = IOATAController::kATAPICmd;			
		return err;
	}

	// if DMA operation, return with status pending.
	
	if( (_currentCommand->getFlags() & mATAFlagUseDMA ) == mATAFlagUseDMA )
	{
		_currentCommand->state = IOATAController::kATAStatus;	
		return err;
	}

	// if PIO write operation, wait for DRQ and send the first sector
	// or sectors if multiple
	if( (_currentCommand->getFlags() 
		& (mATAFlagIOWrite | mATAFlagUseDMA | mATAFlagProtocolATAPI) ) 
		== mATAFlagIOWrite )
	{
	
		// mark the command as data tx state.		
		_currentCommand->state = IOATAController::kATADataTx;
		// send first data segment.
		return asyncData();				
	}
	
	if( (_currentCommand->getFlags() & mATAFlagIORead ) == mATAFlagIORead )
	{
		// read data on next phase.
		_currentCommand->state = IOATAController::kATADataTx;	
	
	}	else {  
	
		// this is a PIO non-data command or a DMA command the next step is to check status.
		_currentCommand->state = IOATAController::kATAStatus;	
	}

	return err;

}


/*---------------------------------------------------------------------------
 *
 *	PIO data transfer phase
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::asyncData(void)
{

	// first check and see if data is remaining for transfer
	
	IOByteCount bytesRemaining = _currentCommand->getByteCount() 
								- _currentCommand->getActualTransfer();
	
	// nothing to do
	if(bytesRemaining < 1)
	{
		_currentCommand->state = kATAStatus;
		return kATANoErr;
	}


	UInt8 status= 0x00;

	// check for DRQ
	while ( !checkTimeout()  )
	{
		// read the alt status reg
		OSSynchronizeIO();
		status = *_tfAltSDevCReg;
		// mask the BSY and DRQ bits
		status &= (mATABusy | mATADataRequest | mATAError);

		// look for BSY=0 and ERR=1
		if( mATAError == status )
		{
			// hardware has indicated an error condition
			_currentCommand->state = kATAStatus;
			return kATADeviceError;
			break;
		}


		// look for BSY=0 and DRQ=1
		if( mATADataRequest == status )
		{
			break;
		}
					
		// No need to loop quickly. This makes it easier to 
		// figure things out on a bus analyzer rather than fill its
		// buffer with thousands of status reads. 	
		 
			IODelay( 10 );  
	 }

	// let the timeout through
	if ( checkTimeout() )
	{			
		_currentCommand->state = kATAStatus;
		return kATATimeoutErr;
	}
	
	IOMemoryDescriptor* descriptor = _currentCommand->getBuffer();

	// The IOMemoryDescriptor may not have a logical address mapping (aka, 
	// virtual address) within the kernel address space. This poses a problem 
	// when doing PIO data transfers, which means the CPU is reading/writing 
	// the device data register and moving data to/from a memory address in the 
	// host. This requires some kind of logical address. However, in protected 
	// memory systems it is costly to map the client's buffer and give it a 
	// virtual address.
	
	// so all PIO data is double buffered to a chunk of mapped and wired memory
	// in the kernel space. IOMemoryDescriptor provides methods to read/write 
	// the physical address it contains.


	IOByteCount xfrPosition = _currentCommand->getPosition() + 
							_currentCommand->getActualTransfer();
	
	IOByteCount thisPass = bytesRemaining;
	IOByteCount overrun = 0;

	// pare down to the number of bytes between interrupts 
	// to be transferred. Do this chunk, then pend the 
	// next IRQ if bytes remain.
	if( thisPass > _currentCommand->getTransferChunkSize() )
	{
		thisPass = _currentCommand->getTransferChunkSize();
	}	
	
	// for atapi, see how many bytes the device is willing to transfer
	// by reading the device registers.
	if( _currentCommand->getFlags() & mATAFlagProtocolATAPI)
	{
		thisPass = readATAPIByteCount();
		
		// check for device overrun
		if( thisPass > bytesRemaining )
		{
			overrun = thisPass - bytesRemaining;
			thisPass = bytesRemaining;
		
		} 		
	
	}
	
	
	while( thisPass > 0 )
	{		
		IOByteCount bufferBytes = (thisPass > _doubleBuffer.bufferSize )? _doubleBuffer.bufferSize : thisPass;
	
		// read
		if( _currentCommand->getFlags() & mATAFlagIORead )
		{
			// device to buffer
			txDataIn(_doubleBuffer.logicalBuffer, bufferBytes );
			// buffer to descriptor
			descriptor->writeBytes(	xfrPosition, (void*) _doubleBuffer.logicalBuffer, bufferBytes);
		
		} else { //write
		
			// descriptor to buffer
			descriptor->readBytes( xfrPosition, (void*) _doubleBuffer.logicalBuffer, bufferBytes );
			// buffer to device
			txDataOut(_doubleBuffer.logicalBuffer, bufferBytes);			
		
		}		
			// update indicators
			xfrPosition += bufferBytes; 	
			thisPass -= bufferBytes;
			_currentCommand->setActualTransfer(_currentCommand->getActualTransfer() + bufferBytes);	
			bytesRemaining -= bufferBytes;	
	}
		
	//	sometimes ATAPI devices indicate more bytes than the host 
	// expects to transfer. We have to read/write the data register to satisfy the 
	// device so it is ready to accept commands afterwards.	
	if(overrun)
	{
		handleOverrun( overrun );
	}	

	if(bytesRemaining > 1)
	{
		// next IRQ means more data		
		_currentCommand->state = kATADataTx;
		
	} else {
	
		// next IRQ is a status check for completion		
		_currentCommand->state = kATAStatus;
	
	}

	return kATANoErr;

}





#pragma mark - Hardware Access -

/*---------------------------------------------------------------------------
 * Notify the controller to update the timing configuration for a  
 * newly-selected device. Controller drivers may need to supply implementation
 * for this command if their hardware doesn't maintain seperate timing registers
 * per device.
 ---------------------------------------------------------------------------*/

void
IOATAController::selectIOTiming( ataUnitID unit )
{


}


/*---------------------------------------------------------------------------
 *
 *  Perform device selection according to ATA standards document
 *	
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::selectDevice( ataUnitID unit )
{
	UInt32 msLoops = _currentCommand->getTimeoutMS()/10;	

	// do a reality check
	if( ! (  (kATADevice0DeviceID == unit) 
			|| (kATADevice1DeviceID == unit ) ) )
	{
	
		DLOG( "IOATA: invalid device ID selected\n");
		return kATAInvalidDevID;
	
	}		
	
	// give a chance for software to select the correct IO Timing 
	// in case the hardware doesn't maintain seperate timing regs 
	// for each device on the bus.
	selectIOTiming( unit );
	
	UInt8 preReqMask = (mATABusy | mATADataRequest );
	UInt8 preReqCondition = 0x00;
	
	// if the device is already selected, no need to reselect it.
	// However, we do need to test for the correct status 
	// before allowing a command to continue. So we check for BSY=0 and DRQ=0
	// before selecting a device that isn't already selected first.
	
	// if the unit needs to be selected, test for a good bus and write the bit.
	if( unit != _selectedUnit)
	{

		// check that BSY and DRQ are clear before selection
		while( !waitForU8Status( preReqMask, preReqCondition ) )
		{
			
			OSSynchronizeIO();
			if( msLoops == 0
				|| (*_tfStatusCmdReg & mATADataRequest) == mATADataRequest
				|| checkTimeout() )
			{
				DLOG( "IOATA: BUSY or DRQ can't select device. \n");
				return kATAErrDevBusy;
			
			}
			msLoops--;
			IOSleep(10);  // allow other threads to run.
		}

		// invalide the currently selected device in case there's an error
		_selectedUnit = kATAInvalidDeviceID;
	
		// write the selection bit
		*_tfSDHReg	= ( unit << 4 );
		OSSynchronizeIO();
	}	

	// unit was either selected above or was already the active device. Test for 
	// pre-requisite condition for commands.

	// for ATA devices, DRDY=1 is required with the exception of 
	// Init Drive Params and Execute Device diagnostics, 90h and 91h
	// as of ATA6, draft d1410r1a 
	// for Packet devices, DRDY is ignored for all commands.
	
	if( _devInfo[ unit ].type == kATADeviceType 
		&& _currentCommand->getOpcode() == kATAFnExecIO
		&& _currentCommand->getTaskFilePtr()->ataTFCommand != 0x90
		&& _currentCommand->getTaskFilePtr()->ataTFCommand != 0x91  )
	{
	
		preReqMask |= mATADriveReady;
		preReqCondition |= mATADriveReady;
	
	}
	
	// wait for BSY to clear
	msLoops = 10;
	 
	while( !waitForU8Status( (mATABusy ), 0x00 ))
	{
		
		OSSynchronizeIO();
		if( msLoops == 0
			|| (*_tfStatusCmdReg & mATADataRequest) == mATADataRequest
			|| checkTimeout() )
		{
			DLOG( "IOATA: BUSY can't select device. \n");			
			return kATAErrDevBusy;
		
		}
		msLoops--;
		IOSleep(10);  // allow other threads to run.
	}

	// enable device interrupt
	*_tfAltSDevCReg = 0x00;
	OSSynchronizeIO();

	// successful device selection.
	_selectedUnit = unit;
	return kATANoErr;


}


/*---------------------------------------------------------------------------
 *	This function issues the task file to the device, including the command.
 *	The device should be selected prior to calling this function. We write 
 *	Dev/head register in this function since it contains parameters, but 
 *	do not check for successful device selection.
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::issueCommand( void )
{
	if( _currentCommand == 0 )
	{
		DLOG("IOATA can't issue nil command\n");
		return kATAErrUnknownType;
	}

	

	if( _currentCommand->getFlags() & mATAFlag48BitLBA )
	{
		IOExtendedLBA* extLBA = _currentCommand->getExtendedLBA();
		*_tfSDHReg = extLBA->getDevice();
		OSSynchronizeIO();
		
		*_tfFeatureReg 	=	(extLBA->getFeatures16() & 0xFF00) >> 8 ;
		*_tfSCountReg 	=	(extLBA->getSectorCount16() & 0xFF00) >> 8 ;
		*_tfSectorNReg 	=	(extLBA->getLBALow16() & 0xFF00) >> 8 ;
		*_tfCylLoReg 	=	(extLBA->getLBAMid16() & 0xFF00) >> 8 ;
		*_tfCylHiReg 	=	(extLBA->getLBAHigh16() & 0xFF00) >> 8 ;
		OSSynchronizeIO();

		*_tfFeatureReg 	=	extLBA->getFeatures16() & 0x00FF;
		*_tfSCountReg 	=	extLBA->getSectorCount16() & 0x00FF;
		*_tfSectorNReg 	=	extLBA->getLBALow16() & 0x00FF;
		*_tfCylLoReg 	=	extLBA->getLBAMid16() & 0x00FF;
		*_tfCylHiReg 	=	extLBA->getLBAHigh16() & 0x00FF;
		OSSynchronizeIO();

		*_tfStatusCmdReg =  extLBA->getCommand();
		OSSynchronizeIO();

	
	} else {
	
		ataTaskFile* tfRegs = _currentCommand->getTaskFilePtr();
	
		OSSynchronizeIO();
	
		*_tfSDHReg		= 	tfRegs->ataTFSDH;
	
		OSSynchronizeIO();

		*_tfFeatureReg 	=	tfRegs->ataTFFeatures;
		*_tfSCountReg 	=	tfRegs->ataTFCount;
		*_tfSectorNReg 	=	tfRegs->ataTFSector;
		*_tfCylLoReg 	=	tfRegs->ataTFCylLo;
		*_tfCylHiReg 	=	tfRegs->ataTFCylHigh;
	
		OSSynchronizeIO();
	
		*_tfStatusCmdReg =  tfRegs->ataTFCommand;
	}

	return kATANoErr;
}




/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::writePacket( void )
{

	UInt32 packetSize = _currentCommand->getPacketSize();
	UInt16* packetData = _currentCommand->getPacketData();

	// First check if this ATAPI command requires a command packet.
	if ( packetSize == 0)						
	{
		return kATANoErr;
	}

	UInt8 status = 0x00;
		
	// While the drive is busy, wait for it to set DRQ.
	// limit the amount of time we will wait for a drive to set DRQ
	// ATA specs imply that all devices should set DRQ within 3ms. 
	// we will allow up to 30ms.
	
	UInt32  breakDRQ = 3;

		
	while ( !waitForU8Status( (mATABusy | mATADataRequest), mATADataRequest)
			&& !checkTimeout()
			&& (breakDRQ != 0)  ) 
	{
		// check for a device abort - not legal under ATA standards,
		// but it could happen		
		status = *_tfAltSDevCReg;
		 //mask the BSY and ERR bits
		status &= (mATABusy | mATAError);

		// look for BSY=0 and ERR = 1
		if( mATAError == status )
		{
			return kATADeviceError;
		}
		
		breakDRQ--;
		IOSleep( 10 );  // allow other threads to run
	 }

	// let the timeout through
	if ( checkTimeout() 
			|| breakDRQ == 0)
	{
		return kATATimeoutErr;
	}
	// write the packet
	UInt32 packetLength = 6;
	
	if( packetSize > 12 )
	{
		packetLength = 8;
	
	}
	
	for( UInt32 i = 0; i < packetLength; i++)
	{
		OSSynchronizeIO();
		* _tfDataReg = *packetData;
		packetData++;	
	}
	
	return  kATANoErr ;

}





/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::softResetBus( bool doATAPI )
{

	IOReturn result = kATANoErr;

	if (doATAPI)
	{	
		// ATAPI resets are directed to a device (0/1) which must be preselected
		// before entering this function.
	
		DLOG("IOATA reset command.\n");
		*_tfStatusCmdReg =  kSOFTRESET;				
		OSSynchronizeIO();
	
	} else {
		
		// begin the ATA soft reset sequence, which affects both devices on the 
		// bus
		
			
		// We will set nIEN bit to 0 to force the IRQ line to be driven by the selected
		// device.  We were seeing a small number of cases where the tristated line
		// caused false interrupts to the host.	

		*_tfAltSDevCReg = mATADCRReset;
		OSSynchronizeIO();						

		// ATA standards only dictate >5us of hold time for the soft reset operation
		// 100 us should be sufficient for any device to notice the soft reset.
		
		IODelay( 100 );
		

		*_tfAltSDevCReg = 0x00;
		OSSynchronizeIO();
		
		DLOG("IOATA soft reset sequenced\n");
	
		// a reset operation has the effect of selecting device 0 as a result.
		// in this case, we will force our host controller to actually execute the 
		// device selection protocol before the next command.
		
		_selectedUnit = kATAInvalidDeviceID;
	
	}

	// ATA-4 and ATA-5 require the host to wait for >2ms 
	// after a sRST before sampling the drive status register.
	IOSleep(50);

	// ATA and ATAPI devices indicate reset completion somewhat differently
	// for ATA, wait for BSY=0 and RDY=1. For ATAPI, wait for BSY=0 only.
	UInt8 readyMask = mATABusy;
	UInt8 readyOn	= 0x00;
	
	if( (_devInfo[0].type == kATADeviceType)
		&& (!doATAPI) )
	{
		readyMask |= mATADriveReady;  //mask-in the busy + ready bit
		readyOn	= mATADriveReady;		// look for a RDY=1 as well as BSY=0
	}

	
	bool resetFailed = true;
	
	// loop for up to 31 seconds following a reset to allow 
	// drives to come on line. Most devices take 50-100ms, a sleeping drive 
	// may need to spin up and touch media to respond. This may take several seconds.
	for( int i = 0; i < 3100; i++)
	{
		
		// read the status register - helps deal with devices which errantly 
		// set interrupt pending states during resets. Reset operations are not 
		// supposed to generate interrupts, but some devices do anyway.
		// interrupt handlers should be prepared to deal with errant interrupts on ATA busses.
		OSSynchronizeIO();
		UInt8 status = *_tfStatusCmdReg;  	
		
		// when drive is ready, break the loop
		if( ( status & readyMask )== readyOn)
		{
			// device reset completed in time
			resetFailed = false;
			break;
		}
		
		IOSleep( 10 );  // sleep thread for another 10 ms
	
	}


	if( resetFailed )
	{
		// it is likely that this hardware is broken. 
		// There's no recovery action if the drive fails 
		// to reset.	
		DLOG("IOATA device failed to reset.\n");	
		result = kATATimeoutErr;
	}
	
	DLOG("IOATA reset complete.\n");

	return result;

}


/*---------------------------------------------------------------------------
 *
 * Subclasses should take necessary action to create DMA channel programs, 
 * for the current memory descriptor in _currentCommand and activate the 
 * the DMA hardware
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::startDMA( void )
{


	DLOG("IOATA Bus controllers that offer DMA must provide implementation/n");

	return kATAModeNotSupported;
}




/*---------------------------------------------------------------------------
 * Subclasses should take all actions necesary to safely shutdown DMA engines
 * in any state of activity, whether finished, pending or stopped. Calling 
 * this function must be harmless reguardless of the state of the engine.
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::stopDMA( void )
{

	DLOG("IOATA Bus controllers that offer DMA must provide implementation/n");

	return kATAModeNotSupported;
}


/*---------------------------------------------------------------------------
//	WaitForU8Status
//	Will wait up to one millisecond for the value in the altStatus register & mask to equal the value
//	passed in. Note that I always use the altStatus register so as not to have the side affect of clearing
//	the interrupt if there is one.
 ---------------------------------------------------------------------------*/
bool		
IOATAController::waitForU8Status (UInt8 mask, UInt8 value)
{
	int	i;
	
	// we will read the status from the alt status register so as not
	// to clear the interrupt accidentally

	for (i=0; i < kStatusDelayLoopMS; i++)
	{
		OSSynchronizeIO();
		
		if ((*_tfAltSDevCReg & mask) == value)
		{
			return true;
		}

		IODelay( kStatusDelayTime );
	}
	return false;																	// time's up
}

/*----------------------------------------------------------------------------------------------------
**	Routine 	ATAPISlaveExists
**
**	Purpose:   Determines whether an ATAPI device seen as a "slave" of a master ATAPI device
**			   is actually present, or the product of the master shadowing a not-present slave's registers
**    		   Call this function when the master device shows EBh 14h, and the slave also shows the ATAPI
**    		   protocol signature.
**	Returns:   False if a device is ruled out. True if a device is verified. Leaves device in a ready state,
** 			   But no longer showing signatures. 

    NOTE:     Device 1 (slave) is assumed already selected.
*/


bool 
IOATAController::ATAPISlaveExists( void )
{
	UInt8						scratchByte;
	UInt16						scratchWord;
	UInt32						dataCounter;
	UInt32						loopCounter;

	// The only option is to issue a command and see what happens.
	OSSynchronizeIO();		
	*_tfAltSDevCReg = 0x02; // disable interrupts

	//issue INDENTIFY PACKET DEVICE 
	OSSynchronizeIO();	
	*_tfStatusCmdReg = 0xA1;  
	
	// reading and disreguarding a register provides the required 400ns delay time.
	OSSynchronizeIO();	
	scratchByte = *_tfAltSDevCReg;

	OSSynchronizeIO();	
	scratchByte = *_tfAltSDevCReg;
	
	// if the device returns status 00h, we declare it not present. A real device would probably be 
	// status BSY (80h) now. An incredibly fast device might be ready to move data and show DRQ.
	// However, by ATA standards, a not present device is required to return 00h.
	// Lucky break, no device and we figured it out in a hurry.
	
	if( scratchByte == 0x00 )
	{
		// enable device interrupt
		*_tfAltSDevCReg = 0x00;
		OSSynchronizeIO();
		return false;
	}
	
	// OK we probably have a device now. We have to wait for drive to send data, and read it and clear it.
	// It is possible that the a misbehaving master has decided to respond to the command. So, we'll
	// break on error bit and say it's not a real slave should that happen.
	
	// take a leisurely approach, this will take a while.

	// give the device up to 10 seconds to respond with data.
	for( loopCounter = 0; loopCounter < 10000; loopCounter++)   
	{		
		OSSynchronizeIO();
		scratchByte =  *_tfAltSDevCReg;
		
		// If drive sets error, clear status and return false. It's probably a misbehaving master
		if( scratchByte & 0x01 )
			break;		
				
		// this means the drive is really there. Clear the data and return true.
		if( (scratchByte & 0x58) == 0x58)  // RDY=1  DRQ=1
		{
			OSSynchronizeIO();
			scratchByte = *_tfStatusCmdReg; // clear pending interrupt state
			
			for( dataCounter = 0; dataCounter < 256; dataCounter++ )
			{
				OSSynchronizeIO();
				scratchWord = *_tfDataReg;
			}				
			// enable device interrupt
			*_tfAltSDevCReg = 0x00;
			OSSynchronizeIO();
			return true;		
		}
		
		// OK, sleep for 10 ms and try again.
		IOSleep(10);			
	}

	// In the ugly case, a drive set BSY, and didn't respond within 10 seconds with data.
	// Otherwise, this is the for loop terminating on seeing the error bit.
	// We'll read status and return false.
	
	OSSynchronizeIO();
	scratchByte = *_tfStatusCmdReg; // clear pending interrupt state	

	// enable device interrupt
	*_tfAltSDevCReg = 0x00;
	OSSynchronizeIO();

	return false;

}



/*---------------------------------------------------------------------------
 *	scan the bus to see if devices are attached. The assumption is that the 
 *  devices are in a cleanly-reset state, showing their protocol signatures, 
 *  and the bus is properly wired with a pull down resistor on DD:7.
 *	If your bus controller does not meet these conditions, you should override 
 *	and supply your own function which meets your specific hardware needs.
 *	Your controller may or may not require a reset, or it may require more 
 *  thorough scanning, or additional configuration prior to looking for drives,
 *	or it may aquire information from firmware indicating the devices attached.
 *	This function should be self contained and not rely upon work loop or 
 * 	or anything other than the register pointers being setup and enabled for access
 ---------------------------------------------------------------------------*/

UInt32 
IOATAController::scanForDrives( void )
{
	UInt32 unitsFound = 0;
	UInt8 status = 0x00;
	// count total time spent searching max time allowed = 31 secs
	// it RARELY takes this long.
	UInt32 milsSpent = 0; 
	
	// wait for a not busy bus
	// should be ready, but some devices may be slow to wake or spin up.
	for( int loopMils = 0; milsSpent < 3100; loopMils++ )
	{
		OSSynchronizeIO();
		status = *_tfStatusCmdReg;
		if( (status & mATABusy) == 0x00 )
			break;
		
		IOSleep( 10 );	
		milsSpent++;
	}

	// spun on BSY for too long, declare bus empty
	if( ! (milsSpent < 3100) )
		goto AllDone;
		
	
	// select each possible device on the bus, wait for BSY- 
	// then check for protocol signatures.	

	for( int unit = 0; unit < 2; unit++ )
	{

		// wait for a not busy bus
		for( int loopMils = 0; milsSpent < 3100; loopMils++ )
		{
			// write the selection bit
			OSSynchronizeIO();
			*_tfSDHReg	= ( unit << 4 );
			IODelay( 10 );
			// typically, devices respond quickly to selection
			// but we'll give it a chance in case it is slow for some reason.
			status = *_tfStatusCmdReg;
			if( (status & mATABusy) == 0x00 )
			{	
				break;	
			}
			
			IOSleep( 10 );	
			milsSpent++;
		}

		// spun on BSY too long, probably bad device
		if( ! (milsSpent < 3100) )
			goto AllDone;

		// check for ATAPI device signature first
		if ( ( *_tfCylLoReg == 0x14) && ( *_tfCylHiReg == 0xEB) )
		{	
			if(    (unit == 1 )
				&& ( _devInfo[0].type == kATAPIDeviceType )  )
			{

			// OK we've met the condition for an indeterminate bus, master is atapi and we see a slave atapi
			// signature. This is legal ATA, though we are fortunate enough that most devices don't do this.

				if( ATAPISlaveExists( ) != true )
				{
					_devInfo[unit].type = kUnknownATADeviceType;
					goto AllDone;
					
				} 

			} 

			 _devInfo[unit].type = kATAPIDeviceType;
			 _devInfo[unit].packetSend = kATAPIDRQFast;  // this is the safest default setting
			unitsFound++;

		} // check for ATA signature, including status RDY=1 and ERR=0
		else if ( (*_tfCylLoReg == 0x00) && (*_tfCylHiReg == 0x00) &&
				  (*_tfSCountReg == 0x01) && (*_tfSectorNReg == 0x01) &&
				  ( (*_tfAltSDevCReg & 0x51) == 0x50) )
		{

			 _devInfo[unit].type = kATADeviceType;
			 _devInfo[unit].packetSend = kATAPIUnknown;  
			unitsFound++;
			
		}else{

			_devInfo[unit].type = kUnknownATADeviceType;
			_devInfo[unit].packetSend = kATAPIUnknown;  
		}

	}


AllDone:

	// reselect device 0
	*_tfSDHReg	= 0x00;
	// enable device interrupts
	*_tfAltSDevCReg = 0x00;
	OSSynchronizeIO();

	// enforce ATA device selection protocol
	// before issuing the next command.
	_selectedUnit = kATAInvalidDeviceID;
	
	return unitsFound;

}



/*____________________________________________________________________________
	Name:		txDataIn
	Function:	Reads data in from the data register of a selected device to 
				the specified buffer.  It is assumed that the DRQ bit has been 
				checked in the status register.  All data transfers are in 16-bit
				words.
	Output:		The buffer contains the number of bytes specified by length
______________________________________________________________________________*/

IOReturn 
IOATAController::txDataIn (IOLogicalAddress buf, IOByteCount length)
{
	register UInt16		*buf16 = (UInt16*)buf;

	// on reads, we expect an interrupt after we send the data except on the last block.
	// in the case of the last block, we will clear this bit when we return to the 
	// AsyncData call
	while (length >= 32)						// read in groups of 16 words at a time
	{
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO(); 
		*buf16++ = *_tfDataReg;
		length -= 32;							// update the length count
		OSSynchronizeIO();	
	}

	while (length >= 2)
	{
		*buf16++ = *_tfDataReg;
		OSSynchronizeIO();									
		length -= 2;							// update the length count
	}

	UInt8* buf8 = (UInt8*)buf16;
	
	if (length)									// This is needed to handle odd byte transfer
	{
		*buf8 = *(IOATARegPtr8Cast(_tfDataReg));
		OSSynchronizeIO();									
		length--;								
	}
	
	return kATANoErr;

}



/*____________________________________________________________________________
	Name:		txDataOut
	Function:	Writes data out to the data register of a selected device from 
				the specified buffer.  It is assumed that the DRQ bit has been 
				checked in the status register.  All transfers are in groups of 
				16-bit words.
______________________________________________________________________________*/

IOReturn 
IOATAController::txDataOut(IOLogicalAddress buf, IOByteCount length)
{
	register UInt16		*buf16 = (UInt16*)buf;

	while (length >= 32)						// write in groups of 16 words at a time
	{
		*_tfDataReg = *buf16++;  
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		length -= 32;							
	}
	while (length >= 2)
	{
		*_tfDataReg = *buf16++;
		OSSynchronizeIO(); 
		length -= 2;								
	}
	
	// Odd byte counts aren't really good on ATA, but we'll do it anyway.
	UInt8* buf8 = (UInt8*)buf16;

	if (length)									
	{
		*(IOATARegPtr8Cast(_tfDataReg)) = *buf8;
		OSSynchronizeIO(); 
		length--;	
	}
	
	return kATANoErr;
}


/*____________________________________________________________________________
	Name:		ATAPIByteCountRegistersRead()
	Function:	This function reads both the low and high byte count registers 
				of the ATAPI task file and returns the read result to the caller.  

	Input:		none
	Output:		An UInt16 value from both the low and high byte count registers
	
______________________________________________________________________________*/

IOByteCount	
IOATAController::readATAPIByteCount (void)
{
	UInt16	ByteCountValue;
	
	ByteCountValue = (*_tfCylHiReg) << 8;
	OSSynchronizeIO();
	ByteCountValue += *_tfCylLoReg;
	OSSynchronizeIO();
	
	return (IOByteCount) ByteCountValue;
}


//----------------------------------------------------------------------------------------
//	FUNCTION:		DetermineATAPIState()
//	Description:	This function determines the next state based on the Interrupt reason
//					register of the device.  If the command hasn't been initiated, the 
//					function returns kATAStarted state.
//					<Current State>		<Interrupt Reason>	<Next State>
//						kATAStarted			Don't care		kATAStarted
//						Don't care			0x00			kATADataTx (Write)
//						Don't care			0x01			kATAPICmd
//						Don't care			0x02			kATADataTx (Read)
//						Don't care			0x03			kATAStatus (or Message - future)
//
//	Input:			Manager parameter block pointer: ATA_PB*
//					Base address pointer: ATAtaskfile*
//	Output:			Next state value: UInt16
//----------------------------------------------------------------------------------------
IOATAController::transState	
IOATAController::determineATAPIState(void)
{
	IOATAController::transState			nextPhase;
	const IOATAController::transState	NextState[4] = {kATADataTx, kATAPICmd, 
														kATADataTx, kATAStatus};
	
	if (_currentCommand->state == kATAStarted)	// the command hasn't started yet
		return (kATAStarted);					//   return the same state

	/* Generate next phase from interrupt reason */
	nextPhase = NextState[ *_tfSCountReg & 0x03];
	
	return nextPhase;
}


/*____________________________________________________________________________
	Name:		handleOverrun
	Function:	This function provides handling of overrun data.  This condition
				will occur if the device indicates more data than the host expects
				asked for.
	Input:		IOByteCount length = number of bytes to do dummy read/write on
				device data register.
	Output:		None
______________________________________________________________________________*/
void			
IOATAController::handleOverrun( IOByteCount length)
{
	UInt16 dummy = 0;
	
	if( _currentCommand->getFlags() & mATAFlagIORead )
	{
	
		while (length >= 2)
		{
			dummy = *_tfDataReg;
			OSSynchronizeIO();							
			length -= 2;								
		}

		while (length > 0)
		{
			dummy = *_tfDataReg;
			OSSynchronizeIO();							
			length--;	
		}

	} else {  // write
	
	
		while (length >= 2)
		{
			*_tfDataReg = dummy;
			OSSynchronizeIO();							
			length -= 2;								
		}

		while (length > 0)
		{
			*_tfDataReg = dummy;
			OSSynchronizeIO();							
			length--;	
		}

	
	
	}
}

UInt16 
IOATAController::readExtRegister( IOATARegPtr8 inRegister )
{
	// read in the lsb of the 16 bit fifo register
	UInt16 result = (*inRegister);
	OSSynchronizeIO();
	// select the HOB bit in the dev ctrl register
	*_tfAltSDevCReg = 0x80;
	OSSynchronizeIO();
	result |= ((UInt16) (*inRegister)) << 8;
	OSSynchronizeIO();
	*_tfAltSDevCReg = 0x00;
	OSSynchronizeIO();
	return result;
}

void 
IOATAController::writeExtRegister( IOATARegPtr8 inRegister, UInt16 inValue )
{
	// read in the lsb of the 16 bit fifo register
	*inRegister = (UInt8)((inValue & 0xFF00) >> 8);
	OSSynchronizeIO();
	*inRegister = (UInt8) (inValue & 0xFF);
	OSSynchronizeIO();
}


/*---------------------------------------------------------------------------
 *
 *	registerAccess read or write the TF registers as indicated by the mask 
 *	in the current command.
 *	input: bool isWrite = true means write the register(s), false = read
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOATAController::registerAccess(bool isWrite)
{
	UInt32	RegAccessMask = _currentCommand->getRegMask();
	IOReturn	err = kATANoErr;
	bool isExtLBA =  _currentCommand->getFlags() & mATAFlag48BitLBA;
	IOExtendedLBA* extLBA = _currentCommand->getExtendedLBA();
	
		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATAErrFeaturesValid)				// error/features register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				writeExtRegister(_tfFeatureReg,extLBA->getFeatures16());
			
			} else {

				*_tfFeatureReg	= _currentCommand->getErrorReg();
			}
		}else{
		
			if(isExtLBA )
			{
				extLBA->setFeatures16( readExtRegister(_tfFeatureReg));
			
			} else {
		
				_currentCommand->setFeatures( *_tfFeatureReg) ;
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATASectorCntValid)					// sector count register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				writeExtRegister( _tfSCountReg, extLBA->getSectorCount16());
			
			} else {

				*_tfSCountReg = _currentCommand->getSectorCount();
			
			}
		}else{

			if(isExtLBA )
			{
				extLBA->setSectorCount16( readExtRegister(_tfSCountReg) );
			
			} else {
				_currentCommand->setSectorCount( *_tfSCountReg );
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATASectorNumValid)					// sector number register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				writeExtRegister( _tfSectorNReg, extLBA->getLBALow16());
			
			} else {
				*_tfSectorNReg	= _currentCommand->getSectorNumber();
			}
			
		}else{
		
			if(isExtLBA )
			{
				extLBA->setLBALow16(readExtRegister(_tfSectorNReg));
			
			} else {
				_currentCommand->setSectorNumber( *_tfSectorNReg );
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATACylinderLoValid)				// cylinder low register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				writeExtRegister( _tfCylLoReg, extLBA->getLBAMid16());
			
			} else {

				*_tfCylLoReg	= _currentCommand->getCylLo();
			}

		}else{
		
			if(isExtLBA )
			{
				extLBA->setLBAMid16(readExtRegister(_tfCylLoReg));
			
			} else {
				_currentCommand->setCylLo( *_tfCylLoReg );
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATACylinderHiValid)				// cylinder high register
	{
		if(isWrite)
		{
			if(isExtLBA )
			{
				writeExtRegister( _tfCylHiReg, extLBA->getLBAHigh16());
			
			} else {

				*_tfCylHiReg	= _currentCommand->getCylHi();
			}
		}else{
		
			if(isExtLBA )
			{
				extLBA->setLBAHigh16(readExtRegister(_tfCylHiReg));
			
			} else {
				_currentCommand->setCylHi( *_tfCylHiReg );
			}
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATASDHValid)						// ataTFSDH register
	{
		if(isWrite)
		{
			*_tfSDHReg	= _currentCommand->getDevice_Head();
		}else{
			_currentCommand->setDevice_Head( *_tfSDHReg );
		}
	}

	
		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATAAltSDevCValid)					// alternate status/device control register
	{
		if(isWrite)
		{
			*_tfAltSDevCReg	= _currentCommand->getAltStatus();
		}else{
			_currentCommand->setControl( *_tfAltSDevCReg );
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATADataValid)						// data register...
	{
		if(isWrite)
		{
			*_tfDataReg	= _currentCommand->getDataReg();
		
		}else{
		
			_currentCommand->setDataReg( *_tfDataReg );
		}
	}

		/////////////////////////////////////////////////////////////////////////
	if (RegAccessMask & mATAStatusCmdValid)					// status/command register
			{
		if(isWrite)
		{
			*_tfStatusCmdReg	= _currentCommand->getStatus();
		}else{
			_currentCommand->setCommand(*_tfStatusCmdReg );
		}
	}

	return err;




}

#pragma mark -misc functions-

/*---------------------------------------------------------------------------
 *
// perform 2-byte endian swap. Only useful on PIO transfers and identify data
// 
 ---------------------------------------------------------------------------*/
void 
IOATAController::swapBytes16( UInt8* dataBuffer, IOByteCount length)
{

	IOByteCount	i;
	UInt8	c;
	unsigned char* 	firstBytePtr;
	
	for (i = 0; i < length; i+=2)
	{
		firstBytePtr = dataBuffer;				// save pointer
		c = *dataBuffer++;						// Save Byte0, point to Byte1
		*firstBytePtr = *dataBuffer;			// Byte0 = Byte1
		*dataBuffer++= c;						// Byte1 = Byte0
	}
	


}


/*****************************************************************************
**  Function bitSigToNumeric
**	This function converts a bit-significant value into an integer which
**	corresponds to the highest-order bit which is active. For example,
**	0x0035 is converted to 5, which is the bit-number of the high bit of 0x0035.
**  Input variable binary = zero is technically illegal; the routine does not check
**  explicitly for this value, but rather returns 0xFFFF as a result of no
**  bit being found, and the return value is decemented below zero. The
**  loop terminates when i becomes zero because of binary = 0.
**	Implicit maximum value of binary: 0x00FF.
**
** Explicit Inputs:
**	binary - a non-zero binary number (0 has no corresponding bit number)
** Return Value:
**	integer - the bit-number of the highest bit active in binary.
**
******************************************************************************/

UInt16 
IOATAController::bitSigToNumeric(UInt16 binary)
{
	UInt16  i, integer;

	/* Test all bits from left to right, terminating at the first non-zero bit. */
	for (i = 0x0080, integer = 7; ((i & binary) == 0 && i != 0) ; i >>= 1, integer-- )
	{;}
	return (integer);
}	/* end BitSigToNumeric() */

