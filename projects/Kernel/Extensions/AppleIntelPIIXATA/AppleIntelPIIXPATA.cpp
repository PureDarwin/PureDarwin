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

#include <sys/systm.h>    // snprintf
#include <IOKit/assert.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include "AppleIntelPIIXPATA.h"
#include <IOKit/IOLocksPrivate.h>
#include <IOKit/storage/ata/IOATAFamilyPriv.h>

#define super IOPCIATA
OSDefineMetaClassAndStructors( AppleIntelPIIXPATA, IOPCIATA )

#if  1
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Controller supported modes.
//
#define PIOModes   \
    (_provider->getPIOModeMask() & ((1 << piixPIOTimingCount) - 1))

#define DMAModes   \
    (_provider->getDMAModeMask() & ((1 << piixDMATimingCount) - 1))

#define UDMAModes  \
    (_provider->getUltraDMAModeMask() & ((1 << piixUDMATimingCount) - 1))

// Increase the PRD table size to one full page or 4096 descriptors to allow
// for large transfers via dma engine. 2048 are required for 1 megabyte of
// transfer assuming no fragmentation and no alignment  issues on the buffer.
// We allocate twice that since there are more issues than simple alignment
// for this DMA engine.

#define kATAXferDMADesc  512
#define kATAMaxDMADesc   kATAXferDMADesc

// up to 2048 ATA sectors per transfer

#define kMaxATAXfer      512 * 2048

class AppleIntelPIIXPATAWorkLoop : public IOWorkLoop
{
	OSDeclareDefaultStructors ( AppleIntelPIIXPATAWorkLoop );
	
public:
	static AppleIntelPIIXPATAWorkLoop * workLoop ( void );
	
protected:
	
	bool init ( void );
	void free ( void );
	
	lck_grp_t *		fLockGroup;
	
};

OSDefineMetaClassAndStructors ( AppleIntelPIIXPATAWorkLoop, IOWorkLoop );

AppleIntelPIIXPATAWorkLoop * AppleIntelPIIXPATAWorkLoop::workLoop()
{
    AppleIntelPIIXPATAWorkLoop *loop;
    
    loop = new AppleIntelPIIXPATAWorkLoop;
    if(!loop)
        return loop;
    if(!loop->init()) {
        loop->release();
        loop = NULL;
    }
    return loop;
}

bool
AppleIntelPIIXPATAWorkLoop::init ( void )
{
	
	char	name[64];
	
	snprintf ( name, 64, "PATA" );
	fLockGroup = lck_grp_alloc_init ( name, LCK_GRP_ATTR_NULL );
	if ( fLockGroup )
	{
		gateLock = IORecursiveLockAllocWithLockGroup ( fLockGroup );
	}
	
	return IOWorkLoop::init ( );
}

void AppleIntelPIIXPATAWorkLoop::free ( void )
{
	
	if ( fLockGroup )
	{
		lck_grp_free ( fLockGroup );
		fLockGroup = NULL;
	}
	
	IOWorkLoop::free ( );
	
}


/*---------------------------------------------------------------------------
 *
 * Start the single-channel PIIX ATA controller driver.
 *
 ---------------------------------------------------------------------------*/

bool AppleIntelPIIXPATA::start( IOService * provider )
{
    bool superStarted = false;
	IOACPIPlatformDevice* myACPINode;
	_drivePowerOn =  true;
	bool safeSleep = false;
	polledPATAAdapter = NULL;

    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, provider);

    // Our provider is a 'nub' that represents a single channel
    // PIIX ATA controller. Note that it is not an IOPCIDevice.

    _provider = OSDynamicCast( AppleIntelPIIXATAChannel, provider );
    if ( _provider == 0 )
        goto fail;

    // Retain and open our provider. The IOPCIDevice object will be
    // returned by the provider if the open was successful.

    _provider->retain();

    if ( ( _provider->open( this, 0, &_pciDevice ) != true ) ||
         ( _pciDevice == 0 ) )
    {
        DLOG("%s: provider open failed\n", getName());
        goto fail;
    }

    // Cache controller properties, and validate them.

    _cmdBlock = _provider->getCommandBlockAddress();
    _ctrBlock = _provider->getControlBlockAddress();
    _channel  = _provider->getChannelNumber();

    if ( _channel > kPIIX_CHANNEL_SECONDARY )
    {
        DLOG("%s: invalid ATA channel number %d\n", getName(), (int)_channel);
        goto fail;
    }

    // Configure the PIIX device.

    if ( configurePCIDevice( _pciDevice, _channel ) != true )
    {
        // DLOG("%s: PIIX PCI configuration failed\n", getName());
        goto fail;
    }

    // Get the base address for the bus master registers in I/O space.

    if ( getBMBaseAddress( _pciDevice, _channel, &_ioBMOffset ) != true )
    {
        DLOG("%s: get bus-master base address failed\n", getName());
        goto fail;
    }

    // Must setup these variables inherited from IOPCIATA before
    // calling its start().

    _bmCommandReg   = IOATAIOReg8::withAddress( _ioBMOffset + kPIIX_IO_BMICX );
    _bmStatusReg    = IOATAIOReg8::withAddress( _ioBMOffset + kPIIX_IO_BMISX );
    _bmPRDAddresReg = IOATAIOReg32::withAddress( _ioBMOffset + kPIIX_IO_BMIDTPX );

    // Reset controller timings for both drives.

    resetTimingsForDevice( kATADevice0DeviceID );
    resetTimingsForDevice( kATADevice1DeviceID );

    // Call super after resolving _cmdBlock and _ctrBlock. This is because our
    // configureTFPointers() function will be called by super.

    if ( super::start(_provider) == false )
    {
        goto fail;
    }
    
    superStarted = true;

    // This driver will handle interrupts using a work loop.
    // Create interrupt event source that will signal the
    // work loop (thread) when a device interrupt occurs.

    if ( _provider->getInterruptVector() == 14 ||
         _provider->getInterruptVector() == 15 )
    {
        // Legacy IRQ are never shared, no need for an interrupt filter.

        _intSrc = IOInterruptEventSource::interruptEventSource(
                      this, &interruptOccurred,
                      _provider, 0 );
    }
    else
    {
        _intSrc = IOFilterInterruptEventSource::filterInterruptEventSource(
                      this, &interruptOccurred, &interruptFilter,
                      _provider, 0 );
    }

    if ( !_intSrc || !_workLoop ||
         (_workLoop->addEventSource(_intSrc) != kIOReturnSuccess) )
    {
        DLOG("%s: interrupt registration error\n", getName());
        goto fail;
    }
	
	// clean up any interrupt glitches left over from powering down the drive. 
	*_bmStatusReg = kPIIX_IO_BMISX_IDEINTS;
	
	// enable interrupts
    _intSrc->enable();

    // Attach to power management.

    initForPM( provider );

	
	myACPINode = getACPIParent();
	if( myACPINode )
	{
	
		if( hasMediaNotify( myACPINode ) )
		{
			_pciACPIDevice = myACPINode;
			
			
			
			_interestNotifier = myACPINode->registerInterest(  gIOGeneralInterest ,
								(IOServiceInterestHandler) AppleIntelPIIXPATA::mediaInterestHandler,
								this);
								
			if( _interestNotifier )
				DLOG ("AppleIntellPIIX registered Interest.\n");
		
		} else {
		
			_pciACPIDevice = NULL;
		
		}
	
	}

	if( _pciACPIDevice == NULL )
	{
		// check for safe-sleep
		
		if( _provider->getProperty( "safe-sleep" ) )
		{
			safeSleep = true;
			DLOG("Intel PATA has safe-sleep\n");
		}
		
		// TODO undo this
		//safeSleep = true;
	}

	 

    // For each device discovered on the ATA bus (by super),
    // create a nub for that device and call registerService() to
    // trigger matching against that device.

    for ( UInt32 i = 0; i < kMaxDrives; i++ )
    {
        if ( _devInfo[i].type != kUnknownATADeviceType )
        {
            DLOG("Registering device %d of type %d\n", i, _devInfo[i].type);
            
            ATADeviceNub * nub;

            nub = ATADeviceNub::ataDeviceNub( (IOATAController*) this,
                                              (ataUnitID) i,
                                              _devInfo[i].type );

            if ( nub )
            {
                if ( _devInfo[i].type == kATAPIDeviceType )
                {
                    nub->setProperty( kIOMaximumSegmentCountReadKey,
                                      kATAMaxDMADesc / 2, 64 );

                    nub->setProperty( kIOMaximumSegmentCountWriteKey,
                                      kATAMaxDMADesc / 2, 64 );

                    nub->setProperty( kIOMaximumByteCountReadKey,
                                      512*256, 64 );

                    nub->setProperty( kIOMaximumByteCountWriteKey,
                                      512*256, 64 );
									  
					// set the media notify property if available
					if( _pciACPIDevice )
					{
						nub->setProperty( kATANotifyOnChangeKey, 1, 32);
					}
									  				  
                }
                else if (  _devInfo[i].type == kATADeviceType && safeSleep == true ) {
						
					polledPATAAdapter = new AppleIntelICHxPATAPolledAdapter;
    
					if( polledPATAAdapter)
					{
						polledPATAAdapter->setOwner( this );
						setProperty ( kIOPolledInterfaceSupportKey, polledPATAAdapter );
						polledPATAAdapter->release();
					}						
						
				}	

                if ( nub->attach( this ) )
                {
                    _nub[i] = (IOATADevice *) nub;
                    _nub[i]->retain();
                    _nub[i]->registerService();
                }
                nub->release();
            }
            else {
                DLOG("Failed to register nub!\n");
            }
        }
    }

    // Successful start, announce our vital properties.

    DLOG("%s: %s (CMD 0x%x, CTR 0x%x, IRQ %d, BM 0x%x)\n", getName(),
          _provider->getControllerName(), _cmdBlock, _ctrBlock,
          _provider->getInterruptVector(), _ioBMOffset);

    return true;

