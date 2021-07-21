/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
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

#include<IOKit/storage/IOStorageProtocolCharacteristics.h>
#include "AppleIntelICHxSATA.h"


//---------------------------------------------------------------------------
// begin implementation AppleIntelICHxSATAPolledAdapter
// --------------------------------------------------------------------------
#undef super
#define super IOPolledInterface

OSDefineMetaClassAndStructors(  AppleIntelICHxSATAPolledAdapter, IOPolledInterface )

IOReturn 
AppleIntelICHxSATAPolledAdapter::probe(IOService * target)
{
    pollingActive = false;
    return kIOReturnSuccess;
}

IOReturn 
AppleIntelICHxSATAPolledAdapter::open( IOOptionBits state, IOMemoryDescriptor * buffer)
{
    switch( state )
    {
	case kIOPolledPreflightState:
	    // nothing to do here for this controller
	    break;
	
	case kIOPolledBeforeSleepState:
	    pollingActive = true;
	    break;
	
	case kIOPolledAfterSleepState:
	    // ivars may be inconsistent at this time. Kernel space is restored by bootx, then executed. 
	    // ivars may be stale depending on the when the image snapshot took place during image write
	    // call the controller to return the ivars to a queiscent state and restore the pci device state.
	    owner->transitionFixup();
	    pollingActive = true;
	    break;	

	case kIOPolledPostflightState:
	    // illegal value should not happen. 
	default:	
	    break;
    }
    return kIOReturnSuccess;
}

IOReturn 
AppleIntelICHxSATAPolledAdapter::close(IOOptionBits state)
{
    switch( state )
    {
	case kIOPolledPreflightState:
	case kIOPolledBeforeSleepState:
	case kIOPolledAfterSleepState:
	case kIOPolledPostflightState:
	default:
	    pollingActive = false;	
	break;
    }

    return kIOReturnSuccess;
}

IOReturn 
AppleIntelICHxSATAPolledAdapter::startIO(uint32_t 	        operation,
					 uint32_t		bufferOffset,
					 uint64_t	        deviceOffset,
					 uint64_t	        length,
					 IOPolledCompletion	completion)
{
    return kIOReturnUnsupported;
}

IOReturn 
AppleIntelICHxSATAPolledAdapter::checkForWork(void)
{

    if( owner )
    {
	owner->pollEntry();
    }

    return kIOReturnSuccess;
}


bool 
AppleIntelICHxSATAPolledAdapter::isPolling( void )
{
    return pollingActive;
}

void
AppleIntelICHxSATAPolledAdapter::setOwner( AppleIntelICHxSATA* myOwner )
{
    owner = myOwner;
    pollingActive = false;
}

//---------------------------------------------------------------------------

#undef super
#define super AppleIntelPIIXPATA
OSDefineMetaClassAndStructors( AppleIntelICHxSATA, AppleIntelPIIXPATA )

//---------------------------------------------------------------------------

// polled mode is called at a time when hardware interrupts are disabled.
// this is a poll-time procedure, when given a slice of time, the poll proc
// checks the state of the hardware to see if it has a pending interrupt status
// and calls the interrupt handlers in place of the interrupt event source.

void 
AppleIntelICHxSATA::pollEntry( void )
{
    // make sure there is a current command before processing further.
    if( 0 == _currentCommand )
	return;

    if ( *(_bmStatusReg) & kPIIX_IO_BMISX_IDEINTS )
    {
	// Clear interrupt latch
	*(_bmStatusReg) = kPIIX_IO_BMISX_IDEINTS;

	// Let our superclass handle the interrupt to advance to the next state
	// in its internal state machine.
	handleDeviceInterrupt();
    }
}

void
AppleIntelICHxSATA::executeEventCallouts( ataEventCode event, ataUnitID unit )
{
    if( polledAdapter && polledAdapter->isPolling())
    {
	return;
    }
    super::executeEventCallouts(event, unit);
}

