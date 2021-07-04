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
 *	IOATABusInfo.cpp
 *
 */


#include<IOKit/IOTypes.h>
#include"IOATATypes.h"
#include"IOATABusInfo.h"

#ifdef DLOG
#undef DLOG
#endif

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


//---------------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors(   IOATABusInfo, OSObject )

OSMetaClassDefineReservedUnused(IOATABusInfo, 0);
OSMetaClassDefineReservedUnused(IOATABusInfo, 1);
OSMetaClassDefineReservedUnused(IOATABusInfo, 2);
OSMetaClassDefineReservedUnused(IOATABusInfo, 3);
OSMetaClassDefineReservedUnused(IOATABusInfo, 4);
OSMetaClassDefineReservedUnused(IOATABusInfo, 5);
OSMetaClassDefineReservedUnused(IOATABusInfo, 6);
OSMetaClassDefineReservedUnused(IOATABusInfo, 7);
OSMetaClassDefineReservedUnused(IOATABusInfo, 8);
OSMetaClassDefineReservedUnused(IOATABusInfo, 9);
OSMetaClassDefineReservedUnused(IOATABusInfo, 10);
OSMetaClassDefineReservedUnused(IOATABusInfo, 11);
OSMetaClassDefineReservedUnused(IOATABusInfo, 12);
OSMetaClassDefineReservedUnused(IOATABusInfo, 13);
OSMetaClassDefineReservedUnused(IOATABusInfo, 14);
OSMetaClassDefineReservedUnused(IOATABusInfo, 15);
OSMetaClassDefineReservedUnused(IOATABusInfo, 16);
OSMetaClassDefineReservedUnused(IOATABusInfo, 17);
OSMetaClassDefineReservedUnused(IOATABusInfo, 18);
OSMetaClassDefineReservedUnused(IOATABusInfo, 19);
OSMetaClassDefineReservedUnused(IOATABusInfo, 20);


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
IOATABusInfo*
IOATABusInfo::atabusinfo(void)
{
	IOATABusInfo* info = new IOATABusInfo;
	
	if( ! info )
		return 0L;

	if( !info->init() )
		return 0L;
		
	return info;


}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
bool 
IOATABusInfo::init()
{

	if( ! super::init() )
		return false;

	zeroData();	
	
	return true;

}

/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
void
IOATABusInfo::zeroData(void)
{

	_PIOModes		= 0x00;
	_MultiDMAModes	= 0x00;
	_UltraDMAModes	= 0x00;
	_ExtendedLBA	= false;
	_Overlapped		= false;
	_DMAQueued		= false;
	_SocketType		= kUnknownSocket;
	_maxBlocksExtended = 256;

}




/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8 
IOATABusInfo::getPIOModes( void )
{

	return	_PIOModes;

}





/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8 
IOATABusInfo::getDMAModes( void )
{


	return	_MultiDMAModes;

}





/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8 
IOATABusInfo::getUltraModes( void )
{

	return	_UltraDMAModes;


}






/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

bool 
IOATABusInfo::supportsDMA( void )
{

	if( (_MultiDMAModes != 0x00) || (_UltraDMAModes != 0x00) )
		return true;
		
	return false;


}


   


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

bool 
IOATABusInfo::supportsExtendedLBA( void )
{

	return	_ExtendedLBA;


}


  


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

bool 
IOATABusInfo::supportsOverlapped( void )
{

	return	_Overlapped;


}


   


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

bool 
IOATABusInfo::supportsDMAQueued( void )
{


	return	_DMAQueued;

}


   


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

ataSocketType 
IOATABusInfo::getSocketType( void )
{

	return	_SocketType;


}


 




/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATABusInfo::setPIOModes( UInt8 inModeBitMap)
{

	_PIOModes = inModeBitMap;


}





/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATABusInfo::setDMAModes( UInt8 inModeBitMap )
{


	_MultiDMAModes = inModeBitMap;

}





/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATABusInfo::setUltraModes( UInt8 inModeBitMap )
{


	_UltraDMAModes = inModeBitMap;

}






/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATABusInfo::setExtendedLBA( bool inState )
{


	_ExtendedLBA = inState;

}


  


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATABusInfo::setOverlapped( bool inState )
{


	_Overlapped = inState;

}


   


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATABusInfo::setDMAQueued( bool inState )
{


	_DMAQueued = inState;

}


    


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATABusInfo::setSocketType( ataSocketType inSocketType )
{

	_SocketType = inSocketType;


}


 


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8 
IOATABusInfo::getUnits( void )
{
	
	return _numUnits;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATABusInfo::setUnits( UInt8 inNumUnits )
{

	_numUnits = inNumUnits;


}


void 
IOATABusInfo::setMaxBlocksExtended( UInt16 inMaxBlocks)
{
	_maxBlocksExtended = inMaxBlocks;

}

UInt16 
IOATABusInfo::maxBlocksExtended(void)
{

	return _maxBlocksExtended;

}
