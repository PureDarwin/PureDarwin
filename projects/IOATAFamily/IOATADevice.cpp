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
 *	IOATADevice.cpp
 *
 */
#include <IOKit/IOTypes.h>
#include "IOATATypes.h"
#include "IOATADevice.h"
#include "IOATAController.h"

#ifdef DLOG
#undef DLOG
#endif

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


//---------------------------------------------------------------------------

#define super IOService

OSDefineMetaClass( IOATADevice, IOService )
OSDefineAbstractStructors( IOATADevice, IOService )
    OSMetaClassDefineReservedUnused(IOATADevice, 0);
    OSMetaClassDefineReservedUnused(IOATADevice, 1);
    OSMetaClassDefineReservedUnused(IOATADevice, 2);
    OSMetaClassDefineReservedUnused(IOATADevice, 3);
    OSMetaClassDefineReservedUnused(IOATADevice, 4);
    OSMetaClassDefineReservedUnused(IOATADevice, 5);
    OSMetaClassDefineReservedUnused(IOATADevice, 6);
    OSMetaClassDefineReservedUnused(IOATADevice, 7);
    OSMetaClassDefineReservedUnused(IOATADevice, 8);
    OSMetaClassDefineReservedUnused(IOATADevice, 9);
    OSMetaClassDefineReservedUnused(IOATADevice, 10);
    OSMetaClassDefineReservedUnused(IOATADevice, 11);
    OSMetaClassDefineReservedUnused(IOATADevice, 12);
    OSMetaClassDefineReservedUnused(IOATADevice, 13);
    OSMetaClassDefineReservedUnused(IOATADevice, 14);
    OSMetaClassDefineReservedUnused(IOATADevice, 15);
    OSMetaClassDefineReservedUnused(IOATADevice, 16);
    OSMetaClassDefineReservedUnused(IOATADevice, 17);
    OSMetaClassDefineReservedUnused(IOATADevice, 18);
    OSMetaClassDefineReservedUnused(IOATADevice, 19);
    OSMetaClassDefineReservedUnused(IOATADevice, 20);    
//---------------------------------------------------------------------------

// Determine whether this device is number 0 or 1 (ie, master/slave)
ataUnitID	
IOATADevice::getUnitID( void )
{

	return _unitNumber;

}

// Find out what kind of device this nub is (ata or atapi)
ataDeviceType 
IOATADevice::getDeviceType( void )
{

	return _deviceType;


}

// Find out the bus capability so the client can choose the features to set and commands to run.
IOReturn 
IOATADevice::provideBusInfo( IOATABusInfo* getInfo)
{

	if( !_provider )
		return -1;

	return _provider->provideBusInfo(getInfo);

}

// Tell the bus what speed to use for your device.
IOReturn 
IOATADevice::selectConfig( IOATADevConfig* configRequest)
{

	return _provider->selectConfig( configRequest, _unitNumber);

}

// Find out what speed the bus has configured for this unit. 
IOReturn 
IOATADevice::provideConfig( IOATADevConfig* configRequest)
{

	return _provider->getConfig( configRequest, _unitNumber);

} 

// Submit IO requests 
IOReturn		
IOATADevice::executeCommand(IOATACommand* command)
{

	// subclass must implement

	return -1;


}
 
// create and destroy IOATACommands
IOATACommand*	
IOATADevice::allocCommand( void )
{

	// subclass must provide implementation.

	return 0L;

}
	

void
IOATADevice::freeCommand( IOATACommand* inCommand)
{

	// subclass must provide implementation.
	
}


//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void	
IOATADevice::notifyEvent( UInt32 event )
{

	messageClients( event ); 

}


/// appearantly needed for matching somehow.
bool 
IOATADevice::matchPropertyTable(OSDictionary * table)
{

	bool	result;
	
	result = compareProperty ( table, "IOUnit" );
    
    return result;
    
}

bool 
IOATADevice::matchPropertyTable(OSDictionary * table, SInt32 * score )
{
	
	bool	result;
	
	result = compareProperty ( table, kATADevPropertyKey );
	
	if ( result == false )
	{
		*score = 0;
	}
	
    return result;
	
}


IOService *
IOATADevice::matchLocation(IOService * client)
{
    return this;
}
