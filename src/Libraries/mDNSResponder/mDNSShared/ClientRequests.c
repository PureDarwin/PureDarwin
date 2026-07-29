/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#include "ClientRequests.h"

#include "DNSCommon.h"
#include "uDNS.h"

#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
#include "QuerierSupport.h"
#endif

#if MDNSRESPONDER_SUPPORTS(APPLE, D2D)
#include "D2D.h"
#endif

#if MDNSRESPONDER_SUPPORTS(APPLE, REACHABILITY_TRIGGER)
#include "mDNSMacOSX.h"
#endif

#if MDNSRESPONDER_SUPPORTS(APPLE, UNREADY_INTERFACES)
#include <dispatch/dispatch.h>
#include <net/if.h>
#endif

#if MDNSRESPONDER_SUPPORTS(APPLE, WEB_CONTENT_FILTER)
#include <WebFilterDNS/WebFilterDNS.h>

int WCFIsServerRunning(WCFConnection *conn) __attribute__((weak_import));
int WCFNameResolvesToAddr(WCFConnection *conn, char* domainName, struct sockaddr* address, uid_t userid) __attribute__((weak_import));
int WCFNameResolvesToName(WCFConnection *conn, char* fromName, char* toName, uid_t userid) __attribute__((weak_import));
#endif

#if MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)
#include "dnssec_v2.h"
#endif

#define RecordTypeIsAddress(TYPE)   (((TYPE) == kDNSType_A) || ((TYPE) == kDNSType_AAAA))

extern mDNS mDNSStorage;
#if MDNSRESPONDER_SUPPORTS(APPLE, UNICAST_DOTLOCAL)
extern domainname ActiveDirectoryPrimaryDomain;
#endif

// Normally we append search domains only for queries with a single label that are not fully qualified. This can be
// overridden to apply search domains for queries (that are not fully qualified) with any number of labels e.g., moon,
// moon.cs, moon.cs.be, etc. - Mohan
mDNSBool AlwaysAppendSearchDomains = mDNSfalse;

// Control enabling optimistic DNS - Phil
mDNSBool EnableAllowExpired = mDNStrue;


typedef struct
{
    mDNSu32                 requestID;
    const domainname *      qname;
    mDNSu16                 qtype;
    mDNSu16                 qclass;
    mDNSInterfaceID         interfaceID;
    mDNSs32                 serviceID;
    mDNSu32                 flags;
    mDNSBool                appendSearchDomains;
    mDNSs32                 effectivePID;
    const mDNSu8 *          effectiveUUID;
    mDNSu32                 peerUID;
    mDNSBool                isInAppBrowserRequest;
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    const mDNSu8 *          resolverUUID;
	mdns_dns_service_id_t	customID;
    mDNSBool                needEncryption;
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
    const audit_token_t *   peerAuditToken;
    const audit_token_t *   delegatorAuditToken;
#endif

}   QueryRecordOpParams;

mDNSlocal void QueryRecordOpParamsInit(QueryRecordOpParams *inParams)
{
	mDNSPlatformMemZero(inParams, (mDNSu32)sizeof(*inParams));
    inParams->serviceID = -1;
}

mDNSlocal mStatus QueryRecordOpCreate(QueryRecordOp **outOp);
mDNSlocal void QueryRecordOpFree(QueryRecordOp *operation);
mDNSlocal mStatus QueryRecordOpStart(QueryRecordOp *inOp, const QueryRecordOpParams *inParams,
    QueryRecordResultHandler inResultHandler, void *inResultContext);
mDNSlocal void QueryRecordOpStop(QueryRecordOp *op);
mDNSlocal mDNSBool QueryRecordOpIsMulticast(const QueryRecordOp *op);
mDNSlocal void QueryRecordOpCallback(mDNS *m, DNSQuestion *inQuestion, const ResourceRecord *inAnswer,
    QC_result inAddRecord);
mDNSlocal void QueryRecordOpResetHandler(DNSQuestion *inQuestion);
mDNSlocal mStatus QueryRecordOpStartQuestion(QueryRecordOp *inOp, DNSQuestion *inQuestion);
mDNSlocal mStatus QueryRecordOpStopQuestion(DNSQuestion *inQuestion);
mDNSlocal mStatus QueryRecordOpRestartUnicastQuestion(QueryRecordOp *inOp, DNSQuestion *inQuestion,
    const domainname *inSearchDomain);
mDNSlocal mStatus InterfaceIndexToInterfaceID(mDNSu32 inInterfaceIndex, mDNSInterfaceID *outInterfaceID);
mDNSlocal mDNSBool DomainNameIsSingleLabel(const domainname *inName);
mDNSlocal mDNSBool StringEndsWithDot(const char *inString);
mDNSlocal const domainname * NextSearchDomain(QueryRecordOp *inOp);
#if MDNSRESPONDER_SUPPORTS(APPLE, UNICAST_DOTLOCAL)
mDNSlocal mDNSBool DomainNameIsInSearchList(const domainname *domain, mDNSBool inExcludeLocal);
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, WEB_CONTENT_FILTER)
mDNSlocal void NotifyWebContentFilter(const ResourceRecord *inAnswer, uid_t inUID);
#endif

mDNSexport void GetAddrInfoClientRequestParamsInit(GetAddrInfoClientRequestParams *inParams)
{
	mDNSPlatformMemZero(inParams, (mDNSu32)sizeof(*inParams));
}

