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
 *	IOPCIATA.cpp
 *	
 *	class defining the portions of PCI-ATA class controllers which 
 *	are generally common to most PCI ATA bus controllers, such as DMA 
 *  programming.
 *
 */

#include <IOKit/IOTypes.h>
#include "IOATATypes.h"
#include "IOATAController.h"
#include "IOATADevice.h"
#include "IOATABusInfo.h"
#include "IOATADevConfig.h"

#include <IOKit/IOMemoryCursor.h>

#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>

#include "IOPCIATA.h"
#include "IOATABusCommand.h"

#ifdef DLOG
#undef DLOG
#endif

//#define ATA_DEBUG 1

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// some day, we'll have an ATA recorder for IOKit

#define ATARecordEventMACRO(type,param,bus,data) 		(void) (type); (void) (param); (void) (bus); (void) (data)


#pragma mark -IOService Overrides -

// 33 prd  descriptors are normally required to satisfy a 
// maximum ATA transaction of 256 * 512 byte sectors.
// however due to restrictions on PCI style DMA engines,
// the number of descriptors must increase to something more 
// than that.
#define kATAXferDMADesc 64
#define kATAMaxDMADesc kATAXferDMADesc

// up to 256 ATA sectors per transfer
#define kMaxATAXfer	512 * 256

#define _prdBuffer	reserved->_prdBuffer


//---------------------------------------------------------------------------

#define super IOATAController

OSDefineMetaClass( IOPCIATA, IOATAController )
OSDefineAbstractStructors( IOPCIATA, IOATAController )
    OSMetaClassDefineReservedUnused(IOPCIATA, 0);
    OSMetaClassDefineReservedUnused(IOPCIATA, 1);
    OSMetaClassDefineReservedUnused(IOPCIATA, 2);
    OSMetaClassDefineReservedUnused(IOPCIATA, 3);
    OSMetaClassDefineReservedUnused(IOPCIATA, 4);
    OSMetaClassDefineReservedUnused(IOPCIATA, 5);
    OSMetaClassDefineReservedUnused(IOPCIATA, 6);
    OSMetaClassDefineReservedUnused(IOPCIATA, 7);
    OSMetaClassDefineReservedUnused(IOPCIATA, 8);
    OSMetaClassDefineReservedUnused(IOPCIATA, 9);
    OSMetaClassDefineReservedUnused(IOPCIATA, 10);
    OSMetaClassDefineReservedUnused(IOPCIATA, 11);
    OSMetaClassDefineReservedUnused(IOPCIATA, 12);
    OSMetaClassDefineReservedUnused(IOPCIATA, 13);
    OSMetaClassDefineReservedUnused(IOPCIATA, 14);
    OSMetaClassDefineReservedUnused(IOPCIATA, 15);
    OSMetaClassDefineReservedUnused(IOPCIATA, 16);
    OSMetaClassDefineReservedUnused(IOPCIATA, 17);
    OSMetaClassDefineReservedUnused(IOPCIATA, 18);
    OSMetaClassDefineReservedUnused(IOPCIATA, 19);
    OSMetaClassDefineReservedUnused(IOPCIATA, 20);

//---------------------------------------------------------------------------