fail:
    
    if ( _provider )
    {
        _provider->close( this );
    }
    
    if (superStarted)
        super::stop( provider );

    return false;
}

/*---------------------------------------------------------------------------
 *
 * Stop the single-channel PIIX ATA controller driver.
 *
 ---------------------------------------------------------------------------*/

void AppleIntelPIIXPATA::stop( IOService * provider )
{
    PMstop();
    super::stop( provider );
}

/*---------------------------------------------------------------------------
 *
 * Release resources before this object is destroyed.
 *
 ---------------------------------------------------------------------------*/

void AppleIntelPIIXPATA::free( void )
{
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    // Release resources created by start().

    RELEASE( _intSrc         );
    RELEASE( _nub[0]         );
    RELEASE( _nub[1]         );
    RELEASE( _provider       );
    RELEASE( _bmCommandReg   );
    RELEASE( _bmStatusReg    );
    RELEASE( _bmPRDAddresReg );

    // Release registers created by configureTFPointers().

    RELEASE( _tfDataReg      );
    RELEASE( _tfFeatureReg   );
    RELEASE( _tfSCountReg    );
    RELEASE( _tfSectorNReg   );
    RELEASE( _tfCylLoReg     );
    RELEASE( _tfCylHiReg     );
    RELEASE( _tfSDHReg       );
    RELEASE( _tfStatusCmdReg );
    RELEASE( _tfAltSDevCReg  );

	if( _DMACursor != NULL )
	{
		_DMACursor->release();
		_DMACursor = NULL;
	}

	if( _workLoop != NULL )
	{
		_workLoop->release();
	}
	
    super::free();
}

/*---------------------------------------------------------------------------
 *
 * Return a new work loop object, or the one (we) previously created.
 *
 ---------------------------------------------------------------------------*/

IOWorkLoop * AppleIntelPIIXPATA::getWorkLoop( void ) const
{
    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    return ( _workLoop ) ? _workLoop :
                           AppleIntelPIIXPATAWorkLoop::workLoop();
}

/*---------------------------------------------------------------------------
 *
 * Override IOATAController::synchronousIO()
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleIntelPIIXPATA::synchronousIO( void )
{
    IOReturn ret;
    
    // IOATAController::synchronousIO() asserts nIEN bit in order to disable
    // drive interrupts during polled mode command execution. The problem is
    // that this will float the INTRQ line and put it in high impedance state,
    // which on certain systems has the undesirable effect of latching a false
    // interrupt on the interrupt controller. Perhaps those systems lack a
    // strong pull down resistor on the INTRQ line. Experiment shows that the
    // interrupt event source is signalled, and its producerCount incremented
    // after every synchronousIO() call. This false interrupt can become
    // catastrophic after reverting to async operations since software can
    // issue a command, handle the false interrupt, and issue another command
    // to the drive before the actual completion of the first command, leading
    // to a irrecoverable bus hang. This function is called after an ATA bus
    // reset. Waking from system sleep will exercise this path.
    // The workaround is to mask the interrupt line while the INTRQ line is
    // floating (or bouncing).

    if (_intSrc) _intSrc->disable();
    ret = super::synchronousIO();
	*_bmStatusReg = kPIIX_IO_BMISX_IDEINTS;
    if (_intSrc) _intSrc->enable();

    return ret;
}

/*---------------------------------------------------------------------------
 *
 * Configure the PIIX PCI device.
 *
 ---------------------------------------------------------------------------*/

bool AppleIntelPIIXPATA::configurePCIDevice( IOPCIDevice * device,
                                             UInt32        channel )
{
    UInt32 reg;

    DLOG("%s::%s( %p, %d )\n", getName(), __FUNCTION__,
         device, channel);

    // Fetch the corresponding primary/secondary IDETIM register and
    // check the individual channel enable bit. We assume that the
    // master IOSE bit was already checked by our provider.

    reg = device->configRead32( kPIIX_PCI_IDETIM );

    if ( channel == kPIIX_CHANNEL_SECONDARY )
        reg >>= 16;  // kPIIX_PCI_IDETIM + 2 for secondary channel

    if ( (reg & kPIIX_PCI_IDETIM_IDE) == 0 )
    {
        DLOG("%s: %s PCI IDE channel is disabled\n", getName(),
              (channel == kPIIX_CHANNEL_PRIMARY) ? "Primary" : "Secondary");
        return false;
    }

    // Enable bus-master. The previous state of the bit is returned
    // but ignored.

    device->setBusMasterEnable( true );

    // Read the IDE config register containing the Ultra DMA clock control,
    // and 80-conductor cable reporting bits.

    _ideConfig = device->configRead16( kPIIX_PCI_IDECONFIG );
    DLOG("%s: IDE_CONFIG = %04x\n", getName(), _ideConfig);

    return true;
}

