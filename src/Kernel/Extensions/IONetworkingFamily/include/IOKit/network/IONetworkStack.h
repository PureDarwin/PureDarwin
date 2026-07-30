/*
 * Copyright (c) 1998-2011 Apple Inc. All rights reserved.
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

#ifndef _IONETWORKSTACK_H
#define _IONETWORKSTACK_H

// User-client keys
//
#define kIONetworkStackUserCommandKey   "IONetworkStackUserCommand"
#define kIONetworkStackUserCommand      "IONetworkStackUserCommand"

enum {
    kIONetworkStackRegisterInterfaceWithUnit        = 0,
    kIONetworkStackRegisterInterfaceWithLowestUnit  = 1,
    kIONetworkStackRegisterInterfaceAll             = 2
};

#ifdef KERNEL

class IONetworkInterface;

class IONetworkStack : public IOService
{
    OSDeclareFinalStructors( IONetworkStack )

protected:
    OSSet *             _ifListNaming;
    OSArray *           _ifListDetach;
    OSArray *           _ifListAttach;
    OSDictionary *      _ifPrefixDict;
    IONotifier *        _ifNotifier;
    IORecursiveLock *   _stateLock;
    thread_call_t       _asyncThread;
    const OSSymbol *    _noBSDAttachSymbol;
    IONotifier *        _sleepWakeNotifier;

    static SInt32       orderNetworkInterfaces(
                            const OSMetaClassBase * obj1,
                            const OSMetaClassBase * obj2,
                            void *                  ref );

    virtual void        free( void ) APPLE_KEXT_OVERRIDE;

    bool                interfacePublished(
                            void *          refCon,
                            IOService *     service,
                            IONotifier *    notifier );

    void                asyncWork( void );

    bool                insertNetworkInterface(
                            IONetworkInterface * netif );

    void                removeNetworkInterface(
                            IONetworkInterface * netif );

    uint32_t            getNextAvailableUnitNumber(
                            const char *         name,
                            uint32_t             startingUnit );

    bool                reserveInterfaceUnitNumber(
                            IONetworkInterface * netif,
                            uint32_t             unit,
                            bool                 isUnitFixed,
                            bool *               attachToBSD );

    IOReturn            attachNetworkInterfaceToBSD(
                            IONetworkInterface * netif );

    IOReturn            registerAllNetworkInterfaces( void );

    IOReturn            registerNetworkInterface(
                            IONetworkInterface * netif,
                            uint32_t             unit,
                            bool                 isUnitFixed );

    static IOReturn     handleSystemSleep(
                            void * target, void * refCon,
                            UInt32 messageType, IOService * provider,
                            void * messageArgument, vm_size_t argSize );

public:
    virtual bool        start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual void        stop( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual bool        finalize( IOOptionBits options ) APPLE_KEXT_OVERRIDE;
    virtual bool        didTerminate( IOService *, IOOptionBits, bool * ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn    setProperties( OSObject * properties ) APPLE_KEXT_OVERRIDE;
};

#endif /* KERNEL */
#endif /* !_IONETWORKSTACK_H */
