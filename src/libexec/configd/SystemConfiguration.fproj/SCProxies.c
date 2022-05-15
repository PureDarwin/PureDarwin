/*
 * Copyright (c) 2000-2004, 2006-2013, 2015, 2016 Apple Inc. All rights reserved.
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
 * May 18, 2001			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <TargetConditionals.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/VPNAppLayerPrivate.h>

#include <netdb.h>
#if	!TARGET_OS_SIMULATOR
#include <ne_session.h>
#endif	// !TARGET_OS_SIMULATOR

CFStringRef
SCDynamicStoreKeyCreateProxies(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreateNetworkGlobalEntity(allocator,
							  kSCDynamicStoreDomainState,
							  kSCEntNetProxies);
}


static void
validate_proxy_content(CFMutableDictionaryRef	proxies,
		       CFStringRef		proxy_enable_key,
		       CFStringRef		proxy_host_key,
		       CFStringRef		proxy_port_key,
		       const char *		proxy_service_name,
		       int			proxy_defaultport,
		       Boolean			multiple_proxies)
{
	int		enabled	= 0;
	CFNumberRef	num;

	num = CFDictionaryGetValue(proxies, proxy_enable_key);
	if (num != NULL) {
		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			goto disable;		// if we don't like the enabled key/value
		}
	}

	if (proxy_host_key != NULL) {
		CFTypeRef	host_val;

		host_val = CFDictionaryGetValue(proxies, proxy_host_key);
		if ((enabled == 0) && (host_val != NULL)) {
			goto disable;		// if not enabled, remove provided key/value
		}

		if (enabled != 0) {
			if (isA_CFString(host_val)) {
				CFStringRef	host	= (CFStringRef)host_val;

				if (multiple_proxies) {
					goto disable;	// if multiple proxies expected
				}

				if (CFStringGetLength(host) == 0) {
					goto disable;	// if proxy host string not valid
				}
			} else if (isA_CFArray(host_val)) {
				CFArrayRef	hosts	= (CFArrayRef)host_val;
				CFIndex		n;

				if (!multiple_proxies) {
					goto disable;	// if single proxy expected
				}

				n = CFArrayGetCount(hosts);
				if (n == 0) {
					goto disable;	// if no hosts provided
				}

				for (CFIndex i = 0; i < n; i++) {
					CFStringRef	host	= CFArrayGetValueAtIndex(hosts, i);

					if (!isA_CFString(host) || (CFStringGetLength(host) == 0)) {
						goto disable;	// if proxy host string not valid
					}
				}
			} else {
				goto disable;	// not valid
			}
		}
	}

	if (proxy_port_key != NULL) {
		CFNumberRef	port;
		int		s_port	= 0;

		port = CFDictionaryGetValue(proxies, proxy_port_key);
		if ((enabled == 0) && (port != NULL)) {
			goto disable;		// if not enabled, remove provided key/value
		}

		if ((enabled != 0) && (port != NULL)) {
			if (!isA_CFNumber(port) ||
			    !CFNumberGetValue(port, kCFNumberIntType, &s_port) ||
			    (s_port > UINT16_MAX)) {
				goto disable;	// if enabled, not provided (or not valid)
			}

			if (s_port == 0) {
				port = NULL;	// if no port # provided, use default
			}
		}

		if ((enabled != 0) && (port == NULL)) {
			struct servent	*service;

			if (proxy_service_name == NULL) {
				goto disable;	// no "default" port available
			}

			service = getservbyname(proxy_service_name, "tcp");
			if (service != NULL) {
				s_port = ntohs(service->s_port);
			} else {
				s_port = proxy_defaultport;
			}
			num = CFNumberCreate(NULL, kCFNumberIntType, &s_port);
			CFDictionarySetValue(proxies, proxy_port_key, num);
			CFRelease(num);
		}
	}

	return;

    disable :

	enabled = 0;
	num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
	CFDictionarySetValue(proxies, proxy_enable_key, num);
	CFRelease(num);
	if (proxy_host_key != NULL) {
		CFDictionaryRemoveValue(proxies, proxy_host_key);
	}
	if (proxy_port_key != NULL) {
		CFDictionaryRemoveValue(proxies, proxy_port_key);
	}

	return;
}


static void
normalize_scoped_proxy(const void *key, const void *value, void *context);


static void
normalize_services_proxy(const void *key, const void *value, void *context);


static void
normalize_supplemental_proxy(const void *value, void *context);


static CF_RETURNS_RETAINED CFDictionaryRef
__SCNetworkProxiesCopyNormalized(CFDictionaryRef proxy)
{
	CFArrayRef		array;
	CFMutableDictionaryRef	newProxy;
	CFNumberRef		num;
	CFDictionaryRef		scoped;
	CFDictionaryRef		services;
	CFArrayRef		supplemental;

	if (!isA_CFDictionary(proxy)) {
		proxy = CFDictionaryCreate(NULL,
					   NULL,
					   NULL,
					   0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
		return proxy;
	}

	newProxy = CFDictionaryCreateMutableCopy(NULL, 0, proxy);

	validate_proxy_content(newProxy,
			       kSCPropNetProxiesFTPEnable,
			       kSCPropNetProxiesFTPProxy,
			       kSCPropNetProxiesFTPPort,
			       "ftp",
			       21,
			       FALSE);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesGopherEnable,
			       kSCPropNetProxiesGopherProxy,
			       kSCPropNetProxiesGopherPort,
			       "gopher",
			       70,
			       FALSE);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesHTTPEnable,
			       kSCPropNetProxiesHTTPProxy,
			       kSCPropNetProxiesHTTPPort,
			       "http",
			       80,
			       FALSE);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesHTTPSEnable,
			       kSCPropNetProxiesHTTPSProxy,
			       kSCPropNetProxiesHTTPSPort,
			       "https",
			       443,
			       FALSE);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesRTSPEnable,
			       kSCPropNetProxiesRTSPProxy,
			       kSCPropNetProxiesRTSPPort,
			       "rtsp",
			       554,
			       FALSE);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesSOCKSEnable,
			       kSCPropNetProxiesSOCKSProxy,
			       kSCPropNetProxiesSOCKSPort,
			       "socks",
			       1080,
			       FALSE);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesTransportConverterEnable,
			       kSCPropNetProxiesTransportConverterProxy,
			       kSCPropNetProxiesTransportConverterPort,
			       NULL,
			       0,
			       TRUE);
	if (CFDictionaryContainsKey(newProxy, kSCPropNetProxiesProxyAutoConfigURLString)) {
		validate_proxy_content(newProxy,
				       kSCPropNetProxiesProxyAutoConfigEnable,
				       kSCPropNetProxiesProxyAutoConfigURLString,
				       NULL,
				       NULL,
				       0,
				       FALSE);

		// and we can't have both URLString and JavaScript keys
		CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesProxyAutoConfigJavaScript);
	} else {
		validate_proxy_content(newProxy,
				       kSCPropNetProxiesProxyAutoConfigEnable,
				       kSCPropNetProxiesProxyAutoConfigJavaScript,
				       NULL,
				       NULL,
				       0,
				       FALSE);
	}
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesProxyAutoDiscoveryEnable,
			       NULL,
			       NULL,
			       NULL,
			       0,
			       FALSE);

	validate_proxy_content(newProxy,
			       kSCPropNetProxiesFallBackAllowed,
			       NULL,
			       NULL,
			       NULL,
			       0,
			       FALSE);

	// validate FTP passive setting
	num = CFDictionaryGetValue(newProxy, kSCPropNetProxiesFTPPassive);
	if (num != NULL) {
		int	enabled	= 0;

		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			// if we don't like the enabled key/value
			enabled = 1;
			num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
			CFDictionarySetValue(newProxy,
					     kSCPropNetProxiesFTPPassive,
					     num);
			CFRelease(num);
		}
	}

	// validate proxy exception list
	array = CFDictionaryGetValue(newProxy, kSCPropNetProxiesExceptionsList);
	if (array != NULL) {
		CFIndex		i;
		CFIndex		n;

		n = isA_CFArray(array) ? CFArrayGetCount(array) : 0;
		for (i = 0; i < n; i++) {
			CFStringRef	str;

			str = CFArrayGetValueAtIndex(array, i);
			if (!isA_CFString(str) || (CFStringGetLength(str) == 0)) {
				// if we don't like the array contents
				n = 0;
				break;
			}
		}

		if (n == 0) {
			CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesExceptionsList);
		}
	}

	// validate exclude simple hostnames setting
	num = CFDictionaryGetValue(newProxy, kSCPropNetProxiesExcludeSimpleHostnames);
	if (num != NULL) {
		int	enabled;

		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			// if we don't like the enabled key/value
			enabled = 0;
			num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
			CFDictionarySetValue(newProxy,
					     kSCPropNetProxiesExcludeSimpleHostnames,
					     num);
			CFRelease(num);
		}
	}

	// cleanup scoped proxies
	scoped = CFDictionaryGetValue(newProxy, kSCPropNetProxiesScoped);
	if (isA_CFDictionary(scoped)) {
		CFMutableDictionaryRef	newScoped;

		newScoped = CFDictionaryCreateMutable(NULL,
						      0,
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);
		CFDictionaryApplyFunction(scoped,
					  normalize_scoped_proxy,
					  newScoped);
		CFDictionarySetValue(newProxy, kSCPropNetProxiesScoped, newScoped);
		CFRelease(newScoped);
	}

	// cleanup services proxies
	services = CFDictionaryGetValue(newProxy, kSCPropNetProxiesServices);
	if (isA_CFDictionary(services)) {
		CFMutableDictionaryRef	newServices;

		newServices = CFDictionaryCreateMutable(NULL,
						      0,
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);
		CFDictionaryApplyFunction(services,
					  normalize_services_proxy,
					  newServices);
		CFDictionarySetValue(newProxy, kSCPropNetProxiesServices, newServices);
		CFRelease(newServices);
	}

	// cleanup split/supplemental proxies
	supplemental = CFDictionaryGetValue(newProxy, kSCPropNetProxiesSupplemental);
	if (isA_CFArray(supplemental)) {
		CFMutableArrayRef	newSupplemental;

		newSupplemental = CFArrayCreateMutable(NULL,
						       0,
						       &kCFTypeArrayCallBacks);
		CFArrayApplyFunction(supplemental,
				     CFRangeMake(0, CFArrayGetCount(supplemental)),
				     normalize_supplemental_proxy,
				     newSupplemental);
		CFDictionarySetValue(newProxy, kSCPropNetProxiesSupplemental, newSupplemental);
		CFRelease(newSupplemental);
	}

	proxy = CFDictionaryCreateCopy(NULL,newProxy);
	CFRelease(newProxy);

	return proxy;
}


static void
normalize_scoped_proxy(const void *key, const void *value, void *context)
{
	CFStringRef		interface	= (CFStringRef)key;
	CFDictionaryRef		proxy		= (CFDictionaryRef)value;
	CFMutableDictionaryRef	newScoped	= (CFMutableDictionaryRef)context;

	proxy = __SCNetworkProxiesCopyNormalized(proxy);
	CFDictionarySetValue(newScoped, interface, proxy);
	CFRelease(proxy);

	return;
}

static void
normalize_services_proxy(const void *key, const void *value, void *context)
{
	CFNumberRef		serviceIndex	= (CFNumberRef)key;
	CFDictionaryRef		proxy		= (CFDictionaryRef)value;
	CFMutableDictionaryRef	newServices	= (CFMutableDictionaryRef)context;

	proxy = __SCNetworkProxiesCopyNormalized(proxy);
	CFDictionarySetValue(newServices, serviceIndex, proxy);
	CFRelease(proxy);

	return;
}

static void
normalize_supplemental_proxy(const void *value, void *context)
{
	CFDictionaryRef		proxy		= (CFDictionaryRef)value;
	CFMutableArrayRef	newSupplemental	= (CFMutableArrayRef)context;

	proxy = __SCNetworkProxiesCopyNormalized(proxy);
	CFArrayAppendValue(newSupplemental, proxy);
	CFRelease(proxy);

	return;
}

CFDictionaryRef
SCDynamicStoreCopyProxies(SCDynamicStoreRef store)
{
	return SCDynamicStoreCopyProxiesWithOptions(store, NULL);
}

const CFStringRef	kSCProxiesNoGlobal	= CFSTR("NO_GLOBAL");

CFDictionaryRef
SCDynamicStoreCopyProxiesWithOptions(SCDynamicStoreRef store, CFDictionaryRef options)
{
	Boolean			bypass	= FALSE;
	CFStringRef		key;
	CFDictionaryRef		proxies	= NULL;

	if (options != NULL) {
		CFBooleanRef	bypassGlobalOption;

		if (!isA_CFDictionary(options)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}

		bypassGlobalOption = CFDictionaryGetValue(options, kSCProxiesNoGlobal);
		if (isA_CFBoolean(bypassGlobalOption) && CFBooleanGetValue(bypassGlobalOption)) {
			bypass = TRUE;
		}
	}


	/* copy proxy information from dynamic store */

	key = SCDynamicStoreKeyCreateProxies(NULL);
	proxies = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);

	if (isA_CFDictionary(proxies) &&
	    CFDictionaryContainsKey(proxies, kSCPropNetProxiesBypassAllowed)) {
		CFMutableDictionaryRef	newProxies;

		newProxies = CFDictionaryCreateMutableCopy(NULL, 0, proxies);
		CFRelease(proxies);

		/*
		 * Remove kSCPropNetProxiesBypassAllowed property from network
		 * service based configurations.
		 */
		CFDictionaryRemoveValue(newProxies, kSCPropNetProxiesBypassAllowed);
		proxies = newProxies;
	}


	if (proxies != NULL) {
		CFDictionaryRef	base	= proxies;

		proxies = __SCNetworkProxiesCopyNormalized(base);
		CFRelease(base);
	} else {
		proxies = CFDictionaryCreate(NULL,
					     NULL,
					     NULL,
					     0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	}

	return proxies;
}