mDNSexport mStatus GetAddrInfoClientRequestStart(GetAddrInfoClientRequest *inRequest,
    const GetAddrInfoClientRequestParams *inParams, QueryRecordResultHandler inResultHandler, void *inResultContext)
{
    mStatus             err;
    domainname          hostname;
    mDNSBool            appendSearchDomains;
    mDNSInterfaceID     interfaceID;
    DNSServiceFlags     flags;
    mDNSs32             serviceID;
    QueryRecordOpParams opParams;

    if (!MakeDomainNameFromDNSNameString(&hostname, inParams->hostnameStr))
    {
        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT,
               "[R%u] ERROR: bad hostname '" PRI_S "'", inParams->requestID, inParams->hostnameStr);
        err = mStatus_BadParamErr;
        goto exit;
    }

    if (inParams->protocols & ~(kDNSServiceProtocol_IPv4|kDNSServiceProtocol_IPv6))
    {
        err = mStatus_BadParamErr;
        goto exit;
    }

    flags = inParams->flags;
    if (inParams->protocols == 0)
    {
        flags |= kDNSServiceFlagsSuppressUnusable;
        inRequest->protocols = kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6;
    }
    else
    {
        inRequest->protocols = inParams->protocols;
    }

    if (flags & kDNSServiceFlagsServiceIndex)
    {
        // NOTE: kDNSServiceFlagsServiceIndex flag can only be set for DNSServiceGetAddrInfo()
        LogInfo("GetAddrInfoClientRequestStart: kDNSServiceFlagsServiceIndex is SET by the client");

        // If kDNSServiceFlagsServiceIndex is SET, interpret the interfaceID as the serviceId and set the interfaceID to 0.
        serviceID   = (mDNSs32)inParams->interfaceIndex;
        interfaceID = mDNSNULL;
    }
    else
    {
        serviceID = -1;
        err = InterfaceIndexToInterfaceID(inParams->interfaceIndex, &interfaceID);
        if (err) goto exit;
    }
    inRequest->interfaceID = interfaceID;

    if (!StringEndsWithDot(inParams->hostnameStr) && (AlwaysAppendSearchDomains || DomainNameIsSingleLabel(&hostname)))
    {
        appendSearchDomains = mDNStrue;
    }
    else
    {
        appendSearchDomains = mDNSfalse;
    }
    QueryRecordOpParamsInit(&opParams);
    opParams.requestID              = inParams->requestID;
    opParams.qname                  = &hostname;
    opParams.qclass                 = kDNSClass_IN;
    opParams.interfaceID            = inRequest->interfaceID;
    opParams.serviceID              = serviceID;
    opParams.flags                  = flags;
    opParams.appendSearchDomains    = appendSearchDomains;
    opParams.effectivePID           = inParams->effectivePID;
    opParams.effectiveUUID          = inParams->effectiveUUID;
    opParams.peerUID                = inParams->peerUID;
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    opParams.resolverUUID           = inParams->resolverUUID;
    opParams.customID               = inParams->customID;
    opParams.needEncryption         = inParams->needEncryption;
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
    opParams.peerAuditToken         = inParams->peerAuditToken;
    opParams.delegatorAuditToken    = inParams->delegatorAuditToken;
    opParams.isInAppBrowserRequest  = inParams->isInAppBrowserRequest;
#endif
    if (inRequest->protocols & kDNSServiceProtocol_IPv6)
    {
        err = QueryRecordOpCreate(&inRequest->op6);
        if (err) goto exit;

        opParams.qtype = kDNSType_AAAA;
        err = QueryRecordOpStart(inRequest->op6, &opParams, inResultHandler, inResultContext);
        if (err) goto exit;
    }
    if (inRequest->protocols & kDNSServiceProtocol_IPv4)
    {
        err = QueryRecordOpCreate(&inRequest->op4);
        if (err) goto exit;

        opParams.qtype = kDNSType_A;
        err = QueryRecordOpStart(inRequest->op4, &opParams, inResultHandler, inResultContext);
        if (err) goto exit;
    }
    err = mStatus_NoError;

exit:
    if (err) GetAddrInfoClientRequestStop(inRequest);
    return err;
}

mDNSexport void GetAddrInfoClientRequestStop(GetAddrInfoClientRequest *inRequest)
{
    if (inRequest->op4) QueryRecordOpStop(inRequest->op4);
    if (inRequest->op6) QueryRecordOpStop(inRequest->op6);

#if MDNSRESPONDER_SUPPORTS(APPLE, REACHABILITY_TRIGGER)
    {
        const QueryRecordOp * const     op4 = inRequest->op4;
        const QueryRecordOp * const     op6 = inRequest->op6;
        const DNSQuestion *             q4  = mDNSNULL;
        const DNSQuestion *             q6  = mDNSNULL;

        if (op4)
        {
            if (op4->answered)
            {
                // If we have a v4 answer and if we timed out prematurely before, provide a trigger to the upper layer so
                // that it can retry questions if needed. - Mohan
                q4 = &op4->q;
            }
            else if (op4->q.TimeoutQuestion)
            {
                // If we are not delivering answers, we may be timing out prematurely. Note down the current state so that
                // we know to retry when we see a valid response again. - Mohan
                mDNSPlatformUpdateDNSStatus(&op4->q);
            }
        }
        if (op6)
        {
            if (op6->answered)
            {
                q6 = &op6->q;
            }
            else if (op6->q.TimeoutQuestion)
            {
                mDNSPlatformUpdateDNSStatus(&op6->q);
            }
        }
        mDNSPlatformTriggerDNSRetry(q4, q6);
    }
#endif

    if (inRequest->op4)
    {
        QueryRecordOpFree(inRequest->op4);
        inRequest->op4 = mDNSNULL;
    }
    if (inRequest->op6)
    {
        QueryRecordOpFree(inRequest->op6);
        inRequest->op6 = mDNSNULL;
    }
}

