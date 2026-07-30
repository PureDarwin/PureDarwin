/*
 *Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 *@APPLE_LICENSE_HEADER_START@
 *
 *The contents of this file constitute Original Code as defined in and
 *are subject to the Apple Public Source License Version 1.1 (the
 *"License").  You may not use this file except in compliance with the
 *License.  Please obtain a copy of the License at
 *http://www.apple.com/publicsource and read it before using this file.
 *
 *This Original Code and all software distributed under the License are
 *distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *License for the specific language governing rights and limitations
 *under the License.
 *
 *@APPLE_LICENSE_HEADER_END@
 */
/*
 *  IOSerialStreamSync.h
 *
 *  2000-10-21	gvdl	Initial real change to IOKit serial family.
 *
 */

#ifndef	_SERIAL_IOSERIALSTREAMSYNC_H
#define	_SERIAL_IOSERIALSTREAMSYNC_H

#include <IOKit/IOService.h>

#ifdef __LP64__

#define PD_DATA_MASK 0x03U
#define PD_DATA_VOID 0x00U
#define PD_DATA_BYTE 0x01U
#define PD_DATA_WORD 0x02U
#define PD_DATA_LONG 0x03U

#else

#define PD_DATA_MASK 0x03UL
#define PD_DATA_VOID 0x00UL
#define PD_DATA_BYTE 0x01UL
#define PD_DATA_WORD 0x02UL
#define PD_DATA_LONG 0x03UL

#endif

#define PD_OP(x)	((x) << 2)
#define PD_E_MASK	(~PD_DATA_MASK)

/* General Command & Status events */
#define PD_E_EOQ		(PD_DATA_VOID|PD_OP( 0))	// Always nextEvent on empty queue
#define PD_E_ACTIVE		(PD_DATA_BYTE|PD_OP( 1))	// BOOL start/shutdown chip

/* Queue management events - all units are multiples of characters
 *based on the current character size
 */
#define PD_E_TXQ_SIZE 		(PD_DATA_LONG|PD_OP( 2))
#define PD_E_RXQ_SIZE		(PD_DATA_LONG|PD_OP( 3))
#define PD_E_TXQ_LOW_WATER	(PD_DATA_LONG|PD_OP( 4))
#define PD_E_RXQ_LOW_WATER	(PD_DATA_LONG|PD_OP( 5))
#define PD_E_TXQ_HIGH_WATER	(PD_DATA_LONG|PD_OP( 6))
#define PD_E_RXQ_HIGH_WATER	(PD_DATA_LONG|PD_OP( 7))
#define PD_E_TXQ_AVAILABLE	(PD_DATA_LONG|PD_OP( 8))
#define PD_E_RXQ_AVAILABLE	(PD_DATA_LONG|PD_OP( 9))
#define PD_E_TXQ_FLUSH 		(PD_DATA_VOID|PD_OP(10))
#define PD_E_RXQ_FLUSH		(PD_DATA_LONG|PD_OP(11))

/* port & link characteristics - units are half bits */
#define PD_E_DATA_RATE 		(PD_DATA_LONG|PD_OP(12))	// raw bits of symbols per second
#define PD_E_RX_DATA_RATE	(PD_DATA_LONG|PD_OP(13))
#define PD_E_DATA_SIZE 		(PD_DATA_LONG|PD_OP(14))	// raw bits per symbol
#define PD_E_RX_DATA_SIZE	(PD_DATA_LONG|PD_OP(15))
#define PD_E_DATA_INTEGRITY	(PD_DATA_LONG|PD_OP(16))	// parity or ECC method
#define PD_E_RX_DATA_INTEGRITY	(PD_DATA_LONG|PD_OP(17))
#define PD_E_EXTERNAL_CLOCK_MODE (PD_DATA_LONG|PD_OP(28))	// multiplier between clock and data rates

/* timeout events */
#define PD_E_DELAY		(PD_DATA_LONG|PD_OP(18))	// inter-character delay (us)
#define PD_E_DATA_LATENCY	(PD_DATA_LONG|PD_OP(19))	// A dequeueData Latency timeout
								// rcs Secondary Timer mainly to fix performance 
								// problems with MIDI and IRDA drivers.
								// Midi and Irda drivers will issue an IOCTL
								// call with this as the selector and
								// the Data as the value they want the rate set
								// to (Typically 1 for IrDA devices) 
#define PD_E_FLOW_CONTROL       (PD_DATA_LONG|PD_OP(20))  	// enable/disable each flow mechanism

