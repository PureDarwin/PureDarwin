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

#ifndef _APPLEINTELICHXSATA_H
#define _APPLEINTELICHXSATA_H

#include "AppleIntelPIIXPATA.h"
#ifndef kIOPolledInterfaceSupportKey
#include <IOKit/IOPolledInterface.h>
#endif

class AppleIntelICHxSATA : public AppleIntelPIIXPATA
{
    OSDeclareDefaultStructors( AppleIntelICHxSATA )

    class AppleIntelICHxSATAPolledAdapter* polledAdapter;

protected:
    bool             _initPortEnable;

    virtual IOReturn selectDevice( ataUnitID unit );

    //override for polling
    virtual void executeEventCallouts(  ataEventCode event, ataUnitID unit);
    virtual IOReturn startTimer( UInt32 inMS);
    virtual void stopTimer( void );

public:
    virtual bool     start( IOService * provider );

    virtual IOReturn provideBusInfo( IOATABusInfo * infoOut );
    
    virtual UInt32   scanForDrives( void );

    virtual IOReturn setPowerState( unsigned long stateIndex,
                                    IOService *   whatDevice );

public:
    virtual void pollEntry( void );
    virtual void transitionFixup( void );
};

class AppleIntelICHxSATAPolledAdapter : public IOPolledInterface

{
    OSDeclareDefaultStructors(AppleIntelICHxSATAPolledAdapter)

protected:
    AppleIntelICHxSATA* owner;
    bool pollingActive;

public:
    virtual IOReturn probe(IOService * target);
    virtual IOReturn open( IOOptionBits state, IOMemoryDescriptor * buffer);
    virtual IOReturn close(IOOptionBits state);

    virtual IOReturn startIO(uint32_t 	        operation,
                             uint32_t           bufferOffset,
                             uint64_t	        deviceOffset,
                             uint64_t	        length,
                             IOPolledCompletion completion) ;

    virtual IOReturn checkForWork(void);
	
    bool isPolling( void );
    
    void setOwner( AppleIntelICHxSATA* owner );
};

#endif /* !_APPLEINTELICHXSATA_H */