static CFArrayRef
_SCNetworkProxiesCopyMatchingInternal(CFDictionaryRef	globalConfiguration,
				      CFStringRef	server,
				      CFStringRef	interface,
				      CFDictionaryRef	options)
{
	CFMutableDictionaryRef		newProxy;
	uuid_t				match_uuid;
	CFArrayRef			proxies		= NULL;
	CFDictionaryRef			proxy;
	int				sc_status	= kSCStatusOK;
	CFStringRef			trimmed		= NULL;


	if (!isA_CFDictionary(globalConfiguration)) {
		// if no proxy configuration
		_SCErrorSet(kSCStatusOK);
		return NULL;
	}

	uuid_clear(match_uuid);

	if (isA_CFDictionary(options)) {
		CFUUIDRef euuid;

		interface = CFDictionaryGetValue(options, kSCProxiesMatchInterface);
		interface = isA_CFString(interface);

		server = CFDictionaryGetValue(options, kSCProxiesMatchServer);
		server = isA_CFString(server);

		euuid = CFDictionaryGetValue(options, kSCProxiesMatchExecutableUUID);
		euuid = isA_CFType(euuid, CFUUIDGetTypeID());
		if (euuid != NULL) {
			CFUUIDBytes uuid_bytes = CFUUIDGetUUIDBytes(euuid);
			uuid_copy(match_uuid, (const uint8_t *)&uuid_bytes);
		}
	}

	if (interface != NULL) {
		CFDictionaryRef		scoped;

		if (!isA_CFString(interface) ||
		    (CFStringGetLength(interface) == 0)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}

		scoped = CFDictionaryGetValue(globalConfiguration, kSCPropNetProxiesScoped);
		if (scoped == NULL) {
#if	!TARGET_OS_SIMULATOR
			if (CFDictionaryContainsKey(globalConfiguration, kSCPropNetProxiesBypassAllowed) &&
			    ne_session_always_on_vpn_configs_present()) {
				/*
				 * The kSCPropNetProxiesBypassAllowed key will be present
				 * for managed proxy configurations where bypassing is *not*
				 * allowed.
				 *
				 * Also (for now), forcing the use of the managed proxy
				 * configurations will only be done with AOVPN present.
				 */
				goto useDefault;
			}
#endif	// !TARGET_OS_SIMULATOR

			// if no scoped proxy configurations
			_SCErrorSet(kSCStatusOK);
			return NULL;
		}

		if (!isA_CFDictionary(scoped)) {
			// if corrupt proxy configuration
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}

		proxy = CFDictionaryGetValue(scoped, interface);
		if (proxy == NULL) {
			// if no scoped proxy configuration for this interface
			_SCErrorSet(kSCStatusOK);
			return NULL;
		}

		if (!isA_CFDictionary(proxy)) {
			// if corrupt proxy configuration
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}

		// return per-interface proxy configuration
		proxies = CFArrayCreate(NULL, (const void **)&proxy, 1, &kCFTypeArrayCallBacks);
		return proxies;
	}


	if (server != NULL) {
		CFIndex			i;
		CFMutableArrayRef	matching	= NULL;
		CFIndex			n		= 0;
		CFIndex			server_len;
		CFArrayRef		supplemental;

		trimmed = _SC_trimDomain(server);
		if (trimmed == NULL) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}

		server = trimmed;
		server_len = CFStringGetLength(server);

		supplemental = CFDictionaryGetValue(globalConfiguration, kSCPropNetProxiesSupplemental);
		if (supplemental != NULL) {
			if (!isA_CFArray(supplemental)) {
				// if corrupt proxy configuration
				sc_status = kSCStatusFailed;
				goto done;
			}

			n = CFArrayGetCount(supplemental);
		}

		for (i = 0; i < n; i++) {
			CFStringRef	domain;
			CFIndex		domain_len;
			CFIndex		n_matching;

			proxy = CFArrayGetValueAtIndex(supplemental, i);
			if (!isA_CFDictionary(proxy)) {
				// if corrupt proxy configuration
				continue;
			}

			domain = CFDictionaryGetValue(proxy, kSCPropNetProxiesSupplementalMatchDomain);
			if (!isA_CFString(domain)) {
				// if corrupt proxy configuration
				continue;
			}

			domain_len = CFStringGetLength(domain);
			if (domain_len > 0) {
				if (!CFStringFindWithOptions(server,
							     domain,
							     CFRangeMake(0, server_len),
							     kCFCompareCaseInsensitive|kCFCompareAnchored|kCFCompareBackwards,
							     NULL)) {
					// if server does not match this proxy domain (or host)
					continue;
				}

				if ((server_len > domain_len) &&
				    !CFStringFindWithOptions(server,
							     CFSTR("."),
							     CFRangeMake(0, server_len - domain_len),
							     kCFCompareCaseInsensitive|kCFCompareAnchored|kCFCompareBackwards,
							     NULL)) {
					// if server does not match this proxy domain
					continue;
				}
//			} else {
//				// if this is a "default" (match all) proxy domain
			}

			if (matching == NULL) {
				matching = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}
			n_matching = CFArrayGetCount(matching);

			newProxy = CFDictionaryCreateMutableCopy(NULL, 0, proxy);
			CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesSupplementalMatchDomain);
			if ((n_matching == 0) ||
			    !CFArrayContainsValue(matching, CFRangeMake(0, n_matching), newProxy)) {
				// add this matching proxy
				CFArrayAppendValue(matching, newProxy);
			}
			CFRelease(newProxy);
		}

		if (matching != NULL) {
			// if we have any supplemental match domains
			proxies = CFArrayCreateCopy(NULL, matching);
			CFRelease(matching);
			goto done;
		}
	}

	// no matches, return "global" proxy configuration