/*---------------------------------------------------------------------------
 *
 * Determine the start of the I/O mapped Bus-Master registers.
 * This range is defined by PCI config space register kPIIX_PCI_BMIBA.
 *
 ---------------------------------------------------------------------------*/

bool AppleIntelPIIXPATA::getBMBaseAddress( IOPCIDevice * provider,
                                           UInt32        channel,
                                           UInt16 *      addrOut )
{
    UInt32 bmiba;

    DLOG("%s::%s( %p, %d, %p )\n", getName(), __FUNCTION__,
         provider, channel, addrOut);

    bmiba = provider->configRead32( kPIIX_PCI_BMIBA );

    if ( (bmiba & kPIIX_PCI_BMIBA_RTE) == 0 )
    {
        DLOG("%s: PCI memory range 0x%02x (0x%08x) is not an I/O range\n",
              getName(), (int)kPIIX_PCI_BMIBA, (int)bmiba);
        return false;
    }

    bmiba &= kPIIX_PCI_BMIBA_MASK;  // get the address portion

    // If bmiba is zero, it is likely that the user has elected to
    // turn off PCI IDE support in the BIOS.

    if ( bmiba == 0 )
        return false;

    if ( channel == kPIIX_CHANNEL_SECONDARY )
        bmiba += kPIIX_IO_BM_OFFSET;

    *addrOut = (UInt16) bmiba;

    DLOG("%s::%s ioBMOffset = %04x\n", getName(), __FUNCTION__, *addrOut);

    return true;
}

/*---------------------------------------------------------------------------
 *
 * Reset all timing registers to the slowest (most compatible) timing.
 * UDMA modes are disabled.
 *
 ---------------------------------------------------------------------------*/

void AppleIntelPIIXPATA::resetTimingsForDevice( ataUnitID unit )
{
    _pioTiming[unit]  = &piixPIOTiming[ 0 ];  // PIO Mode 0
    _dmaTiming[unit]  = 0;
    _udmaTiming[unit] = 0;

    // Compute the timing register values.

    computeTimingRegisters( unit );
    computeUDMATimingRegisters( unit );

    // Write the timing values to hardware.

    writeTimingRegisters();
}

/*---------------------------------------------------------------------------
 *
 * Setup the location of the task file registers.
 *
 ---------------------------------------------------------------------------*/

bool AppleIntelPIIXPATA::configureTFPointers( void )
{
    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    _tfDataReg      = IOATAIOReg16::withAddress( _cmdBlock + 0 );
    _tfFeatureReg   = IOATAIOReg8::withAddress(  _cmdBlock + 1 );
    _tfSCountReg    = IOATAIOReg8::withAddress(  _cmdBlock + 2 );
    _tfSectorNReg   = IOATAIOReg8::withAddress(  _cmdBlock + 3 );
    _tfCylLoReg     = IOATAIOReg8::withAddress(  _cmdBlock + 4 );
    _tfCylHiReg     = IOATAIOReg8::withAddress(  _cmdBlock + 5 );
    _tfSDHReg       = IOATAIOReg8::withAddress(  _cmdBlock + 6 );
    _tfStatusCmdReg = IOATAIOReg8::withAddress(  _cmdBlock + 7 );
    _tfAltSDevCReg  = IOATAIOReg8::withAddress(  _ctrBlock + 2 );

    if ( !_tfDataReg || !_tfFeatureReg || !_tfSCountReg ||
         !_tfSectorNReg || !_tfCylLoReg || !_tfCylHiReg ||
         !_tfSDHReg || !_tfStatusCmdReg || !_tfAltSDevCReg )
    {
        return false;
    }

    return true;
}

/*---------------------------------------------------------------------------
 *
 * Filter interrupts that are not originated by our hardware. This will help
 * to prevent waking up our work loop thread when a shared interrupt line is
 * asserted by another device.
 *
 ---------------------------------------------------------------------------*/

bool AppleIntelPIIXPATA::interruptFilter( OSObject * owner,
                                          IOFilterInterruptEventSource * src )
{
    AppleIntelPIIXPATA * self = (AppleIntelPIIXPATA *) owner;

    if ( *(self->_bmStatusReg) & kPIIX_IO_BMISX_IDEINTS )
	{
    	// Clear interrupt latch
		*(self->_bmStatusReg) = kPIIX_IO_BMISX_IDEINTS;
		return true;   // wakeup the work loop
	}
    else
        return false;  // ignore this interrupt
}

/*---------------------------------------------------------------------------
 *
 * The work loop based interrupt handler called by our interrupt event
 * source.
 *
 ---------------------------------------------------------------------------*/

void AppleIntelPIIXPATA::interruptOccurred( OSObject *               owner,
                                            IOInterruptEventSource * src,
                                            int                      count )
{
    AppleIntelPIIXPATA * self = (AppleIntelPIIXPATA *) owner;


    // Let our superclass handle the interrupt to advance to the next state
    // in its internal state machine.

    self->handleDeviceInterrupt();
}

/*---------------------------------------------------------------------------
 *
 * Extend the implementation of scanForDrives() from IOATAController
 * to issue a soft reset before scanning for ATA/ATAPI drive signatures.
 *
 ---------------------------------------------------------------------------*/

UInt32 AppleIntelPIIXPATA::scanForDrives( void )
{
    UInt32 unitsFound;

    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    *_tfAltSDevCReg = mATADCRReset;

    IODelay( 100 );

    *_tfAltSDevCReg = 0x0;

    IOSleep( 10 );

    unitsFound = super::scanForDrives();

#if ENABLE_VPC4_DRIVESCAN_WORKAROUND
    // FIXME: Hack for Darwin/x86 on VPC compatibility.
    // VPC 4.0 will set the error bit (bit 0) in the status register
    // following an assertion of SRST. The scanForDrives() code in
    // IOATAController will see that this bit is set, and will not
    // recognize the ATA device. The following code will re-scan the
    // bus and ignore the error bit.

    for ( int unit = 0; ((unit < 2) && (unitsFound < 2)); unit++ )
    {
        if ( _devInfo[unit].type == kUnknownATADeviceType )
        {
            UInt32 milsSpent;

            // Select unit and wait for a not busy bus.

            for ( milsSpent = 0; milsSpent < 10000; )
            {
                *_tfSDHReg = ( unit << 4 );
                IODelay( 10 );

                if ( (*_tfStatusCmdReg & mATABusy) == 0x00 ) break;

                IOSleep( 10 );
                milsSpent += 10;
            }
            if ( milsSpent >= 10000 ) break;

            // Ignore the error bit in the status register, and check
            // for a ATA device signature.

            if ( (*_tfCylLoReg == 0x00) && (*_tfCylHiReg == 0x00) &&
                 (*_tfSCountReg == 0x01) && (*_tfSectorNReg == 0x01) &&
                 ( (*_tfAltSDevCReg & 0x50) == 0x50) )
            {
                _devInfo[unit].type = kATADeviceType;
                _devInfo[unit].packetSend = kATAPIUnknown;
                unitsFound++;
            }
        }
    }
#endif

    *_tfSDHReg = 0x00;  // Initialize device selection to device 0.

    return unitsFound;
}


/*---------------------------------------------------------------------------
 *
 * Determine the ATAPI device's state.
 *
 ---------------------------------------------------------------------------*/