mDNSexport const domainname * GetAddrInfoClientRequestGetQName(const GetAddrInfoClientRequest *inRequest)
{
    if (inRequest->op4) return &inRequest->op4->q.qname;
    if (inRequest->op6) return &inRequest->op6->q.qname;
    return (const domainname *)"";
}

mDNSexport mDNSBool GetAddrInfoClientRequestIsMulticast(const GetAddrInfoClientRequest *inRequest)
{
    if ((inRequest->op4 && QueryRecordOpIsMulticast(inRequest->op4)) ||
        (inRequest->op6 && QueryRecordOpIsMulticast(inRequest->op6)))
    {
        return mDNStrue;
    }
    return mDNSfalse;
}

mDNSexport void QueryRecordClientRequestParamsInit(QueryRecordClientRequestParams *inParams)
{
	mDNSPlatformMemZero(inParams, (mDNSu32)sizeof(*inParams));
}

mDNSexport mStatus QueryRecordClientRequestStart(QueryRecordClientRequest *inRequest,
    const QueryRecordClientRequestParams *inParams, QueryRecordResultHandler inResultHandler, void *inResultContext)
{
    mStatus             err;
    domainname          qname;
    mDNSInterfaceID     interfaceID;
    mDNSBool            appendSearchDomains;
    QueryRecordOpParams opParams;
#if MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)
    dnssec_context_t *  dnssecContext = mDNSNULL;
#endif

    err = InterfaceIndexToInterfaceID(inParams->interfaceIndex, &interfaceID);
    if (err) goto exit;

    if (!MakeDomainNameFromDNSNameString(&qname, inParams->qnameStr))
    {
        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT,
               "[R%u] ERROR: bad domain name '" PRI_S "'", inParams->requestID, inParams->qnameStr);
        err = mStatus_BadParamErr;
        goto exit;
    }

    if (RecordTypeIsAddress(inParams->qtype) && !StringEndsWithDot(inParams->qnameStr) &&
        (AlwaysAppendSearchDomains || DomainNameIsSingleLabel(&qname)))
    {
        appendSearchDomains = mDNStrue;
    }
    else
    {
        appendSearchDomains = mDNSfalse;
    }
    QueryRecordOpParamsInit(&opParams);
    opParams.requestID              = inParams->requestID;
    opParams.qname                  = &qname;
    opParams.qtype                  = inParams->qtype;
    opParams.qclass                 = inParams->qclass;
    opParams.interfaceID            = interfaceID;
    opParams.appendSearchDomains    = appendSearchDomains;
    opParams.effectivePID           = inParams->effectivePID;
    opParams.effectiveUUID          = inParams->effectiveUUID;
    opParams.peerUID                = inParams->peerUID;
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    opParams.resolverUUID           = inParams->resolverUUID;
    opParams.customID               = inParams->customID;
    opParams.needEncryption         = inParams->needEncryption;
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
    opParams.peerAuditToken         = inParams->peerAuditToken;
    opParams.delegatorAuditToken    = inParams->delegatorAuditToken;
    opParams.isInAppBrowserRequest  = inParams->isInAppBrowserRequest;
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)
    // Query ends with ".local." and query for RRSIG or ANY type cannot be validated by DNSSEC even if the user sets the
    // kDNSServiceFlagsEnableDNSSEC flag.
    if (FLAGS_CONTAIN_DNSOK_BIT(inParams->flags) && is_eligible_for_dnssec(&qname, inParams->qtype))
    {
        opParams.flags = inParams->flags | kDNSServiceFlagsReturnIntermediates; // to handle CNAME reference
        err = create_dnssec_context_t(inRequest, inParams->requestID, &qname, inParams->qtype, inParams->qclass,
            interfaceID, -1, inParams->flags, appendSearchDomains, inParams->effectivePID, inParams->effectiveUUID,
            inParams->peerUID,
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
            inParams->peerAuditToken, inParams->delegatorAuditToken,
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
            mDNSNULL, inParams->needEncryption, inParams->customID,
#endif
            inResultHandler, inResultContext, mDNSNULL, &dnssecContext);
        require_action(err == mStatus_NoError, exit, log_debug("create_dnssec_context_t failed; error_description='%s'",
            mStatusDescription(err)));

        err = QueryRecordOpStart(&inRequest->op, &opParams, query_record_result_reply_with_dnssec, dnssecContext);
    } else
#endif // MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)
    {
        opParams.flags = inParams->flags;
        err = QueryRecordOpStart(&inRequest->op, &opParams, inResultHandler, inResultContext);
    }

exit:
    if (err) QueryRecordClientRequestStop(inRequest);
    return err;
}

mDNSexport void QueryRecordClientRequestStop(QueryRecordClientRequest *inRequest)
{
    QueryRecordOpStop(&inRequest->op);

#if MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)
    stop_dnssec_if_enable_dnssec(inRequest);
#endif

#if MDNSRESPONDER_SUPPORTS(APPLE, REACHABILITY_TRIGGER)
    if (inRequest->op.answered)
    {
        DNSQuestion *v4q, *v6q;
        // If we are receiving positive answers, provide the hint to the upper layer. - Mohan
        v4q = (inRequest->op.q.qtype == kDNSType_A)    ? &inRequest->op.q : mDNSNULL;
        v6q = (inRequest->op.q.qtype == kDNSType_AAAA) ? &inRequest->op.q : mDNSNULL;
        mDNSPlatformTriggerDNSRetry(v4q, v6q);
    }
#endif
}

