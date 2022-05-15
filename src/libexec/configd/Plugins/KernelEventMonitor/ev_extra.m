/*
 * Copyright (c) 2013-2016, 2018, 2020, 2021 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * October 7, 2013	Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include "eventmon.h"
#include "ev_extra.h"

#include <CoreWiFi/CoreWiFi.h>


static Boolean
haveCoreWiFiFramework()
{
	Boolean	haveFramework;

	haveFramework = ([CWFInterface class] != nil);
	return haveFramework;
}


static CFBooleanRef
is_expensive(SCNetworkInterfaceRef _Nonnull interface)
{
	CFBooleanRef	expensive;
	CFStringRef	interfaceType;

	while (interface != NULL) {
		SCNetworkInterfaceRef	child;

		child = SCNetworkInterfaceGetInterface(interface);
		if (child == NULL) {
			break;
		}

		interface = child;
	}
	// by default, don't set/clear expensive
	expensive = NULL;
	interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
	if (_SCNetworkInterfaceIsTethered(interface)) {
		// if tethered (to iOS) interface
		expensive = kCFBooleanTrue;
	} else if (_SCNetworkInterfaceIsBluetoothPAN(interface)) {
		// if BT-PAN interface
		expensive = kCFBooleanTrue;
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypeIEEE80211)) {
		// assume WiFi is not expensive
		expensive = kCFBooleanFalse;

		if (haveCoreWiFiFramework()) {
			@autoreleasepool
			{
				CWFScanResult	*currentNetwork;
				CWFInterface	*interface;

				interface = [[CWFInterface alloc] init];
				[interface activate];
				currentNetwork = [interface currentScanResult];
				if (currentNetwork.isMetered) {
					expensive = kCFBooleanTrue;
				}
				[interface invalidate];
				[interface release];
			}
		}
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypeWWAN)) {
		// if WWAN [Ethernet] interface
		expensive = kCFBooleanTrue;
	}

	return expensive;
}


static int
ifexpensive_set(int s, const char * name, uint32_t expensive)
{
#if	defined(SIOCGIFEXPENSIVE) && defined(SIOCSIFEXPENSIVE) && !defined(MAIN)
	struct ifreq	ifr;
	int		ret;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ret = ioctl(s, SIOCGIFEXPENSIVE, &ifr);
	if ((ret == -1) && (errno != EPERM)) {
		SC_log(LOG_ERR, "%s: ioctl(SIOCGIFEXPENSIVE) failed: %s", name, strerror(errno));
		return ret;
	}

	if (ifr.ifr_expensive == expensive) {
		// if no change
		return ret;
	}

	ifr.ifr_expensive = expensive;
	ret = ioctl(s, SIOCSIFEXPENSIVE, &ifr);
	if ((ret == -1) && (errno != EPERM)) {
		SC_log(LOG_ERR, "%s: ioctl(SIOCSIFEXPENSIVE) failed: %s", name, strerror(errno));
	}

	return ret;
#else	// defined(SIOCSIFEXPENSIVE) && !defined(MAIN)
	return 0;
#endif	// defined(SIOCSIFEXPENSIVE) && !defined(MAIN)
}


__private_extern__
CFBooleanRef
interface_update_expensive(const char *if_name)
{
	CFBooleanRef		expensive;
	SCNetworkInterfaceRef	interface;
	CFStringRef		interface_name;
	int			s;

	interface_name = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingUTF8);
	interface = _SCNetworkInterfaceCreateWithBSDName(NULL, interface_name, kIncludeNoVirtualInterfaces);
	CFRelease(interface_name);
	if (interface == NULL) {
		return NULL;
	}
	expensive = is_expensive(interface);
	CFRelease(interface);
	if (expensive == NULL) {
		return NULL;
	}
	// mark ... or clear ... the [if_name] interface as "expensive"
	s = dgram_socket(AF_INET);
	if (s != -1) {
		ifexpensive_set(s,
				if_name,
				CFBooleanGetValue(expensive) ? 1 : 0);
		close(s);
	}

	return expensive;
}


#ifdef	MAIN

int
dgram_socket(int domain)
{
	int	s;

	s = socket(domain, SOCK_DGRAM, 0);
	if (s == -1) {
		SC_log(LOG_ERR, "socket() failed: %s", strerror(errno));
	}

	return s;
}

int
main(int argc, char * const argv[])
{
	CFBooleanRef	expensive;

	if (argc < 1 + 1) {
		SCPrint(TRUE, stderr, CFSTR("usage: %s <interface>\n"), argv[0]);
		exit(1);
	}

	expensive = interface_update_expensive(argv[1]);
	if (expensive != NULL) {
		SCPrint(TRUE, stdout, CFSTR("%s: set expensive to %@\n"), argv[1], expensive);
	} else {
		SCPrint(TRUE, stdout, CFSTR("%s: not changing expensive\n"), argv[1]);
	}

	exit(0);
}
#endif	// MAIN
