/*
 * Copyright (c) 2009-2018 Apple Inc. All rights reserved.
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
 * DHCPDUID.h
 * - definition of DHCP DUID
 */

/* 
 * Modification History
 *
 * September 30, 2009		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _S_DHCPDUID_H
#define _S_DHCPDUID_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <CoreFoundation/CFString.h>
#include "nbo.h"
#include "symbol_scope.h"

enum {
    kDHCPDUIDTypeNone = 0,
    kDHCPDUIDTypeLLT = 1,
    kDHCPDUIDTypeEN = 2,
    kDHCPDUIDTypeLL = 3
};

typedef int  	DHCPDUIDType;

typedef struct {
    uint8_t		duid_type[2];	/* 1 */
    uint8_t		hardware_type[2];
    uint8_t		time[4]; /* seconds since midnight UTC 2000/01/01 */
    uint8_t		linklayer_address[1]; /* variable length */
} DHCPDUID_LLT, * DHCPDUID_LLTRef;

typedef struct {
    uint8_t		duid_type[2];	/* 2 */
    uint8_t		enterprise_number[4];
    uint8_t		identifier[1]; /* variable length */
} DHCPDUID_EN, * DHCPDUID_ENRef;

typedef struct {
    uint8_t		duid_type[2];	/* 3 */
    uint8_t		hardware_type[2];
    uint8_t		linklayer_address[1]; /* variable length */
} DHCPDUID_LL, * DHCPDUID_LLRef;

typedef union {
    DHCPDUID_LLT	llt;    
    DHCPDUID_EN		en;
    DHCPDUID_LL		ll;
} DHCPDUID, * DHCPDUIDRef;


INLINE uint16_t
DHCPDUIDGetType(const DHCPDUIDRef duid)
{
    return (net_uint16_get(duid->llt.duid_type));
}

INLINE void
DHCPDUIDSetType(DHCPDUIDRef duid, uint16_t duid_type)
{
    net_uint16_set(duid->llt.duid_type, duid_type);
    return;
}

void
DHCPDUIDPrintToString(CFMutableStringRef str,
		      const DHCPDUIDRef duid, int duid_len);

INLINE uint16_t
DHCPDUID_LLTGetHardwareType(const DHCPDUID_LLTRef llt)
{
    return (net_uint16_get(llt->hardware_type));
    
}

INLINE void
DHCPDUID_LLTSetHardwareType(DHCPDUID_LLTRef llt, uint16_t hardware_type)
{
    net_uint16_set(llt->hardware_type, hardware_type);
    return;
}

INLINE uint32_t
DHCPDUID_LLTGetTime(const DHCPDUID_LLTRef llt)
{
    return (net_uint32_get(llt->time));
    
}

INLINE void
DHCPDUID_LLTSetTime(DHCPDUID_LLTRef llt, uint32_t time)
{
    net_uint32_set(llt->time, time);
    return;
}

INLINE uint16_t
DHCPDUID_ENGetEnterpriseNumber(const DHCPDUID_ENRef en)
{
    return (net_uint32_get(en->enterprise_number));
    
}

INLINE void
DHCPDUID_ENSetEnterpriseNumber(DHCPDUID_ENRef en, uint32_t ent_num)
{
    net_uint32_set(en->enterprise_number, ent_num);
    return;
}

INLINE uint16_t
DHCPDUID_LLGetHardwareType(const DHCPDUID_LLRef ll)
{
    return (net_uint16_get(ll->hardware_type));
    
}

INLINE void
DHCPDUID_LLSetHardwareType(DHCPDUID_LLRef ll, uint16_t hardware_type)
{
    net_uint16_set(ll->hardware_type, hardware_type);
    return;
}

bool
DHCPDUIDIsValid(const DHCPDUIDRef duid, int duid_len);

CFDataRef
DHCPDUID_LLDataCreate(const void * ll_addr, int ll_len, int ll_type);

CFDataRef
DHCPDUID_LLTDataCreate(const void * ll_addr, int ll_len, int ll_type);

#endif /* _S_DHCPDUID_H */
