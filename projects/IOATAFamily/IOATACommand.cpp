/*
 * Copyright (c) 2000-2014 Apple Inc. All rights reserved.
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
 *	IOATACommand.cpp
 *
 */
 
 
#include<IOKit/IOTypes.h>
#include <IOKit/IOLib.h>

#include"IOATATypes.h"
#include"IOATACommand.h"

#ifdef DLOG
#undef DLOG
#endif

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

//---------------------------------------------------------------------------

#define super IOCommand

OSDefineMetaClass( IOATACommand, IOCommand )
OSDefineAbstractStructors( IOATACommand, IOCommand )

    OSMetaClassDefineReservedUsed(IOATACommand, 0)  //setendResult()
    OSMetaClassDefineReservedUsed(IOATACommand, 1) // getExtendedLBAPtr()
    OSMetaClassDefineReservedUnused(IOATACommand, 2);
    OSMetaClassDefineReservedUnused(IOATACommand, 3);
    OSMetaClassDefineReservedUnused(IOATACommand, 4);
    OSMetaClassDefineReservedUnused(IOATACommand, 5);
    OSMetaClassDefineReservedUnused(IOATACommand, 6);
    OSMetaClassDefineReservedUnused(IOATACommand, 7);
    OSMetaClassDefineReservedUnused(IOATACommand, 8);
    OSMetaClassDefineReservedUnused(IOATACommand, 9);
    OSMetaClassDefineReservedUnused(IOATACommand, 10);
    OSMetaClassDefineReservedUnused(IOATACommand, 11);
    OSMetaClassDefineReservedUnused(IOATACommand, 12);
    OSMetaClassDefineReservedUnused(IOATACommand, 13);
    OSMetaClassDefineReservedUnused(IOATACommand, 14);
    OSMetaClassDefineReservedUnused(IOATACommand, 15);
    OSMetaClassDefineReservedUnused(IOATACommand, 16);
    OSMetaClassDefineReservedUnused(IOATACommand, 17);
    OSMetaClassDefineReservedUnused(IOATACommand, 18);
    OSMetaClassDefineReservedUnused(IOATACommand, 19);
    OSMetaClassDefineReservedUnused(IOATACommand, 20);



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
bool
IOATACommand::init()
{
	fExpansionData = (ExpansionData*) IOMalloc( sizeof( ExpansionData) ); 
	fExpansionData->extLBA = IOExtendedLBA::createIOExtendedLBA( this );
	
	if( ! super::init() || fExpansionData == NULL || fExpansionData->extLBA == NULL )
		return false;

	zeroCommand();
	
	return true;

}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
IOATACommand::free()
{

	getExtendedLBA()->release();
	IOFree( fExpansionData, sizeof( ExpansionData) );
	super::free();

}
/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
void 
IOATACommand::zeroCommand(void)
{

	_opCode	= kATANoOp;			
	_flags = 0;		 		
	_unit = kATAInvalidDeviceID;
	_timeoutMS = 0;			
	_desc = 0L;        		
	_position = 0;			
	_byteCount = 0;			
	_regMask = (ataRegMask) 0;		
	_callback = 0L ;  
 	_result = 0;			
	_actualByteCount = 0;  	
 	_status = 0;			
 	_errReg = 0;
	_logicalChunkSize = kATADefaultSectorSize;
 	_inUse = false;			
	
	_taskFile.ataDataRegister = 0x0000;
	_taskFile.ataAltSDevCReg = 0x00;
	_taskFile.taskFile.ataTFFeatures = 0;   		
	_taskFile.taskFile.ataTFCount  = 0;  		
	_taskFile.taskFile.ataTFSector  = 0; 		
	_taskFile.taskFile.ataTFCylLo  = 0;  		
	_taskFile.taskFile.ataTFCylHigh  = 0;  		
	_taskFile.taskFile.ataTFSDH  = 0;  		
	_taskFile.taskFile.ataTFCommand  = 0;  		
    
    bzero ( _packet.atapiCommandByte, sizeof ( _packet.atapiCommandByte ) );
    
	_packet.atapiPacketSize = 0;
	
	getExtendedLBA()->zeroData();

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
void 
IOATACommand::setOpcode( ataOpcode inCode)
{

	_opCode = inCode;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATACommand::setFlags( UInt32 inFlags)
{

	_flags = inFlags;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATACommand::setUnit( ataUnitID inUnit)
{

	_unit = inUnit;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATACommand::setTimeoutMS( UInt32 inMS)
{

	_timeoutMS = inMS;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void 
IOATACommand::setCallbackPtr (IOATACompletionFunction* inCompletion)
{

	_callback = inCompletion;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
void 
IOATACommand::setRegMask( ataRegMask mask)
{

	_regMask = mask;

}

/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void
IOATACommand::setBuffer ( IOMemoryDescriptor* inDesc)
{

	_desc = inDesc;

}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void
IOATACommand::setPosition (IOByteCount fromPosition)
{

	_position = fromPosition;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void
IOATACommand::setByteCount (IOByteCount numBytes)
{

	_byteCount = numBytes;

}

/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void
IOATACommand::setTransferChunkSize (IOByteCount numBytes)
{

	_logicalChunkSize = numBytes;

}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void
IOATACommand::setFeatures( UInt8 in)
{

	_taskFile.taskFile.ataTFFeatures = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8
IOATACommand::getErrorReg (void )
{

	return _taskFile.taskFile.ataTFFeatures;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	
void
IOATACommand::setSectorCount( UInt8 in)
{

	_taskFile.taskFile.ataTFCount = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8
IOATACommand::getSectorCount (void )
{

	return  _taskFile.taskFile.ataTFCount;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void
IOATACommand::setSectorNumber( UInt8 in)
{

	_taskFile.taskFile.ataTFSector = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8
IOATACommand::getSectorNumber (void )
{

	return _taskFile.taskFile.ataTFSector;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	
void
IOATACommand::setCylLo ( UInt8 in)
{

	_taskFile.taskFile.ataTFCylLo = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/


UInt8
IOATACommand::getCylLo (void )
{

	return _taskFile.taskFile.ataTFCylLo;

}

/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
void
IOATACommand::setCylHi( UInt8 in)
{

	_taskFile.taskFile.ataTFCylHigh = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8
IOATACommand::getCylHi (void )
{

	return _taskFile.taskFile.ataTFCylHigh;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	
void
IOATACommand::setDevice_Head( UInt8 in)
{

	_taskFile.taskFile.ataTFSDH = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8
IOATACommand::getDevice_Head (void )
{

	return _taskFile.taskFile.ataTFSDH;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	
void
IOATACommand::setCommand ( UInt8 in)
{

	_taskFile.taskFile.ataTFCommand = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

UInt8
IOATACommand::getStatus (void )
{

	return _taskFile.taskFile.ataTFCommand;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/



IOReturn 
IOATACommand::setPacketCommand( UInt16 packetSizeBytes, UInt8* packetBytes)
{
//	IOLog("ATACommand::setPacket size %d  bytePtr = %lx\n", packetSizeBytes, packetBytes);
	
	if( ( packetSizeBytes > 16 ) || (packetBytes == 0L))
		return -1;  

	UInt8* cmdBytes = (UInt8*) _packet.atapiCommandByte;

	for( int i = 0; i < packetSizeBytes; i++ )
	{
		cmdBytes[ i ] = packetBytes[ i ];
	}

	_packet.atapiPacketSize = packetSizeBytes;
	
	return kATANoErr;

}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

void
IOATACommand::setDataReg ( UInt16 in)
{

	_taskFile.ataDataRegister = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/


UInt16 
IOATACommand::getDataReg (void )
{

	return _taskFile.ataDataRegister;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/



void
IOATACommand::setControl ( UInt8 in)
{

	_taskFile.ataAltSDevCReg = in;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/


UInt8
IOATACommand::getAltStatus (void )
{

	return _taskFile.ataAltSDevCReg;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/




IOReturn 
IOATACommand::getResult (void)
{

	return _result;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/


IOMemoryDescriptor* 
IOATACommand::getBuffer ( void )
{

	return _desc;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

 
IOByteCount 
IOATACommand::getActualTransfer ( void )
{

	return _actualByteCount;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/


UInt8	
IOATACommand::getEndStatusReg (void)
{

	return _status;

}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/



UInt8
IOATACommand::getEndErrorReg( void )
{

	return _errReg;

}


/*-----------------------------------------------------------------------------
 * returns true if IOATAController is using the command.
 *
 *-----------------------------------------------------------------------------*/

bool	
IOATACommand::getCommandInUse( void )
{


	return _inUse;


}

/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/

IOReturn 
IOATACommand::setLBA28( UInt32 lba, ataUnitID inUnit)
{
	// param check the inputs
	
	if( (lba & 0xF0000000) != 0x00000000 
		|| !(inUnit == kATADevice0DeviceID || inUnit == kATADevice1DeviceID) )
	{
		//param out of range
		return -1;		
	}
	
	
	setSectorNumber( (lba &      0xFF) );  //LBA 7:0
	setCylLo( ((lba &          0xFF00) >> 8) );	// LBA 15:8
	setCylHi( ((lba &      0x00FF0000) >> 16) );  // LBA 23:16
	setDevice_Head(((lba & 0x0F000000) >> 24 ) |  mATALBASelect |( ((UInt8) inUnit) << 4));  //LBA 27:24

	return kATANoErr;

}

void 
IOATACommand::setEndResult(UInt8 inStatus, UInt8 endError  )
{
	_status = inStatus;
	_errReg = endError;
}



IOExtendedLBA* 
IOATACommand::getExtendedLBA(void)
{

	return fExpansionData->extLBA;

}



////////////////////////////////////////////////////////////////////////
#pragma mark IOExtendedLBA
#undef super

#define super OSObject
OSDefineMetaClassAndStructors( IOExtendedLBA, OSObject )

OSMetaClassDefineReservedUnused(IOExtendedLBA, 0)
OSMetaClassDefineReservedUnused(IOExtendedLBA, 1)
OSMetaClassDefineReservedUnused(IOExtendedLBA, 2);
OSMetaClassDefineReservedUnused(IOExtendedLBA, 3);
OSMetaClassDefineReservedUnused(IOExtendedLBA, 4);
OSMetaClassDefineReservedUnused(IOExtendedLBA, 5);
OSMetaClassDefineReservedUnused(IOExtendedLBA, 6);
OSMetaClassDefineReservedUnused(IOExtendedLBA, 7);
OSMetaClassDefineReservedUnused(IOExtendedLBA, 8);
OSMetaClassDefineReservedUnused(IOExtendedLBA, 9);
OSMetaClassDefineReservedUnused(IOExtendedLBA, 10);





IOExtendedLBA* 
IOExtendedLBA::createIOExtendedLBA(IOATACommand* inOwner)
{

	IOExtendedLBA* me = new IOExtendedLBA;
	if( me == NULL)
	{	
		return NULL;
	}
	
	me->owner = inOwner;
	me->zeroData();
	
	return me;

}


	
void 
IOExtendedLBA::setLBALow16( UInt16 inLBALow)
{
	lbaLow =  inLBALow ;

}


UInt16 
IOExtendedLBA::getLBALow16 (void)
{

	return lbaLow ;

}
	
void 
IOExtendedLBA::setLBAMid16 (UInt16 inLBAMid)
{

	lbaMid =  inLBAMid ;
}


UInt16 
IOExtendedLBA::getLBAMid16( void )
{

	return lbaMid;


}
	
void 
IOExtendedLBA::setLBAHigh16( UInt16 inLBAHigh )
{
	lbaHigh =  inLBAHigh ;
}


UInt16 
IOExtendedLBA::getLBAHigh16( void )
{

	return lbaHigh;

}
	
void 
IOExtendedLBA::setSectorCount16( UInt16 inSectorCount )
{

	sectorCount =  inSectorCount ;

}

UInt16 
IOExtendedLBA::getSectorCount16( void )
{
	return sectorCount;
}
	
void 
IOExtendedLBA::setFeatures16( UInt16 inFeatures )
{
	features =  inFeatures;
}

UInt16 
IOExtendedLBA::getFeatures16( void )
{

	return features;

}

void 
IOExtendedLBA::setDevice( UInt8 inDevice )
{

	device = inDevice;

}


UInt8 
IOExtendedLBA::getDevice( void )
{


	return device;

}

void 
IOExtendedLBA::setCommand( UInt8 inCommand )
{


	command = inCommand;

}


UInt8 
IOExtendedLBA::getCommand( void )
{

	return command;
}


	
void 
IOExtendedLBA::setExtendedLBA( UInt32 inLBAHi, UInt32 inLBALo, ataUnitID inUnit, UInt16 extendedCount, UInt8 extendedCommand )
{

	UInt8 lba7, lba15, lba23, lba31, lba39, lba47;
	lba7 = (inLBALo & 0xff);
	lba15 = (inLBALo & 0xff00) >> 8;
	lba23 = (inLBALo & 0xff0000) >> 16;
	lba31 = (inLBALo & 0xff000000) >> 24;
	lba39 = (inLBAHi & 0xff);
	lba47 = (inLBAHi & 0xff00) >> 8;

	setLBALow16(  lba7 | (lba31 << 8) );
	setLBAMid16(  lba15 | (lba39 << 8));
	setLBAHigh16(  lba23 | (lba47 << 8));
	
	setSectorCount16( extendedCount );
	setCommand( extendedCommand );
	setDevice(   mATALBASelect |( ((UInt8) inUnit) << 4)); // set the LBA bit and device select bits. The rest are reserved in extended addressing.

}

void 
IOExtendedLBA::getExtendedLBA( UInt32* outLBAHi, UInt32* outLBALo )
{



	*outLBALo = (getLBALow16() & 0xff) | ( (getLBAMid16() & 0xff) << 8) | ((getLBAHigh16() & 0xff) << 16) | ((getLBALow16() & 0xff00) << 16) ; 
	
	*outLBAHi = (getLBAHigh16() & 0xff00) | ((getLBAMid16() & 0xff00) >> 8) ; 

}

void 
IOExtendedLBA::zeroData(void)
{
	lbaLow = lbaMid = lbaHigh = sectorCount = features = device = command = 0;
}
	

