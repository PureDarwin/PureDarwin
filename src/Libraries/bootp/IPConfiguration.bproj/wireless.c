/*
 * Copyright (c) 1999-2020 Apple Inc. All rights reserved.
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
 * wireless.c
 */
/* 
 * Modification History
 *
 * July 6, 2020 	Dieter Siegmund (dieter@apple.com)
 * - moved out of ipconfigd.c
 */
#include "wireless.h"
#include "cfutil.h"
#include "mylog.h"
#include "symbol_scope.h"

#ifndef TEST_WIRELESS
#include "my_darwin.h"
#endif

#ifdef NO_WIRELESS

PRIVATE_EXTERN const char *
wifi_auth_type_string(wifi_auth_type auth_type)
{
    return ("<unknown>");
}

PRIVATE_EXTERN CFStringRef
wireless_copy_ssid_bssid(CFStringRef ifname, struct ether_addr * ap_mac,
			 wifi_auth_type * auth_type_p)
{
#pragma unused(ifname)
#pragma unused(ap_mac)
#pragma unused(auth_type_p)
    return (NULL);
}

#else /* NO_WIRELESS */

#include <Apple80211/Apple80211API.h>
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>

#define _CASSERT(x)	_Static_assert(x, "compile-time assertion failed")

_CASSERT(WIFI_AUTH_TYPE_UNKNOWN == APPLE80211_AUTHTYPE_UNKNOWN);
_CASSERT(WIFI_AUTH_TYPE_NONE == APPLE80211_AUTHTYPE_NONE);

PRIVATE_EXTERN const char *
wifi_auth_type_string(wifi_auth_type auth_type)
{
    const char * str;

    switch (auth_type) {
    case APPLE80211_AUTHTYPE_NONE:
	str = "NONE";
	break;
    case APPLE80211_AUTHTYPE_WPA:
	str = "WPA";
	break;
    case APPLE80211_AUTHTYPE_WPA_PSK:
	str = "WPA_PSK";
	break;
    case APPLE80211_AUTHTYPE_WPA2:
	str = "WPA2";
	break;
    case APPLE80211_AUTHTYPE_WPA2_PSK:
	str = "WPA2_PSK";
	break;
    case APPLE80211_AUTHTYPE_FT_PSK:
	str = "FT_PSK";
	break;
    case APPLE80211_AUTHTYPE_LEAP:
	str = "LEAP";
	break;
    case APPLE80211_AUTHTYPE_8021X:
	str = "8021X";
	break;
    case APPLE80211_AUTHTYPE_FT_8021X:
	str = "FT_8021X";
	break;
    case APPLE80211_AUTHTYPE_WPS:
	str = "WPS";
	break;
    case APPLE80211_AUTHTYPE_WAPI:
	str = "WAPI";
	break;
    case APPLE80211_AUTHTYPE_SHA256_PSK:
	str = "SHA256_PSK";
	break;
    case APPLE80211_AUTHTYPE_SHA256_8021X:
	str = "SHA256_8021X";
	break;
    case APPLE80211_AUTHTYPE_WPA3_SAE:
	str = "WPA3_SAE";
	break;
    case APPLE80211_AUTHTYPE_FT_SAE:
	str = "FT_SAE";
	break;
    case APPLE80211_AUTHTYPE_SHA384_8021X:
	str = "SHA384_8021X";
	break;
    case APPLE80211_AUTHTYPE_SHA384_FT_8021X:
	str = "SHA384_FT_8021X";
	break;
    case APPLE80211_AUTHTYPE_UNKNOWN:
	str = "UNKNOWN";
	break;
    default:
	str = "UNRECOGNIZED";
	break;
    }
    return (str);
}

