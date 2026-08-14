/*
 * Copyright (c) 2009-2020 Apple Inc. All rights reserved.
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
 * DHCPv6Options.h
 * - definitions and API's to handle DHCPv6 options
 */

/* 
 * Modification History
 *
 * September 18, 2009		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _S_DHCPV6OPTIONS_H
#define _S_DHCPV6OPTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "symbol_scope.h"
#include "DHCPv6.h"

typedef uint16_t DHCPv6OptionLength;

typedef CF_ENUM(uint16_t, DHCPv6OptionCode) {
    kDHCPv6OPTION_NONE			= 0,
    kDHCPv6OPTION_CLIENTID		= 1,
    kDHCPv6OPTION_SERVERID		= 2,
    kDHCPv6OPTION_IA_NA			= 3,
    kDHCPv6OPTION_IA_TA			= 4,
    kDHCPv6OPTION_IAADDR		= 5,
    kDHCPv6OPTION_ORO			= 6,
    kDHCPv6OPTION_PREFERENCE		= 7,
    kDHCPv6OPTION_ELAPSED_TIME		= 8,
    kDHCPv6OPTION_RELAY_MSG		= 9,
    kDHCPv6OPTION_AUTH			= 11,
    kDHCPv6OPTION_UNICAST		= 12,
    kDHCPv6OPTION_STATUS_CODE		= 13,
    kDHCPv6OPTION_RAPID_COMMIT		= 14,
    kDHCPv6OPTION_USER_CLASS		= 15,
    kDHCPv6OPTION_VENDOR_CLASS		= 16,
    kDHCPv6OPTION_VENDOR_OPTS		= 17,
    kDHCPv6OPTION_INTERFACE_ID		= 18,
    kDHCPv6OPTION_RECONF_MSG		= 19,
    kDHCPv6OPTION_RECONF_ACCEPT		= 20,
    kDHCPv6OPTION_SIP_SERVER_D		= 21,
    kDHCPv6OPTION_SIP_SERVER_A		= 22,
    kDHCPv6OPTION_DNS_SERVERS		= 23,
    kDHCPv6OPTION_DOMAIN_LIST		= 24,
    kDHCPv6OPTION_CAPTIVE_PORTAL_URL	= 103,
};

typedef struct {
    uint8_t		code[2];
    uint8_t		len[2];
    uint8_t		data[1]; /* variable length */
} DHCPv6Option, * DHCPv6OptionRef;

#define DHCPV6_OPTION_HEADER_SIZE	offsetof(DHCPv6Option, data)

INLINE int
DHCPv6OptionGetLength(DHCPv6OptionRef option)
{
    return (net_uint16_get(option->len));
}

INLINE void
DHCPv6OptionSetLength(DHCPv6OptionRef option, DHCPv6OptionLength len)
{
    net_uint16_set(option->len, len);
    return;
}

INLINE int
DHCPv6OptionGetCode(DHCPv6OptionRef option)
{
    return (net_uint16_get(option->code));
}

INLINE void
DHCPv6OptionSetCode(DHCPv6OptionRef option, DHCPv6OptionCode code)
{
    net_uint16_set(option->code, code);
}

INLINE const uint8_t *
DHCPv6OptionGetData(DHCPv6OptionRef option)
{
    return (option->data);
}

const char * 
DHCPv6OptionCodeGetName(DHCPv6OptionCode option_code);

DHCPv6OptionCode
DHCPv6OptionNameGetCode(const char * name);

typedef CF_ENUM(uint32_t, DHCPv6OptionType) {
    kDHCPv6OptionTypeNone = 0,
    kDHCPv6OptionTypeUnknown = 1,
    kDHCPv6OptionTypeDUID = 2,
    kDHCPv6OptionTypeUInt16 = 3,
    kDHCPv6OptionTypeUInt32 = 4,
    kDHCPv6OptionTypeIPv6Address = 5,
    kDHCPv6OptionTypeDNSNameList = 6,
    kDHCPv6OptionTypeIA_NA = 7,
    kDHCPv6OptionTypeIAADDR = 8,
    kDHCPv6OptionTypeStatusCode = 9,
    kDHCPv6OptionTypeString = 10,
};