IOATAController::transState	
AppleIntelPIIXPATA::determineATAPIState(void)
{
	IOATAController::transState			drivePhase = super::determineATAPIState();
	if(  ( IOATAController::transState ) _currentCommand->state > drivePhase
		|| _currentCommand->state == kATAStarted )
	{
		return (IOATAController::transState) _currentCommand->state;
	}

	return drivePhase;
}



/*---------------------------------------------------------------------------
 *
 * Provide information on the bus capability.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleIntelPIIXPATA::provideBusInfo( IOATABusInfo * infoOut )
{
    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, infoOut);

    if ( infoOut == 0 )
    {
        DLOG("%s::%s bad argument\n", getName(), __FUNCTION__);
        return -1;
    }

    infoOut->zeroData();
    infoOut->setSocketType( kInternalATASocket );

    infoOut->setPIOModes( PIOModes );
    infoOut->setDMAModes( DMAModes );
    infoOut->setUltraModes( UDMAModes );
    infoOut->setExtendedLBA( true );
    infoOut->setMaxBlocksExtended( 0x0800 );  // 2048 sectors for ext LBA

    UInt8 units = 0;
    if ( _devInfo[0].type != kUnknownATADeviceType ) units++;
    if ( _devInfo[1].type != kUnknownATADeviceType ) units++;

    infoOut->setUnits( units );

    return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Return the currently configured timing for the drive unit.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleIntelPIIXPATA::getConfig( IOATADevConfig * configOut,
                                        UInt32           unit )
{
    DLOG("%s::%s( %p, %d )\n", getName(), __FUNCTION__,
         configOut, unit);

    if ( (configOut == 0) || (unit > kATADevice1DeviceID) )
    {
        DLOG("%s::%s bad argument\n", getName(), __FUNCTION__);
        return -1;
    }

    configOut->setPIOMode( 0 );
    configOut->setDMAMode( 0 );
    configOut->setUltraMode( 0 );

    if ( _pioTiming[unit] )
    {
        configOut->setPIOMode( 1 << _pioTiming[unit]->mode );
        configOut->setPIOCycleTime( _pioTiming[unit]->cycleTime );
    }

    if ( _dmaTiming[unit] )
    {
        configOut->setDMAMode( 1 << _dmaTiming[unit]->mode );
        configOut->setDMACycleTime( _dmaTiming[unit]->cycleTime );
    }

    if ( _udmaTiming[unit] )
    {
        configOut->setUltraMode( 1 << _udmaTiming[unit]->mode );
    }

    configOut->setPacketConfig( _devInfo[unit].packetSend );

    return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Select the bus timings for a given drive.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleIntelPIIXPATA::selectConfig( IOATADevConfig * config,
                                           UInt32           unit )
{
    const PIIXTiming *     pioTiming  = 0;
    const PIIXTiming *     dmaTiming  = 0;
    const PIIXUDMATiming * udmaTiming = 0;

    DLOG("%s::%s( %p, %ud )\n", getName(), __FUNCTION__,
         config, unit);

    if ( (config == 0) || (unit > kATADevice1DeviceID) )
    {
        DLOG("%s::%s bad argument\n", getName(), __FUNCTION__);
        return -1;
    }

    removeProperty( kSelectedPIOModeKey );
    removeProperty( kSelectedDMAModeKey );
    removeProperty( kSelectedUltraDMAModeKey );

    // Verify that the selected PIO mode is supported.

    if ( ( config->getPIOMode() & PIOModes ) == 0 )
    {
        DLOG("%s: Unsupported PIO mode %x\n", getName(), config->getPIOMode());
        pioTiming = &piixPIOTiming[0];
    }
    else
    {
        UInt8  pioModeNumber;

        // Convert a bit-significant indicators to a numeric values.

        pioModeNumber = bitSigToNumeric( config->getPIOMode() );
        DLOG("%s: pioModeNumber = %d\n", getName(), pioModeNumber);

        // Even though the drive supports the given PIO mode, it is
        // necessary to check the cycle time reported by the drive,
        // and downgrade the controller timings if the drive cannot
        // support the cycle time for the target PIO mode.

        for ( ; pioModeNumber > 0; pioModeNumber-- )
        {
            // If the mode is supported by the controller, and the
            // drive supported cycle time is less than or equal to
            // the mode's cycle time, then select the mode.

            if ( ( ( (1 << pioModeNumber) & PIOModes ) ) &&
                 ( config->getPIOCycleTime() <=
                   piixPIOTiming[ pioModeNumber ].cycleTime ) )
            {
                break;
            }

            DLOG("%s: pioModeNumber = %d\n", getName(), pioModeNumber - 1);
        }

        setDriveProperty( unit, kSelectedPIOModeKey, pioModeNumber, 8 );
        pioTiming = &piixPIOTiming[ pioModeNumber ];
    }

    // Look at the selected Multi-Word DMA mode.

    if ( config->getDMAMode() )
    {
        UInt8  dmaModeNumber;

        // Is the selected DMA mode supported?

        if ( ( config->getDMAMode() & DMAModes ) == 0 )
        {
            DLOG("%s: Unsupported DMA mode\n", getName());
            return kATAModeNotSupported;
        }

        dmaModeNumber = bitSigToNumeric( config->getDMAMode() );
        DLOG("%s: dmaModeNumber = %d\n", getName(), dmaModeNumber);

        // Even though the drive supports the given DMA mode, it is
        // necessary to check the cycle time reported by the drive,
        // and downgrade the controller timings if the drive cannot
        // support the cycle time for the target DMA mode.

        for ( ; dmaModeNumber > 0; dmaModeNumber-- )
        {
            // If the mode is supported by the controller, and the
            // drive supported cycle time is less than or equal to
            // the mode's cycle time, then select the mode.

            if ( ( ( (1 << dmaModeNumber) & DMAModes ) ) &&
                 ( config->getDMACycleTime() <=
                   piixDMATiming[ dmaModeNumber ].cycleTime ) )
            {
                break;
            }

            DLOG("%s: dmaModeNumber = %d\n", getName(), dmaModeNumber - 1);
        }

        if ( (1 << dmaModeNumber) & DMAModes )
        {
            setDriveProperty( unit, kSelectedDMAModeKey, dmaModeNumber, 8 );
            dmaTiming = &piixDMATiming[ dmaModeNumber ];
        }
        else
            dmaTiming = 0;  // No Multi-Word DMA mode selected.
    }

    // Look at the selected U-DMA mode.

    if ( config->getUltraMode() )
    {
        UInt8 udmaModeNumber;
    
        // Is the selected Ultra-DMA mode supported?

        if ( ( config->getUltraMode() & UDMAModes ) == 0 )
        {
            DLOG("%s: Unsupported U-DMA mode\n", getName());
            return kATAModeNotSupported;
        }

        udmaModeNumber = bitSigToNumeric( config->getUltraMode() );

        // For Ultra DMA mode 3 or higher, a 80-conductor cable must
        // be present. Otherwise, the drive will be limited to mode 2.

        if ( udmaModeNumber > 2 &&
             _provider->getChannelMode() == kChannelModePATA )
        {
            UInt16 cableMask = kPIIX_PCI_IDECONFIG_PCR0;
            if ( unit == kATADevice1DeviceID )         cableMask <<= 1;
            if ( _channel == kPIIX_CHANNEL_SECONDARY ) cableMask <<= 2;

            if ( ( cableMask & _ideConfig ) == 0 )
            {
                DLOG("%s: 80-conductor cable not detected on %s channel\n",
                      getName(), (_channel == kPIIX_CHANNEL_PRIMARY) ?
                      "primary" : "secondary" );
                udmaModeNumber = 2;   // limited to mode 2
            }
        }

        setDriveProperty( unit, kSelectedUltraDMAModeKey, udmaModeNumber, 8 );
        DLOG("%s: udmaModeNumber = %d\n", getName(), udmaModeNumber);
        udmaTiming = &piixUDMATiming[ udmaModeNumber ];
    }

    // If DMA and PIO require different timings, we use fast
    // timing for DMA only, and revert to compatible timing
    // for PIO data register access.

    if ( dmaTiming && (pioTiming->cycleTime != dmaTiming->cycleTime) )
    {
        DLOG("%s: forcing PIO compatible timing\n", getName());
        pioTiming = &piixPIOTiming[0];
    }

    // Cache the selected timings.

    _pioTiming[unit]  = pioTiming;
    _dmaTiming[unit]  = dmaTiming;
    _udmaTiming[unit] = udmaTiming;

    // Compute the timing register values.

    computeTimingRegisters( (ataUnitID) unit );
    computeUDMATimingRegisters( (ataUnitID) unit );

    // Write the timing values to hardware.

    writeTimingRegisters();

    _devInfo[unit].packetSend = config->getPacketConfig();

    return getConfig( config, unit );
}

/*---------------------------------------------------------------------------
 *
 * Write the timing values to the PIIX timing registers.
 *
 ---------------------------------------------------------------------------*/

