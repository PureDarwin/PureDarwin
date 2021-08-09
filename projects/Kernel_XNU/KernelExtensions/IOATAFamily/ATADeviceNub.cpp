/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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
 *	ATADeviceNub.cpp
 *
 */

 
#include <IOKit/IOTypes.h>
#include "IOATATypes.h"
#include "IOATADevice.h"
#include "IOATAController.h"
#include "ATADeviceNub.h"
#include "IOATADevConfig.h"

#include <IOKit/IOSyncer.h>

enum{

	kDoIDDataComplete,
	kDoSetFeatureComplete
};


struct completionInfo{

	UInt32 whatToDo;
	IOSyncer* sync;	

};

#ifdef DLOG
#undef DLOG
#endif

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


#define kIDBufferBytes 512

//---------------------------------------------------------------------------

#define super IOATADevice

OSDefineMetaClassAndStructors(   ATADeviceNub, IOATADevice )
    OSMetaClassDefineReservedUnused(ATADeviceNub, 0);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 1);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 2);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 3);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 4);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 5);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 6);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 7);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 8);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 9);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 10);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 11);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 12);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 13);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 14);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 15);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 16);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 17);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 18);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 19);
    OSMetaClassDefineReservedUnused(ATADeviceNub, 20);

//---------------------------------------------------------------------------



// static creator function - used by IOATAControllers to create nubs.
ATADeviceNub* 
ATADeviceNub::ataDeviceNub( IOATAController* provider, ataUnitID unit, ataDeviceType devType)
{

	ATADeviceNub*  nub = new ATADeviceNub;

	if( ! nub )
		return 0L;
	
	if( !nub->init( provider, unit, devType) )
	{
			nub->release();
			return 0L;
	}	
	return nub;

}


//---------------------------------------------------------------------------

bool 
ATADeviceNub::init(IOATAController* provider, ataUnitID unit, ataDeviceType devType)
{

	if( !super::init( (OSDictionary*) 0L) )
		return false;

	_provider = provider;
	_unitNumber = unit;
	_deviceType = devType;

	// allocate a buffer for the identify info from the device	
	buffer = (UInt8*) IOMalloc( kIDBufferBytes );
	
	if( !buffer )
		return false;
	
	IOReturn err = kATANoErr;
	
	// issue the identify command so we can get the vendor strings
	err = getDeviceID();

	if( err )
	{
		DLOG("ATADeviceNub failed identify device %ld\n", (long int) err);

		IOFree( buffer, kIDBufferBytes);	
		return false;	
	}
	
	publishProperties();
	publishBusProperties();
	publishVendorProperties();

	IOFree( buffer, kIDBufferBytes);	
	buffer = 0L;
		
	return true;

}



//---------------------------------------------------------------------------
 

//---------------------------------------------------------------------------
bool
ATADeviceNub::attach(IOService* provider )
{

	IOATAController* controller = OSDynamicCast( IOATAController, provider);
	
	if( !controller )
	{
		DLOG("ATANub: Provider not IOATAController\n");
		return false;
	}


	if( !super::attach( provider) )
		return false;

		
	return true;

}



//---------------------------------------------------------------------------

// create and destroy IOATACommands
//---------------------------------------------------------------------------

IOATACommand*	
ATADeviceNub::allocCommand( void )
{

	IOATABusCommand64* cmd = IOATABusCommand64::allocateCmd32();
	
	return (IOATACommand*) cmd;

}
	
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void
ATADeviceNub::freeCommand( IOATACommand* inCommand)
{
	
	inCommand->release();
}




//---------------------------------------------------------------------------


//---------------------------------------------------------------------------

// Submit IO requests 
IOReturn		
ATADeviceNub::executeCommand(IOATACommand* command)
{

	IOSyncer* mySync = 0L;
	IOATABusCommand* cmd = OSDynamicCast( IOATABusCommand, command);
	
	if( !cmd )
		return -1;
		
	if( cmd->getCallbackPtr() == 0L)
	{
		mySync = IOSyncer::create();
		cmd->syncer = mySync;
	
	}

	IOReturn err = _provider->executeCommand( this, cmd);	

	if( mySync )
	{
		mySync->wait();
		err = cmd->getResult();
	}
	
	return err;	

}