mDNSexport const domainname * QueryRecordClientRequestGetQName(const QueryRecordClientRequest *inRequest)
{
    return &inRequest->op.q.qname;
}

mDNSexport mDNSu16 QueryRecordClientRequestGetType(const QueryRecordClientRequest *inRequest)
{
    return inRequest->op.q.qtype;
}

mDNSexport mDNSBool QueryRecordClientRequestIsMulticast(QueryRecordClientRequest *inRequest)
{
    return (QueryRecordOpIsMulticast(&inRequest->op) ? mDNStrue : mDNSfalse);
}
#if MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)
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
    void *                      inResultContext) {
    QueryRecordOpParams opParams;
    QueryRecordOpParamsInit(&opParams);
    opParams.requestID           = inReqID;
    opParams.qname               = inQName;
    opParams.qtype               = inQType;
    opParams.qclass              = inQClass;
    opParams.interfaceID         = inInterfaceID;
    opParams.serviceID           = inServiceID;
    opParams.flags               = inFlags;
    opParams.appendSearchDomains = inAppendSearchDomains;
    opParams.effectivePID        = inPID;
    opParams.effectiveUUID       = inUUID;
    opParams.peerUID             = inUID;
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    opParams.resolverUUID        = inResolverUUID;
    opParams.customID            = inCustomID;
    opParams.needEncryption      = inNeedEncryption;
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
    opParams.peerAuditToken      = inPeerAuditTokenPtr;
    opParams.delegatorAuditToken = inDelegateAuditTokenPtr;
#endif
    return QueryRecordOpStart(inOp, &opParams, inResultHandler, inResultContext);
}

mDNSexport void QueryRecordOpStopForClientRequest(QueryRecordOp *op) {
    QueryRecordOpStop(op);
}

#endif // MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)

mDNSlocal mStatus QueryRecordOpCreate(QueryRecordOp **outOp)
{
    mStatus err;
    QueryRecordOp *op;

    op = (QueryRecordOp *) mDNSPlatformMemAllocateClear(sizeof(*op));
    if (!op)
    {
        err = mStatus_NoMemoryErr;
        goto exit;
    }
    *outOp = op;
    err = mStatus_NoError;

exit:
    return err;
}

mDNSlocal void QueryRecordOpFree(QueryRecordOp *operation)
{
    mDNSPlatformMemFree(operation);
}

#define VALID_MSAD_SRV_TRANSPORT(T) \
    (SameDomainLabel((T)->c, (const mDNSu8 *)"\x4_tcp") || SameDomainLabel((T)->c, (const mDNSu8 *)"\x4_udp"))
#define VALID_MSAD_SRV(Q) ((Q)->qtype == kDNSType_SRV && VALID_MSAD_SRV_TRANSPORT(SecondLabel(&(Q)->qname)))

