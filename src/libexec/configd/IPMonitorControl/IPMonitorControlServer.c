/*
 * Copyright (c) 2013-2021 Apple Inc. All rights reserved.
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
 * IPMonitorControlServer.c
 * - IPC channel to IPMonitor
 * - used to create interface rank assertions
 */

/*
 * Modification History
 *
 * December 16, 2013	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <CoreFoundation/CoreFoundation.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <sys/queue.h>
#include <CoreFoundation/CFRunLoop.h>
#include <SystemConfiguration/SCNetworkConfigurationPrivate.h>
#include <SystemConfiguration/SCPrivate.h>
#include <os/state_private.h>
#include "IPMonitorControlServer.h"
#include "symbol_scope.h"
#include "IPMonitorControlPrivate.h"
#include "IPMonitorAWDReport.h"

#ifdef TEST_IPMONITOR_CONTROL
#define	my_log(__level, __format, ...)	SCPrint(TRUE, stdout, CFSTR(__format "\n"), ## __VA_ARGS__)

#else /* TEST_IPMONITOR_CONTROL */
#include "ip_plugin.h"
#endif /* TEST_IPMONITOR_CONTROL */

STATIC dispatch_queue_t 	S_IPMonitorControlServerQueue;

STATIC dispatch_queue_t
IPMonitorControlServerGetQueue(void)
{
    return (S_IPMonitorControlServerQueue);
}

STATIC void
IPMonitorControlServerSetQueue(dispatch_queue_t queue)
{
    S_IPMonitorControlServerQueue = queue;
}

typedef struct ControlSession ControlSession, * ControlSessionRef;

#define LIST_HEAD_ControlSession LIST_HEAD(ControlSessionHead, ControlSession)
#define LIST_ENTRY_ControlSession LIST_ENTRY(ControlSession)
LIST_HEAD_ControlSession	S_ControlSessions;

struct ControlSession {
    LIST_ENTRY_ControlSession	link;
    xpc_connection_t		connection;
    pid_t			pid;
    char *			process_name;

    CFMutableDictionaryRef	assertions; /* ifname<string> = rank<number> */
    CFMutableDictionaryRef	advisories; /* ifname<string> = advisory<number> */
};

/**
 ** Support Functions
 **/
STATIC CFMutableArrayRef	S_if_changes;
STATIC CFRange			S_if_changes_range;