void AppleIntelPIIXPATA::writeTimingRegisters( ataUnitID unit )
{
    UInt8  idetimOffset;
    UInt8  sidetimMask;
    UInt8  udmactlMask;
    UInt16 udmatimMask;
    UInt16 ideConfigMask;

    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    if ( _channel == kPIIX_CHANNEL_PRIMARY )
    {
        idetimOffset = kPIIX_PCI_IDETIM;
    
        sidetimMask  = kPIIX_PCI_SIDETIM_PRTC1_MASK |
                       kPIIX_PCI_SIDETIM_PISP1_MASK;
        
        udmactlMask  = kPIIX_PCI_UDMACTL_PSDE0 |
                       kPIIX_PCI_UDMACTL_PSDE1;
        
        udmatimMask  = kPIIX_PCI_UDMATIM_PCT0_MASK |
                       kPIIX_PCI_UDMATIM_PCT1_MASK;
    
        ideConfigMask = kPIIX_PCI_IDECONFIG_PCB0      |
                        kPIIX_PCI_IDECONFIG_PCB1      |
                        kPIIX_PCI_IDECONFIG_FAST_PCB0 |
                        kPIIX_PCI_IDECONFIG_FAST_PCB1 |
                        kPIIX_PCI_IDECONFIG_WR_PP_EN;
    }
    else
    {
        idetimOffset = kPIIX_PCI_IDETIM_S;

        sidetimMask  = kPIIX_PCI_SIDETIM_SRTC1_MASK |
                       kPIIX_PCI_SIDETIM_SISP1_MASK;

        udmactlMask  = kPIIX_PCI_UDMACTL_SSDE0 |
                       kPIIX_PCI_UDMACTL_SSDE1;

        udmatimMask  = kPIIX_PCI_UDMATIM_SCT0_MASK |
                       kPIIX_PCI_UDMATIM_SCT1_MASK;

        ideConfigMask = kPIIX_PCI_IDECONFIG_SCB0      |
                        kPIIX_PCI_IDECONFIG_SCB1      |
                        kPIIX_PCI_IDECONFIG_FAST_SCB0 |
                        kPIIX_PCI_IDECONFIG_FAST_SCB1 |
                        kPIIX_PCI_IDECONFIG_WR_PP_EN;
    }

    // Timing registers are shared between primary and secondary ATA
    // channels. Call the PCI config space write functions in our
    // provider to serialize changes in those timing registers.

    if ( _provider->hasSharedDriveTimings() == true )
    {
        _provider->pciConfigWrite16( idetimOffset, _idetim[unit] );
        
        DLOG("%s: IDETIM[%d] = %04x\n", getName(), unit, _idetim[unit]);
    }
    else
    {
        _provider->pciConfigWrite16( idetimOffset, _idetim[0] | _idetim[1]);

        _provider->pciConfigWrite8( kPIIX_PCI_SIDETIM, _sidetim, sidetimMask );

        _provider->pciConfigWrite8( kPIIX_PCI_UDMACTL, _udmactl, udmactlMask );

        _provider->pciConfigWrite16( kPIIX_PCI_UDMATIM, _udmatim, udmatimMask );

        _provider->pciConfigWrite16( kPIIX_PCI_IDECONFIG,
                                     _ideConfig, ideConfigMask );

        DLOG("%s: IDETIM = %04x SIDETIM = %02x UDMACTL = %02x UDMATIM = %04x\n",
             getName(),
             _idetim[0] | _idetim[1], _sidetim, _udmactl, _udmatim);
    }
}

/*---------------------------------------------------------------------------
 *
 * Compute the U-DMA timing register values.
 *
 ---------------------------------------------------------------------------*/

void AppleIntelPIIXPATA::computeUDMATimingRegisters( ataUnitID unit )
{
    UInt8  udmaEnableBit   = kPIIX_PCI_UDMACTL_PSDE0;
    UInt8  udmatimShifts   = kPIIX_PCI_UDMATIM_PCT0_SHIFT;
    UInt8  udmaClockShifts = 0;

    DLOG("%s::%s( %d )\n", getName(), __FUNCTION__, unit);

    if ( _channel == kPIIX_CHANNEL_SECONDARY )
    {
        udmaEnableBit  <<= 2;
        udmatimShifts   += 8;
        udmaClockShifts += 2;
    }
    if ( unit == kATADevice1DeviceID )
    {
        udmaEnableBit  <<= 1;
        udmatimShifts   += 4;
        udmaClockShifts += 1;
    }

    // Disable U-DMA for this drive unit.

    _udmactl &= ~udmaEnableBit;

    // If U-DMA is enabled for this unit, update timing register
    // and enable U-DMA.

    if ( _udmaTiming[unit] )
    {
        _udmatim &= ~( kPIIX_PCI_UDMATIM_PCT0_MASK << udmatimShifts );
        _udmatim |= (( _udmaTiming[unit]->udmatim << udmatimShifts ) &
                     ( kPIIX_PCI_UDMATIM_PCT0_MASK << udmatimShifts ));

        _ideConfig &= ~(( kPIIX_PCI_IDECONFIG_PCB0 |
                          kPIIX_PCI_IDECONFIG_FAST_PCB0 ) << udmaClockShifts);
        _ideConfig |= ( _udmaTiming[unit]->udmaClock << udmaClockShifts );

        _udmactl |= udmaEnableBit;
    }
}

/*---------------------------------------------------------------------------
 *
 * Compute the PIO/DMA timing register values.
 *
 ---------------------------------------------------------------------------*/

