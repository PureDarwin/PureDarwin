/*
 * SCNetworkReachability for plain addresses.
 *
 * Apple's SCNetworkReachability.c is built on libnetwork (nw_path evaluation),
 * which is closed source. The address-based half of the API does not need it:
 * "can I reach this address" is a routing question, and the kernel answers it
 * when a UDP socket is connected - connect(2) on SOCK_DGRAM performs the route
 * lookup and sends nothing.
 *
 * The consumer is _dns_resolver_set_reach_flags(), which stamps each resolver in
 * the DNS configuration with the reachability of its nameservers, and ranks
 * several nameservers against each other. Only kSCNetworkReachabilityFlagsReachable
 * and kSCNetworkReachabilityFlagsConnectionRequired affect that ranking
 * (__SCNetworkReachabilityRank), and a system with no on-demand VPN never sets
 * the latter.
 *
 * Name-based reachability still returns NULL - see the comment further down.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCNetworkReachability.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * The target is a CFDictionary holding the validated options, so that CFRetain
 * and CFRelease work on it exactly as callers expect without registering a
 * CFRuntime class for something this small.
 */
#define PD_REACH_ADDRESS	CFSTR("pd-remote-address")
#define PD_REACH_IFINDEX	CFSTR("pd-interface-index")

/* A nameserver address may carry port 0; connect(2) wants a usable port. */
#define PD_REACH_FALLBACK_PORT	53

static Boolean
pd_reach_copy_sockaddr(CFDictionaryRef options, struct sockaddr_storage *ss)
{
	CFDataRef	data;
	CFIndex		len;

	data = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionRemoteAddress);
	if (!isA_CFData(data)) {
		return FALSE;
	}

	len = CFDataGetLength(data);
	if ((len < (CFIndex)sizeof(struct sockaddr)) ||
	    (len > (CFIndex)sizeof(struct sockaddr_storage))) {
		return FALSE;
	}

	memset(ss, 0, sizeof(*ss));
	memcpy(ss, CFDataGetBytePtr(data), (size_t)len);

	switch (ss->ss_family) {
		case AF_INET :
			if (len < (CFIndex)sizeof(struct sockaddr_in)) {
				return FALSE;
			}
			if (((struct sockaddr_in *)ss)->sin_port == 0) {
				((struct sockaddr_in *)ss)->sin_port = htons(PD_REACH_FALLBACK_PORT);
			}
			ss->ss_len = sizeof(struct sockaddr_in);
			break;
		case AF_INET6 :
			if (len < (CFIndex)sizeof(struct sockaddr_in6)) {
				return FALSE;
			}
			if (((struct sockaddr_in6 *)ss)->sin6_port == 0) {
				((struct sockaddr_in6 *)ss)->sin6_port = htons(PD_REACH_FALLBACK_PORT);
			}
			ss->ss_len = sizeof(struct sockaddr_in6);
			break;
		default :
			return FALSE;
	}

	return TRUE;
}

SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithOptions(CFAllocatorRef allocator,
				       CFDictionaryRef options)
{
	CFMutableDictionaryRef	target;
	CFStringRef		interface;
	struct sockaddr_storage	ss;
	unsigned int		if_index	= 0;
	CFDataRef		addrData;

	if (!isA_CFDictionary(options)) {
		return NULL;
	}

	/* Only the address form is supported; a nodename needs a resolver. */
	if (CFDictionaryContainsKey(options, kSCNetworkReachabilityOptionNodeName) ||
	    CFDictionaryContainsKey(options, kSCNetworkReachabilityOptionPTRAddress)) {
		return NULL;
	}

	if (!pd_reach_copy_sockaddr(options, &ss)) {
		return NULL;
	}

	/*
	 * Failing on an unknown interface is load-bearing: the caller retries
	 * without the interface, and treats that second success as "the interface
	 * name is no longer valid" and marks the resolver unreachable.
	 */
	interface = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionInterface);
	if (interface != NULL) {
		char	if_name[IFNAMSIZ];

		if (!isA_CFString(interface) ||
		    !CFStringGetCString(interface, if_name, sizeof(if_name), kCFStringEncodingASCII)) {
			return NULL;
		}
		if_index = if_nametoindex(if_name);
		if (if_index == 0) {
			return NULL;
		}
	}

	target = CFDictionaryCreateMutable(allocator, 0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
	if (target == NULL) {
		return NULL;
	}

	addrData = CFDataCreate(allocator, (const UInt8 *)&ss, (CFIndex)ss.ss_len);
	if (addrData == NULL) {
		CFRelease(target);
		return NULL;
	}
	CFDictionarySetValue(target, PD_REACH_ADDRESS, addrData);
	CFRelease(addrData);

	if (if_index != 0) {
		CFNumberRef	num;

		num = CFNumberCreate(allocator, kCFNumberIntType, &(int){ (int)if_index });
		if (num != NULL) {
			CFDictionarySetValue(target, PD_REACH_IFINDEX, num);
			CFRelease(num);
		}
	}

	return (SCNetworkReachabilityRef)target;
}