bool 
IOPCIATA::init(OSDictionary * properties)
{

    DLOG("IOPCIATA::init() starting\n");

    // Initialize instance variables.
	_bmCommandReg = 0;
	_bmStatusReg = 0;
	_bmPRDAddresReg = 0;

	_prdTable = 0;
	_prdTablePhysical = 0;
	_DMACursor = 0;
	
	_dmaState = IOPCIATA::kATADMAInactive;


   
    if (super::init(properties) == false)
    {
        DLOG("IOPCIATA: super::init() failed\n");
        return false;
    }


    DLOG("IOPCIATA::init() done\n");


    return true;
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
IOPCIATA::start(IOService *provider)
{
     DLOG("IOPCIATA::start() begin\n");

 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("IOPCIATA: super::start() failed\n");
        return false;
	}
	
	reserved = ( ExpansionData * ) IOMalloc ( sizeof ( ExpansionData ) );
	if ( !reserved )
		return false;
	
	bzero ( reserved, sizeof ( ExpansionData ) );
	
	// Allocate the DMA descriptor area
	if( ! allocDMAChannel() )
	{
        DLOG("IOPCIATA:  allocDMAChannel failed\n");
		return false;	
	
	}

    DLOG("IOPCIATA::start() done\n");
    return true;
}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
IOPCIATA::free()
{

	freeDMAChannel();
	
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
 *	allocate memory and resources for the DMA descriptors.
 *
 *
 ---------------------------------------------------------------------------*/
bool
IOPCIATA::allocDMAChannel(void)
{

	if(  _bmCommandReg == 0
		||	_bmStatusReg == 0
		|| _bmPRDAddresReg == 0 )
	{
	
		DLOG("IOPCIATA bm regs not initialised.\n");
		return false;	
	
	}

	_prdBuffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
		kernel_task,
		kIODirectionInOut | kIOMemoryPhysicallyContiguous,
		sizeof(PRD) * kATAMaxDMADesc,
		0xFFFFFFF0UL );
    
    if ( !_prdBuffer )
    {
        IOLog("%s: PRD buffer allocation failed\n", getName());
        return false;
    }
 	
	_prdBuffer->prepare ( );
	
	_prdTable			= (PRD *) _prdBuffer->getBytesNoCopy();
	_prdTablePhysical	= _prdBuffer->getPhysicalAddress();
	
	_DMACursor = IONaturalMemoryCursor::withSpecification(0x10000, /*64K*/
                                       					kMaxATAXfer  /* 256 * 512 */
                                     					/*inAlignment - Memory descriptors and cursors don't support alignment
                                     					flags yet. */);

	
	if( ! _DMACursor )
	{
		freeDMAChannel();
		DLOG("IOPCIATA alloc DMACursor failed\n");
		return false;
	}


	// fill the chain with stop commands to initialize it.	
	initATADMAChains(_prdTable);
	
	return true;
}


/*---------------------------------------------------------------------------
 *
 *	deallocate memory and resources for the DMA descriptors.
 *
 *
 ---------------------------------------------------------------------------*/
bool
IOPCIATA::freeDMAChannel(void)
{
	
	if( _prdBuffer )
	{
		// make sure the engine is stopped
		stopDMA();

		// free the descriptor table.
        _prdBuffer->complete();
        _prdBuffer->release();
        _prdBuffer = NULL;
        _prdTable = NULL;
        _prdTablePhysical = 0;
	}

	if( _DMACursor )
	{
		
		_DMACursor->release();
		_DMACursor = NULL;
		
	}
	
	return true;
}




#pragma mark -DMA Interface-

/*---------------------------------------------------------------------------
 *
 * Subclasses should take necessary action to create DMA channel programs, 
 * for the current memory descriptor in _currentCommand and activate the 
 * the DMA hardware
 ---------------------------------------------------------------------------*/
IOReturn
IOPCIATA::startDMA( void )
{

	IOReturn err = kATANoErr;

	// first make sure the engine is stopped.
	stopDMA();
	
	
	// reality check the memory descriptor in the current command
	
	// state flag
	_dmaState = kATADMAStarting;
	
	// create the channel commands
	err = createChannelCommands();
	
	if(	err )
	{
	
		DLOG("IOPCIATA error createChannelCmds err = %ld\n", (long int)err);
		stopDMA();
		return err;
	
	}
	
	// fire the engine
	activateDMAEngine();
	
	return err;
	

}