/*---------------------------------------------------------------------------




---------------------------------------------------------------------------*/
IOReturn 
ATADeviceNub::getDeviceID( void )
{
	OSString* string;

	IOMemoryDescriptor* desc = IOMemoryDescriptor::withAddress((void *) buffer,
                                            kIDBufferBytes,
                                            kIODirectionIn);

	if( !desc )
	{
		
 		string = OSString::withCString( "failed" );
		setProperty( "Alloc descriptor", (OSObject *)string );
	 	string->release();
	 	return -1;	
	
	}

	IOATABusCommand* cmd = (IOATABusCommand*) allocCommand();
	
	if(!cmd)
	{
		
 		string = OSString::withCString( "failed" );
		setProperty( "Alloc command", (OSObject *)string );
	 	string->release();
		return -1;
	}	
	
	
	// tell the bus what to do, what unit and how long to allow
	cmd->setOpcode( kATAFnExecIO);
	cmd->setFlags(mATAFlagIORead);
	cmd->setUnit( _unitNumber  );
	cmd->setTimeoutMS( 30000);

	// setup the buffer for the data
	cmd->setBuffer ( desc);
	cmd->setPosition ((IOByteCount) 0);
	cmd->setByteCount ((IOByteCount) kIDBufferBytes);


	// setup the actual taskfile params for the device
	// only two parameters are needed, the device bit for the unit
	// and the actual command for the device to execute
	cmd->setDevice_Head( ((UInt8)_unitNumber) << 4);

	if(_deviceType == kATADeviceType)
	{
		cmd->setCommand ( kATAcmdDriveIdentify );
	
	} else {
	
		cmd->setCommand ( 0xA1 );  // packet identify	
	}
	
	// set up a call back pointer for the command to complete. 
	// the IOATAController only allows async commands
	
	cmd->setCallbackPtr ( (IOATACompletionFunction*) MyATACallback);

	
	// set the refCon so the callback knows what to do.
	completionInfo* completion = (completionInfo*)IOMalloc(sizeof(completionInfo));
	completion->whatToDo = 	kDoIDDataComplete;
	completion->sync = IOSyncer::create();
	cmd->refCon = (void*) completion;
	cmd->refCon2 = (void*) this;
	
	desc->prepare(kIODirectionIn);
	// tell the bus to exec the command
	DLOG("Sending ID command to bus controller\n");	
	IOReturn err =	executeCommand( cmd);	
	DLOG("Command returned error = %ld\n",(long int)err );
	if(!err)
	{
		completion->sync->wait();
	}
	
	desc->complete( kIODirectionIn );
		
	IOFree( completion, sizeof(completionInfo));

	if( cmd->getResult() )
	{
		err = cmd->getResult();
	}
	
	freeCommand(cmd);
	
#if defined(__BIG_ENDIAN__)
// The identify device info needs to be byte-swapped on ppc (big-endian) 
// systems becuase it is data that is produced by the drive, read across a 
// 16-bit little-endian PCI interface, directly into a big-endian system.
// Regular data doesn't need to be byte-swapped because it is written and 
// read from the host and is intrinsically byte-order correct.	
		swapBytes16( buffer, kIDBufferBytes);
#else /* __LITTLE_ENDIAN__ */
    // Swap the strings in the identify data.
    swapBytes16( &buffer[46], 8);   // Firmware revision
    swapBytes16( &buffer[54], 40);  // Model number
    swapBytes16( &buffer[20], 20);  // Serial number
#endif

	return err;
	
	// the 512 byte buffer should contain the correctly byte-ordered
	// raw identity info from the device at this point. 
}



//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

void 
ATADeviceNub::publishBusProperties( void )
{

	OSString* string;
//	OSNumber* number;

 	// get some bus info
 	
 	IOATABusInfo* theInfo = IOATABusInfo::atabusinfo();
 	if( !theInfo )
	{
		DLOG("ATANub IOATABusInfo alloc fail\n");

 		return;
 	}
 	
 	if(_provider->provideBusInfo( theInfo ))
 	{
 		// blow it off on error
		DLOG("ATANub provide info failed\n");
 		theInfo->release();
 		return;	
 	}
 	
 	switch( theInfo->getSocketType() )
 	{
 		case kInternalATASocket:
	 		string = OSString::withCString( kATAInternalSocketString );
 		break;
	
		case kMediaBaySocket:
 			string = OSString::withCString( kATAMediaBaySocketString );
 		break;
	
		case kPCCardSocket:
 			string = OSString::withCString( kATAPCCardSocketString );	
		break;

		case kInternalSATA:
 			string = OSString::withCString( kATAInternalSATAString );	
		break;

		case kSATABay:
 			string = OSString::withCString( kATASATABayString );	
		break;
		
		case kInternalSATA2:
 			string = OSString::withCString( kATAInternalSATA2 );	
		break;
		
		
		case kSATA2Bay:
 			string = OSString::withCString( kATASATA2BayString );	
		break;

	
 		default:
 			string = OSString::withCString( kATAUnkownSocketString );
		break;
	
 	}
 	
  	setProperty( kATASocketKey, (OSObject *)string );
 	string->release();


	// these properties may be published in the future 
	// if conditions warrant
/*	
	number = OSNumber::withNumber( theInfo->getPIOModes(), 32 );
	setProperty( "piomode bitmap", (OSObject *) number);
 	number->release();

	number = OSNumber::withNumber( theInfo->getDMAModes(), 32 );
	setProperty( "dmamode bitmap", (OSObject *) number);
 	number->release();

	number = OSNumber::withNumber( theInfo->getUltraModes(), 32 );
	setProperty( "ultramode bitmap", (OSObject *) number);
 	number->release();

	number = OSNumber::withNumber( theInfo->getUnits(), 32 );
	setProperty( "units on bus", (OSObject *) number);
 	number->release();
*/


	// these properties may be published in the future for support of advanced ATA modes.
/*	setProperty( "DMA supported", theInfo->supportsDMA());
	setProperty( "48-bit LBA supported", theInfo->supportsExtendedLBA());
	setProperty( "command overlap supported", theInfo->supportsOverlapped());
	setProperty( "DMA-Queued supported", theInfo->supportsDMAQueued());
*/

 theInfo->release();

}