Boolean
SCNetworkReachabilityGetFlags(SCNetworkReachabilityRef target,
			      SCNetworkReachabilityFlags *flags)
{
	CFDictionaryRef		dict	= (CFDictionaryRef)target;
	CFDataRef		addrData;
	CFNumberRef		num;
	struct sockaddr_storage	ss;
	int			sock;
	int			if_index = 0;
	Boolean			reachable;

	if (flags != NULL) {
		*flags = 0;
	}

	/* Targets from SCNetworkReachabilityCreateWithName are never created. */
	if ((dict == NULL) || (CFGetTypeID(dict) != CFDictionaryGetTypeID())) {
		return FALSE;
	}

	addrData = CFDictionaryGetValue(dict, PD_REACH_ADDRESS);
	if (!isA_CFData(addrData) ||
	    (CFDataGetLength(addrData) > (CFIndex)sizeof(ss))) {
		return FALSE;
	}
	memset(&ss, 0, sizeof(ss));
	memcpy(&ss, CFDataGetBytePtr(addrData), (size_t)CFDataGetLength(addrData));

	num = CFDictionaryGetValue(dict, PD_REACH_IFINDEX);
	if (isA_CFNumber(num)) {
		(void)CFNumberGetValue(num, kCFNumberIntType, &if_index);
	}

	sock = socket(ss.ss_family, SOCK_DGRAM, 0);
	if (sock < 0) {
		return FALSE;
	}

	/* Scoped resolvers must be judged on their own interface, not the default route. */
	if (if_index != 0) {
#if defined(IP_BOUND_IF) && defined(IPV6_BOUND_IF)
		if (ss.ss_family == AF_INET) {
			(void)setsockopt(sock, IPPROTO_IP, IP_BOUND_IF, &if_index, sizeof(if_index));
		} else {
			(void)setsockopt(sock, IPPROTO_IPV6, IPV6_BOUND_IF, &if_index, sizeof(if_index));
		}
#endif
	}

	/* connect(2) on a datagram socket resolves a route without sending anything. */
	reachable = (connect(sock, (struct sockaddr *)&ss, ss.ss_len) == 0);
	(void)close(sock);

	if (reachable && (flags != NULL)) {
		*flags = kSCNetworkReachabilityFlagsReachable;
	}

	return TRUE;
}

/*
 * The remaining entry points exist for IPConfiguration's stf.c (6to4), which
 * resolves a relay hostname and watches it for changes. That needs a resolver
 * rather than a route lookup, so creating a target still fails, and stf.c
 * checks: stf_set_relay_hostname() treats a NULL target as "cannot resolve the
 * relay" and leaves the 6to4 service unconfigured. 6to4 is a deprecated IPv6
 * transition mechanism; nothing else in the tree asks for these.
 */
SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithName(CFAllocatorRef allocator, const char *nodename)
{
	(void)allocator;
	(void)nodename;
	return NULL;
}

CFArrayRef
SCNetworkReachabilityCopyResolvedAddress(SCNetworkReachabilityRef target, int *error_num)
{
	(void)target;
	if (error_num != NULL) {
		*error_num = 0;
	}
	return NULL;
}

/*
 * No change notifications: reachability here is evaluated on demand, and there
 * is no route-monitoring engine behind it to drive a callback.
 */
Boolean
SCNetworkReachabilitySetCallback(SCNetworkReachabilityRef target,
				 SCNetworkReachabilityCallBack callout,
				 SCNetworkReachabilityContext *context)
{
	(void)target;
	(void)callout;
	(void)context;
	return FALSE;
}

Boolean
SCNetworkReachabilityScheduleWithRunLoop(SCNetworkReachabilityRef target,
					 CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
	(void)target;
	(void)runLoop;
	(void)runLoopMode;
	return FALSE;
}

Boolean
SCNetworkReachabilityUnscheduleFromRunLoop(SCNetworkReachabilityRef target,
					   CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
	(void)target;
	(void)runLoop;
	(void)runLoopMode;
	return FALSE;
}
