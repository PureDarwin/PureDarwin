/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ClientRequests_h
#define __ClientRequests_h

#include "mDNSEmbeddedAPI.h"
#include "dns_sd_internal.h"

typedef void (*QueryRecordResultHandler)(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord,
    DNSServiceErrorType error, void *context);

typedef struct
{
    DNSQuestion                 q;                      // DNSQuestion for record query.
    domainname *                qname;                  // Name of the original record.
    mDNSInterfaceID             interfaceID;            // Interface over which to perform query.
    QueryRecordResultHandler    resultHandler;          // Handler for query record operation results.
    void *                      resultContext;          // Context to pass to result handler.
    mDNSu32                     reqID;                  // 
    int                         searchListIndex;        // Index that indicates the next search domain to try.
#if MDNSRESPONDER_SUPPORTS(APPLE, UNICAST_DOTLOCAL)
    DNSQuestion *               q2;                     // DNSQuestion for unicast version of a record with a dot-local name.
    mDNSu16                     q2Type;                 // q2's original qtype value.
    mDNSBool                    q2LongLived;            // q2's original LongLived value.
    mDNSBool                    q2ReturnIntermed;       // q2's original ReturnIntermed value.
    mDNSBool                    q2TimeoutQuestion;      // q2's original TimeoutQuestion value.
    mDNSBool                    q2AppendSearchDomains;  // q2's original AppendSearchDomains value.
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, REACHABILITY_TRIGGER)
    mDNSBool                    answered;               // True if the query was answered.
#endif

}   QueryRecordOp;

typedef struct
{
    mDNSInterfaceID     interfaceID;    // InterfaceID being used for query record operations.
    mDNSu32             protocols;      // Protocols (IPv4, IPv6) specified by client.
    QueryRecordOp *     op4;            // Query record operation object for A record.
    QueryRecordOp *     op6;            // Query record operation object for AAAA record.

}   GetAddrInfoClientRequest;

typedef struct
{
    QueryRecordOp       op; // Query record operation object.

}   QueryRecordClientRequest;

typedef struct
{
    mDNSu32                 requestID;
    const char *            hostnameStr;
    mDNSu32                 interfaceIndex;
    DNSServiceFlags         flags;
    mDNSu32                 protocols;
    mDNSs32                 effectivePID;
    const mDNSu8 *          effectiveUUID;
    mDNSu32                 peerUID;
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    mDNSBool                needEncryption;
    const mDNSu8 *          resolverUUID;
    mdns_dns_service_id_t   customID;
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
    const audit_token_t *   peerAuditToken;
    const audit_token_t *   delegatorAuditToken;
    mDNSBool                isInAppBrowserRequest;
#endif

}   GetAddrInfoClientRequestParams;

typedef struct
{
    mDNSu32                 requestID;
    const char *            qnameStr;
    mDNSu32                 interfaceIndex;
    DNSServiceFlags         flags;
    mDNSu16                 qtype;
    mDNSu16                 qclass;
    mDNSs32                 effectivePID;
    const mDNSu8 *          effectiveUUID;
    mDNSu32                 peerUID;
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    mDNSBool                needEncryption;
    const mDNSu8 *          resolverUUID;
	mdns_dns_service_id_t	customID;
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
    const audit_token_t *   peerAuditToken;
    const audit_token_t *   delegatorAuditToken;
    mDNSBool                isInAppBrowserRequest;
#endif

}   QueryRecordClientRequestParams;

#ifdef __cplusplus
extern "C" {
#endif

mDNSexport void GetAddrInfoClientRequestParamsInit(GetAddrInfoClientRequestParams *inParams);
mDNSexport mStatus GetAddrInfoClientRequestStart(GetAddrInfoClientRequest *inRequest,
    const GetAddrInfoClientRequestParams *inParams, QueryRecordResultHandler inResultHandler, void *inResultContext);
mDNSexport void GetAddrInfoClientRequestStop(GetAddrInfoClientRequest *inRequest);
mDNSexport const domainname * GetAddrInfoClientRequestGetQName(const GetAddrInfoClientRequest *inRequest);
mDNSexport mDNSBool GetAddrInfoClientRequestIsMulticast(const GetAddrInfoClientRequest *inRequest);

mDNSexport void QueryRecordClientRequestParamsInit(QueryRecordClientRequestParams *inParams);
mDNSexport mStatus QueryRecordClientRequestStart(QueryRecordClientRequest *inRequest,
    const QueryRecordClientRequestParams *inParams, QueryRecordResultHandler inResultHandler, void *inResultContext);
mDNSexport void QueryRecordClientRequestStop(QueryRecordClientRequest *inRequest);
mDNSexport const domainname * QueryRecordClientRequestGetQName(const QueryRecordClientRequest *inRequest);
mDNSexport mDNSu16 QueryRecordClientRequestGetType(const QueryRecordClientRequest *inRequest);
mDNSexport mDNSBool QueryRecordClientRequestIsMulticast(QueryRecordClientRequest *inRequest);

#if MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)
// This is a "mDNSexport" wrapper around the "static" QueryRecordOpStart that cannot be called by outside, which can be
// called by the outside(dnssec related function).
mDNSexport mStatus QueryRecordOpStartForClientRequest(
    QueryRecordOp *             inOp,
    mDNSu32                     inReqID,
    const domainname *          inQName,
    mDNSu16                     inQType,
    mDNSu16                     inQClass,
    mDNSInterfaceID             inInterfaceID,
    mDNSs32                     inServiceID,
    mDNSu32                     inFlags,
    mDNSBool                    inAppendSearchDomains,
    mDNSs32                     inPID,
    const mDNSu8                inUUID[UUID_SIZE],
    mDNSu32                     inUID,
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
    const audit_token_t *       inPeerAuditTokenPtr,
    const audit_token_t *       inDelegateAuditTokenPtr,
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    const mDNSu8                inResolverUUID[UUID_SIZE],
    mDNSBool                    inNeedEncryption,
    const mdns_dns_service_id_t inCustomID,
#endif
    QueryRecordResultHandler    inResultHandler,
    void *                      inResultContext);

mDNSexport void QueryRecordOpStopForClientRequest(QueryRecordOp *op);
#endif // MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)

#ifdef __cplusplus
}
#endif

#endif  // __ClientRequests_h
