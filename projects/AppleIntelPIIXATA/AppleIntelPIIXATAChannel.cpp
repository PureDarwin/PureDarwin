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

#include <IOKit/IODeviceTreeSupport.h>
#include "AppleIntelPIIXATAChannel.h"
#include "AppleIntelPIIXATARoot.h"
#include "AppleIntelPIIXATAKeys.h"
#include "AppleIntelPIIXATAHW.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleIntelPIIXATAChannel, IOService )

//---------------------------------------------------------------------------

#define CastOSNumber(x) OSDynamicCast( OSNumber, x )
#define CastOSString(x) OSDynamicCast( OSString, x )

struct ChannelModeEntry {
    UInt8  maxDriveCount;
    UInt8  driveUnitToSATAPort[2];
};

static const ChannelModeEntry gChannelModeTable[ kChannelModeCount ] =
{
    { 0, { kSerialATAPortX, kSerialATAPortX } },
    { 2, { kSerialATAPortX, kSerialATAPortX } },
    { 1, { kSerialATAPort0, kSerialATAPortX } },
    { 1, { kSerialATAPort1, kSerialATAPortX } },
    { 2, { kSerialATAPort0, kSerialATAPort1 } },
    { 2, { kSerialATAPort1, kSerialATAPort0 } },
    { 2, { kSerialATAPort0, kSerialATAPort2 } },
    { 2, { kSerialATAPort1, kSerialATAPort3 } }
};

//---------------------------------------------------------------------------

bool AppleIntelPIIXATAChannel::getNumberValue( const char * propKey,
                                               void       * outValue,
                                               UInt32       outBits )
{
    OSNumber * num = CastOSNumber( getProperty( propKey ) );
    bool   success = false;
    
    if ( num )
    {
        success = true;

        switch ( outBits )
        {
            case 32:
                *(UInt32 *) outValue = num->unsigned32BitValue();
                break;
            
            case 16:
                *(UInt16 *) outValue = num->unsigned16BitValue();
                break;

            case 8:
                *(UInt8 *) outValue = num->unsigned8BitValue();
                break;
            
            default:
                success = false;
                break;
        }
    }
    return success;
}

//---------------------------------------------------------------------------
//
// Create the interrupt specifier/controller properties.
//

bool
AppleIntelPIIXATAChannel::setupInterrupt( IOService * provider, UInt32 line )
{
    IOReturn ret = provider->callPlatformFunction( "SetDeviceInterrupts",
                   /* waitForFunction */ false,
                   /* nub             */ this,
                   /* vectors         */ (void *) &line,
                   /* vectorCount     */ (void *) 1,
                   /* exclusive       */ (void *) false ); /* XXX */

    if (ret == kIOReturnSuccess) {
        return true;
    } else {
        return false;
    }
}

//---------------------------------------------------------------------------

void AppleIntelPIIXATAChannel::mergeProperties( OSDictionary * properties )
{
    OSCollectionIterator * propIter =
        OSCollectionIterator::withCollection( properties );

    if ( propIter )
    {
        const OSSymbol * propKey;
        while ((propKey = (const OSSymbol *)propIter->getNextObject()))
            setProperty(propKey, properties->getObject(propKey));

        propIter->release();
    }
}
            
//---------------------------------------------------------------------------
//
// Initialize the ATA channel.
//

bool AppleIntelPIIXATAChannel::init( IOService *       provider,
                                     OSDictionary *    properties,
                                     IORegistryEntry * dtEntry )
{
    if ( dtEntry )
    {
        if ( super::init( dtEntry, gIODTPlane ) == false ) return false;
        mergeProperties( properties );
    }
    else
    {
        if ( super::init(properties) == false ) return false;
    }

    _provider = provider;

    // Decode the command and control block addresses and interrupt line
    // properties.

    if ( !getNumberValue( kCommandBlockAddressKey, &_cmdBlock, 16 )
      || !getNumberValue( kControlBlockAddressKey, &_ctrBlock, 16 )
      || !getNumberValue( kInterruptVectorKey, &_irq, 8 )
      || !getNumberValue( kChannelNumberKey, &_channelNum, 32 )
      || !getNumberValue( kChannelModeKey, &_channelMode, 32 )
      || _channelMode >= kChannelModeCount
       ) return false;
    
    if ( provider )
    {
        OSString *  str;

        str = CastOSString( provider->getProperty( kTransferModesKey ) );
        if ( str )
        {
            UInt32 mask = strtoul( str->getCStringNoCopy(), NULL, 16 );
            _pioModeMask  = (UInt8) mask;
            _dmaModeMask  = (UInt8) (mask >> 8);
            _udmaModeMask = (UInt8) (mask >> 16);
        }

        str = CastOSString( provider->getProperty( kControllerNameKey ) );
        if ( str )
            _controllerName = str->getCStringNoCopy();

        _hasSharedDriveTimings = false;
        if ( provider->getProperty(kSharedDriveTimingsKey) == kOSBooleanTrue )
            _hasSharedDriveTimings = true;
    }

    if ( !setupInterrupt( provider, _irq ) )
        return false;

    setLocation( _channelNum ? "1" : "0" );

    return true;
}

