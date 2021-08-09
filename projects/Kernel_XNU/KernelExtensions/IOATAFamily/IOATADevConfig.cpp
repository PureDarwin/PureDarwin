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
 *	IOATADevConfig.cpp
 *
 */

const int configword 		= 00;
const int pioModeNumber 	= 51;
const int fieldValidity 	= 53;
const int mwDMAWord 		= 63;
const int pioAdvancedMode 	= 64;
const int minimumDMATime 	= 65;
const int recommendDMATime 	= 66;
const int pioCycleNoFlow 	= 67;
const int pioCycleIORDY 	= 68;
const int ultraDMAWord 		= 88;
 
#include<IOKit/IOTypes.h>
#include"IOATATypes.h"
#include"IOATADevConfig.h"
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
OSDefineMetaClassAndStructors ( IOATADevConfig, OSObject )
    OSMetaClassDefineReservedUnused(IOATADevConfig, 0);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 1);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 2);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 3);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 4);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 5);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 6);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 7);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 8);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 9);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 10);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 11);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 12);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 13);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 14);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 15);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 16);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 17);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 18);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 19);
    OSMetaClassDefineReservedUnused(IOATADevConfig, 20);


IOATADevConfig* 
IOATADevConfig::atadevconfig(void)
{
	
	IOATADevConfig* config = new IOATADevConfig;
	
	if( !config )
		return 0L;
	
	if( !config->init() )
	{
	
		return 0L;
	
	}


	return config;

}


// intialize the data fields to nil values

/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

bool
IOATADevConfig::init( )
{

	if( ! super::init() )
		return false;
		
	_atapiIRQForPacket = kATAPIUnknown;			

	_ataPIOMode = 0x00;			
	_ataPIOCycleTime = 0;			
	_ataMultiDMAMode = 0x00;			
	_ataMultiCycleTime = 0;			
	_ataUltraDMAMode = 0x00;			
//	_ataUltraCycleTime = 0;
	
	return true;				
}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

IOReturn	
IOATADevConfig::initWithBestSelection( const UInt16* identifyData, IOATABusInfo* busInfo)
{
	IOReturn err = kATANoErr;

	err = assignFromData( identifyData );
	if( err )
		return err;
	
	// AND mask with the modes supported by the bus controller
	
	_ataPIOMode	     = _ataPIOMode & busInfo->getPIOModes();
	_ataMultiDMAMode = _ataMultiDMAMode & busInfo->getDMAModes();
	_ataUltraDMAMode = _ataUltraDMAMode & busInfo->getUltraModes();

	// select only one kind of DMA mode, preferring UDMA over MWDMA
	
	if( _ataUltraDMAMode != 0x00 )
	{
		_ataMultiDMAMode = 0x00;	
	} 
	
	// clear all but the most-significant matching bit to indicate the best match between
	// device and bus.
	
	_ataPIOMode = _MostSignificantBit(_ataPIOMode);
	_ataMultiDMAMode = _MostSignificantBit(_ataMultiDMAMode);
	_ataUltraDMAMode = _MostSignificantBit(_ataUltraDMAMode);

	return kATANoErr;  

}


//-----------------------------------------------------------------------------
IOReturn 
IOATADevConfig::assignFromData( const UInt16* identifyData )
{	
	
	if ( ( identifyData[0] & 0xC000 ) == 0xC000 )
	{
		// unknown device type 
		return kATAErrUnknownType;			
	}

	// for atapi devices, figure out the command-to-packet protocol
	if( ( identifyData[0] & 0x8000 )  )
	{
		
		// Atapi device that uses INTRQ
		_atapiIRQForPacket = ( atapiConfig )( ( identifyData[0] & 0x0060 ) >> 5 );
		
	}



	_AssignPIOData ( identifyData );
	_AssignDMAData ( identifyData );
	_AssignUltraData ( identifyData );

	return kATANoErr;
	
}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