IOReturn 
AppleIntelICHxSATA::startTimer( UInt32 inMS)
{
    if( polledAdapter && polledAdapter->isPolling())
    {
	return kIOReturnSuccess;
    }
    return super::startTimer( inMS);
}

void
AppleIntelICHxSATA::stopTimer(void)
{
    if( polledAdapter && polledAdapter->isPolling())
    {
	return;
    }
    return super::stopTimer( );
}

void 
AppleIntelICHxSATA::transitionFixup( void )
{
    // ivars working up the chain of inheritance:
    
    // from IOATAController		
    _queueState = IOATAController::kQueueOpen;
    _busState = IOATAController::kBusFree;
    _currentCommand = 0L;
    _selectedUnit = kATAInvalidDeviceID;
    _queueState = IOATAController::kQueueOpen;
    _immediateGate = IOATAController::kImmediateOK;

    // make sure the hardware is running
    _pciDevice->restoreDeviceState();
}


bool AppleIntelICHxSATA::start( IOService * provider )
{
    // Override P-ATA reporting in IOATAController::start()
    // for SystemProfiler.
    setProperty( kIOPropertyPhysicalInterconnectTypeKey,
                 kIOPropertyPhysicalInterconnectTypeSerialATA );


    polledAdapter = new AppleIntelICHxSATAPolledAdapter;
    
    if( polledAdapter)
    {
	polledAdapter->setOwner( this );
	setProperty ( kIOPolledInterfaceSupportKey, polledAdapter );
	polledAdapter->release();
    }

    return super::start(provider);
}

//---------------------------------------------------------------------------

IOReturn AppleIntelICHxSATA::provideBusInfo( IOATABusInfo * infoOut )
{
    if ( super::provideBusInfo( infoOut ) != kATANoErr )
    {
        return -1;
    }

    // Override the socket type reported by the PATA driver

    infoOut->setSocketType( kInternalSATA );

    return kATANoErr;
}

//---------------------------------------------------------------------------