/*---------------------------------------------------------------------------
 * Subclasses should take all actions necesary to safely shutdown DMA engines
 * in any state of activity, whether finished, pending or stopped. Calling 
 * this function must be harmless reguardless of the state of the engine.
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOPCIATA::stopDMA( void )
{

	if(_dmaState != kATADMAInactive)
		shutDownATADMA();
	
	
	_dmaState = kATADMAInactive;
	return kATANoErr;

}

#pragma mark -DMA Implementation-


//----------------------------------------------------------------------------------------
//	Function:		InitATADMAChains
//	Description:	Initializes the chains with STOP commands.
//
//	Input:			Pointer to the DBDMA descriptor area: descPtr
//	
//	Output:			None
//----------------------------------------------------------------------------------------
void	
IOPCIATA::initATADMAChains (PRD* descPtr)
{
	UInt32 i;
	
	/* Initialize the data-transfer PRD channel command descriptors. */


	for (i = 0; i < kATAMaxDMADesc; i++)
	{
		descPtr->bufferPtr = 0;
		descPtr->byteCount = 1;
		// set the stop DMA bit on the last transaction.
		descPtr->flags = OSSwapHostToLittleInt16( (UInt16) kLast_PRD);
		descPtr++;
	}
}


/*----------------------------------------------------------------------------------------
//	Function:		stopDMAEngine
//	Description:	Stops the DMA engine itself.  on the ATA bus 
//	Input:			none
//	Output:			None
----------------------------------------------------------------------------------------*/
void
IOPCIATA::stopDMAEngine(void)
{
	OSSynchronizeIO();
	*_bmCommandReg = mBMCmdStop;

}



/*----------------------------------------------------------------------------------------
//	Function:		activateDMAEngine
//	Description:	Activate the dma engine on the ATA bus associated with current device.
					engine will begin executing the command chain already programmed.
//	Input:			None
//	Output:			None
//----------------------------------------------------------------------------------------*/
void			
IOPCIATA::activateDMAEngine(void)
{
 
	DLOG("IOPCIATA prd table is at: %lx\n", _prdTablePhysical);	

	// clear error bit prior to starting.
	*_bmStatusReg = (UInt8) mBMStatusError | mBMStatusInt | (_currentCommand->getUnit() == 0 ? mBMStatusDrv0 : mBMStatusDrv1) ; 
	OSSynchronizeIO();
	
	// set the address pointer.
	*_bmPRDAddresReg = OSSwapHostToLittleInt32((UInt32) _prdTablePhysical);
	OSSynchronizeIO();

	// active the DMA engine.
	UInt8 theCommand = (_currentCommand->getFlags() & mATAFlagIORead) ?  mBMCmdStartInput : mBMCmdStartOutput;
	
	DLOG("IOPCIATA: bmCommand is %X\n", theCommand);
	
	
	*_bmCommandReg = theCommand;
	OSSynchronizeIO();


	DLOG("IOPCIATA: bmStaus is %X\n", *_bmStatusReg);
	

}


//----------------------------------------------------------------------------------------
//	Function:		ShutDownATADMA
//	Description:	Stops the dma engine on the current ATA bus.
//					This routine is used to stop the DMA 
//					such as may be desired during error recovery.
//	Input:			None
//	Output:			None
//----------------------------------------------------------------------------------------
void	
IOPCIATA::shutDownATADMA (void)
{

	// set the state semaphore 
	_dmaState = kATADMAInactive;

	stopDMAEngine();

}


/********************************************************************************
*                                                                               *
*	s e t P R D                                                                 *
*                                                                               *
*********************************************************************************
*
*	Purpose:	Fills in the "Physical Region Descriptor" with the correct
*				endian-ness
*
*	Input:		bffr - pointer to data
*				count - of bytes in this data buffer
*				tableElement - points to PRD to use
*
*	Output:		PRD - filled in
*
********************************************************************************/

void 
IOPCIATA::setPRD(UInt8 *bffr, UInt16 count, PRD *tableElement, UInt16 end)
{
	DLOG("IOPCIATA set PRD ptr = %lx count = %x flags = %x\n", (long) bffr, count, end);

	tableElement->bufferPtr = OSSwapHostToLittleInt32((UInt32)(uintptr_t)bffr);
	tableElement->byteCount = OSSwapHostToLittleInt16(count);
	tableElement->flags = OSSwapHostToLittleInt16(end);
}