mDNSlocal mStatus QueryRecordOpStart(QueryRecordOp *inOp, const QueryRecordOpParams *inParams,
    QueryRecordResultHandler inResultHandler, void *inResultContext)
{
    mStatus                 err;
    DNSQuestion * const     q = &inOp->q;
    mDNSu32                 len;

    // Save the original qname.

    len = DomainNameLength(inParams->qname);
    inOp->qname = (domainname *) mDNSPlatformMemAllocate(len);
    if (!inOp->qname)
    {
        err = mStatus_NoMemoryErr;
        goto exit;
    }
    mDNSPlatformMemCopy(inOp->qname, inParams->qname, len);

    inOp->interfaceID   = inParams->interfaceID;
    inOp->reqID         = inParams->requestID;
    inOp->resultHandler = inResultHandler;
    inOp->resultContext = inResultContext;

    // Set up DNSQuestion.

    if (EnableAllowExpired && (inParams->flags & kDNSServiceFlagsAllowExpiredAnswers))
    {
        q->allowExpired = AllowExpired_AllowExpiredAnswers;
    }
    else
    {
        q->allowExpired = AllowExpired_None;
    }
    q->ServiceID = inParams->serviceID;
#if MDNSRESPONDER_SUPPORTS(APPLE, AUDIT_TOKEN)
    q->inAppBrowserRequest = inParams->isInAppBrowserRequest;
    if (inParams->peerAuditToken)
    {
        q->peerAuditToken = *inParams->peerAuditToken;
    }
    if (inParams->delegatorAuditToken)
    {
        q->delegateAuditToken = *inParams->delegatorAuditToken;
    }
#endif
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    if (inParams->resolverUUID)
    {
        mDNSPlatformMemCopy(q->ResolverUUID, inParams->resolverUUID, UUID_SIZE);
    }
#endif
    q->InterfaceID          = inParams->interfaceID;
    q->flags                = inParams->flags;
    AssignDomainName(&q->qname, inParams->qname);
    q->qtype                = inParams->qtype;
    q->qclass               = inParams->qclass;
    q->LongLived            = (inParams->flags & kDNSServiceFlagsLongLivedQuery)            ? mDNStrue : mDNSfalse;
    q->ForceMCast           = (inParams->flags & kDNSServiceFlagsForceMulticast)            ? mDNStrue : mDNSfalse;
    q->ReturnIntermed       = (inParams->flags & kDNSServiceFlagsReturnIntermediates)       ? mDNStrue : mDNSfalse;
    q->SuppressUnusable     = (inParams->flags & kDNSServiceFlagsSuppressUnusable)          ? mDNStrue : mDNSfalse;
    q->TimeoutQuestion      = (inParams->flags & kDNSServiceFlagsTimeout)                   ? mDNStrue : mDNSfalse;
    q->UseBackgroundTraffic = (inParams->flags & kDNSServiceFlagsBackgroundTrafficClass)    ? mDNStrue : mDNSfalse;
    q->AppendSearchDomains  = inParams->appendSearchDomains;
#if MDNSRESPONDER_SUPPORTS(APPLE, QUERIER)
    q->RequireEncryption    = inParams->needEncryption;
    q->CustomID             = inParams->customID;
#endif
    q->InitialCacheMiss     = mDNSfalse;

#if MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)
    err = initialize_dnssec_status_t(&q->DNSSECStatus, inParams->qname, inParams->qtype, inParams->flags, inResultContext);
    require_action(err == mStatus_NoError, exit, log_debug("initialize_dnssec_status failed; error_description='%s'", mStatusDescription(err)));
#endif // MDNSRESPONDER_SUPPORTS(APPLE, DNSSECv2)

    q->pid              = inParams->effectivePID;
    if (inParams->effectiveUUID)
    {
        mDNSPlatformMemCopy(q->uuid, inParams->effectiveUUID, UUID_SIZE);
    }
    q->euid             = inParams->peerUID;
    q->request_id       = inParams->requestID;
    q->QuestionCallback = QueryRecordOpCallback;
    q->ResetHandler     = QueryRecordOpResetHandler;

    // For single label queries that are not fully qualified, look at /etc/hosts, cache and try search domains before trying
    // them on the wire as a single label query. - Mohan

    if (q->AppendSearchDomains && DomainNameIsSingleLabel(inOp->qname)) q->InterfaceID = mDNSInterface_LocalOnly;
    err = QueryRecordOpStartQuestion(inOp, q);
    if (err) goto exit;

#if MDNSRESPONDER_SUPPORTS(APPLE, D2D)
    if (callExternalHelpers(q->InterfaceID, &q->qname, q->flags))
    {
        external_start_browsing_for_service(q->InterfaceID, &q->qname, q->qtype, q->flags, q->pid);
    }
#endif

#if MDNSRESPONDER_SUPPORTS(APPLE, UNICAST_DOTLOCAL)
    if ((RecordTypeIsAddress(q->qtype) || VALID_MSAD_SRV(&inOp->q)) && !q->ForceMCast &&
        SameDomainLabel(LastLabel(&q->qname), (const mDNSu8 *)&localdomain))
    {
        DNSQuestion *       q2;

        q2 = (DNSQuestion *) mDNSPlatformMemAllocate((mDNSu32)sizeof(*inOp->q2));
        if (!q2)
        {
            err = mStatus_NoMemoryErr;
            goto exit;
        }
        inOp->q2 = q2;

        *q2 = *q;
        q2->IsUnicastDotLocal = mDNStrue;

        if ((CountLabels(&q2->qname) == 2) && !SameDomainName(&q2->qname, &ActiveDirectoryPrimaryDomain)
            && !DomainNameIsInSearchList(&q2->qname, mDNSfalse))
        {
            inOp->q2Type                = q2->qtype;
            inOp->q2LongLived           = q2->LongLived;
            inOp->q2ReturnIntermed      = q2->ReturnIntermed;
            inOp->q2TimeoutQuestion     = q2->TimeoutQuestion;
            inOp->q2AppendSearchDomains = q2->AppendSearchDomains;

            AssignDomainName(&q2->qname, &localdomain);
            q2->qtype                   = kDNSType_SOA;
            q2->LongLived               = mDNSfalse;
            q2->ReturnIntermed          = mDNStrue;
            q2->TimeoutQuestion         = mDNSfalse;
            q2->AppendSearchDomains     = mDNSfalse;
        }

        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_INFO,
               "[R%u] QueryRecordOpStart: starting parallel unicast query for " PRI_DM_NAME " " PUB_S,
               inOp->reqID, DM_NAME_PARAM(&q2->qname), DNSTypeName(q2->qtype));

        err = QueryRecordOpStartQuestion(inOp, q2);
        if (err) goto exit;
    }
#endif
    err = mStatus_NoError;

exit:
    if (err) QueryRecordOpStop(inOp);
    return err;
}

mDNSlocal void QueryRecordOpStop(QueryRecordOp *op)
{
    if (op->q.QuestionContext)
    {
        QueryRecordOpStopQuestion(&op->q);
#if MDNSRESPONDER_SUPPORTS(APPLE, D2D)
        if (callExternalHelpers(op->q.InterfaceID, op->qname, op->q.flags))
        {
            external_stop_browsing_for_service(op->q.InterfaceID, &op->q.qname, op->q.qtype, op->q.flags, op->q.pid);
        }
#endif
    }
    if (op->qname)
    {
        mDNSPlatformMemFree(op->qname);
        op->qname = mDNSNULL;
    }
#if MDNSRESPONDER_SUPPORTS(APPLE, UNICAST_DOTLOCAL)
    if (op->q2)
    {
        if (op->q2->QuestionContext) QueryRecordOpStopQuestion(op->q2);
        mDNSPlatformMemFree(op->q2);
        op->q2 = mDNSNULL;
    }
#endif
}

