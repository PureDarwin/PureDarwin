/*
 * SCNetworkReachability, as far as dnsinfo_create.c needs it.
 *
 * The real SCNetworkReachability.c is built on Apple's libnetwork (nw_path
 * evaluation), which is closed source, so it is not part of this build. Its one
 * consumer here is _dns_resolver_set_reach_flags(), which stamps each resolver
 * in the DNS configuration with the reachability of its nameservers.
 *
 * Reporting "cannot create a target" is deliberate, and is not a behaviour this
 * file invents: _dns_resolver_set_reach_flags() already has that path. It
 * initialises flags to kSCNetworkReachabilityFlagsReachable, logs once, and
 * keeps that value - so a resolver ends up marked reachable, which is the right
 * answer for a plain nameserver address on a working interface and exactly what
 * Apple's own code falls back to.
 *
 * What is lost is the ranking between several nameservers by reachability; all
 * of them come out equally reachable. Ordering still follows the configuration.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCNetworkReachability.h>

SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithOptions(CFAllocatorRef allocator,
				       CFDictionaryRef options)
{
	(void)allocator;
	(void)options;
	return NULL;
}

Boolean
SCNetworkReachabilityGetFlags(SCNetworkReachabilityRef target,
			      SCNetworkReachabilityFlags *flags)
{
	(void)target;
	if (flags != NULL) {
		*flags = 0;
	}
	return FALSE;
}