/*---------------------------------------------------------------------------
 *
 *	create the DMA channel commands.
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOPCIATA::createChannelCommands(void)
{
	IOMemoryDescriptor* descriptor = _currentCommand->getBuffer();
	IOMemoryCursor::PhysicalSegment physSegment;
	UInt32 index = 0;
	UInt8		*xferDataPtr, *ptr2EndData, *next64KBlock, *starting64KBlock;
	UInt32		xferCount, count2Next64KBlock;
	
	if( ! descriptor )
	{
	
		DLOG("IOPCIATA nil buffer!\n");
		return -1;
	}

	// This form of DMA engine can only do 1 pass. It cannot execute multiple chains.

	// calculate remaining bytes in this transfer
	IOByteCount bytesRemaining = _currentCommand->getByteCount() ;

	// calculate position pointer
	IOByteCount xfrPosition = _currentCommand->getPosition() ;
	
	IOByteCount  transferSize = 0; 

	// There's a unique problem with pci-style controllers, in that each dma transaction is not allowed to
	// cross a 64K boundary. This leaves us with the yucky task of picking apart any descriptor segments that
	// cross such a boundary ourselves.  

		while( _DMACursor->getPhysicalSegments(
											descriptor,
					       					xfrPosition,
					       					&physSegment,
					     					1,
					     					bytesRemaining,  // limit to the requested number of bytes in the event the descriptors is larger
					       					&transferSize) )
		{
				       					
			xferDataPtr = (UInt8*) physSegment.location;
			xferCount = physSegment.length;
			
			// update the bytes remaining after this pass
			bytesRemaining -= xferCount;
			xfrPosition += xferCount;
			
			// now we have to examine the segment to see whether it crosses (a) 64k boundary(s)
			starting64KBlock = (UInt8*) ( (uintptr_t) xferDataPtr & 0xffff0000);
			ptr2EndData = xferDataPtr + xferCount;
			next64KBlock  = (starting64KBlock + 0x10000);


			// loop until this physical segment is fully accounted for.
			// it is possible to have a memory descriptor which crosses more than one 64K boundary in a 
			// single span.
			while( xferCount > 0 )
			{
				if (ptr2EndData > next64KBlock)
				{
					count2Next64KBlock = next64KBlock - xferDataPtr;
					if (index < kATAMaxDMADesc)
					{
						setPRD(xferDataPtr, (UInt16)count2Next64KBlock, &_prdTable[index], kContinue_PRD);
						xferDataPtr = next64KBlock;
						next64KBlock += 0x10000;
						xferCount -= count2Next64KBlock;
						index++;
					
					} else {
					
						DLOG("IOPCIATA dma too big, PRD table exhausted A.\n");
						_dmaState = kATADMAError;
						return -1;
					}
				
				} else {
				
					if (index < kATAMaxDMADesc)
					{
						setPRD(xferDataPtr, (UInt16) xferCount, &_prdTable[index], (bytesRemaining == 0) ? kLast_PRD : kContinue_PRD);
						xferCount = 0;
						index++;

					} else {
					
						DLOG("IOPCIATA dma too big, PRD table exhausted B.\n");
						_dmaState = kATADMAError;
						return -1;
					}
				}
			}

	} // end of segment counting loop.
	
	
		
	// transfer is satisfied and only need to check status on interrupt.
	_dmaState = kATADMAStatus;

	DLOG("IOPCIATA PRD chain end %ld \n", index);

	
	// chain is now ready for execution.

	return kATANoErr;

}



/*---------------------------------------------------------------------------
 *
 *	handleDeviceInterrupt - overriden here so we can make sure that the DMA has
 * processed in the event first.
 *
 ---------------------------------------------------------------------------*/
IOReturn
IOPCIATA::handleDeviceInterrupt(void)
{


	if( _dmaState == kATADMAStatus )
	{
		OSSynchronizeIO();
		UInt8 bmStatus = *_bmStatusReg;
	
		if( bmStatus & mBMStatusError )
		{
			
			_dmaState = kATADMAError;
		
		} else {
	
			_currentCommand->setActualTransfer(_currentCommand->getByteCount());
			_dmaState = kATADMAComplete;
		}
		stopDMA();
		
	}
	return super::handleDeviceInterrupt();
	
}