static wifi_auth_type
get_wifi_auth_type(Apple80211Ref wref)
{
    uint32_t		auth_type;
    CFNumberRef		auth_type_cf;
    wifi_auth_type	auth_type_ret = WIFI_AUTH_TYPE_UNKNOWN;
    Apple80211Err 	error;
    CFDictionaryRef	info = NULL;

    error = Apple80211CopyValue(wref, APPLE80211_IOC_AUTH_TYPE, NULL, &info);
    if (error != kA11NoErr) {
	my_log(LOG_NOTICE,
	       "Apple80211CopyValue(APPLE80211_IOC_AUTH_TYPE) failed, 0x%x",
	       error);
	goto done;
    }
    if (info == NULL) {
	goto done;
    }
    auth_type_cf = CFDictionaryGetValue(info, APPLE80211KEY_AUTH_UPPER);
    if (auth_type_cf == NULL) {
	uint32_t		auth_type_lower;
	CFNumberRef		auth_type_lower_cf;

	auth_type_lower_cf
	    = CFDictionaryGetValue(info, APPLE80211KEY_AUTH_LOWER);
	if (auth_type_lower_cf == NULL) {
	    goto done;
	}
	if (!CFNumberGetValue(auth_type_lower_cf, kCFNumberSInt32Type,
			      &auth_type_lower)) {
	    goto done;
	}
	if (auth_type_lower == APPLE80211_AUTHTYPE_OPEN) {
	    auth_type = WIFI_AUTH_TYPE_NONE;
	}
	else {
	    goto done;
	}
    }
    else if (!CFNumberGetValue(auth_type_cf, kCFNumberSInt32Type, &auth_type)) {
	goto done;
    }
    auth_type_ret = auth_type;

 done:
    my_CFRelease(&info);
    return (auth_type_ret);
}

PRIVATE_EXTERN CFStringRef
wireless_copy_ssid_bssid(CFStringRef ifname, struct ether_addr * ap_mac,
			 wifi_auth_type * auth_type_p)
{
    wifi_auth_type	auth_type = WIFI_AUTH_TYPE_UNKNOWN;
    Apple80211Err	error;
    CFMutableDataRef	ssid;
    CFStringRef		ssid_str = NULL;
    Apple80211Ref	wref = NULL;

    error = Apple80211Open(&wref);
    if (error != kA11NoErr) {
	my_log(LOG_NOTICE, "Apple80211Open failed, 0x%x", error);
	goto done;
    }
    error = Apple80211BindToInterface(wref, ifname);
    if (error != kA11NoErr) {
	goto done;
    }
    ssid = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if ((Apple80211Get(wref, APPLE80211_IOC_SSID, 0, ssid, 0) == kA11NoErr)
	&& (Apple80211Get(wref, APPLE80211_IOC_BSSID, 0,
			  ap_mac, sizeof(*ap_mac)) == kA11NoErr)) {
	/* we have both the SSID and BSSID */
	ssid_str = CFStringCreateWithBytes(NULL,
					   CFDataGetBytePtr(ssid),
					   CFDataGetLength(ssid),
					   kCFStringEncodingUTF8,
					   FALSE);
	if (ssid_str == NULL) {
	    ssid_str = CFStringCreateWithBytes(NULL,
					       CFDataGetBytePtr(ssid),
					       CFDataGetLength(ssid),
					       kCFStringEncodingMacRoman,
					       FALSE);
	}
	auth_type = get_wifi_auth_type(wref);
    }
    CFRelease(ssid);

 done:
    *auth_type_p = auth_type;
    if (wref != NULL) {
	Apple80211Close(wref);
    }
    return (ssid_str);
}

#endif /* NO_WIRELESS */

#if TEST_WIRELESS

#include <SystemConfiguration/SCPrivate.h>

int
main(int argc, char * argv[])
{
    struct ether_addr 	ap_mac;
    wifi_auth_type	auth_type;
    CFStringRef		ifname;
    CFStringRef		ssid;

    if (argc < 2) {
	fprintf(stderr, "usage: wireless <ifname>\n");
	exit(2);
    }
    ifname = CFStringCreateWithCString(NULL, argv[1], kCFStringEncodingUTF8);
    ssid = wireless_copy_ssid_bssid(ifname, &ap_mac, &auth_type);
    if (ssid != NULL) {
	SCPrint(TRUE, stdout,
		CFSTR("%s: SSID %@ BSSID %s Security %s\n"),
		argv[1], ssid, ether_ntoa(&ap_mac),
		wifi_auth_type_string(auth_type));
    }
    else {
	printf("%s: no SSID\n", argv[1]);
    }
    exit(0);
    return (0);
}

#endif /* TEST_WIRELESS */
