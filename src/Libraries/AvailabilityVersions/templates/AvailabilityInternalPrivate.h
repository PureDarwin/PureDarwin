/*
 * Copyright (c) 2007-2017 by Apple Inc.. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
    File:       AvailabilityInternalPrivate.h
 
    Contains:   SPI_AVAILABLE macro implementation

*/
#ifndef __AVAILABILITY_INTERNAL_PRIVATE__
#define __AVAILABILITY_INTERNAL_PRIVATE__

#include <AvailabilityInternal.h>

/*
 * SPI Availability
 *
 * These macros complement their API counterparts, and behave the same
 * for Apple internal clients.
 *
 * All SPI macros will be transformed to API_UNAVAILABLE for the public SDK
 * to prevent 3rd party developers from using the symbol.
 *
 * SPI_AVAILABLE
 * <rdar://problem/37321035> API_PROHIBITED should support version numbers too
 * <rdar://problem/40864547> Define SPI_AVAILABLE as an alternative to API_PROHIBITED
 *
 * SPI_DEPRECATED
 * SPI_DEPRECATED_WITH_REPLACEMENT
 * <rdar://problem/41506001> For parity, define SPI_DEPRECATED to provide an SPI variant of API_DEPRECATED
 */ 

#if defined(__has_feature) && defined(__has_attribute)
 #if __has_attribute(availability)

// @@AVAILABILITY_MACRO_INTERFACE(__SPI_AVAILABLE,__API_AVAILABLE)@@
// @@AVAILABILITY_MACRO_INTERFACE(__SPI_AVAILABLE,__API_AVAILABLE_BEGIN,scoped_availablity=TRUE)@@

// @@AVAILABILITY_MACRO_INTERFACE(SPI_AVAILABLE,__API_AVAILABLE)@@
// @@AVAILABILITY_MACRO_INTERFACE(SPI_AVAILABLE,__API_AVAILABLE_BEGIN,scoped_availablity=TRUE)@@

// @@AVAILABILITY_MACRO_INTERFACE(__SPI_DEPRECATED,__API_DEPRECATED_MSG,argCount=1)@@
// @@AVAILABILITY_MACRO_INTERFACE(SPI_DEPRECATED,__API_DEPRECATED_MSG,argCount=1)@@

// @@AVAILABILITY_MACRO_INTERFACE(__SPI_DEPRECATED_WITH_REPLACEMENT,__API_DEPRECATED_REP,argCount=1)@@
// @@AVAILABILITY_MACRO_INTERFACE(SPI_DEPRECATED_WITH_REPLACEMENT,__API_DEPRECATED_REP,argCount=1)@@
   
 #endif /* __has_attribute(availability) */
#endif /*  #if defined(__has_feature) && defined(__has_attribute) */

#endif /* __AVAILABILITY_INTERNAL_PRIVATE__ */