UInt32 AppleIntelICHxSATA::scanForDrives( void )
{
    UInt32 unitsFound;

    // Try real hard to reset the port(s) and attached devices.

    for ( int loopMs = 0; loopMs <= 3000; loopMs += 10 )
    {
        if ( (loopMs % 1000) == 0 )
        {
            for ( UInt32 i = 0; i < _provider->getMaxDriveUnits(); i++ )
                _provider->setSerialATAPortEnableForDrive( i, false );
        
            IOSleep( 20 );
        
            for ( UInt32 i = 0; i < _provider->getMaxDriveUnits(); i++ )
                _provider->setSerialATAPortEnableForDrive( i, true );
    
            IOSleep( 20 );

            *_tfAltSDevCReg = mATADCRReset;  // ATA reset
    
            IODelay( 100 );
    
            *_tfAltSDevCReg = 0x0;            
        }

        if ( (*_tfStatusCmdReg & mATABusy) == 0x00 )
            break;

        IOSleep( 10 );
    }

    // ICH5 does offer a device present flag for each SATA port. This
    // information can be used to speed up boot by reducing unnecessary
    // bus scanning when no devices are present. In addition, the port
    // can be disabled to reduce power usage. For now we still use the
    // standard bus scanning implementation in IOATAController.

    unitsFound = IOPCIATA::scanForDrives();

    // Fixup discrepancies between the results from ATA bus scanning,
    // and the SATA device present status.

    for ( UInt32 unit = 0; unit < kMaxDrives; unit++ )
    {
        if ( _devInfo[unit].type != kUnknownATADeviceType &&
             ( unit >= _provider->getMaxDriveUnits() ||
               _provider->getSerialATAPortPresentStatusForDrive( unit ) == false ) )
        {
            // Detected a device, but SATA reports that no device are
            // present on the port. Trust SATA since if the device was
            // detected then surely the port present bit would be set.

            _devInfo[unit].type = kUnknownATADeviceType;
        }
    }

    // Turn off unused SATA ports.
    
    for ( UInt32 unit = 0; unit < _provider->getMaxDriveUnits(); unit++ )
    {
        if ( _devInfo[unit].type == kUnknownATADeviceType )
        {
            _provider->setSerialATAPortEnableForDrive( unit, false );
        }
    }

    return unitsFound;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelICHxSATA::selectDevice( ataUnitID unit )
{
    // This override is needed when a single device is connected to
    // the second port (virtual slave) of an ICH6 SATA channel. The
    // status register will float (0x7f) following an ATA reset, at
    // boot time or waking from sleep, and also to recover from bus
    // errors and super::selectDevice() will time out while waiting
    // for DRQ to clear. We just accelerate the device selection to
    // the only possible drive unit on the bus. Check _selectedUnit
    // to eliminate unnecessary taskfile accesses.

    if (_selectedUnit    != kATADevice1DeviceID   &&
        _devInfo[0].type == kUnknownATADeviceType &&
        _devInfo[1].type != kUnknownATADeviceType)
    {
        *_tfSDHReg = (1 << 4);  // force device selection to unit 1
    }

    return super::selectDevice( unit );
}

//---------------------------------------------------------------------------

IOReturn AppleIntelICHxSATA::setPowerState( unsigned long stateIndex,
                                            IOService *   whatDevice )
{
    if ( stateIndex == kPIIXPowerStateOff )
    {
        // Record the fact that the driver should initialize the port
        // enable bits when power is raised.

        _initPortEnable = true;
    }
    else if ( _initPortEnable )
    {
        // Power state was OFF, refresh the port enable bits in case
        // the controller lost hardware context.

        for ( UInt32 unit = 0; unit < _provider->getMaxDriveUnits(); unit++ )
        {
            _provider->setSerialATAPortEnableForDrive(
                unit, _devInfo[unit].type != kUnknownATADeviceType );
        }
        _initPortEnable = false;
    }

    return super::setPowerState( stateIndex, whatDevice );
}

#if 0
static void dumpRegsICH6( IOPCIDevice * pci )
{
    kprintf("INT_LN      %x\n", pci->configRead8(0x3c));
    kprintf("INT_PN      %x\n", pci->configRead8(0x3d));
    kprintf("IDE_TIMP    %x\n", pci->configRead16(0x40));
    kprintf("IDE_TIMS    %x\n", pci->configRead16(0x42));
    kprintf("IDE_SIDETIM %x\n", pci->configRead8(0x44));
    kprintf("SDMA_CNT    %x\n", pci->configRead8(0x48));
    kprintf("SDMA_TIM    %x\n", pci->configRead16(0x4a));
    kprintf("IDE_CONFIG  %x\n", pci->configRead32(0x54));
    kprintf("PID         %x\n", pci->configRead16(0x70));
    kprintf("PC          %x\n", pci->configRead16(0x72));
    kprintf("PMCS        %x\n", pci->configRead16(0x74));
    kprintf("MAP         %x\n", pci->configRead8(0x90));
    kprintf("PCS         %x\n", pci->configRead16(0x92));
    kprintf("SIR         %x\n", pci->configRead32(0x94));
    kprintf("ATC         %x\n", pci->configRead8(0xc0));
    kprintf("ATS         %x\n", pci->configRead8(0xc4));
    kprintf("BFCS        %x\n", pci->configRead32(0xe0));

    pci->configWrite32(0xa0, 0);
    kprintf("Index 0x00  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x18);
    kprintf("Index 0x18  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x1c);
    kprintf("Index 0x1c  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x28);
    kprintf("Index 0x28  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x54);
    kprintf("Index 0x54  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x64);
    kprintf("Index 0x64  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x74);
    kprintf("Index 0x74  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x84);
    kprintf("Index 0x84  %0x\n", pci->configRead32(0xa4));
}
#endif

