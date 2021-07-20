/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Michael Burg
 */

#include <architecture/i386/pio.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include "Apple8259PIC.h"

// This must agree with the trap number reported by the low-level
// interrupt handler (osfmk/i386/locore.s).

#define kIntelReservedIntVectors  0x40

OSSymbol * gIntelPICName;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors(Apple8259PIC, IOInterruptController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool Apple8259PIC::start(IOService * provider)
{
    gIntelPICName = (OSSymbol *) OSSymbol::withCStringNoCopy("8259-pic");
    
    kprintf("Apple8259PIC::start(): ");
    IOInterruptAction  handler;
    
    if ( super::start(provider) == false ) {
        kprintf("parent failed\n");
        return false;
    }
    
    // Allocate the memory for the vectors.
    
    vectors = (IOInterruptVector *) IOMalloc( kNumVectors *
                                             sizeof(IOInterruptVector) );
    if ( vectors == NULL ) {
        kprintf("failed to allocate interrupt vector\n");
        return false;
    }
    
    bzero(vectors, kNumVectors * sizeof(IOInterruptVector));
    
    // Allocate locks for the vectors.
    
    for ( int cnt = 0; cnt < kNumVectors; cnt++ )
    {
        vectors[cnt].interruptLock = IOLockAlloc();
        
        if ( vectors[cnt].interruptLock == NULL )
        {
            kprintf("failed to allocate locks\n");
            return false;
        }
    }
    
    // Mask out the interrupts except for the casacde line.
    
    _interruptMasks = 0xffff & ~(1 << kPICSlaveID);
    
    // Initialize master PIC.
    
    initializePIC( kPIC1BasePort,
                  /* ICW1 */   kPIC_ICW1_IC4,
                  /* ICW2 */   kIntelReservedIntVectors,
                  /* ICW3 */   (1 << kPICSlaveID),
                  /* ICW4 */   kPIC_ICW4_uPM );
    
    // Write to OCW1, OCW3, OCW2.
    // The priority order is changed to (highest to lowest)
    // 3 4 5 6 7 0 1 2
    // The default priority after initialization is (highest to lowest)
    // 0 1 2 3 4 5 6 7
    
    outb( kPIC_OCW1(kPIC1BasePort), _interruptMasks & 0xff );
    outb( kPIC_OCW3(kPIC1BasePort), kPIC_OCW3_MBO | kPIC_OCW3_RR );
    outb( kPIC_OCW2(kPIC1BasePort), kPIC_OCW2_R   |
         kPIC_OCW2_SL  |
         kPIC_OCW2_LEVEL(2) );
    
    // Initialize slave PIC.
    
    initializePIC( kPIC2BasePort,
                  /* ICW1 */   kPIC_ICW1_IC4,
                  /* ICW2 */   kIntelReservedIntVectors + 8,
                  /* ICW3 */   kPICSlaveID,
                  /* ICW4 */   kPIC_ICW4_uPM );
    
    // Write to OCW1, and OCW3.
    
    outb( kPIC_OCW1(kPIC2BasePort), _interruptMasks >> 8 );
    outb( kPIC_OCW3(kPIC2BasePort), kPIC_OCW3_MBO | kPIC_OCW3_RR );
    
    // Record trigger type.
    _interruptTriggerTypes = inb( kPIC1TriggerTypePort ) | ( inb( kPIC2TriggerTypePort ) << 8 );
    
    // Primary interrupt controller
    getPlatform()->setCPUInterruptProperties(provider);
    
    // Register the interrupt handler function so it can service interrupts.
    handler = getInterruptHandlerAddress();
    
    if ( provider->registerInterrupt(0, this, handler, 0) != kIOReturnSuccess )
        panic("Apple8259PIC: Failed to install platform interrupt handler");
    
    provider->enableInterrupt(0);
    enableInterrupt(0);
    
    // Register this interrupt controller so clients can find it.
    getPlatform()->registerInterruptController(gIntelPICName, this);
    
    kprintf("success!!\n");
    
    return true;
}

//---------------------------------------------------------------------------
// Free the interrupt controller object. Deallocate all resources.

void Apple8259PIC::free(void)
{
    if ( vectors )
    {
        for ( int cnt = 0; cnt < kNumVectors; cnt++ )
        {
            if (vectors[cnt].interruptLock)
                IOLockFree(vectors[cnt].interruptLock);
        }
        
        IOFree( vectors, kNumVectors * sizeof(IOInterruptVector) );
        vectors = 0;
    }
    
    super::free();
}

//---------------------------------------------------------------------------
// Initialize the PIC by sending the Initialization Command Words (ICW).

void Apple8259PIC::initializePIC( UInt16 port,
                                         UInt8  icw1, UInt8  icw2,
                                         UInt8  icw3, UInt8  icw4 )
{
    // Initialize 8259's. Start the initialization sequence by
    // issuing ICW1 (Initialization Command Word 1).
    // Bit 4 must be set.
    
    outb( kPIC_ICW1(port), kPIC_ICW1_MBO | icw1 );
    
    // ICW2
    // Upper 5 bits of the interrupt vector address. The lower three
    // bits are set according to the interrupt level serviced.
    
    outb( kPIC_ICW2(port), icw2 );
    
    // ICW3 (Master Device)
    // Set a 1 bit for each IR line that has a slave.
    
    outb( kPIC_ICW3(port), icw3 );
    
    // ICW4
    
    outb( kPIC_ICW4(port), icw4 );
}

//---------------------------------------------------------------------------
// Report whether the interrupt line is edge or level triggered.

int Apple8259PIC::getVectorType(long vectorNumber,
                                        IOInterruptVector * vector)
{
    return getTriggerType(vectorNumber);
}

//---------------------------------------------------------------------------
//

IOInterruptAction Apple8259PIC::getInterruptHandlerAddress(void)
{
    return OSMemberFunctionCast(IOInterruptAction,
                                this, &Apple8259PIC::handleInterrupt);
}

//---------------------------------------------------------------------------
// Handle an interrupt by servicing the 8259, and dispatch the
// handler associated with the interrupt vector.

IOReturn Apple8259PIC::handleInterrupt(void *      savedState,
                                               IOService * nub,
                                               int         source)
{
    IOInterruptVector * vector;
    long                vectorNumber;
    
    typedef void (*IntelClockFuncType)(void *);
    IntelClockFuncType	clockFunc;
    
    vectorNumber = source - kIntelReservedIntVectors;

    kprintf("Apple8259PIC::handleInterrupt %d (%d)\n", source, (int)vectorNumber);
    
    if (vectorNumber >= kNumVectors)
        return kIOReturnSuccess;
    
    // Disable and ack interrupt.
    
    disableInterrupt(vectorNumber);
    ackInterrupt( vectorNumber);
    
    // Process the interrupt.
    
    vector = &vectors[vectorNumber];
    
    vector->interruptActive = 1;
    
    if ( !vector->interruptDisabledSoft )
    {
        if ( vector->interruptRegistered )
        {
            // Call registered interrupt handler.
            
            if (vectorNumber == kClockIRQ)  // FIXME
            {
                clockFunc = (IntelClockFuncType) vector->handler;
                clockFunc(savedState);
            }
            else
            {
                kprintf("calling handler!!\n");
                vector->handler(vector->target, vector->refCon,
                                vector->nub, vector->source);
            }
            
            // interruptDisabledSoft flag may be set by the
            // handler to indicate that the interrupt should
            // be disabled.
            
            if ( vector->interruptDisabledSoft )
            {
                // Already "hard" disabled, set interruptDisabledHard
                // to indicate this.
                
                vector->interruptDisabledHard = 1;
            }
            else
            {
                // Re-enable the interrupt line.
                
                enableInterrupt(vectorNumber);
            }
        }
    }
    else
    {
        vector->interruptDisabledHard = 1;
    }
    
    vector->interruptActive = 0;
    
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
//

bool Apple8259PIC::vectorCanBeShared(long vectorNumber,
                                             IOInterruptVector * vector)
{
    if ( getVectorType(vectorNumber, vector) == kIOInterruptTypeLevel )
        return true;
    else
        return false;
}

//---------------------------------------------------------------------------
//

void Apple8259PIC::initVector(long vectorNumber,
                                      IOInterruptVector * vector)
{
    super::initVector((IOInterruptVectorNumber)vectorNumber, vector);
}

//---------------------------------------------------------------------------
//

void Apple8259PIC::disableVectorHard(long vectorNumber,
                                             IOInterruptVector * vector)
{
    // Sorry, cacade/slave interrupt line cannot be disable.
    
    if (vectorNumber == kPICSlaveID) return;
    
    disableInterrupt(vectorNumber);
}

//---------------------------------------------------------------------------
//

void Apple8259PIC::enableVector(long vectorNumber,
                                        IOInterruptVector * vector)
{
    enableInterrupt(vectorNumber);
}

void Apple8259PIC::prepareForSleep( void )
{
    // Mask all interrupts before platform sleep
    
//    _interruptMasks = 0xffff & ~(1 << kPICSlaveID);
//    writeInterruptMask(0);
}

void Apple8259PIC::resumeFromSleep( void )
{
    // idfk w/e fuck this shit i ahte you apple
}

IOReturn Apple8259PIC::callPlatformFunction( const OSSymbol * function,
                                         bool waitForFunction,
                                         void * param1, void * param2,
                                         void * param3, void * param4 )
{
    if ( function == _handleSleepWakeFunction )
    {
        if ( param1 )
            prepareForSleep();   /* prior to system sleep */
        else
            resumeFromSleep();   /* after system wake */
        
        return kIOReturnSuccess;
    }
        
    return super::callPlatformFunction( function, waitForFunction,
                                       param1, param2, param3, param4 );
}
