/*
 * Copyright (c) 2017-2018 Apple Inc. All rights reserved.
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
 * IPv6AWDReport.h
 * - C shim layer to interact with AWD to generate and submit a metric
 */

#ifndef _S_IPV6_AWD_REPORT_H
#define _S_IPV6_AWD_REPORT_H

/*
 * Modification History
 *
 * May 23, 2017		Dieter Siegmund (dieter@apple.com)
 * - created
 */

typedef CFTypeRef IPv6AWDReportRef;

//typedef struct __IPv6AWDReport * IPv6AWDReportRef;

typedef enum {
    kInterfaceTypeOther = 0,
    kInterfaceTypeWiFi = 1,
    kInterfaceTypeCellular = 2,
    kInterfaceTypeWired = 3,
} InterfaceType;

IPv6AWDReportRef
IPv6AWDReportCreate(InterfaceType type);

void
IPv6AWDReportSubmit(IPv6AWDReportRef report);

void IPv6AWDReportSetAPNName(IPv6AWDReportRef report, CFStringRef apn_name);

void IPv6AWDReportSetLinkLocalAddressDuplicated(IPv6AWDReportRef report);
void IPv6AWDReportSetAutoconfAddressDuplicated(IPv6AWDReportRef report);
void IPv6AWDReportSetAutoconfAddressDeprecated(IPv6AWDReportRef report);
void IPv6AWDReportSetAutoconfAddressDetached(IPv6AWDReportRef report);
void IPv6AWDReportSetAutoconfAddressAcquired(IPv6AWDReportRef report);
void IPv6AWDReportSetAutoconfRestarted(IPv6AWDReportRef report);
void IPv6AWDReportSetAutoconfRDNSS(IPv6AWDReportRef report);
void IPv6AWDReportSetAutoconfDNSSL(IPv6AWDReportRef report);
void IPv6AWDReportSetDHCPv6AddressAcquired(IPv6AWDReportRef report);
void IPv6AWDReportSetDHCPv6DNSServers(IPv6AWDReportRef report);
void IPv6AWDReportSetDHCPv6DNSDomainList(IPv6AWDReportRef report);
void IPv6AWDReportSetManualAddressConfigured(IPv6AWDReportRef report);
void IPv6AWDReportSetXLAT464Enabled(IPv6AWDReportRef report);
void IPv6AWDReportSetXLAT464PLATDiscoveryFailed(IPv6AWDReportRef report);
void IPv6AWDReportSetPrefixPreferredLifetime(IPv6AWDReportRef report,
					     uint32_t lifetime);
void IPv6AWDReportSetPrefixValidLifetime(IPv6AWDReportRef report,
					 uint32_t lifetime);
void IPv6AWDReportSetPrefixLifetimeNotInfinite(IPv6AWDReportRef report);
void IPv6AWDReportSetRouterLifetime(IPv6AWDReportRef report, uint16_t lifetime);
void IPv6AWDReportSetRouterLifetimeNotMaximum(IPv6AWDReportRef report);
void IPv6AWDReportSetRouterSourceAddressCollision(IPv6AWDReportRef report);
void IPv6AWDReportSetRouterLifetimeZero(IPv6AWDReportRef report);

void IPv6AWDReportSetDefaultRouterCount(IPv6AWDReportRef report, UInt32 count);
void IPv6AWDReportSetExpiredDefaultRouterCount(IPv6AWDReportRef report,
					       UInt32 count);
void IPv6AWDReportSetPrefixCount(IPv6AWDReportRef report, UInt32 count);
void IPv6AWDReportSetExpiredPrefixCount(IPv6AWDReportRef report, UInt32 count);
void IPv6AWDReportSetRouterSolicitationCount(IPv6AWDReportRef report,
					     UInt32 count);
void IPv6AWDReportSetControlQueueUnsentCount(IPv6AWDReportRef report,
					     UInt32 count);

void IPv6AWDReportSetAutoconfAddressAcquisitionSeconds(IPv6AWDReportRef report,
						       UInt32 seconds);
void IPv6AWDReportSetDHCPv6AddressAcquisitionSeconds(IPv6AWDReportRef report,
						     UInt32 seconds);
void IPv6AWDReportSetDNSConfigurationAcquisitionSeconds(IPv6AWDReportRef report,
							UInt32 seconds);
#endif /*  _S_IPV6_AWD_REPORT_H */