DHCPv6OptionType
DHCPv6OptionCodeGetType(DHCPv6OptionCode option_code);

typedef struct {
    char	str[256];
} DHCPv6OptionErrorString, *DHCPv6OptionErrorStringRef;

/**
 ** DHCPv6OptionArea
 **/
typedef struct {
    uint8_t *			buf;
    int				size;
    int				used;
} DHCPv6OptionArea, * DHCPv6OptionAreaRef;

void
DHCPv6OptionAreaInit(DHCPv6OptionAreaRef oa_p, uint8_t * buf, int size);

int
DHCPv6OptionAreaGetUsedLength(DHCPv6OptionAreaRef oa_p);

bool
DHCPv6OptionAreaAddOption(DHCPv6OptionAreaRef oa_p,
			  DHCPv6OptionCode option_code, 
			  DHCPv6OptionLength option_len,
			  const void * option_data,
			  DHCPv6OptionErrorStringRef err_p);
bool
DHCPv6OptionAreaAddOptionRequestOption(DHCPv6OptionAreaRef oa_p, 
				       const DHCPv6OptionCode * requested_opts,
				       int count,
				       DHCPv6OptionErrorStringRef err_p);
/**
 ** DHCPv6OptionList
 **/
typedef struct DHCPv6OptionList * DHCPv6OptionListRef;

DHCPv6OptionListRef
DHCPv6OptionListCreate(const uint8_t * buf, int buf_size,
		       DHCPv6OptionErrorStringRef err_p);

DHCPv6OptionListRef
DHCPv6OptionListCreateWithPacket(const DHCPv6PacketRef pkt, int pkt_len,
				 DHCPv6OptionErrorStringRef err_p);

void
DHCPv6OptionListRelease(DHCPv6OptionListRef * dhcpol_p);

const uint8_t *
DHCPv6OptionListGetOptionDataAndLength(DHCPv6OptionListRef options,
				       DHCPv6OptionCode option_code, int * ret_length,
				       int * start_index);
void
DHCPv6OptionListFPrint(FILE * file, DHCPv6OptionListRef options);

void
DHCPv6OptionListPrintToString(CFMutableStringRef str, 
			      DHCPv6OptionListRef options);


int
DHCPv6OptionListGetCount(DHCPv6OptionListRef options);

DHCPv6OptionRef
DHCPv6OptionListGetOptionAtIndex(DHCPv6OptionListRef options, int i);

/**
 ** DHCPv6OptionsDictionary
 **/
CFDictionaryRef
DHCPv6OptionsDictionaryCreate(CFDictionaryRef dict);

CFDataRef
DHCPv6OptionsDictionaryGetOption(CFDictionaryRef dict, DHCPv6OptionCode code);


/**
 ** IA_NA option
 **/
typedef struct {
    uint8_t		iaid[4];
    uint8_t		t1[4];
    uint8_t		t2[4];
    uint8_t		options[1]; /* variable length */
} DHCPv6OptionIA_NA, * DHCPv6OptionIA_NARef;

#define DHCPv6OptionIA_NA_MIN_LENGTH	((int)offsetof(DHCPv6OptionIA_NA, options))

INLINE uint32_t
DHCPv6OptionIA_NAGetIAID(DHCPv6OptionIA_NARef ia_na)
{
    return (net_uint32_get(ia_na->iaid));
}

INLINE void
DHCPv6OptionIA_NASetIAID(DHCPv6OptionIA_NARef ia_na, uint32_t iaid)
{
    net_uint32_set(ia_na->iaid, iaid);
    return;
}

INLINE uint32_t
DHCPv6OptionIA_NAGetT1(DHCPv6OptionIA_NARef ia_na)
{
    return (net_uint32_get(ia_na->t1));
}

INLINE void
DHCPv6OptionIA_NASetT1(DHCPv6OptionIA_NARef ia_na, uint32_t t1)
{
    net_uint32_set(ia_na->t1, t1);
    return;
}

INLINE uint32_t
DHCPv6OptionIA_NAGetT2(DHCPv6OptionIA_NARef ia_na)
{
    return (net_uint32_get(ia_na->t2));
}