void AppleIntelPIIXPATA::computeTimingRegisters( ataUnitID unit )
{
    const PIIXTiming * timing;
    UInt8              index;
    bool               slowPIO = false;

    DLOG("%s::%s( %d )\n", getName(), __FUNCTION__, unit);

    assert( _pioTiming[unit] != 0 );

    timing = _dmaTiming[unit] ? _dmaTiming[unit] : _pioTiming[unit];

    if ( timing->cycleTime != _pioTiming[unit]->cycleTime )
    {
        DLOG("%s: using PIO compatible timing\n", getName());
        slowPIO = true;
    }

    // Get register programming index.

    index = timing->registerIndex;

    if ( _provider->hasSharedDriveTimings() == true )
    {
        _idetim[unit] = piixIDETIM[index][unit];
    }
    else /* PIIX3 or better */
    {
        _idetim[unit] = piix3IDETIM[index][unit];

        /* Update SIDETIM register for Drive 1 */

        if ( unit == kATADevice1DeviceID )
        {
            _sidetim &= ~piix3SIDETIM[0][_channel];
            _sidetim |=  piix3SIDETIM[index][_channel];
        }

        // Always enable the PIO performance feature on
        // ICH controllers.
    
        _ideConfig |= kPIIX_PCI_IDECONFIG_WR_PP_EN;
    }

    if ( slowPIO )
    {
        if ( unit == kATADevice1DeviceID )
            _idetim[unit] &= ~kPIIX_PCI_IDETIM_DTE1;
        else
            _idetim[unit] &= ~kPIIX_PCI_IDETIM_DTE0;
    }
}

/*---------------------------------------------------------------------------
 *
 * Select the bus timing configuration for a particular device.
 *
 ---------------------------------------------------------------------------*/

void AppleIntelPIIXPATA::selectIOTiming( ataUnitID unit )
{
    if ( _provider->hasSharedDriveTimings() == true )
    {
        DLOG("%s::%s( %d )\n", getName(), __FUNCTION__, unit);
        writeTimingRegisters( unit );
    }
}

/*---------------------------------------------------------------------------
 *
 * Flush the outstanding commands in the command queue.
 * Implementation borrowed from MacIOATA in IOATAFamily.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleIntelPIIXPATA::handleQueueFlush( void )
{
    UInt32 savedQstate = _queueState;

    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    _queueState = IOATAController::kQueueLocked;

    IOATABusCommand * cmdPtr = 0;

    while ( (cmdPtr = dequeueFirstCommand()) )
    {
        cmdPtr->setResult( kIOReturnError );
        cmdPtr->executeCallback();
    }

    _queueState = savedQstate;

    return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Handle termination notification from the provider.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleIntelPIIXPATA::message( UInt32      type,
                                      IOService * provider,
                                      void *      argument )
{
    if ( ( provider == _provider ) &&
         ( type == kIOMessageServiceIsTerminated ) )
    {
        _provider->close( this );
        return kIOReturnSuccess;
    }

    return super::message( type, provider, argument );
}

/*---------------------------------------------------------------------------
 *
 * Publish a numeric property pertaining to a drive to the registry.
 *
 ---------------------------------------------------------------------------*/

bool AppleIntelPIIXPATA::setDriveProperty( UInt32       driveUnit,
                                           const char * key,
                                           UInt32       value,
                                           UInt32       numberOfBits)
{
    char keyString[40];
    
    snprintf(keyString, 40, "Drive %d %s", (int)driveUnit, key);
    
    return super::setProperty( keyString, value, numberOfBits );
}

//---------------------------------------------------------------------------

IOReturn AppleIntelPIIXPATA::createChannelCommands( void )
{
    DLOG("%s::%s()\n", getName(), __FUNCTION__);
	IOATABusCommand64* currentCommand64 = OSDynamicCast( IOATABusCommand64, _currentCommand );

    IODMACommand* currentDMACmd = currentCommand64->GetDMACommand();
    IODMACommand::Segment32 physSegment32;
    UInt32 index = 0;
    UInt8  *xferDataPtr, *ptr2EndData, *next64KBlock, *starting64KBlock;
    UInt32 xferCount, count2Next64KBlock;
	IOReturn DMAStatus = 0;
	UInt32 numSegments = 1;

    if ( NULL == currentDMACmd
		|| currentDMACmd->getMemoryDescriptor() == NULL)
    {
        
		DLOG("%s: DMA buffer not set on command\n", getName());
		return -1;
    }

    // This form of DMA engine can only do 1 pass.
    // It cannot execute multiple chains.

    IOByteCount	bytesRemaining	= _currentCommand->getByteCount() ;
    IOByteCount	xfrPosition		= _currentCommand->getPosition() ;
    UInt64		transferSize	= xfrPosition; 
	
	// Clients should not be sending larger than 1MB transactions, since that's all
	// we have PRD table entries for...
	if ( bytesRemaining > kMaxATAXfer )
	{
		return kIOReturnBadArgument;
	}
	
    // There's a unique problem with pci-style controllers, in that each
    // dma transaction is not allowed to cross a 64K boundary. This leaves
    // us with the yucky task of picking apart any descriptor segments that
    // cross such a boundary ourselves.  
	
//    while ( _DMACursor->getPhysicalSegments(
//                           /* descriptor */ descriptor,
//                           /* position   */ xfrPosition,
//                           /* segments   */ &physSegment,
//                           /* max segs   */ 1,
//                           /* max xfer   */ bytesRemaining,
//                           /* xfer size  */ &transferSize) )
  
	
	  
	while ( bytesRemaining )
	{
		
		DMAStatus = currentDMACmd->gen32IOVMSegments( &transferSize, &physSegment32, &numSegments);
		if ( ( DMAStatus != kIOReturnSuccess ) || ( numSegments != 1 ) || ( physSegment32.fLength == 0 ) )
		{
			
			panic ( "AppleIntelPIIXPATA::createChannelCommands [%d] status %x segs %d phys %x:%x \n", __LINE__, DMAStatus, ( int ) numSegments, ( int ) physSegment32.fIOVMAddr, ( int ) physSegment32.fLength );
		    break;
		    
		}
		
		
		xferDataPtr = (UInt8 *) (uint64_t) physSegment32.fIOVMAddr;
        xferCount   = physSegment32.fLength;

        if ( (uintptr_t) xferDataPtr & 0x01 )
        {
            DLOG("%s: DMA buffer %p not 2 byte aligned\n",
                  getName(), xferDataPtr);
            return kIOReturnNotAligned;        
        }

        if ( xferCount & 0x01 )
        {
            DLOG("%s: DMA buffer length %d is odd\n",
                  getName(), (int)xferCount);
        }
		
		if( xferCount > bytesRemaining )
		{
			xferCount = bytesRemaining;
		}
		
        // Update bytes remaining count after this pass.
        bytesRemaining -= xferCount;
        xfrPosition += xferCount;
            
        // Examine the segment to see whether it crosses (a) 64k boundary(s)
        starting64KBlock = (UInt8*) ( (uintptr_t) xferDataPtr & 0xffff0000);
        ptr2EndData  = xferDataPtr + xferCount;
        next64KBlock = starting64KBlock + 0x10000;

        // Loop until this physical segment is fully accounted for.
        // It is possible to have a memory descriptor which crosses more
        // than one 64K boundary in a single span.
        
        while ( xferCount > 0 )
        {
            if (ptr2EndData > next64KBlock)
            {
                count2Next64KBlock = next64KBlock - xferDataPtr;
                if ( index < kATAMaxDMADesc )
                {
                    setPRD( xferDataPtr, (UInt16)count2Next64KBlock,
                            &_prdTable[index], kContinue_PRD);
                    
                    xferDataPtr = next64KBlock;
                    next64KBlock += 0x10000;
                    xferCount -= count2Next64KBlock;
                    index++;
                }
                else
                {
                    DLOG("%s: PRD table exhausted error 1\n", getName());
                    _dmaState = kATADMAError;
                    return -1;
                }
            }
            else
            {
                if (index < kATAMaxDMADesc)
                {
                    setPRD( xferDataPtr, (UInt16) xferCount,
                            &_prdTable[index],
                            (bytesRemaining == 0) ? kLast_PRD : kContinue_PRD);
                    xferCount = 0;
                    index++;
                }
                else
                {
                    DLOG("%s: PRD table exhausted error 2\n", getName());
                    _dmaState = kATADMAError;
                    return -1;
                }
            }
        }
    } // end of segment counting loop.

    if (index == 0)
    {
        DLOG("%s: rejected command with zero PRD count (0x%x bytes)\n",
              getName(), (uint32_t)_currentCommand->getByteCount());
        return kATADeviceError;
    }

    // Transfer is satisfied and only need to check status on interrupt.
    _dmaState = kATADMAStatus;
    
    // Chain is now ready for execution.
    return kATANoErr;
}