mDNSlocal mDNSBool QueryRecordOpIsMulticast(const QueryRecordOp *op)
{
    return ((mDNSOpaque16IsZero(op->q.TargetQID) && (op->q.ThisQInterval > 0)) ? mDNStrue : mDNSfalse);
}

// GetTimeNow is a callback-safe alternative to mDNS_TimeNow(), which expects to be called with m->mDNS_busy == 0.
mDNSlocal mDNSs32 GetTimeNow(mDNS *m)
{
    mDNSs32 time;
    mDNS_Lock(m);
    time = m->timenow;
    mDNS_Unlock(m);
    return time;
}

mDNSlocal void QueryRecordOpCallback(mDNS *m, DNSQuestion *inQuestion, const ResourceRecord *inAnswer, QC_result inAddRecord)
{
    mStatus                     resultErr;
    QueryRecordOp *const        op = (QueryRecordOp *)inQuestion->QuestionContext;
    const domainname *          domain;

#if MDNSRESPONDER_SUPPORTS(APPLE, UNICAST_DOTLOCAL)
    if ((inQuestion == op->q2) && (inQuestion->qtype == kDNSType_SOA))
    {
        DNSQuestion * const     q2 = op->q2;

        if (inAnswer->rrtype != kDNSType_SOA) goto exit;
        QueryRecordOpStopQuestion(q2);

        // Restore DNSQuestion variables that were modified for the SOA query.

        q2->qtype               = op->q2Type;
        q2->LongLived           = op->q2LongLived;
        q2->ReturnIntermed      = op->q2ReturnIntermed;
        q2->TimeoutQuestion     = op->q2TimeoutQuestion;
        q2->AppendSearchDomains = op->q2AppendSearchDomains;

        if (inAnswer->RecordType != kDNSRecordTypePacketNegative)
        {
            QueryRecordOpRestartUnicastQuestion(op, q2, mDNSNULL);
        }
        else if (q2->AppendSearchDomains)
        {
            domain = NextSearchDomain(op);
            if (domain) QueryRecordOpRestartUnicastQuestion(op, q2, domain);
        }
        goto exit;
    }
#endif

    if (inAddRecord == QC_suppressed)
    {
        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEBUG,
               "[R%u] QueryRecordOpCallback: Suppressed question " PRI_DM_NAME " (" PUB_S ")",
               op->reqID, DM_NAME_PARAM(&inQuestion->qname), DNSTypeName(inQuestion->qtype));

        resultErr = kDNSServiceErr_NoSuchRecord;
    }
    else if (inAnswer->RecordType == kDNSRecordTypePacketNegative)
    {
        if (inQuestion->TimeoutQuestion && ((GetTimeNow(m) - inQuestion->StopTime) >= 0))
        {
            LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_INFO,
                   "[R%u] QueryRecordOpCallback: Question " PRI_DM_NAME " (" PUB_S ") timing out, InterfaceID %p",
                   op->reqID, DM_NAME_PARAM(&inQuestion->qname), DNSTypeName(inQuestion->qtype),
                   inQuestion->InterfaceID);
            resultErr = kDNSServiceErr_Timeout;
        }
        else
        {
            if (inQuestion->AppendSearchDomains && (op->searchListIndex >= 0) && inAddRecord)
            {
                domain = NextSearchDomain(op);
                if (domain || DomainNameIsSingleLabel(op->qname))
                {
                    QueryRecordOpStopQuestion(inQuestion);
                    QueryRecordOpRestartUnicastQuestion(op, inQuestion, domain);
                    goto exit;
                }
            }
#if MDNSRESPONDER_SUPPORTS(APPLE, UNICAST_DOTLOCAL)
            if (!inAnswer->InterfaceID && IsLocalDomain(inAnswer->name))
            {
                if ((RecordTypeIsAddress(inQuestion->qtype) &&
                    (inAnswer->negativeRecordType == kNegativeRecordType_NoData)) ||
                    DomainNameIsInSearchList(&inQuestion->qname, mDNStrue))
                {
                    LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_INFO,
                           "[R%u] QueryRecordOpCallback: Question " PRI_DM_NAME " (" PUB_S ") answering local with negative unicast response",
                           op->reqID, DM_NAME_PARAM(&inQuestion->qname), DNSTypeName(inQuestion->qtype));
                }
                else
                {
                    goto exit;
                }
            }
#endif
            resultErr = kDNSServiceErr_NoSuchRecord;
        }
    }
    else
    {
        resultErr = kDNSServiceErr_NoError;
    }

#if MDNSRESPONDER_SUPPORTS(APPLE, REACHABILITY_TRIGGER)
    if ((resultErr != kDNSServiceErr_Timeout) && (inAddRecord == QC_add))
    {
        op->answered = mDNStrue;
    }
#endif

    if (op->resultHandler) op->resultHandler(m, inQuestion, inAnswer, inAddRecord, resultErr, op->resultContext);
    if (resultErr == kDNSServiceErr_Timeout) QueryRecordOpStopQuestion(inQuestion);

#if MDNSRESPONDER_SUPPORTS(APPLE, WEB_CONTENT_FILTER)
    NotifyWebContentFilter(inAnswer, inQuestion->euid);
#endif

exit:
    return;
}