STATIC CFMutableDictionaryRef
mutable_dict_create(CFIndex capacity)
{
    return CFDictionaryCreateMutable(NULL,
				     capacity,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
}

STATIC CFMutableArrayRef
mutable_array_create(CFIndex capacity)
{
    return CFArrayCreateMutable(NULL, capacity, &kCFTypeArrayCallBacks);
}

STATIC CFNumberRef
RankLastNumberGet(void)
{
    STATIC CFNumberRef		rank_last;

    if (rank_last == NULL) {
	SCNetworkServicePrimaryRank	rank;

	rank = kSCNetworkServicePrimaryRankLast;
	rank_last = CFNumberCreate(NULL, kCFNumberSInt32Type, &rank);
    }
    return (rank_last);
}

STATIC void
InterfaceChangedListAddInterface(CFStringRef ifname)
{
    if (S_if_changes == NULL) {
	S_if_changes = mutable_array_create(0);
	CFArrayAppendValue(S_if_changes, ifname);
	S_if_changes_range.length = 1;
    }
    else if (!CFArrayContainsValue(S_if_changes, S_if_changes_range, ifname)) {
	CFArrayAppendValue(S_if_changes, ifname);
	S_if_changes_range.length++;
    }
}

STATIC CFArrayRef
InterfaceChangedListCopy(void)
{
    CFArrayRef		current_list;

    current_list = S_if_changes;
    S_if_changes = NULL;
    return (current_list);
}

STATIC void
InterfaceRankAssertionAdd(const void * key, const void * value, void * context)
{
    CFMutableDictionaryRef * 	assertions_p;
    CFNumberRef			existing_rank;
    CFNumberRef			rank = (CFNumberRef)value;

    assertions_p = (CFMutableDictionaryRef *)context;
    if (*assertions_p == NULL) {
	*assertions_p = mutable_dict_create(0);
	CFDictionarySetValue(*assertions_p, key, rank);
	return;
    }
    existing_rank = CFDictionaryGetValue(*assertions_p, key);
    if (existing_rank == NULL
	|| (CFNumberCompare(rank, existing_rank, NULL)
	    == kCFCompareGreaterThan)) {
	CFDictionarySetValue(*assertions_p, key, rank);
    }
    return;
}

STATIC void
InterfaceAdvisoryAdd(const void * key, const void * value, void * context)
{
#pragma unused(value)
    CFMutableDictionaryRef * 	assertions_p;
    CFNumberRef			existing_rank;
    CFNumberRef			rank;

    /* an interface advisory implies RankLast */
    rank = RankLastNumberGet();
    assertions_p = (CFMutableDictionaryRef *)context;
    if (*assertions_p == NULL) {
	*assertions_p = mutable_dict_create(0);
	CFDictionarySetValue(*assertions_p, key, rank);
	return;
    }
    existing_rank = CFDictionaryGetValue(*assertions_p, key);
    if (existing_rank == NULL
	|| (CFNumberCompare(rank, existing_rank, NULL)
	    == kCFCompareGreaterThan)) {
	CFDictionarySetValue(*assertions_p, key, rank);
    }
    return;
}

STATIC CFDictionaryRef
InterfaceRankAssertionsCopy(void)
{
    CFMutableDictionaryRef	assertions = NULL;
    ControlSessionRef		session;

    LIST_FOREACH(session, &S_ControlSessions, link) {
	if (session->advisories != NULL) {
	    CFDictionaryApplyFunction(session->advisories,
				      InterfaceAdvisoryAdd,
				      &assertions);
	}
	if (session->assertions != NULL) {
	    CFDictionaryApplyFunction(session->assertions,
				      InterfaceRankAssertionAdd,
				      &assertions);
	}
    }
    return (assertions);
}

STATIC Boolean
InterfaceHasAdvisory(CFStringRef ifname,
		     SCNetworkInterfaceAdvisory advisory)
{
    ControlSessionRef		session;

    LIST_FOREACH(session, &S_ControlSessions, link) {
	if (session->advisories != NULL) {
	    CFNumberRef		this_advisory_cf;

	    this_advisory_cf = CFDictionaryGetValue(session->advisories, ifname);
	    if (this_advisory_cf != NULL) {
		SCNetworkInterfaceAdvisory 	this_advisory;

		if (advisory == kSCNetworkInterfaceAdvisoryNone) {
		    /* not looking for a specific advisory value */
		    return (TRUE);
		}
		(void)CFNumberGetValue(this_advisory_cf, kCFNumberSInt32Type,
				       &this_advisory);
		if (this_advisory == advisory) {
		    /* found the specific advisory */
		    return (TRUE);
		}
		/* keep looking for the specific advisory */
	    }
	}
    }
    return (FALSE);
}


STATIC AWDIPMonitorInterfaceAdvisoryReport_Flags
advisory_to_flags(SCNetworkInterfaceAdvisory advisory)
{
    AWDIPMonitorInterfaceAdvisoryReport_Flags	flags;

    switch (advisory) {
    case kSCNetworkInterfaceAdvisoryNone:
    default:
	flags = 0;
	break;
    case kSCNetworkInterfaceAdvisoryLinkLayerIssue:
	flags = AWDIPMonitorInterfaceAdvisoryReport_Flags_LINK_LAYER_ISSUE;
	break;
    case kSCNetworkInterfaceAdvisoryUplinkIssue:
	flags = AWDIPMonitorInterfaceAdvisoryReport_Flags_UPLINK_ISSUE;
	break;
    }
    return (flags);
}

STATIC AWDIPMonitorInterfaceAdvisoryReport_Flags
InterfaceGetAdvisoryFlags(CFStringRef ifname,
			  ControlSessionRef exclude_session,
			  uint32_t * ret_count)
{
    uint32_t					count;
    AWDIPMonitorInterfaceAdvisoryReport_Flags	flags = 0;
    ControlSessionRef				session;

    count = 0;
    LIST_FOREACH(session, &S_ControlSessions, link) {
	SCNetworkInterfaceAdvisory 	advisory = 0;
	CFNumberRef			advisory_cf;

	if (session->advisories == NULL) {
	    continue;
	}
	if (exclude_session != NULL && exclude_session == session) {
	    continue;
	}
	advisory_cf = CFDictionaryGetValue(session->advisories, ifname);
	if (advisory_cf == NULL) {
	    /* session has no advisories for this interface */
	    continue;
	}
	(void)CFNumberGetValue(advisory_cf, kCFNumberSInt32Type, &advisory);
	flags |= advisory_to_flags(advisory);
	count++;
    }
    *ret_count = count;
    return (flags);
}

STATIC Boolean
AnyInterfaceHasAdvisories(void)
{
    ControlSessionRef		session;

    LIST_FOREACH(session, &S_ControlSessions, link) {
	if (session->advisories != NULL) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

STATIC CFRunLoopRef		S_runloop;
STATIC CFRunLoopSourceRef	S_signal_source;

STATIC void
SetNotificationInfo(CFRunLoopRef runloop, CFRunLoopSourceRef rls)
{
    S_runloop = runloop;
    S_signal_source = rls;
    return;
}

STATIC void
NotifyIPMonitor(void)
{
    if (S_signal_source != NULL) {
	CFRunLoopSourceSignal(S_signal_source);
	if (S_runloop != NULL) {
	    CFRunLoopWakeUp(S_runloop);
	}
    }
    return;
}

STATIC void
NotifyInterfaceRankAssertion(CFStringRef ifname)
{
    CFStringRef		key;

    key = _IPMonitorControlCopyInterfaceRankAssertionNotificationKey(ifname);
    SCDynamicStoreNotifyValue(NULL, key);
    CFRelease(key);
    return;
}

STATIC void
NotifyInterfaceAdvisory(CFStringRef ifname)
{
    CFStringRef		key;

    key = _IPMonitorControlCopyInterfaceAdvisoryNotificationKey(ifname);
    SCDynamicStoreNotifyValue(NULL, key);
    CFRelease(key);
    return;
}

STATIC void
SubmitInterfaceAdvisoryMetric(CFStringRef ifname,
			      AWDIPMonitorInterfaceAdvisoryReport_Flags flags,
			      uint32_t count)
{
    InterfaceAdvisoryReportRef	report;
    AWDIPMonitorInterfaceType	type;

    /* XXX need to actually figure out what the interface type is */
    if (CFStringHasPrefix(ifname, CFSTR("pdp"))) {
	type = AWDIPMonitorInterfaceType_IPMONITOR_INTERFACE_TYPE_CELLULAR;
    }
    else {
	type = AWDIPMonitorInterfaceType_IPMONITOR_INTERFACE_TYPE_WIFI;
    }
    report = InterfaceAdvisoryReportCreate(type);
    if (report == NULL) {
	return;
    }
    InterfaceAdvisoryReportSetFlags(report, flags);
    InterfaceAdvisoryReportSetAdvisoryCount(report, count);
    InterfaceAdvisoryReportSubmit(report);
    my_log(LOG_NOTICE, "%@: submitted AWD report %@", ifname, report);
    CFRelease(report);
}

/**
 ** ControlSession
 **/
STATIC pid_t
ControlSessionGetPID(ControlSessionRef session)
{
    return (session->pid);
}

STATIC const char *
ControlSessionGetProcessName(ControlSessionRef session)
{
    return (session->process_name);
}

STATIC void
AddChangedInterfaceNotifyRankAssertion(const void * key, const void * value,
				       void * context)
{
#pragma unused(value)
#pragma unused(context)
    InterfaceChangedListAddInterface((CFStringRef)key);
    NotifyInterfaceRankAssertion((CFStringRef)key);
    return;
}

STATIC void
AddChangedInterfaceNotifyAdvisory(const void * key, const void * value,
				  void * context)
{
#pragma unused(value)
#pragma unused(context)
    InterfaceChangedListAddInterface((CFStringRef)key);
    NotifyInterfaceAdvisory((CFStringRef)key);
    return;
}

STATIC void
GenerateMetricForInterfaceAtSessionClose(const void * key, const void * value,
					 void * context)
{
    uint32_t		count_after;
    uint32_t		count_before;
    AWDIPMonitorInterfaceAdvisoryReport_Flags flags_after;
    AWDIPMonitorInterfaceAdvisoryReport_Flags flags_before;
    CFStringRef		ifname = (CFStringRef)key;
    ControlSessionRef	session = (ControlSessionRef)context;

#pragma unused(value)
    /*
     * Get the flags and count including this session, then again
     * excluding this session. If either flags or count are different,
     * generate the metric.
     */
    flags_before = InterfaceGetAdvisoryFlags(ifname, NULL, &count_before);
    flags_after	= InterfaceGetAdvisoryFlags(ifname, session, &count_after);
    if (flags_before != flags_after || count_before != count_after) {
	SubmitInterfaceAdvisoryMetric(ifname, flags_after, count_after);
    }
    return;
}

STATIC void
ControlSessionGenerateMetricsAtClose(ControlSessionRef session)
{
    if (session->advisories == NULL) {
	return;
    }
    CFDictionaryApplyFunction(session->advisories,
			      GenerateMetricForInterfaceAtSessionClose,
			      session);
}

STATIC void
ControlSessionInvalidate(ControlSessionRef session)
{
    my_log(LOG_DEBUG, "Invalidating %p", session);
    ControlSessionGenerateMetricsAtClose(session);
    LIST_REMOVE(session, link);
    if (session->assertions != NULL || session->advisories != NULL) {
	if (session->advisories != NULL) {
	    my_log(LOG_NOTICE,
		   "pid %d removing advisories %@",
		   xpc_connection_get_pid(session->connection),
		   session->advisories);
	    CFDictionaryApplyFunction(session->advisories,
				      AddChangedInterfaceNotifyAdvisory,
				      NULL);
	    my_CFRelease(&session->advisories);
	}
	if (session->assertions != NULL) {
	    my_log(LOG_NOTICE,
		   "pid %d removing assertions %@",
		   xpc_connection_get_pid(session->connection),
		   session->assertions);
	    CFDictionaryApplyFunction(session->assertions,
				      AddChangedInterfaceNotifyRankAssertion,
				      NULL);
	    my_CFRelease(&session->assertions);
	}
	NotifyIPMonitor();
    }
    return;
}

STATIC void
ControlSessionRelease(void * p)
{
    ControlSessionRef	session = (ControlSessionRef)p;

    my_log(LOG_DEBUG, "Releasing %s [%d] session %p",
	   ControlSessionGetProcessName(session),
	   ControlSessionGetPID(session), p);
    free(session->process_name);
    free(p);
    return;
}

STATIC ControlSessionRef
ControlSessionLookup(xpc_connection_t connection)
{
    return ((ControlSessionRef)xpc_connection_get_context(connection));
}


STATIC ControlSessionRef
ControlSessionCreate(xpc_connection_t connection, xpc_object_t request)
{
    const char *	process_name;
    ControlSessionRef	session;

    session = (ControlSessionRef)malloc(sizeof(*session));
    memset(session, 0, sizeof(*session));
    session->connection = connection;
    session->pid = xpc_connection_get_pid(connection);
    process_name
	= xpc_dictionary_get_string(request,
				    kIPMonitorControlRequestKeyProcessName);
    if (process_name == NULL) {
	process_name = "<unknown>";
    }
    session->process_name = strdup(process_name);
    xpc_connection_set_finalizer_f(connection, ControlSessionRelease);
    xpc_connection_set_context(connection, session);
    LIST_INSERT_HEAD(&S_ControlSessions, session, link);
    my_log(LOG_DEBUG, "Created %s [%d] session %p (connection %p)",
	   ControlSessionGetProcessName(session),
	   ControlSessionGetPID(session),
	   session, connection);
    return (session);
}

STATIC ControlSessionRef
ControlSessionForConnection(xpc_connection_t connection, xpc_object_t request)
{
    ControlSessionRef	session;

    session = ControlSessionLookup(connection);
    if (session != NULL) {
	return (session);
    }
    return (ControlSessionCreate(connection, request));
}

STATIC void
ControlSessionSetInterfaceRank(ControlSessionRef session,
			       const char * ifname,
			       SCNetworkServicePrimaryRank rank)
{
    CFStringRef		ifname_cf;

    if (session->assertions == NULL) {
	if (rank == kSCNetworkServicePrimaryRankDefault) {
	    /* no assertions, no need to store rank */
	    return;
	}
	session->assertions = mutable_dict_create(0);
    }
    ifname_cf = CFStringCreateWithCString(NULL, ifname,
					  kCFStringEncodingUTF8);

    if (rank == kSCNetworkServicePrimaryRankDefault) {
	CFDictionaryRemoveValue(session->assertions, ifname_cf);
	if (CFDictionaryGetCount(session->assertions) == 0) {
	    CFRelease(session->assertions);
	    session->assertions = NULL;
	}
    }
    else {
	CFNumberRef	rank_cf;

	rank_cf = CFNumberCreate(NULL, kCFNumberSInt32Type, &rank);
	CFDictionarySetValue(session->assertions, ifname_cf, rank_cf);
	CFRelease(rank_cf);
    }
    InterfaceChangedListAddInterface(ifname_cf);
    NotifyIPMonitor();
    NotifyInterfaceRankAssertion(ifname_cf);
    CFRelease(ifname_cf);
    return;
}

STATIC SCNetworkServicePrimaryRank
ControlSessionGetInterfaceRank(ControlSessionRef session,
			       const char * ifname)
{
    SCNetworkServicePrimaryRank	rank = kSCNetworkServicePrimaryRankDefault;

    if (session->assertions != NULL) {
	CFStringRef		ifname_cf;
	CFNumberRef		rank_cf;

	ifname_cf = CFStringCreateWithCString(NULL, ifname,
					      kCFStringEncodingUTF8);
	rank_cf = CFDictionaryGetValue(session->assertions, ifname_cf);
	CFRelease(ifname_cf);
	if (rank_cf != NULL) {
	    (void)CFNumberGetValue(rank_cf, kCFNumberSInt32Type, &rank);
	}
    }
    return (rank);
}

STATIC void
ControlSessionSetInterfaceAdvisory(ControlSessionRef session,
				   const char * ifname,
				   SCNetworkInterfaceAdvisory advisory)
{
    uint32_t		count_after;
    uint32_t		count_before;
    AWDIPMonitorInterfaceAdvisoryReport_Flags flags_after;
    AWDIPMonitorInterfaceAdvisoryReport_Flags flags_before;
    CFStringRef		ifname_cf;

    if (session->advisories == NULL) {
	if (advisory == kSCNetworkInterfaceAdvisoryNone) {
	    /* no advisories, no need to store advisory */
	    return;
	}
	session->advisories = mutable_dict_create(0);
    }
    ifname_cf = CFStringCreateWithCString(NULL, ifname,
					  kCFStringEncodingUTF8);
    flags_before = InterfaceGetAdvisoryFlags(ifname_cf, NULL, &count_before);
    if (advisory == kSCNetworkInterfaceAdvisoryNone) {
	CFDictionaryRemoveValue(session->advisories, ifname_cf);
	if (CFDictionaryGetCount(session->advisories) == 0) {
	    CFRelease(session->advisories);
	    session->advisories = NULL;
	}
    }
    else {
	CFNumberRef			advisory_cf;

	advisory_cf = CFNumberCreate(NULL, kCFNumberSInt32Type, &advisory);
	CFDictionarySetValue(session->advisories, ifname_cf, advisory_cf);
	CFRelease(advisory_cf);
    }
    flags_after = InterfaceGetAdvisoryFlags(ifname_cf, NULL, &count_after);
    if (flags_before != flags_after || count_before != count_after) {
	SubmitInterfaceAdvisoryMetric(ifname_cf, flags_after, count_after);
    }
    InterfaceChangedListAddInterface(ifname_cf);
    NotifyInterfaceAdvisory(ifname_cf);
    NotifyIPMonitor();
    CFRelease(ifname_cf);
    return;
}

/**
 ** IPMonitorControlServer
 **/
STATIC Boolean
IPMonitorControlServerConnectionIsRoot(xpc_connection_t connection)
{
    uid_t		uid;

    uid = xpc_connection_get_euid(connection);
    return (uid == 0);
}

STATIC Boolean
IPMonitorControlServerConnectionHasEntitlement(xpc_connection_t connection,
					       const char * entitlement)
{
    Boolean 		entitled = FALSE;
    xpc_object_t 	val;

    val = xpc_connection_copy_entitlement_value(connection, entitlement);
    if (val != NULL) {
	if (xpc_get_type(val) == XPC_TYPE_BOOL) {
	    entitled = xpc_bool_get_value(val);
	}
	xpc_release(val);
    }
    return (entitled);
}

STATIC const char *
get_rank_str(SCNetworkServicePrimaryRank rank)
{
    const char *	str = NULL;

    switch (rank) {
    case kSCNetworkServicePrimaryRankDefault:
	str = "Default";
	break;
    case kSCNetworkServicePrimaryRankFirst:
	str = "First";
	break;
    case kSCNetworkServicePrimaryRankLast:
	str = "Last";
	break;
    case kSCNetworkServicePrimaryRankNever:
	str = "Never";
	break;
    case kSCNetworkServicePrimaryRankScoped:
	str = "Scoped";
	break;
    default:
	break;
    }
    return (str);
}

STATIC int
HandleSetInterfaceRank(xpc_connection_t connection,
		       xpc_object_t request,
		       xpc_object_t reply)
{
#pragma unused(reply)
    const char *		ifname;
    SCNetworkServicePrimaryRank	rank;
    const char *		rank_str;
    ControlSessionRef		session;

    if (!IPMonitorControlServerConnectionIsRoot(connection)) {
	my_log(LOG_INFO, "connection %p pid %d permission denied",
	       connection, xpc_connection_get_pid(connection));
	return (EPERM);
    }
    ifname
	= xpc_dictionary_get_string(request,
				    kIPMonitorControlRequestKeyInterfaceName);
    if (ifname == NULL) {
	return (EINVAL);
    }
    rank = (SCNetworkServicePrimaryRank)
	xpc_dictionary_get_int64(request,
				 kIPMonitorControlRequestKeyPrimaryRank);
    rank_str = get_rank_str(rank);
    if (rank_str == NULL) {
	return (EINVAL);
    }
    session = ControlSessionForConnection(connection, request);
    ControlSessionSetInterfaceRank(session, ifname, rank);
    my_log(LOG_NOTICE, "%s[%d] SetInterfaceRank(%s) = %s (%u)",
	   ControlSessionGetProcessName(session), ControlSessionGetPID(session),
	   ifname, rank_str, rank);
    return (0);
}

STATIC int
HandleGetInterfaceRank(xpc_connection_t connection,
		       xpc_object_t request,
		       xpc_object_t reply)
{
    const char *		ifname;
    SCNetworkServicePrimaryRank	rank;
    ControlSessionRef		session;

    if (reply == NULL) {
	/* no point in processing the request if we can't provide an answer */
	return (EINVAL);
    }
    session = ControlSessionLookup(connection);
    if (session == NULL) {
	/* no session, no rank assertion */
	return (ENOENT);
    }
    ifname
	= xpc_dictionary_get_string(request,
				    kIPMonitorControlRequestKeyInterfaceName);
    if (ifname == NULL) {
	return (EINVAL);
    }
    rank = ControlSessionGetInterfaceRank(session, ifname);
    xpc_dictionary_set_int64(reply, kIPMonitorControlResponseKeyPrimaryRank,
			     rank);
    return (0);
}

STATIC const char *
get_advisory_str(SCNetworkInterfaceAdvisory advisory)
{
    const char *	str = NULL;

    switch (advisory) {
    case kSCNetworkInterfaceAdvisoryNone:
	str = "None";
	break;
    case kSCNetworkInterfaceAdvisoryLinkLayerIssue:
	str = "LinkLayerIssue";
	break;
    case kSCNetworkInterfaceAdvisoryUplinkIssue:
	str = "UplinkIssue";
	break;
    case kSCNetworkInterfaceAdvisoryBetterInterfaceAvailable:
	str = "BetterInterfaceAvailable";
	break;
    default:
	break;
    }
    return (str);
}

STATIC int
HandleSetInterfaceAdvisory(xpc_connection_t connection,
			   xpc_object_t request,
			   xpc_object_t reply)
{
#pragma unused(reply)
    SCNetworkInterfaceAdvisory	advisory;
    const char *		advisory_str;
    const char *		ifname;
    const char *		reason;
    ControlSessionRef		session;

#define ENTITLEMENT "com.apple.SystemConfiguration.SCNetworkInterfaceSetAdvisory"
    if (!IPMonitorControlServerConnectionIsRoot(connection)
	&& !IPMonitorControlServerConnectionHasEntitlement(connection,
							   ENTITLEMENT)) {
	my_log(LOG_INFO, "connection %p pid %d permission denied",
	       connection, xpc_connection_get_pid(connection));
	return (EPERM);
    }
    ifname
	= xpc_dictionary_get_string(request,
				    kIPMonitorControlRequestKeyInterfaceName);
    if (ifname == NULL) {
	return (EINVAL);
    }
    reason
	= xpc_dictionary_get_string(request,
				    kIPMonitorControlRequestKeyReason);
    advisory = (SCNetworkInterfaceAdvisory)
	xpc_dictionary_get_int64(request, kIPMonitorControlRequestKeyAdvisory);

    /* validate the advisory code */
    advisory_str = get_advisory_str(advisory);
    if (advisory_str == NULL) {
	return (EINVAL);
    }
    session = ControlSessionForConnection(connection, request);
    ControlSessionSetInterfaceAdvisory(session, ifname, advisory);
    my_log(LOG_NOTICE, "%s[%d] SetInterfaceAdvisory(%s) = %s (%u) reason='%s'",
	   ControlSessionGetProcessName(session),
	   ControlSessionGetPID(session),
	   ifname, advisory_str, advisory,
	   reason != NULL ? reason : "" );
    return (0);
}

STATIC int
HandleInterfaceAdvisoryIsSet(xpc_connection_t connection,
			     xpc_object_t request,
			     xpc_object_t reply)
{
#pragma unused(connection)
    SCNetworkInterfaceAdvisory	advisory;
    const char *		ifname;
    CFStringRef			ifname_cf;

    if (reply == NULL) {
	/* no point in processing the request if we can't provide an answer */
	return (EINVAL);
    }
    ifname
	= xpc_dictionary_get_string(request,
				    kIPMonitorControlRequestKeyInterfaceName);
    if (ifname == NULL) {
	return (EINVAL);
    }
    ifname_cf = CFStringCreateWithCString(NULL, ifname,
					  kCFStringEncodingUTF8);
    advisory = (SCNetworkInterfaceAdvisory)
	xpc_dictionary_get_int64(request, kIPMonitorControlRequestKeyAdvisory);
    xpc_dictionary_set_bool(reply,
			    kIPMonitorControlResponseKeyAdvisoryIsSet,
			    InterfaceHasAdvisory(ifname_cf, advisory));
    CFRelease(ifname_cf);
    return (0);
}

STATIC int
HandleAnyInterfaceAdvisoryIsSet(xpc_connection_t connection,
				xpc_object_t request,
				xpc_object_t reply)
{
#pragma unused(connection)
#pragma unused(request)
    if (reply == NULL) {
	/* no point in processing the request if we can't provide an answer */
	return (EINVAL);
    }
    xpc_dictionary_set_bool(reply,
			    kIPMonitorControlResponseKeyAdvisoryIsSet,
			    AnyInterfaceHasAdvisories());
    return (0);
}

STATIC xpc_object_t
assertion_dict_create(const char * process_name,
		      pid_t pid, SCNetworkServicePrimaryRank rank)
{
    xpc_object_t	dict;

    dict = xpc_dictionary_create_empty();
    xpc_dictionary_set_string(dict,
			      kIPMonitorControlRankAssertionInfoProcessName,
			      process_name);
    xpc_dictionary_set_int64(dict,
			     kIPMonitorControlRankAssertionInfoProcessID,
			     pid);
    xpc_dictionary_set_int64(dict,
			     kIPMonitorControlRankAssertionInfoPrimaryRank,
			     rank);
    return (dict);
}

STATIC xpc_object_t
assertion_info_copy(CFStringRef ifname_cf)
{
    ControlSessionRef		session;
    xpc_object_t		list = NULL;

    LIST_FOREACH(session, &S_ControlSessions, link) {
	xpc_object_t			dict;
	SCNetworkServicePrimaryRank	rank;
	CFNumberRef			rank_cf;

	if (session->assertions == NULL) {
	    continue;
	}
	rank_cf = CFDictionaryGetValue(session->assertions, ifname_cf);
	if (rank_cf == NULL) {
	    /* session has no assertions for this interface */
	    continue;
	}
	(void)CFNumberGetValue(rank_cf, kCFNumberSInt32Type, &rank);
	if (list == NULL) {
	    list = xpc_array_create_empty();
	}
	dict = assertion_dict_create(ControlSessionGetProcessName(session),
				     ControlSessionGetPID(session), rank);
	xpc_array_append_value(list, dict);
	xpc_release(dict);
    }
    return (list);
}

STATIC int
HandleGetRankAssertionInfo(xpc_connection_t connection,
			   xpc_object_t request,
			   xpc_object_t reply)
{
#pragma unused(connection)
    int			error = 0;
    const char *	ifname;
    CFStringRef		ifname_cf;
    xpc_object_t	info;

    if (reply == NULL) {
	/* no point in processing the request if we can't provide an answer */
	error = EINVAL;
	goto done;
    }
    ifname
	= xpc_dictionary_get_string(request,
				    kIPMonitorControlRequestKeyInterfaceName);
    if (ifname == NULL) {
	error = EINVAL;
	goto done;
    }
    ifname_cf = CFStringCreateWithCString(NULL, ifname,
					  kCFStringEncodingUTF8);
    info = assertion_info_copy(ifname_cf);
    CFRelease(ifname_cf);
    if (info != NULL) {
	xpc_dictionary_set_value(reply,
				 kIPMonitorControlResponseKeyRankAssertionInfo,
				 info);
	xpc_release(info);
    }
    else {
	error = ENOENT;
    }
 done:
    return (error);
}

STATIC xpc_object_t
advisory_dict_create(const char * process_name,
		     pid_t pid, SCNetworkInterfaceAdvisory advisory)
{
    xpc_object_t	dict;

    dict = xpc_dictionary_create_empty();
    xpc_dictionary_set_string(dict,
			      kIPMonitorControlAdvisoryInfoProcessName,
			      process_name);
    xpc_dictionary_set_int64(dict,
			     kIPMonitorControlAdvisoryInfoProcessID,
			     pid);
    xpc_dictionary_set_int64(dict,
			     kIPMonitorControlAdvisoryInfoAdvisory,
			     advisory);
    return (dict);
}

STATIC xpc_object_t
advisory_info_copy(CFStringRef ifname_cf)
{
    ControlSessionRef		session;
    xpc_object_t		list = NULL;

    LIST_FOREACH(session, &S_ControlSessions, link) {
	xpc_object_t			dict;
	SCNetworkInterfaceAdvisory 	advisory;
	CFNumberRef			advisory_cf;

	if (session->advisories == NULL) {
	    continue;
	}
	advisory_cf = CFDictionaryGetValue(session->advisories, ifname_cf);
	if (advisory_cf == NULL) {
	    /* session has no advisories for this interface */
	    continue;
	}
	(void)CFNumberGetValue(advisory_cf, kCFNumberSInt32Type, &advisory);
	if (list == NULL) {
	    list = xpc_array_create_empty();
	}
	dict = advisory_dict_create(ControlSessionGetProcessName(session),
				    ControlSessionGetPID(session),
				    advisory);
	xpc_array_append_value(list, dict);
	xpc_release(dict);
    }
    return (list);
}

STATIC int
HandleGetAdvisoryInfo(xpc_connection_t connection,
		      xpc_object_t request,
		      xpc_object_t reply)
{
#pragma unused(connection)
    int			error = 0;
    const char *	ifname;
    CFStringRef		ifname_cf;
    xpc_object_t	info;

    if (reply == NULL) {
	/* no point in processing the request if we can't provide an answer */
	error = EINVAL;
	goto done;
    }
    ifname
	= xpc_dictionary_get_string(request,
				    kIPMonitorControlRequestKeyInterfaceName);
    if (ifname == NULL) {
	error = EINVAL;
	goto done;
    }
    ifname_cf = CFStringCreateWithCString(NULL, ifname,
					  kCFStringEncodingUTF8);
    info = advisory_info_copy(ifname_cf);
    CFRelease(ifname_cf);
    if (info != NULL) {
	xpc_dictionary_set_value(reply,
				 kIPMonitorControlResponseKeyAdvisoryInfo,
				 info);
	xpc_release(info);
    }
    else {
	error = ENOENT;
    }
 done:
    return (error);
}


STATIC bool
my_xpc_array_contains_string(xpc_object_t list, const char * str)
{
    __block bool	contains_string = false;
    xpc_array_applier_t	match_string;

    match_string = ^ bool(size_t index, xpc_object_t _Nonnull value)
	{
#pragma unused(index)
#pragma unused(value)
	 const char * this_str = xpc_string_get_string_ptr(value);

	 if (strcmp(this_str, str) == 0) {
	     contains_string = true;
	     return (false);
	 }
	 return (true);
	};
    (void)xpc_array_apply(list, match_string);
    return (contains_string);
}

STATIC void
AddUniqueCFDictionaryKeyToXPCArray(const void *key, const void *value,
				   void *context)
{
#pragma unused (value)
    xpc_object_t *	list_p = (xpc_object_t *)context;
    char *		str;
    bool		string_present;

    str = _SC_cfstring_to_cstring((CFStringRef)key, NULL, 0,
				  kCFStringEncodingUTF8);
    if (*list_p == NULL) {
	*list_p = xpc_array_create_empty();
	string_present = false;
    } else {
	string_present = my_xpc_array_contains_string(*list_p, str);
    }
    if (!string_present) {
	xpc_array_set_string(*list_p, XPC_ARRAY_APPEND, str);
    }
    CFAllocatorDeallocate(NULL, str);
    return;
}

STATIC xpc_object_t
rank_assertion_copy_interface_names(void)
{
    ControlSessionRef		session;
    xpc_object_t		list = NULL;

    LIST_FOREACH(session, &S_ControlSessions, link) {
	if (session->assertions == NULL) {
	    continue;
	}
	CFDictionaryApplyFunction(session->assertions,
				  AddUniqueCFDictionaryKeyToXPCArray,
				  &list);
    }
    return (list);
}
STATIC int
HandleGetRankAssertionInterfaceNames(xpc_connection_t connection,
				     xpc_object_t request,
				     xpc_object_t reply)
{
#pragma unused(connection)
#pragma unused(request)
    int			error = 0;
    xpc_object_t	names;

    if (reply == NULL) {
	/* no point in processing the request if we can't provide an answer */
	error = EINVAL;
	goto done;
    }
    names = rank_assertion_copy_interface_names();
    if (names != NULL) {
	xpc_dictionary_set_value(reply,
				 kIPMonitorControlResponseKeyInterfaceNames,
				 names);
	xpc_release(names);
    }
    else {
	error = ENOENT;
    }
 done:
    return (error);
}

STATIC xpc_object_t
advisory_copy_interface_names(void)
{
    ControlSessionRef		session;
    xpc_object_t		list = NULL;

    LIST_FOREACH(session, &S_ControlSessions, link) {
	if (session->advisories == NULL) {
	    continue;
	}
	CFDictionaryApplyFunction(session->advisories,
				  AddUniqueCFDictionaryKeyToXPCArray,
				  &list);
    }
    return (list);
}
STATIC int
HandleGetAdvisoryInterfaceNames(xpc_connection_t connection,
				xpc_object_t request,
				xpc_object_t reply)
{
#pragma unused(connection)
#pragma unused(request)
    int			error = 0;
    xpc_object_t	names;

    if (reply == NULL) {
	/* no point in processing the request if we can't provide an answer */
	error = EINVAL;
	goto done;
    }
    names = advisory_copy_interface_names();
    if (names != NULL) {
	xpc_dictionary_set_value(reply,
				 kIPMonitorControlResponseKeyInterfaceNames,
				 names);
	xpc_release(names);
    }
    else {
	error = ENOENT;
    }
 done:
    return (error);
}

STATIC void
IPMonitorControlServerHandleDisconnect(xpc_connection_t connection)
{
    ControlSessionRef	session;

    my_log(LOG_DEBUG, "IPMonitorControlServer: client %p went away",
	   connection);
    session = ControlSessionLookup(connection);
    if (session == NULL) {
	/* never asserted anything */
	return;
    }
    ControlSessionInvalidate(session);
    return;
}

STATIC void
IPMonitorControlServerHandleRequest(xpc_connection_t connection,
				    xpc_object_t request)
{
    xpc_type_t	type;

    type = xpc_get_type(request);
    if (type == XPC_TYPE_DICTIONARY) {
	int			error = 0;
	IPMonitorControlRequestType request_type;
	xpc_connection_t	remote;
	xpc_object_t		reply;

	request_type = (IPMonitorControlRequestType)
	    xpc_dictionary_get_int64(request,
				     kIPMonitorControlRequestKeyType);
	reply = xpc_dictionary_create_reply(request);
	switch (request_type) {
	case kIPMonitorControlRequestTypeSetInterfaceRank:
	    error = HandleSetInterfaceRank(connection, request, reply);
	    break;
	case kIPMonitorControlRequestTypeGetInterfaceRank:
	    error = HandleGetInterfaceRank(connection, request, reply);
	    break;
	case kIPMonitorControlRequestTypeSetInterfaceAdvisory:
	    error = HandleSetInterfaceAdvisory(connection, request, reply);
	    break;
	case kIPMonitorControlRequestTypeInterfaceAdvisoryIsSet:
	    error = HandleInterfaceAdvisoryIsSet(connection, request, reply);
	    break;
	case kIPMonitorControlRequestTypeAnyInterfaceAdvisoryIsSet:
	    error = HandleAnyInterfaceAdvisoryIsSet(connection, request, reply);
	    break;
	case kIPMonitorControlRequestTypeGetInterfaceRankAssertionInfo:
	    error = HandleGetRankAssertionInfo(connection, request, reply);
	    break;
	case kIPMonitorControlRequestTypeGetInterfaceAdvisoryInfo:
	    error = HandleGetAdvisoryInfo(connection, request, reply);
	    break;
	case kIPMonitorControlRequestTypeGetInterfaceRankAssertionInterfaceNames:
	    error = HandleGetRankAssertionInterfaceNames(connection, request, reply);
	    break;
	case kIPMonitorControlRequestTypeGetInterfaceAdvisoryInterfaceNames:
	    error = HandleGetAdvisoryInterfaceNames(connection, request, reply);
	    break;
	default:
	    error = EINVAL;
	    break;
	}
	if (reply == NULL) {
	    /* client didn't want a reply */
	    return;
	}
	xpc_dictionary_set_int64(reply, kIPMonitorControlResponseKeyError,
				 error);
	remote = xpc_dictionary_get_remote_connection(request);
	xpc_connection_send_message(remote, reply);
	xpc_release(reply);
    }
    else if (type == XPC_TYPE_ERROR) {
	if (request == XPC_ERROR_CONNECTION_INVALID) {
	    IPMonitorControlServerHandleDisconnect(connection);
	}
	else if (request == XPC_ERROR_CONNECTION_INTERRUPTED) {
	    my_log(LOG_INFO, "connection interrupted");
	}
    }
    else {
	my_log(LOG_NOTICE, "unexpected event");
    }
    return;
}

STATIC void
IPMonitorControlServerHandleNewConnection(xpc_connection_t connection)
{
    xpc_handler_t	handler;

    handler = ^(xpc_object_t event) {
	IPMonitorControlServerHandleRequest(connection, event);
    };
    xpc_connection_set_event_handler(connection, handler);
    xpc_connection_set_target_queue(connection, IPMonitorControlServerGetQueue());
    xpc_connection_resume(connection);
    return;
}

STATIC xpc_connection_t
IPMonitorControlServerCreate(dispatch_queue_t queue, const char * name)
{
    uint64_t		flags = XPC_CONNECTION_MACH_SERVICE_LISTENER;
    xpc_connection_t	connection;
    xpc_handler_t	handler;

    connection = xpc_connection_create_mach_service(name, queue, flags);
    if (connection == NULL) {
	return (NULL);
    }
    handler = ^(xpc_object_t event) {
	xpc_type_t	type;

	type = xpc_get_type(event);
	if (type == XPC_TYPE_CONNECTION) {
	    IPMonitorControlServerHandleNewConnection(event);
	}
	else if (type == XPC_TYPE_ERROR) {
	    const char	*	desc;

	    desc = xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION);
	    if (event == XPC_ERROR_CONNECTION_INVALID) {
		my_log(LOG_NOTICE, "%s", desc);
		xpc_release(connection);
	    }
	    else {
		my_log(LOG_NOTICE, "%s", desc);
	    }
	}
	else {
	    my_log(LOG_NOTICE, "unknown event %p", type);
	}
    };
    IPMonitorControlServerSetQueue(queue);
    xpc_connection_set_event_handler(connection, handler);
    xpc_connection_resume(connection);
    return (connection);
}

STATIC CFDictionaryRef
sessionInfoCopy(ControlSessionRef session)
{
    CFDictionaryRef	dict;
#define N_SESSION_KEYS	4
    CFIndex		i;
    const void *	keys[N_SESSION_KEYS];
    const void *	values[N_SESSION_KEYS];
    CFNumberRef		pid_cf;
    CFStringRef		process_name_cf;

    if (session->assertions == NULL && session->advisories == NULL) {
	return (NULL);
    }
    pid_cf = CFNumberCreate(NULL, kCFNumberSInt32Type, &session->pid);
    process_name_cf = CFStringCreateWithCString(NULL, session->process_name,
						kCFStringEncodingUTF8);
    i = 0;
    keys[i] = CFSTR("ProcessID"); 	/* 0 */
    values[i] = pid_cf;
    i++;
    keys[i] = CFSTR("ProcessName");	/* 1 */
    values[i] = process_name_cf;
    i++;
    if (session->assertions != NULL) {	/* 2 */
	keys[i] = CFSTR("Assertions");
	values[i] = session->assertions;
	i++;
    }
    if (session->advisories != NULL) {	/* 3 */
	keys[i] = CFSTR("Advisories");
	values[i] = session->advisories;
	i++;
    }
    dict = CFDictionaryCreate(NULL,
			      (const void * *)keys,
			      (const void * *)values,
			      i,
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    CFRelease(pid_cf);
    CFRelease(process_name_cf);
    return (dict);
}

STATIC os_state_data_t
IPMonitorControlCopyOSStateData(os_state_hints_t hints)
{
#pragma unused(hints)
    CFDataRef			data;
    CFMutableArrayRef		list;
    ControlSessionRef		session;
    os_state_data_t		state_data;
    size_t			state_data_size;
    CFIndex			state_len;

    list = mutable_array_create(0);
    LIST_FOREACH(session, &S_ControlSessions, link) {
	CFDictionaryRef		info;

	info = sessionInfoCopy(session);
	if (info != NULL) {
	    CFArrayAppendValue(list, info);
	    CFRelease(info);
	}
    }
    /* serialize the array into XML plist form */
    data = CFPropertyListCreateData(NULL,
				    list,
				    kCFPropertyListBinaryFormat_v1_0,
				    0,
				    NULL);
    CFRelease(list);
    state_len = CFDataGetLength(data);
    state_data_size = OS_STATE_DATA_SIZE_NEEDED(state_len);
    if (state_data_size > MAX_STATEDUMP_SIZE) {
	state_data = NULL;
	my_log(LOG_NOTICE, "%s: state data too large (%zd > %d)",
	       __func__, state_data_size, MAX_STATEDUMP_SIZE);
    }
    else {
	state_data = calloc(1, state_data_size);
	state_data->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
	state_data->osd_data_size = (uint32_t)state_len;
	strlcpy(state_data->osd_title,
		"IPMonitorControl Sessions",
		sizeof(state_data->osd_title));
	memcpy(state_data->osd_data, CFDataGetBytePtr(data), state_len);
    }
    CFRelease(data);
    return (state_data);
}

STATIC void
IPMonitorControlServerAddStateHandler(void)
{
    os_state_block_t	dump_state;

    dump_state = ^os_state_data_t(os_state_hints_t hints)
	{
	 return (IPMonitorControlCopyOSStateData(hints));
	};

    (void) os_state_add_handler(IPMonitorControlServerGetQueue(), dump_state);
}

PRIVATE_EXTERN Boolean
IPMonitorControlServerStart(CFRunLoopRef runloop, CFRunLoopSourceRef rls,
			    Boolean * verbose)
{
#pragma unused(verbose)
    dispatch_queue_t	q;
    xpc_connection_t	connection;

    SetNotificationInfo(runloop, rls);
    q = dispatch_queue_create("IPMonitorControlServer", NULL);
    connection = IPMonitorControlServerCreate(q, kIPMonitorControlServerName);
    if (connection == NULL) {
	my_log(LOG_ERR,
	       "IPMonitorControlServer: failed to create server");
	dispatch_release(q);
	return (FALSE);
    }
    IPMonitorControlServerAddStateHandler();
    return (TRUE);
}

PRIVATE_EXTERN CFArrayRef
IPMonitorControlServerCopyInterfaceRankInformation(CFDictionaryRef * info)
{
    __block CFArrayRef		changed;
    __block CFDictionaryRef	dict;

    dispatch_sync(IPMonitorControlServerGetQueue(),
		  ^{
		      dict = InterfaceRankAssertionsCopy();
		      changed = InterfaceChangedListCopy();
		  });
    *info = dict;
    return (changed);
}