//---------------------------------------------------------------------------

bool AppleIntelPIIXPATA::allocDMAChannel( void )
{
	DLOG("%s::%s()\n", getName(), __FUNCTION__);
	_prdBuffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
		kernel_task,
		kIODirectionInOut | kIOMemoryPhysicallyContiguous,
		sizeof(PRD) * kATAMaxDMADesc,
		0xFFFF0000UL );
    
    if ( !_prdBuffer )
    {
        DLOG("%s: PRD buffer allocation failed\n", getName());
        return false;
    }
 	
	_prdBuffer->prepare ( );
	
	_prdTable			= (PRD *) _prdBuffer->getBytesNoCopy();
	_prdTablePhysical	= _prdBuffer->getPhysicalAddress();
	
    _DMACursor = IONaturalMemoryCursor::withSpecification(
                          /* max segment size  */ 0x10000,
                          /* max transfer size */ kMaxATAXfer );
    
    if ( !_DMACursor )
    {
        freeDMAChannel();
        DLOG("%s: Memory cursor allocation failed\n", getName());
        return false;
    }

    // fill the chain with stop commands to initialize it.    
    initATADMAChains( _prdTable );

    return true;
}

//---------------------------------------------------------------------------

bool AppleIntelPIIXPATA::freeDMAChannel( void )
{
    if ( _prdBuffer )
    {
        // make sure the engine is stopped.
        stopDMA();

        // free the descriptor table.
        _prdBuffer->complete();
        _prdBuffer->release();
        _prdBuffer = NULL;
        _prdTable = NULL;
        _prdTablePhysical = 0;
        
    }

    return true;
}

//---------------------------------------------------------------------------

void AppleIntelPIIXPATA::initATADMAChains( PRD * descPtr )
{
    UInt32 i;
    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, descPtr );

    /* Initialize the data-transfer PRD channel command descriptors. */

    for (i = 0; i < kATAMaxDMADesc; i++)
    {
        descPtr->bufferPtr = 0;
        descPtr->byteCount = 1;
        descPtr->flags = OSSwapHostToLittleConstInt16( kLast_PRD );
        descPtr++;
    }
}

//---------------------------------------------------------------------------

void AppleIntelPIIXPATA::initForPM( IOService * provider )
{
    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, provider);
    
    static const IOPMPowerState powerStates[ kPIIXPowerStateCount ] =
    {
        { 1, 0, 0,             0,             0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 0, IOPMSoftSleep, IOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 0, IOPMPowerOn,   IOPMPowerOn,   0, 0, 0, 0, 0, 0, 0, 0 }
    };

    PMinit();

    registerPowerDriver( this, (IOPMPowerState *) powerStates,
                         kPIIXPowerStateCount );

    provider->joinPMtree( this );
}

//---------------------------------------------------------------------------

IOReturn AppleIntelPIIXPATA::setPowerState( unsigned long stateIndex,
                                            IOService *   whatDevice )
{
    if ( stateIndex == kPIIXPowerStateOff )
    {
        _initTimingRegisters = true;
    }
    else if ( _initTimingRegisters )
    {
        writeTimingRegisters();
        _initTimingRegisters = false;
    }

    return IOPMAckImplied;
}

#pragma mark -
#pragma mark acpi code
IOACPIPlatformDevice*
AppleIntelPIIXPATA::getACPIParent( void )
{

	// get the ACPI path
	OSObject* acpi_path_object = NULL;
	OSString* acpi_path = NULL;
	IORegistryEntry* acpi_device_entry = NULL;
	IOACPIPlatformDevice* acpi_device = NULL;
	
	acpi_path_object = _provider->getProvider()->getProvider()->getProperty("acpi-path");
	if( acpi_path_object != NULL )
	{
		acpi_path = OSDynamicCast( OSString, acpi_path_object);
		if( acpi_path )
		{
		
			// get the acpi nub
			acpi_device_entry = IORegistryEntry::fromPath( acpi_path->getCStringNoCopy() );
			if( acpi_device_entry )
			{
			
				if( acpi_device_entry->metaCast( "IOACPIPlatformDevice") )
				{
					DLOG( "++++++--->>> PIIX got the ACPI device\n");
					acpi_device = ( IOACPIPlatformDevice*) acpi_device_entry;
					
				}
			
			} else {
			
				DLOG("****------> PIIXATA could not get registry acpi_path entry \n");
			
			}
		
		
		} else {
		
			DLOG("****------> PIIXATA could not cast acpi_path\n");
		}
		
	
	
	
	} else {
	
		DLOG("------> ** PIIX ATA no acpi-path\n");
	
	}
	return acpi_device;
}

bool
AppleIntelPIIXPATA::hasMediaNotify(IOACPIPlatformDevice* acpi_device)
{
	
	OSData *compatibleEntry;
	bool result = false;
	
	compatibleEntry = OSDynamicCast( OSData, acpi_device->getProperty( "compatible") );
	if( compatibleEntry )
	{
		
		if( compatibleEntry->isEqualTo( "media-notify" , 12 ) )
		{
				result = true;
				DLOG( "PIIXATA has mediaNotify\n");
		}
		

	}
	
	return result;
}

void
AppleIntelPIIXPATA::turnOffDrive( void )
{

    if (_intSrc) 
		_intSrc->disable();

	// drive-low the ATA interface
	UInt32 maskBits = 0x3 << 16;
	UInt32 disableBits = 0x02 << 16;
	UInt32 ideConfigBits = _pciDevice->configRead32( kPIIX_PCI_IDECONFIG );
	
	ideConfigBits &= ~maskBits;
	
	_pciDevice->configWrite32( kPIIX_PCI_IDECONFIG , ( disableBits | ideConfigBits));
	
	// turn off the power
	_pciACPIDevice->evaluateObject( "_PS3" );
	
	_drivePowerOn = false;
	
	// clean up any interrupt glitches left over from powering down the drive. 
	*_bmStatusReg = kPIIX_IO_BMISX_IDEINTS;
    if (_intSrc) 
		_intSrc->enable();
}