/* I/O events */
#define PD_E_VALID_DATA 	(PD_DATA_VOID|PD_OP(21))	// A valid frame was received
#define PD_E_VALID_DATA_BYTE 	(PD_DATA_BYTE|PD_E_VALID_DATA)
#define PD_E_VALID_DATA_WORD 	(PD_DATA_WORD|PD_E_VALID_DATA)
#define PD_E_VALID_DATA_LONG 	(PD_DATA_LONG|PD_E_VALID_DATA)

#define PD_E_SPECIAL		(PD_DATA_VOID|PD_OP(22))	// valid data received with 'special' value
#define PD_E_SPECIAL_BYTE 	(PD_DATA_BYTE|PD_E_SPECIAL)
#define PD_E_SPECIAL_WORD 	(PD_DATA_WORD|PD_E_SPECIAL)
#define PD_E_SPECIAL_LONG 	(PD_DATA_LONG|PD_E_SPECIAL)

#define PD_E_FRAMING_ERROR 	(PD_DATA_VOID|PD_OP(23))	// Failure occured assembling frame
#define PD_E_FRAMING_BYTE 	(PD_DATA_BYTE|PD_E_FRAMING_ERROR)
#define PD_E_FRAMING_WORD 	(PD_DATA_WORD|PD_E_FRAMING_ERROR)
#define PD_E_FRAMING_LONG 	(PD_DATA_LONG|PD_E_FRAMING_ERROR)

#define PD_E_INTEGRITY_ERROR 	(PD_DATA_VOID|PD_OP(24))	// An uncorrectable frame was received
#define PD_E_INTEGRITY_BYTE 	(PD_DATA_BYTE|PD_E_INTEGRITY_ERROR)
#define PD_E_INTEGRITY_WORD 	(PD_DATA_WORD|PD_E_INTEGRITY_ERROR)
#define PD_E_INTEGRITY_LONG 	(PD_DATA_LONG|PD_E_INTEGRITY_ERROR)

#define PD_E_INTEGRITY_WARNING 	(PD_DATA_VOID|PD_OP(25))	// A correctable frame was received
#define PD_E_HW_OVERRUN_ERROR 	(PD_DATA_VOID|PD_OP(26))	// Hardware's buffer overflowed, data lost
#define PD_E_SW_OVERRUN_ERROR 	(PD_DATA_VOID|PD_OP(27))	// Driver's buffer overflowed, data lost

/*
 *The variables below make up the bits returned
 *by the State methods:- getState, setState, watchState
 */
#define PD_S_MASK		0xffff0000UL
#define PD_S_RX_OFFSET		7

#define PD_S_ACQUIRED		0x80000000UL
#define PD_S_ACTIVE		0x40000000UL

#define PD_S_TX_ENABLE		0x20000000UL
#define PD_S_TX_BUSY		0x10000000UL
#define PD_S_TX_EVENT 		0x08000000UL
#define PD_S_TXQ_EMPTY 		0x04000000UL
#define PD_S_TXQ_LOW_WATER 	0x02000000UL
#define PD_S_TXQ_HIGH_WATER 	0x01000000UL
#define PD_S_TXQ_FULL 		0x00800000UL
#define PD_S_TXQ_MASK		( PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER \
                                | PD_S_TXQ_FULL  | PD_S_TXQ_HIGH_WATER )

#define PD_S_RX_ENABLE		(PD_S_TX_ENABLE      >> PD_S_RX_OFFSET)
#define PD_S_RX_BUSY		(PD_S_TX_BUSY        >> PD_S_RX_OFFSET)
#define PD_S_RX_EVENT		(PD_S_TX_EVENT       >> PD_S_RX_OFFSET)
#define PD_S_RXQ_EMPTY 		(PD_S_TXQ_EMPTY      >> PD_S_RX_OFFSET)
#define PD_S_RXQ_LOW_WATER	(PD_S_TXQ_LOW_WATER  >> PD_S_RX_OFFSET)
#define PD_S_RXQ_HIGH_WATER	(PD_S_TXQ_HIGH_WATER >> PD_S_RX_OFFSET)
#define PD_S_RXQ_FULL 		(PD_S_TXQ_FULL       >> PD_S_RX_OFFSET)
#define PD_S_RXQ_MASK		(PD_S_TXQ_MASK       >> PD_S_RX_OFFSET)