INLINE void
DHCPv6OptionIA_NASetT2(DHCPv6OptionIA_NARef ia_na, uint32_t t2)
{
    net_uint32_set(ia_na->t2, t2);
    return;
}

/**
 ** IAADDR option
 **/
typedef struct {
    uint8_t		address[16];
    uint8_t		preferred_lifetime[4];
    uint8_t		valid_lifetime[4];
    uint8_t		options[1]; /* variable length */
} DHCPv6OptionIAADDR, * DHCPv6OptionIAADDRRef;

#define DHCPv6OptionIAADDR_MIN_LENGTH	((int)offsetof(DHCPv6OptionIAADDR, options))

INLINE const uint8_t *
DHCPv6OptionIAADDRGetAddress(DHCPv6OptionIAADDRRef ia_addr)
{
   return ((const uint8_t *)ia_addr->address);
}

INLINE void
DHCPv6OptionIAADDRSetAddress(DHCPv6OptionIAADDRRef ia_addr,
			     const void * addr_p)
{
    bcopy(addr_p, ia_addr->address, sizeof(struct in6_addr));
    return;
}

INLINE uint32_t
DHCPv6OptionIAADDRGetPreferredLifetime(DHCPv6OptionIAADDRRef ia_addr)
{
    return (net_uint32_get(ia_addr->preferred_lifetime));
}

INLINE void
DHCPv6OptionIAADDRSetPreferredLifetime(DHCPv6OptionIAADDRRef ia_addr,
				       uint32_t preferred_lifetime)
{
    net_uint32_set(ia_addr->preferred_lifetime, preferred_lifetime);
    return;
}

INLINE uint32_t
DHCPv6OptionIAADDRGetValidLifetime(DHCPv6OptionIAADDRRef ia_addr)
{
    return (net_uint32_get(ia_addr->valid_lifetime));
}

INLINE void
DHCPv6OptionIAADDRSetValidLifetime(DHCPv6OptionIAADDRRef ia_addr,
				   uint32_t valid_lifetime)
{
    net_uint32_set(ia_addr->valid_lifetime, valid_lifetime);
    return;
}

void
DHCPv6OptionIAADDRPrintToString(CFMutableStringRef str,
				DHCPv6OptionIAADDRRef ia_addr, 
				int ia_addr_len);

/**
 ** Status Code option
 **/
typedef struct {
    uint8_t		code[2];
    uint8_t		message[1]; /* variable length */
} DHCPv6OptionSTATUS_CODE, * DHCPv6OptionSTATUS_CODERef;

#define DHCPv6OptionSTATUS_CODE_MIN_LENGTH	((int)offsetof(DHCPv6OptionSTATUS_CODE, message))

typedef CF_ENUM(int, DHCPv6StatusCode) {
    kDHCPv6StatusCodeSuccess		= 0,
    kDHCPv6StatusCodeFailure		= 1,
    kDHCPv6StatusCodeNoAddrsAvail	= 2,
    kDHCPv6StatusCodeNoBinding		= 3,
    kDHCPv6StatusCodeNotOnLink		= 4,
    kDHCPv6StatusCodeUseMulticast	= 5
};

const char * 
DHCPv6StatusCodeGetName(int status_code);

INLINE DHCPv6OptionCode
DHCPv6OptionSTATUS_CODEGetCode(DHCPv6OptionSTATUS_CODERef status_p)
{
    return (net_uint16_get(status_p->code));
}

INLINE void 
DHCPv6OptionSTATUS_CODESetCode(DHCPv6OptionSTATUS_CODERef status_p,
			       DHCPv6OptionCode code)
{
    net_uint16_set(status_p->code, code);
    return;
}

void 
DHCPv6OptionSTATUS_CODEFPrint(FILE * f, DHCPv6OptionSTATUS_CODERef status_p,
			      int status_len);

/**
 ** PREFERENCE option
 **/
typedef struct {
    uint8_t		value;
} DHCPv6OptionPREFERENCE, * DHCPv6OptionPREFERENCERef;

#define DHCPv6OptionPREFERENCE_MIN_LENGTH	1
#define kDHCPv6OptionPREFERENCEMinValue		0
#define kDHCPv6OptionPREFERENCEMaxValue		255

#endif /* _S_DHCPV6OPTIONS_H */