void
AppleIntelPIIXPATA::turnOnDrive( void )
{
    if (_intSrc) 
		_intSrc->disable();

	_drivePowerOn = true;
	
	// turn on the power
	_pciACPIDevice->evaluateObject( "_PS0" );

	// enable the interface
	UInt32 disableBits = 0x3 << 16;
	UInt32 ideConfigBits =  _pciDevice->configRead32( kPIIX_PCI_IDECONFIG ); 
	
	_pciDevice->configWrite32( kPIIX_PCI_IDECONFIG , (~disableBits) &  ideConfigBits);
	
	IOSleep( 500 );
	
	// wait for the drive to go not Busy for up to 31 seconds
	bool resetFailed = true;
	for( int i = 0; i < 3100; i++)
	{
	
		UInt8 status = *_tfStatusCmdReg;
		if( (status & 0x80) == 0x00 )
		{	
			resetFailed = false;
			DLOG(" AppleIntelPIIX cleared busy in %d seconds\n", i);
			break;
		
		}
		IOSleep( 10 );
	}
	
	if( resetFailed )
	{
	
		DLOG("AppleIntelPIIX failed to clear busy on power control\n");
	
	}
 
	// clean up any interrupt glitches left over from powering up the drive. 
	*_bmStatusReg = kPIIX_IO_BMISX_IDEINTS;
    if (_intSrc) 
		_intSrc->enable();

	executeEventCallouts( kATAResetEvent, kATAInvalidDeviceID);

}

IOReturn
AppleIntelPIIXPATA::mediaInterestHandler( void* target, 
										void* refCon,
										UInt32 messageType, 
										IOService* provider,
										void* messageArgument,
										vm_size_t argSize)
{
	IOReturn result = kIOReturnSuccess;
	
	
	// target is this pointer
	AppleIntelPIIXPATA* self = (AppleIntelPIIXPATA*) target;
	// refCon is whatever I set
	//DLOG( "PIIXPata got messageType = %x\n", messageType);
	
	if( messageType == kIOACPIMessageDeviceNotification )
	{
	
		UInt32 messageVal = * ((UInt32*)messageArgument);
		
		switch( messageVal )
		{
			case 0x81:
				DLOG( "PIIXPata got insert message\n");
				self->_cmdGate->runAction(
					OSMemberFunctionCast( IOCommandGate::Action, self, &AppleIntelPIIXPATA::handleInsert) );
			break;
			
			case 0x82:
				DLOG( "PIIXPata got remove message \n");
				
			break;
			
			default:
				DLOG("PIIXPata got unknown ACPI message value\n");
				
		}
	
	}
	
	
	return result;


}


void 
AppleIntelPIIXPATA::completeIO( IOReturn commandResult)
{

	bool quiescePower = false;
	
	if (_currentCommand->getFlags() & mATAFlagQuiesce)
	{
		quiescePower = true;
	
	}

	super::completeIO( commandResult );
	
	if( quiescePower )
	{
		turnOffDrive();
	}


}

IOReturn 
AppleIntelPIIXPATA::dispatchNext( void )
{
	
	if( ! _drivePowerOn ) 
	{
		turnOnDrive();
	}

	return super::dispatchNext();

}

IOReturn
AppleIntelPIIXPATA::handleInsert( void )
{
	
	DLOG("AppleIntelPIIXPATA handling Insertion\n");

	if( ! _drivePowerOn )
	{
	
		turnOnDrive();
	
	}

	else
	{
		
		executeEventCallouts( ( ataEventCode ) kATANewMediaEvent, kATAInvalidDeviceID);
		
	}
	
	return kIOReturnSuccess;
	
}


IOReturn
AppleIntelPIIXPATA::selectDevice( ataUnitID unit )
{
	
	DLOG("%s::%s( %d )\n", getName(), __FUNCTION__, unit);
	
	IOReturn result = kIOReturnSuccess;
	
	// temporarily disable interrupts
    if (_intSrc) _intSrc->disable();
	
	// let the superclass switch drives
	result = super::selectDevice( unit );
	
	// clean up any interrupt glitches left over from switching drives.
	*_bmStatusReg = kPIIX_IO_BMISX_IDEINTS;
	
	// re-eanble interrupts
    if (_intSrc) _intSrc->enable();
	
	return result;
	
}


void
AppleIntelPIIXPATA::executeEventCallouts( ataEventCode event, ataUnitID unit )
{
    if( polledPATAAdapter && polledPATAAdapter->isPolling())
    {
		return;
    }
    super::executeEventCallouts(event, unit);
}

IOReturn 
AppleIntelPIIXPATA::startTimer( UInt32 inMS)
{
    if( polledPATAAdapter && polledPATAAdapter->isPolling())
    {
		return kIOReturnSuccess;
    }
    return super::startTimer( inMS);
}

void
AppleIntelPIIXPATA::stopTimer(void)
{
    if( polledPATAAdapter && polledPATAAdapter->isPolling())
    {
		return;
    }
    return super::stopTimer( );
}



void 
AppleIntelPIIXPATA::pollEntry( void )
{
	//kprintf( "+ PIIXPata pollEntry\n");
	
	// make sure there is a current command before processing further.
    if( 0 == _currentCommand )
		return;

    if ( *(_bmStatusReg) & kPIIX_IO_BMISX_IDEINTS )
    {
		// Clear interrupt latch
		*(_bmStatusReg) = kPIIX_IO_BMISX_IDEINTS;

		// Let our superclass handle the interrupt to advance to the next state
		// in its internal state machine.
		//kprintf( "PIIXPata pollEntry INTRQ is set, completing request\n");

		handleDeviceInterrupt();
    }
	
	//kprintf( "- PIIXPata pollEntry\n");
	
}

void 
AppleIntelPIIXPATA::transitionFixup( void )
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


//---------------------------------------------------------------------------
// begin implementation AppleIntelICHxPATAPolledAdapter
// --------------------------------------------------------------------------
#undef super
#define super IOPolledInterface

OSDefineMetaClassAndStructors(  AppleIntelICHxPATAPolledAdapter, IOPolledInterface )

IOReturn 
AppleIntelICHxPATAPolledAdapter::probe(IOService * target)
{
    pollingActive = false;
    return kIOReturnSuccess;
}

IOReturn 
AppleIntelICHxPATAPolledAdapter::open( IOOptionBits state, IOMemoryDescriptor * buffer)
{
   // kprintf( "Opening PATAPolledAdapter state= 0x%lx\n", state);
	
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
AppleIntelICHxPATAPolledAdapter::close(IOOptionBits state)
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
AppleIntelICHxPATAPolledAdapter::startIO(uint32_t 	        operation,
					 uint32_t		bufferOffset,
					 uint64_t	        deviceOffset,
					 uint64_t	        length,
					 IOPolledCompletion	completion)
{
    return kIOReturnUnsupported;
}

IOReturn 
AppleIntelICHxPATAPolledAdapter::checkForWork(void)
{

    if( owner )
    {
		owner->pollEntry();
    }

    return kIOReturnSuccess;
}


bool 
AppleIntelICHxPATAPolledAdapter::isPolling( void )
{
    return pollingActive;
}

void
AppleIntelICHxPATAPolledAdapter::setOwner( AppleIntelPIIXPATA* myOwner )
{
    owner = myOwner;
    pollingActive = false;
}