mDNSlocal void QueryRecordOpResetHandler(DNSQuestion *inQuestion)
{
    QueryRecordOp *const        op = (QueryRecordOp *)inQuestion->QuestionContext;

    AssignDomainName(&inQuestion->qname, op->qname);
    if (inQuestion->AppendSearchDomains && DomainNameIsSingleLabel(op->qname))
    {
        inQuestion->InterfaceID = mDNSInterface_LocalOnly;
    }
    else
    {
        inQuestion->InterfaceID = op->interfaceID;
    }
    op->searchListIndex = 0;
}

mDNSlocal mStatus QueryRecordOpStartQuestion(QueryRecordOp *inOp, DNSQuestion *inQuestion)
{
    mStatus     err;

    inQuestion->QuestionContext = inOp;
    err = mDNS_StartQuery(&mDNSStorage, inQuestion);
    if (err)
    {
        LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_DEFAULT,
               "[R%u] ERROR: QueryRecordOpStartQuestion mDNS_StartQuery for " PRI_DM_NAME " " PUB_S " failed with error %d",
               inOp->reqID, DM_NAME_PARAM(&inQuestion->qname), DNSTypeName(inQuestion->qtype), err);
        inQuestion->QuestionContext = mDNSNULL;
    }
    return err;
}

mDNSlocal mStatus QueryRecordOpStopQuestion(DNSQuestion *inQuestion)
{
    mStatus     err;

    err = mDNS_StopQuery(&mDNSStorage, inQuestion);
    inQuestion->QuestionContext = mDNSNULL;
    return err;
}

mDNSlocal mStatus QueryRecordOpRestartUnicastQuestion(QueryRecordOp *inOp, DNSQuestion *inQuestion,
    const domainname *inSearchDomain)
{
    mStatus     err;

    inQuestion->InterfaceID = inOp->interfaceID;
    AssignDomainName(&inQuestion->qname, inOp->qname);
    if (inSearchDomain) AppendDomainName(&inQuestion->qname, inSearchDomain);
    if (SameDomainLabel(LastLabel(&inQuestion->qname), (const mDNSu8 *)&localdomain))
    {
        inQuestion->IsUnicastDotLocal = mDNStrue;
    }
    else
    {
        inQuestion->IsUnicastDotLocal = mDNSfalse;
    }
    err = QueryRecordOpStartQuestion(inOp, inQuestion);
    return err;
}

mDNSlocal mStatus InterfaceIndexToInterfaceID(mDNSu32 inInterfaceIndex, mDNSInterfaceID *outInterfaceID)
{
    mStatus             err;
    mDNSInterfaceID     interfaceID;

    interfaceID = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, inInterfaceIndex);

#if MDNSRESPONDER_SUPPORTS(APPLE, UNREADY_INTERFACES)
    // The request is scoped to a specific interface index, but the interface is not currently in our list.
    if ((inInterfaceIndex != kDNSServiceInterfaceIndexAny) && (interfaceID == mDNSInterface_Any))
    {
        static dispatch_once_t      getLoopbackIndexOnce = 0;
        static mDNSu32              loopbackIndex = 0;

        dispatch_once(&getLoopbackIndexOnce,
        ^{
            loopbackIndex = if_nametoindex("lo0");
        });

        // If it's one of the specially defined inteface index values, just return an error. Also, caller should return an
        // error immediately if lo0 is not configured into the current active interfaces. See <rdar://problem/21967160>.
        if ((inInterfaceIndex == kDNSServiceInterfaceIndexLocalOnly) ||
            (inInterfaceIndex == kDNSServiceInterfaceIndexUnicast)   ||
            (inInterfaceIndex == kDNSServiceInterfaceIndexP2P)       ||
            (inInterfaceIndex == kDNSServiceInterfaceIndexBLE)       ||
            (inInterfaceIndex == loopbackIndex))
        {
            LogInfo("ERROR: bad interfaceIndex %d", inInterfaceIndex);
            err = mStatus_BadParamErr;
            goto exit;
        }

        // Otherwise, use the specified interface index value and the request will be applied to that interface when it
        // comes up.
        interfaceID = (mDNSInterfaceID)(uintptr_t)inInterfaceIndex;
        LogInfo("Query pending for interface index %d", inInterfaceIndex);
    }
#endif

    *outInterfaceID = interfaceID;
    err = mStatus_NoError;

#if MDNSRESPONDER_SUPPORTS(APPLE, UNREADY_INTERFACES)
exit:
#endif
    return err;
}

mDNSlocal mDNSBool DomainNameIsSingleLabel(const domainname *inName)
{
    const mDNSu8 *const     label = inName->c;
    return (((label[0] != 0) && (label[1 + label[0]] == 0)) ? mDNStrue : mDNSfalse);
}

mDNSlocal mDNSBool StringEndsWithDot(const char *inString)
{
    const char *        ptr;
    mDNSu32             escapeCount;
    mDNSBool            result;

    // Loop invariant: escapeCount is the number of consecutive escape characters that immediately precede *ptr.
    // - If escapeCount is even, then *ptr is immediately preceded by escapeCount / 2 consecutive literal backslash
    //   characters, so *ptr is not escaped.
    // - If escapeCount is odd, then *ptr is immediately preceded by (escapeCount - 1) / 2 consecutive literal backslash
    //   characters followed by an escape character, so *ptr is escaped.
    escapeCount = 0;
    result = mDNSfalse;
    for (ptr = inString; *ptr != '\0'; ptr++)
    {
        if (*ptr == '\\')
        {
            escapeCount++;
        }
        else
        {
            if ((*ptr == '.') && (ptr[1] == '\0'))
            {
                if ((escapeCount % 2) == 0) result = mDNStrue;
                break;
            }
            escapeCount = 0;
        }
    }
    return result;
}