IOReturn						
IOATADevConfig::_AssignPIOData( const UInt16* identifyData)
{
	UInt16 advancedPIO = 0x0000;
	
	// set internal fields to no modes.
	_ataPIOCycleTime = 0;
	_ataPIOMode = 0;
	
	//Check word 53 bit 1 if advanced fields are valid.
	if( 0x0002 & identifyData[fieldValidity] )
	{

		advancedPIO = (identifyData[ pioAdvancedMode ] & 0x03) << 3	;
		
		if(advancedPIO != 0x0000)
		{
			
			// set ns cycle time.
			_ataPIOCycleTime = identifyData[ pioCycleIORDY ];
			if( _ataPIOCycleTime == 0 )
			{
				//  some devices fail to support cycle times. 
				// supply a default setting that equals PIO mode 3 timing
				_ataPIOCycleTime = 180;
			
			}

			// set all lower mode bits as per ATA standards
			_ataPIOMode = advancedPIO | 0x0007;  			
			return kATANoErr;	
		}		
		
		// otherwise fall through and use the mode number in word 51.	
	
	}
	
	
	
	// Otherwise, this is a basic device
	switch( identifyData[ pioModeNumber] & 0xFF00 )
	{
		case 0x0200:
		 	_ataPIOMode = 0x07; 	// mode 2, 1, 0
			_ataPIOCycleTime = 240;
		break;
		
		case 0x0100:
			_ataPIOMode = 0x03;	// mode 1, 0
			_ataPIOCycleTime = 383;
		break;
		
		default:
			_ataPIOMode = 0x01;	// mode 0 only.
			_ataPIOCycleTime = 600;
		break;
		 
	
	}
		
	return kATANoErr;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

IOReturn						
IOATADevConfig::_AssignDMAData(const UInt16* identifyData)
{

	// set internal fields to no support
	_ataMultiDMAMode = 0;
	_ataMultiCycleTime = 0;


	//Check word 53 bit 1 if dma fields are valid.
	if( 0x0002 & identifyData[fieldValidity] )
	{
		_ataMultiDMAMode = identifyData[mwDMAWord] & 0x0007;		
		
		if( identifyData[recommendDMATime] > identifyData[minimumDMATime] )
		{
			_ataMultiCycleTime = identifyData[recommendDMATime];
		
		} else {
		
			_ataMultiCycleTime = identifyData[minimumDMATime];				
		}

	}

	return kATANoErr;


}




/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

IOReturn						
IOATADevConfig::_AssignUltraData(const UInt16* identifyData)
{

	// set internal fields to no support
	_ataUltraDMAMode = 0;


	//Check word 53 bit 2 if ultra fields are valid.
	if( 0x0004 & identifyData[fieldValidity] )
	{
		_ataUltraDMAMode = identifyData[ultraDMAWord] & 0x00FF;	 	
	}

	return kATANoErr;



}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void
IOATADevConfig::setPacketConfig ( atapiConfig packetConfig)
{
	
	_atapiIRQForPacket = packetConfig;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

atapiConfig 
IOATADevConfig::getPacketConfig( void )
{

	return _atapiIRQForPacket;

}	




/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATADevConfig::setPIOMode( UInt8 inModeBitMap)
{

	_ataPIOMode = inModeBitMap;


}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8 
IOATADevConfig::getPIOMode( void )
{


	return _ataPIOMode;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATADevConfig::setDMAMode( UInt8 inModeBitMap )
{

	_ataMultiDMAMode = inModeBitMap;


}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8 
IOATADevConfig::getDMAMode( void )
{

	return _ataMultiDMAMode;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATADevConfig::setUltraMode( UInt8 inModeBitMap )
{


	_ataUltraDMAMode = inModeBitMap;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8 
IOATADevConfig::getUltraMode( void )
{


	return _ataUltraDMAMode;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATADevConfig::setPIOCycleTime( UInt16 inNS )
{


	_ataPIOCycleTime = inNS;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt16 
IOATADevConfig::getPIOCycleTime( void )
{

	return _ataPIOCycleTime;


}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATADevConfig::setDMACycleTime( UInt16 inNS )
{


	_ataMultiCycleTime = inNS;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt16  
IOATADevConfig::getDMACycleTime( void )
{

	return _ataMultiCycleTime;


}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8
IOATADevConfig::_MostSignificantBit( UInt8 inByte)
{
	// return 0 if input is 0
	if( inByte == 0)
		return inByte;
	
	// start at the top and work down.	
	UInt8 mask = 0x80;
	
	for( int i = 7; i >0; i--)
	{
		// if it masks, return.
		if( mask & inByte )
		{
			return mask;
		}
		
		// shift one right and try again.
		mask >>= 1;
	}
	
	// bit zero is the winner.
	return 0x01;

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

UInt8 
IOATADevConfig::bitSigToNumeric(UInt16 binary)
{
	UInt16  i, integer;

	/* Test all bits from left to right, terminating at the first non-zero bit. */
	for (i = 0x0080, integer = 7; ((i & binary) == 0 && i != 0) ; i >>= 1, integer-- )
	{;}
	return (integer);
}	/* end BitSigToNumeric() */

bool 
IOATADevConfig::sDriveSupports48BitLBA( const UInt16* identifyData )
{

	if( (identifyData[83] & 0x0400) 
		&& (identifyData[86] & 0x0400))
	{
		return true;
	}
	
	return false;
}

UInt32 
IOATADevConfig::sDriveExtendedLBASize( UInt32* lbaHi, UInt32* lbaLo, const UInt16* identifyData)
{

	UInt32 lowerLBA = 0;
	UInt32 upperLBA = 0;

	if( IOATADevConfig::sDriveSupports48BitLBA( identifyData ) )
	{
		lowerLBA = identifyData[ 100 ] | ( identifyData[101] << 16 );
		upperLBA = identifyData[102] | ( identifyData[103] << 16 );
	}
	
	*lbaLo = lowerLBA;
	*lbaHi = upperLBA;
	
	return lowerLBA;

}