//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void 
ATADeviceNub::publishProperties( void )
{

		
	OSString* string;
	
	switch( _deviceType )
	{
		case kATADeviceType:
			string = OSString::withCString( kATATypeATAString );
		break;
	
		case kATAPIDeviceType:
			string = OSString::withCString( kATATypeATAPIString );
		break;
	
		default:
			string = OSString::withCString( kATATypeUnknownString );
		break;	
	}

 	setProperty( kATADevPropertyKey, (OSObject *)string );
 	string->release();
 	
 	
	OSNumber* number = OSNumber::withNumber( _unitNumber, 32 );

	setProperty( kATAUnitNumberKey, (OSObject *) number);
	setProperty( "IOUnit", (OSObject *) number);
 	number->release();
	
	if( _unitNumber == 0 )
	{
		setLocation("0");
	
	} else {
	
		setLocation("1");
	}
	
 

}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void
ATADeviceNub::publishVendorProperties(void)
{
	
	if( IOATADevConfig::sDriveSupports48BitLBA( ( const UInt16*) buffer ) )
	{
		UInt32 upperLBA, lowerLBA;
		IOATADevConfig::sDriveExtendedLBASize(   &upperLBA, &lowerLBA, ( const UInt16*) buffer );
		UInt64 largeLBASize = 0;
		
		largeLBASize = ( ((UInt64) upperLBA) << 32) | ((UInt64) lowerLBA );
		
		OSNumber* extendedCapacity = OSNumber::withNumber( largeLBASize, 64 );
		setProperty( "extended LBA capacity", (OSObject *) extendedCapacity);
		extendedCapacity->release();
	
	}
	// terminate the strings with 0's
	// this changes the identify data, so we MUST do this part last.
	buffer[94] = 0;
	buffer[40] = 0;

	// Model number runs from byte 54 to 93 inclusive - byte 94 is set to 
	// zero to terminate that string
	OSString* modelNum = OSString::withCString((const char*) &buffer[54]);

	// now that we have made a deep copy of the model string, poke a 0 into byte 54 
	// in order to terminate the fw-vers string which runs from bytes 46 to 53 inclusive.
	buffer[54] = 0;
	
	OSString* firmVers = OSString::withCString((const char*) &buffer[46]);

	// serial number runs from byte 20 to byte 39 inclusive and byte 40 has been terminated with a null
	OSString* serial = OSString::withCString( (const char*) &buffer[20]);	
	
 	setProperty( kATAVendorPropertyKey, (OSObject *)modelNum );
 	setProperty( kATARevisionPropertyKey, (OSObject *)firmVers );
 	setProperty( kATASerialNumPropertyKey, (OSObject *)serial );


	serial->release();
	modelNum->release();
	firmVers->release();

}


//---------------------------------------------------------------------------

//---------------------------------------------------------------------------



void
ATADeviceNub::MyATACallback(IOATACommand* command )
{
	if( command->getResult() )
	{
	
		DLOG("Command result error = %ld\n",(long int)command->getResult() );
	
	}


	ATADeviceNub* self = (ATADeviceNub*) command->refCon2;
	
	self->processCallback( command );



}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

void
ATADeviceNub::processCallback(IOATACommand* command )
{
	completionInfo* completer = (completionInfo*) command->refCon;

	switch( completer->whatToDo )
	{
		case  kDoIDDataComplete:
		
			completer->sync->signal();
		break;
		
		
		
		// do nothing on set features.
		case kDoSetFeatureComplete:
		
			completer->sync->signal();
		default:
		break;
		
	}// end switch	

}

//---------------------------------------------------------------------------
void 
ATADeviceNub::swapBytes16( UInt8* dataBuffer, IOByteCount length)
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




