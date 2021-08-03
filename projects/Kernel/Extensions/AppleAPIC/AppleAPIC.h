/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_APPLEAPIC_H
#define _IOKIT_APPLEAPIC_H

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

#if OSTYPES_K64_REV < 1
typedef long IOInterruptVectorNumber;
#endif

/* APIC direct register offsets */

enum {
    kOffsetIND    = 0x00,  /*  8-bits R/W Index             */
    kOffsetDAT    = 0x10,  /* 32-bits R/W Data              */
    kOffsetIRQPA  = 0x20,  /* 32-bits WO  IRQ Pin Assertion */
    kOffsetEOIR   = 0x40   /* 32-bits WO  EOI               */
};

#define IOAPIC_REG(reg) \
        (*((volatile UInt32 *)(_apicBaseAddr + kOffset##reg)))

/* APIC indirect registers indices */

enum {
    kIndexID      = 0x00,  /* Identification */
    kIndexVER     = 0x01,  /* Version        */
    kIndexARBID   = 0x02,  /* Arbitration ID */
    kIndexBOOT    = 0x03,  /* Boot Config    */
    kIndexRTLO    = 0x10,  /* Redirection table lower 32-bits */
    kIndexRTHI    = 0x11   /* Redirection table upper 32-bits */
};

/* Bit fields for indirect registers */

enum {
    /* ID Register */
    kIDAPICIDMask                   = 0x0F000000,
    kIDAPICIDShift                  = 24,
    
    /* Version Register */
    kVERMaxEntriesMask              = 0x00FF0000,
    kVERMaxEntriesShift             = 16,
    kVERPRQMask                     = 0x00008000,
    kVERPRQShift                    = 15,
    kVERVersionMask                 = 0x000000FF,
    kVERVersionShift                = 0,

    /* Arbitration ID Register */
    kARBIDArbitrationIDMask         = 0x0F000000,
    kARBIDArbitrationIDShift        = 24,

    /* Boot Configuration Register */
    kBOOTDeliveryTypeMask           = 0x00000001,
    kBOOTDeliveryTypeShift          = 0,

    /* Redirection Table Entries */
    kRTLOVectorNumberMask           = 0x000000FF,
    kRTLOVectorNumberShift          = 0,
    
    kRTLODeliveryModeMask           = 0x00000700,
    kRTLODeliveryModeShift          = 8,
    kRTLODeliveryModeFixed          = 0 << kRTLODeliveryModeShift,
    kRTLODeliveryModeLowestPriority = 1 << kRTLODeliveryModeShift,
    kRTLODeliveryModeSMI            = 2 << kRTLODeliveryModeShift,
    kRTLODeliveryModeNMI            = 4 << kRTLODeliveryModeShift,
    kRTLODeliveryModeINIT           = 5 << kRTLODeliveryModeShift,
    kRTLODeliveryModeExtINT         = 7 << kRTLODeliveryModeShift,

    kRTLODestinationModeMask        = 0x00000800,
    kRTLODestinationModeShift       = 11,
    kRTLODestinationModePhysical    = 0 << kRTLODestinationModeShift,
    kRTLODestinationModeLogical     = 1 << kRTLODestinationModeShift,

    kRTLODeliveryStatusMask         = 0x00001000,
    kRTLODeliveryStatusShift        = 12,

    kRTLOInputPolarityMask          = 0x00002000,
    kRTLOInputPolarityShift         = 13,
    kRTLOInputPolarityHigh          = 0 << kRTLOInputPolarityShift,
    kRTLOInputPolarityLow           = 1 << kRTLOInputPolarityShift,

    kRTLORemoteIRRMask              = 0x00004000,
    kRTLORemoteIRRShift             = 14,

    kRTLOTriggerModeMask            = 0x00008000,
    kRTLOTriggerModeShift           = 15,
    kRTLOTriggerModeEdge            = 0 << kRTLOTriggerModeShift,
    kRTLOTriggerModeLevel           = 1 << kRTLOTriggerModeShift,

    kRTLOMaskMask                   = 0x00010000,
    kRTLOMaskShift                  = 16,
    kRTLOMaskEnabled                = 0,
    kRTLOMaskDisabled               = kRTLOMaskMask,

    kRTHIExtendedDestinationIDMask  = 0x00FF0000,
    kRTHIExtendedDestinationIDShift = 16,
    
    kRTHIDestinationMask            = 0xFF000000,
    kRTHIDestinationShift           = 24
};

/* Redirection Table vector entry */

struct VectorEntry {
    UInt32 l32;
    UInt32 h32;
};

#define AppleAPIC AppleAPICInterruptController

class AppleAPIC : public IOInterruptController
{
    OSDeclareDefaultStructors( AppleAPICInterruptController )

protected:
    const OSSymbol *      _handleSleepWakeFunction;
	const OSSymbol *      _setVectorPhysicalDestination;

    // APIC registers are memory mapped.

    IOMemoryDescriptor *  _apicMemory;
    IOMemoryMap *         _apicMemoryMap;
    IOVirtualAddress      _apicBaseAddr;
    IOSimpleLock *        _apicLock;

    // The base global system interrupt number. This should be
    // zero for the first or only IO APIC in the system.

    IOInterruptVectorNumber _vectorBase;

    // A cache of entries in the vector table. Makes restoring
    // hardware context following system sleep easier, and also
    // avoids a register read on vector updates.

    VectorEntry *           _vectorTable;
    IOInterruptVectorNumber _vectorCount;

    // The APIC ID of the CPU that will handle the interrupt.
    // in physical mode.

    IOInterruptVectorNumber _destinationAddress;

    // ID register at register index 0, saved across sleep/wake.

    UInt32                _apicIDRegister;

    // Inline functions to read and write to the APIC
    // indirect registers. Must be accessed as 32-bit values.

    inline UInt32    indexRead( UInt32 index )
    {
        IOAPIC_REG( IND ) = index;
        return IOAPIC_REG( DAT );
    }

    inline void      indexWrite( UInt32 index, UInt32 value )
    {
        IOAPIC_REG( IND ) = index;
        IOAPIC_REG( DAT ) = value;
    }

    // Enable or disable (mask) a vector entry. Protected with
    // a spinlock with interrupt disabled.

    inline void      enableVectorEntry( IOInterruptVectorNumber vectorNumber )
    {
        IOInterruptState state;
        state = IOSimpleLockLockDisableInterrupt( _apicLock );
        _vectorTable[vectorNumber].l32 &= ~kRTLOMaskDisabled;
        indexWrite( kIndexRTLO + vectorNumber * 2,
                    _vectorTable[vectorNumber].l32 );
        IOSimpleLockUnlockEnableInterrupt( _apicLock, state );
    }

    inline void      disableVectorEntry( IOInterruptVectorNumber vectorNumber )
    {
        IOInterruptState state;
        state = IOSimpleLockLockDisableInterrupt( _apicLock );
        _vectorTable[vectorNumber].l32 |= kRTLOMaskDisabled;
        indexWrite( kIndexRTLO + vectorNumber * 2,
                    _vectorTable[vectorNumber].l32 );
        IOSimpleLockUnlockEnableInterrupt( _apicLock, state );
    }

    void             resetVectorTable( void );

    void             writeVectorEntry( IOInterruptVectorNumber vectorNumber );

    void             writeVectorEntry( IOInterruptVectorNumber vectorNumber,
                                       VectorEntry entry );

    void             dumpRegisters( void );

    void             prepareForSleep( void );

    void             resumeFromSleep( void );

    IOReturn         setVectorPhysicalDestination( UInt32 vectorNumber,
                                                   UInt32 apicID );

    virtual void     free( void );

public:
    virtual bool     start( IOService * provider );

    virtual IOReturn getInterruptType( IOService * nub,
                                       int   source,
                                       int * interruptType );

    virtual IOReturn registerInterrupt( IOService *        nub,
                                        int                source,
                                        void *             target,
                                        IOInterruptHandler handler,
                                        void *             refCon );

    virtual void     initVector( IOInterruptVectorNumber vectorNumber,
                                 IOInterruptVector * vector );

    virtual bool     vectorCanBeShared( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual void     enableVector( IOInterruptVectorNumber vectorNumber,
                                   IOInterruptVector * vector );

    virtual void     disableVectorHard( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual IOReturn handleInterrupt( void * savedState,
                                      IOService * nub,
                                      int source );

    virtual IOInterruptAction getInterruptHandlerAddress( void );

    virtual IOReturn callPlatformFunction( const OSSymbol * function,
                                           bool waitForFunction,
                                           void * param1, void * param2,
                                           void * param3, void * param4 );
};

#endif /* !_IOKIT_APPLEAPIC_H */
