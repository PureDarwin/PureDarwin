/*
 * Copyright (c) 2015-2020 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _DNS_SD_PRIVATE_H
#define _DNS_SD_PRIVATE_H

#include <dns_sd.h>

#if !defined(DNS_SD_EXCLUDE_PRIVATE_API)
    #if defined(__APPLE__)
        #define DNS_SD_EXCLUDE_PRIVATE_API  0
    #else
        #define DNS_SD_EXCLUDE_PRIVATE_API  1
    #endif
#endif

// Private flags (kDNSServiceFlagsPrivateOne, kDNSServiceFlagsPrivateTwo, kDNSServiceFlagsPrivateThree, kDNSServiceFlagsPrivateFour, kDNSServiceFlagsPrivateFive) from dns_sd.h
enum
{
    kDNSServiceFlagsDenyConstrained        = 0x2000,
    /*
     * This flag is meaningful only for Unicast DNS queries. When set, the daemon will restrict
     * DNS resolutions on interfaces defined as constrained for that request.
     */

    kDNSServiceFlagsDenyCellular           = 0x8000000,
    /*
     * This flag is meaningful only for Unicast DNS queries. When set, the daemon will restrict
     * DNS resolutions on the cellular interface for that request.
     */
    kDNSServiceFlagsServiceIndex           = 0x10000000,
    /*
     * This flag is meaningful only for DNSServiceGetAddrInfo() for Unicast DNS queries.
     * When set, DNSServiceGetAddrInfo() will interpret the "interfaceIndex" argument of the call
     * as the "serviceIndex".
     */

    kDNSServiceFlagsDenyExpensive          = 0x20000000,
    /*
     * This flag is meaningful only for Unicast DNS queries. When set, the daemon will restrict
     * DNS resolutions on interfaces defined as expensive for that request.
     */

    kDNSServiceFlagsPathEvaluationDone     = 0x40000000
    /*
     * This flag is meaningful for only Unicast DNS queries.
     * When set, it indicates that Network PathEvaluation has already been performed.
     */
};

#if !DNS_SD_EXCLUDE_PRIVATE_API
/* DNSServiceCreateDelegateConnection()
 *
 * Parameters:
 *
 * sdRef:           A pointer to an uninitialized DNSServiceRef. Deallocating
 *                  the reference (via DNSServiceRefDeallocate()) severs the
 *                  connection and deregisters all records registered on this connection.
 *
 * pid :            Process ID of the delegate
 *
 * uuid:            UUID of the delegate
 *
 *                  Note that only one of the two arguments (pid or uuid) can be specified. If pid
 *                  is zero, uuid will be assumed to be a valid value; otherwise pid will be used.
 *
 * return value:    Returns kDNSServiceErr_NoError on success, otherwise returns
 *                  an error code indicating the specific failure that occurred (in which
 *                  case the DNSServiceRef is not initialized). kDNSServiceErr_NotAuth is
 *                  returned to indicate that the calling process does not have entitlements
 *                  to use this API.
 */
DNSSD_EXPORT
DNSServiceErrorType DNSSD_API DNSServiceCreateDelegateConnection(DNSServiceRef *sdRef, int32_t pid, uuid_t uuid);

// Map the source port of the local UDP socket that was opened for sending the DNS query
// to the process ID of the application that triggered the DNS resolution.
//
/* DNSServiceGetPID() Parameters:
 *
 * srcport:         Source port (in network byte order) of the UDP socket that was created by
 *                  the daemon to send the DNS query on the wire.
 *
 * pid:             Process ID of the application that started the name resolution which triggered
 *                  the daemon to send the query on the wire. The value can be -1 if the srcport
 *                  cannot be mapped.
 *
 * return value:    Returns kDNSServiceErr_NoError on success, or kDNSServiceErr_ServiceNotRunning
 *                  if the daemon is not running. The value of the pid is undefined if the return
 *                  value has error.
 */
DNSSD_EXPORT
DNSServiceErrorType DNSSD_API DNSServiceGetPID
(
    uint16_t srcport,
    int32_t *pid
);

DNSSD_EXPORT
DNSServiceErrorType DNSSD_API DNSServiceSetDefaultDomainForUser(DNSServiceFlags flags, const char *domain);

SPI_AVAILABLE(macos(10.15.4), ios(13.2.2), watchos(6.2), tvos(13.2))
DNSServiceErrorType DNSSD_API DNSServiceSleepKeepalive_sockaddr
(
    DNSServiceRef *                 sdRef,
    DNSServiceFlags                 flags,
    const struct sockaddr *         localAddr,
    const struct sockaddr *         remoteAddr,
    unsigned int                    timeout,
    DNSServiceSleepKeepaliveReply   callBack,
    void *                          context
);

/*!
 *  @brief
 *      Sets the default DNS resolver settings for the caller's process.
 *
 *  @param plist_data_ptr
 *      Pointer to an nw_resolver_config's binary property list data.
 *
 *  @param plist_data_len
 *      Byte-length of the binary property list data. Ignored if plist_data_ptr is NULL.
 *
 *  @param require_encryption
 *      Pass true if the process requires that DNS queries use an encrypted DNS service, such as DNS over HTTPS.
 *
 *  @result
 *      This function returns kDNSServiceErr_NoError on success, kDNSServiceErr_Invalid if plist_data_len
 *      exceeds 32,768, and kDNSServiceErr_NoMemory if it fails to allocate memory.
 *
 *  @discussion
 *      These settings only apply to the calling process's DNSServiceGetAddrInfo and DNSServiceQueryRecord
 *      requests. This function exists for code that may still use the legacy DNS-SD API for resolving
 *      hostnames, i.e., it implements the functionality of dnssd_getaddrinfo_set_need_encrypted_query(), but at
 *      a process-wide level of granularity.
 *
 *      Due to underlying IPC limitations, there's currently a 32 KB limit on the size of the binary property
 *      list data.
 */
SPI_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0))
DNSServiceErrorType DNSSD_API DNSServiceSetResolverDefaults(const void *plist_data_ptr, size_t plist_data_len,
    bool require_encryption);
#endif  // !DNS_SD_EXCLUDE_PRIVATE_API

#define kDNSServiceCompPrivateDNS   "PrivateDNS"
#define kDNSServiceCompMulticastDNS "MulticastDNS"

#endif  // _DNS_SD_PRIVATE_H