mDNSlocal const domainname * NextSearchDomain(QueryRecordOp *inOp)
{
    const domainname *      domain;

    while ((domain = uDNS_GetNextSearchDomain(inOp->interfaceID, &inOp->searchListIndex, mDNSfalse)) != mDNSNULL)
    {
        if ((DomainNameLength(inOp->qname) - 1 + DomainNameLength(domain)) <= MAX_DOMAIN_NAME) break;
    }
    if (!domain) inOp->searchListIndex = -1;
    return domain;
}

#if MDNSRESPONDER_SUPPORTS(APPLE, UNICAST_DOTLOCAL)
mDNSlocal mDNSBool DomainNameIsInSearchList(const domainname *inName, mDNSBool inExcludeLocal)
{
    const SearchListElem *      item;
    int                         labelCount, domainLabelCount;

    labelCount = CountLabels(inName);
    for (item = SearchList; item; item = item->next)
    {
        if (inExcludeLocal && SameDomainName(&item->domain, &localdomain)) continue;
        domainLabelCount = CountLabels(&item->domain);
        if (labelCount >= domainLabelCount)
        {
            if (SameDomainName(&item->domain, SkipLeadingLabels(inName, (labelCount - domainLabelCount))))
            {
                return mDNStrue;
            }
        }
    }
    return mDNSfalse;
}
#endif

#if MDNSRESPONDER_SUPPORTS(APPLE, WEB_CONTENT_FILTER)
mDNSlocal void NotifyWebContentFilter(const ResourceRecord *inAnswer, uid_t inUID)
{
    if (WCFIsServerRunning)
    {
		const mDNS *const m = &mDNSStorage;

        if (WCFIsServerRunning(m->WCF) && inAnswer->rdlength != 0)
        {
			struct sockaddr_storage addr;
			addr.ss_len = 0;
			if (inAnswer->rrtype == kDNSType_A || inAnswer->rrtype == kDNSType_AAAA)
			{
				if (inAnswer->rrtype == kDNSType_A)
				{
					struct sockaddr_in *const sin = (struct sockaddr_in *)&addr;
					sin->sin_port = 0;
					// Instead of this stupid call to putRData it would be much simpler to just assign the value in the sensible way, like this:
					// sin->sin_addr.s_addr = inAnswer->rdata->u.ipv4.NotAnInteger;
					if (!putRData(mDNSNULL, (mDNSu8 *)&sin->sin_addr, (mDNSu8 *)(&sin->sin_addr + sizeof(mDNSv4Addr)), inAnswer))
						LogMsg("NotifyWebContentFilter: WCF AF_INET putRData failed");
					else
					{
						addr.ss_len = sizeof (struct sockaddr_in);
						addr.ss_family = AF_INET;
					}
				}
				else if (inAnswer->rrtype == kDNSType_AAAA)
				{
					struct sockaddr_in6 *const sin6 = (struct sockaddr_in6 *)&addr;
					sin6->sin6_port = 0;
					// Instead of this stupid call to putRData it would be much simpler to just assign the value in the sensible way, like this:
					// sin6->sin6_addr.__u6_addr.__u6_addr32[0] = inAnswer->rdata->u.ipv6.l[0];
					// sin6->sin6_addr.__u6_addr.__u6_addr32[1] = inAnswer->rdata->u.ipv6.l[1];
					// sin6->sin6_addr.__u6_addr.__u6_addr32[2] = inAnswer->rdata->u.ipv6.l[2];
					// sin6->sin6_addr.__u6_addr.__u6_addr32[3] = inAnswer->rdata->u.ipv6.l[3];
					if (!putRData(mDNSNULL, (mDNSu8 *)&sin6->sin6_addr, (mDNSu8 *)(&sin6->sin6_addr + sizeof(mDNSv6Addr)), inAnswer))
						LogMsg("NotifyWebContentFilter: WCF AF_INET6 putRData failed");
					else
					{
						addr.ss_len = sizeof (struct sockaddr_in6);
						addr.ss_family = AF_INET6;
					}
				}
				if (addr.ss_len)
				{
        			char name[MAX_ESCAPED_DOMAIN_NAME];
        			ConvertDomainNameToCString(inAnswer->name, name);

					debugf("NotifyWebContentFilter: Name %s, uid %u, addr length %d", name, inUID, addr.ss_len);
					if (WCFNameResolvesToAddr)
					{
						WCFNameResolvesToAddr(m->WCF, name, (struct sockaddr *)&addr, inUID);
					}
				}
			}
			else if (inAnswer->rrtype == kDNSType_CNAME)
			{
				domainname cname;
        		char name[MAX_ESCAPED_DOMAIN_NAME];
				char cname_cstr[MAX_ESCAPED_DOMAIN_NAME];

				if (!putRData(mDNSNULL, cname.c, (mDNSu8 *)(cname.c + MAX_DOMAIN_NAME), inAnswer))
					LogMsg("NotifyWebContentFilter: WCF CNAME putRData failed");
				else
				{
        			ConvertDomainNameToCString(inAnswer->name, name);
					ConvertDomainNameToCString(&cname, cname_cstr);
					if (WCFNameResolvesToAddr)
					{
						WCFNameResolvesToName(m->WCF, name, cname_cstr, inUID);
					}
				}
			}
        }
    }
}
#endif