class IOSerialDriverSync;
class IOSerialStreamSync : public IOService
{
    OSDeclareDefaultStructors(IOSerialStreamSync);

protected:
    IOSerialDriverSync *fProvider;
    void *fRefCon;

public:
    /* acquire tests and sets the state of the port object.  If the port was
    *available, then the state is set to busy, and kIOReturnSuccess is returned.
    *If the port was already busy and sleep is YES, then the thread will sleep
    *until the port is freed, then re-attempts the acquire.  If the port was
    *already busy and sleep in NO, then kIOReturnExclusiveAccess is returned.
    */   
    virtual IOReturn acquirePort(bool sleep);
    
    /* release sets the state of the port object to available and wakes up any
    *threads sleeping for access to this port.  It will return kIOReturnSuccess
    *if the port was in a busy state, and kIOReturnNotOpen if it was available.
    */
    virtual IOReturn releasePort();
    
    /*
     *Set the state for the port device.
     */
    virtual IOReturn setState(UInt32 state, UInt32 mask);

    /*
     *Get the state for the port device.
     */
    virtual UInt32 getState();
    
    /*
     *Wait for the at least one of the state bits defined in mask to be equal
     *to the value defined in state.
     *Check on entry then sleep until necessary.
     */
    virtual IOReturn watchState(UInt32 *state, UInt32 mask);

    /* nextEvent returns the type of the next event on the RX queue.  If no
     *events are present on the RX queue, then EOQ is returned.
     */
    virtual UInt32 nextEvent();

    /* executeEvent causes the specified event to be processed immediately.
     *This is primarily used for channel control commands like START & STOP
     */
    virtual IOReturn executeEvent(UInt32 event, UInt32 data);

    /* requestEvent processes the specified event as an immediate request and
     *returns the results in data.  This is primarily used for getting link
     *status information and verifying baud rate and such.
     */
    virtual IOReturn requestEvent(UInt32 event, UInt32 *data);

    /* enqueueEvent will place the specified event into the TX queue.  The
     *sleep argument allows the caller to specify the enqueueEvent's
     *behaviour when the TX queue is full.  If sleep is true, then this
     *method will sleep until the event is enqueued.  If sleep is false,
     *then enqueueEvent will immediatly return kIOReturnNoResources.
     */
    virtual IOReturn enqueueEvent(UInt32 event, UInt32 data, bool sleep);

    /* dequeueEvent will remove the oldest event from the RX queue and return
     *it in event & data.  The sleep argument defines the behaviour if the RX
     *queue is empty.  If sleep is true, then this method will sleep until an
     *event is available.  If sleep is false, then an EOQ event will be
     *returned.  In either case kIOReturnSuccess is returned.
     */
    virtual IOReturn dequeueEvent(UInt32 *event, UInt32 *data, bool sleep);

    /* enqueueData will attempt to copy data from the specified buffer to the
     *TX queue as a sequence of VALID_DATA events.  The argument bufferSize
     *specifies the number of bytes to be sent.  The actual number of bytes
     *transferred is returned in transferCount.  If sleep is true, then this
     *method will sleep until all bytes can be transferred.  If sleep is
     *false, then as many bytes as possible will be copied to the TX queue.
     */
    virtual IOReturn enqueueData(UInt8 *buffer,  UInt32 size, UInt32 *count, bool sleep );

    /* dequeueData will attempt to copy data from the RX queue to the specified
     *buffer.  No more than bufferSize VALID_DATA events will be transferred.
     *In other words, copying will continue until either a non-data event is
     *encountered or the transfer buffer is full.  The actual number of bytes
     *transferred is returned in transferCount.
     *
     *The sleep semantics of this method are slightly more complicated than
     *other methods in this API:  Basically, this method will continue to
     *sleep until either minCount characters have been received or a non
     *data event is next in the RX queue.  If minCount is zero, then this
     *method never sleeps and will return immediatly if the queue is empty.
     *
     *The latency parameter specifies the maximum amount of time that should
     *pass after the first character is available before the routine returns.
     *This allows the caller to specify a 'packet' timeout.  The unit of the
     *latency parameter is microseconds, though the exact delay may vary
     *depending on the granularity of the timeout services available to the
     *driver.
     */
    virtual IOReturn dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);

    /*	Internal for IOSerialDriver only */
    virtual bool init(OSDictionary *dictionary = 0, void *refCon = 0);
    virtual bool attach(IOService *provider) APPLE_KEXT_OVERRIDE;
    void *getRefCon() const { return fRefCon; }

OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  0);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  1);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  2);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  3);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  4);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  5);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  6);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  7);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  8);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync,  9);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync, 10);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync, 11);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync, 12);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync, 13);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync, 14);
OSMetaClassDeclareReservedUnused(IOSerialStreamSync, 15);
};

#endif /* !_SERIAL_IOSERIALSTREAMSYNC_H */