//---------------------------------------------------------------------------
//
// Perform custom matching on channel drivers.
//

bool AppleIntelPIIXATAChannel::matchPropertyTable( OSDictionary * table,
                                                   SInt32 *       score )
{
    bool drvSATA = ( table->getObject( kSerialATAKey ) == kOSBooleanTrue );

    // Let the PATA driver handle parallel ATA channels, even for the
    // parallel ATA channel exposed by the SATA controller in combined
    // mode.

    if (( getChannelMode() != kChannelModePATA &&  drvSATA ) ||
        ( getChannelMode() == kChannelModePATA && !drvSATA ))
    {
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
//
// Handle open and close from our client.
//

bool AppleIntelPIIXATAChannel::handleOpen( IOService *  client,
                                           IOOptionBits options,
                                           void *       arg )
{
    bool ret = false;

    if ( _provider && _provider->open( this, options, arg ) )
    {
        ret = super::handleOpen( client, options, arg );
        if ( ret == false )
            _provider->close( this );
    }
    
    return ret;
}

void AppleIntelPIIXATAChannel::handleClose( IOService *  client,
                                            IOOptionBits options )
{
    super::handleClose( client, options );
    if ( _provider ) _provider->close( this );
}

//---------------------------------------------------------------------------
//
// Accessor functions to assist our client.
//

UInt16 AppleIntelPIIXATAChannel::getCommandBlockAddress( void ) const
{
    return _cmdBlock;
}

UInt16 AppleIntelPIIXATAChannel::getControlBlockAddress( void ) const
{
    return _ctrBlock;
}

UInt8 AppleIntelPIIXATAChannel::getInterruptVector( void ) const
{
    return _irq;
}

UInt32 AppleIntelPIIXATAChannel::getChannelNumber( void ) const
{
    return _channelNum;
}

UInt32 AppleIntelPIIXATAChannel::getChannelMode( void ) const
{
    return _channelMode;
}

void AppleIntelPIIXATAChannel::pciConfigWrite8( UInt8 offset,
                                                UInt8 data,
                                                UInt8 mask )
{
    if ( _provider )
         ((AppleIntelPIIXATARoot *) _provider)->pciConfigWrite8(
            offset, data, mask );
}

void AppleIntelPIIXATAChannel::pciConfigWrite16( UInt8  offset,
                                                 UInt16 data,
                                                 UInt16 mask )
{
    if ( _provider )
         ((AppleIntelPIIXATARoot *) _provider)->pciConfigWrite16(
            offset, data, mask );
}

UInt8 AppleIntelPIIXATAChannel::getPIOModeMask( void ) const
{
    return _pioModeMask;
}

UInt8 AppleIntelPIIXATAChannel::getDMAModeMask( void ) const
{
    return _dmaModeMask;
}

UInt8 AppleIntelPIIXATAChannel::getUltraDMAModeMask( void ) const
{
    return _udmaModeMask;
}

const char * AppleIntelPIIXATAChannel::getControllerName( void ) const
{
    return _controllerName ? _controllerName : "Unknown Controller";
}

bool AppleIntelPIIXATAChannel::hasSharedDriveTimings( void ) const
{
    return _hasSharedDriveTimings;
}

UInt32 AppleIntelPIIXATAChannel::getMaxDriveUnits( void ) const
{
    return gChannelModeTable[_channelMode].maxDriveCount;
}

UInt32 AppleIntelPIIXATAChannel::getSerialATAPortForDrive( UInt32 unit ) const
{
    if ( unit < 2 )
        return gChannelModeTable[_channelMode].driveUnitToSATAPort[unit];
    else
        return kSerialATAPortX;
}

void AppleIntelPIIXATAChannel::setSerialATAPortEnableForDrive(
                                UInt32 unit, bool enable )
{
    AppleIntelPIIXATARoot * root = (AppleIntelPIIXATARoot *) _provider;
    int     port = getSerialATAPortForDrive( unit );

    if (root)
        root->setSerialATAPortEnable( port, enable );
}

bool AppleIntelPIIXATAChannel::getSerialATAPortPresentStatusForDrive( UInt32 unit )
{
    AppleIntelPIIXATARoot * root = (AppleIntelPIIXATARoot *) _provider;
    int     port = getSerialATAPortForDrive( unit );
    bool    present = false;

    if (root)
        present = root->getSerialATAPortPresentStatus( port );

    return present;
}
