/*
 * Entry points for the four IPMonitor features that cannot be built here, each
 * left out for a dependency PureDarwin does not have. ip_plugin.c calls all of
 * them unconditionally, so they have to resolve; every one below does nothing,
 * and the specific capability that is therefore missing is named.
 *
 *   set-hostname.c        resolves the primary address through
 *                         SCNetworkReachability, which needs Apple's closed
 *                         libnetwork. Effect: the system's HostName is not
 *                         derived by reverse lookup of the primary address.
 *                         A HostName set explicitly in preferences is
 *                         unaffected - that path is in SCDHostName.c.
 *
 *   smb-configuration.c   needs <smb_server_prefs.h> from Apple's smb project.
 *                         Effect: no NetBIOS name or workgroup is computed.
 *                         Nothing in PureDarwin serves SMB.
 *
 *   nat64-configuration.c uses the nw_* object API from libnetwork. Effect: no
 *                         NAT64 prefix discovery, so IPv6-only networks that
 *                         rely on NAT64 will not get synthesised addresses.
 *                         Dual-stack and IPv4-only networks are unaffected.
 *
 *   agent-monitor.m       drives libnetwork's network-agent machinery through
 *                         Foundation. Effect: DNS and proxy configuration are
 *                         not additionally published as network agents. The
 *                         configuration itself still reaches clients through
 *                         dnsinfo and SCDynamicStore, which is the path
 *                         libresolv and mDNSResponder actually read.
 *
 * is_nat64_prefix_request() returns FALSE so ip_plugin.c treats no store change
 * as a NAT64 request, and nat64_prefix_request_add_pattern() adds no pattern, so
 * configd never asks to be notified about one in the first place.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <uuid/uuid.h>

void
load_hostname(Boolean verbose)
{
	(void)verbose;
}

void
load_smb_configuration(Boolean verbose)
{
	(void)verbose;
}

Boolean
is_nat64_prefix_request(CFStringRef change, CFStringRef *interface)
{
	(void)change;
	if (interface != NULL) {
		*interface = NULL;
	}
	return FALSE;
}

void
nat64_prefix_request_add_pattern(CFMutableArrayRef patterns)
{
	(void)patterns;
}

void
nat64_configuration_update(CFSetRef updates, CFSetRef requests,
			   CFSetRef cancellations)
{
	(void)updates;
	(void)requests;
	(void)cancellations;
}

void
process_AgentMonitor(void)
{
}

void
process_AgentMonitor_DNS(void)
{
}

void
process_AgentMonitor_Proxy(void)
{
}

const void *
copy_proxy_information_for_agent_uuid(uuid_t uuid, uint64_t *length)
{
	(void)uuid;
	if (length != NULL) {
		*length = 0;
	}
	return NULL;
}

const void *
copy_dns_information_for_agent_uuid(uuid_t uuid, uint64_t *length)
{
	(void)uuid;
	if (length != NULL) {
		*length = 0;
	}
	return NULL;
}
