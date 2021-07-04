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

#if defined( __i386__ ) || defined( __x86_64__ )

#include <IOKit/IOTypes.h>
#include <architecture/i386/pio.h>  // x86 IN/OUT inline asm
#include "IOATARegI386.h"

OSDefineMetaClassAndAbstractStructors( IOATAReg8,  OSObject )
OSDefineMetaClassAndAbstractStructors( IOATAReg16, OSObject )
OSDefineMetaClassAndAbstractStructors( IOATAReg32, OSObject )

OSDefineMetaClassAndStructors( IOATAIOReg8,  IOATAReg8  )
OSDefineMetaClassAndStructors( IOATAIOReg16, IOATAReg16 )
OSDefineMetaClassAndStructors( IOATAIOReg32, IOATAReg32 )

#define ImplementIOATAIOReg(w, s)                             \
IOATAIOReg##w * IOATAIOReg##w::withAddress( UInt16 address )  \
{                                                             \
    IOATAIOReg##w * reg = new IOATAIOReg##w;                  \
                                                              \
    if ( reg && !reg->initWithAddress(address) )              \
    {                                                         \
        reg->release();                                       \
        reg = 0;                                              \
    }                                                         \
    return reg;                                               \
}                                                             \
                                                              \
bool IOATAIOReg##w::initWithAddress( UInt16 address )         \
{                                                             \
    if ( IOATAReg##w::init() == false ) return false;         \
    _address = address;                                       \
    return true;                                              \
}                                                             \
                                                              \
UInt16 IOATAIOReg##w::getAddress() const                      \
{                                                             \
    return _address;                                          \
}                                                             \
                                                              \
void IOATAIOReg##w::operator = (UInt##w rhs)                  \
{                                                             \
    out##s(_address, rhs);                                    \
}                                                             \
                                                              \
IOATAIOReg##w::operator UInt##w() const                       \
{                                                             \
    return in##s(_address);                                   \
}

ImplementIOATAIOReg( 8,  b )
ImplementIOATAIOReg( 16, w )
ImplementIOATAIOReg( 32, l )

#endif /* __i386__ || __x86_64__ */