#if	!TARGET_OS_SIMULATOR
    useDefault :
#endif	// !TARGET_OS_SIMULATOR

	newProxy = CFDictionaryCreateMutableCopy(NULL, 0, globalConfiguration);
	CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesScoped);
	CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesServices);
	CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesSupplemental);
	proxies = CFArrayCreate(NULL, (const void **)&newProxy, 1, &kCFTypeArrayCallBacks);
	CFRelease(newProxy);

    done :

	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);

//		Note: if we are returning an error then we must
//		      return w/proxies==NULL.  At present, there
//		      is no code (above) that would get here with
//		      proxies!=NULL so we don't need to take any
//		      action but future coder's should beware :-)
//		if (proxies != NULL) {
//			CFRelease(proxies);
//			proxies = NULL;
//		}
	}
	if (trimmed != NULL) CFRelease(trimmed);

	return proxies;
}

CFDataRef
SCNetworkProxiesCreateProxyAgentData(CFDictionaryRef proxyConfig)
{
	CFDataRef result = NULL;
	CFArrayRef newProxy = NULL;

	if (!isA_CFDictionary(proxyConfig)) {
		SC_log(LOG_ERR, "Invalid proxy configuration");
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	newProxy = CFArrayCreate(NULL, (const void **)&proxyConfig, 1, &kCFTypeArrayCallBacks);
	(void)_SCSerialize(newProxy, &result, NULL, NULL);
	CFRelease(newProxy);

	return result;
}

CFArrayRef
SCNetworkProxiesCopyMatching(CFDictionaryRef	globalConfiguration,
			     CFStringRef	server,
			     CFStringRef	interface)
{
	return _SCNetworkProxiesCopyMatchingInternal(globalConfiguration, server, interface, NULL);
}

CFArrayRef
SCNetworkProxiesCopyMatchingWithOptions(CFDictionaryRef		globalConfiguration,
					CFDictionaryRef		options)
{
	return _SCNetworkProxiesCopyMatchingInternal(globalConfiguration, NULL, NULL, options);
}
