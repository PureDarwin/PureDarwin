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

#ifndef _APPLEINTELPIIXATAROOT_H
#define _APPLEINTELPIIXATAROOT_H

#include <IOKit/IOLocks.h>
#include <IOKit/pci/IOPCIDevice.h>

class AppleIntelPIIXATARoot : public IOService
{
    OSDeclareDefaultStructors( AppleIntelPIIXATARoot )

protected:
    OSSet *       _nubs;
    OSSet *       _openNubs;
    IOPCIDevice * _provider;
    IOLock *      _pciConfigLock;

    virtual OSSet * createATAChannelNubs( void );

    virtual OSDictionary * createNativeModeChannelInfo( UInt32 ataChannel,
                                                        UInt32 channelMode );

    virtual OSDictionary * createLegacyModeChannelInfo( UInt32 ataChannel,
                                                        UInt32 channelMode );

    virtual OSDictionary * createChannelInfo( UInt32 ataChannel,
                                              UInt32 channelMode,
                                              UInt16 commandPort,
                                              UInt16 controlPort,
                                              UInt8  interruptVector );

    virtual IORegistryEntry * getDTChannelEntry( int channelID );

public:
    virtual IOService * probe( IOService * provider,
                               SInt32 *    score );

    virtual bool start( IOService * provider );

    virtual void free( void );

    virtual bool handleOpen( IOService *  client,
                             IOOptionBits options,
                             void *       arg );
    
    virtual void handleClose( IOService *  client,
                              IOOptionBits options );

    virtual bool handleIsOpen( const IOService * client ) const;

    virtual void pciConfigWrite8( UInt8 offset,
                                  UInt8 data,
                                  UInt8 mask = 0xff );
    
    virtual void pciConfigWrite16( UInt8  offset,
                                   UInt16 data,
                                   UInt16 mask = 0xffff );

	virtual void setSerialATAPortEnable( UInt32 port, bool enable );

	virtual bool getSerialATAPortPresentStatus( UInt32 port );

    virtual bool serializeProperties( OSSerialize * s ) const;
};

#endif /* !_APPLEINTELPIIXATAROOT_H */
