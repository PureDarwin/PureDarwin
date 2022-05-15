/*
 * Copyright (c) 2000, 2001, 2003-2018, 2020-2021 Apple Inc. All rights reserved.
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
 * July 9, 2001			Allan Nathanson <ajn@apple.com>
 * - added "-r" option for checking network reachability
 * - added "-w" option to check/wait for the presence of a
 *   dynamic store key.
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "scutil.h"
#include "prefs.h"
#include "tests.h"

#include <netdb.h>
#include <netdb_async.h>
#include <notify.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define	my_log(__level, __format, ...)	SCPrint(TRUE, stdout, CFSTR(__format "\n"), ## __VA_ARGS__)

#include <dnsinfo.h>
#include "dnsinfo_internal.h"
#include "dnsinfo_logging.h"

#include <network_information.h>
#include "network_state_information_logging.h"
#include "network_state_information_priv.h"

#include "SCNetworkReachabilityInternal.h"

#include <CommonCrypto/CommonDigest.h>


static Boolean	resolver_bypass;


static CF_RETURNS_RETAINED CFMutableDictionaryRef
_setupReachabilityOptions(int argc, char * const argv[], const char *interface)
{
	int			i;
	CFMutableDictionaryRef	options;

	options = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "interface") == 0) {
			if (++i >= argc) {
				SCPrint(TRUE, stderr, CFSTR("No interface\n"));
				CFRelease(options);
				exit(1);
			}

			interface = argv[i];
			continue;
		}


		if (strcasecmp(argv[i], "no-connection-on-demand") == 0) {
			CFDictionarySetValue(options,
					     kSCNetworkReachabilityOptionConnectionOnDemandBypass,
					     kCFBooleanTrue);
			continue;
		}

		if (strcasecmp(argv[i], "no-resolve") == 0) {
			CFDictionarySetValue(options,
					     kSCNetworkReachabilityOptionResolverBypass,
					     kCFBooleanTrue);
			resolver_bypass = TRUE;
			continue;
		}

		if (strcasecmp(argv[i], "ptr") == 0) {
			CFDictionarySetValue(options,
					     kSCNetworkReachabilityOptionPTRAddress,
					     kCFBooleanTrue);
			continue;
		}

		if (strlen(argv[i]) == 0) {
			continue;
		}

		SCPrint(TRUE, stderr, CFSTR("Unrecognized option: %s\n"), argv[i]);
		CFRelease(options);
		exit(1);
	}

	if (interface != NULL) {
		CFStringRef	str;

		if (if_nametoindex(interface) == 0) {
			SCPrint(TRUE, stderr, CFSTR("No interface: %s\n"), interface);
			exit(1);
		}

		str  = CFStringCreateWithCString(NULL, interface, kCFStringEncodingASCII);
		CFDictionarySetValue(options, kSCNetworkReachabilityOptionInterface, str);
		CFRelease(str);
	}

	if (CFDictionaryGetCount(options) == 0) {
		CFRelease(options);
		options = NULL;
	}

	return options;
}


static SCNetworkReachabilityRef
_setupReachability(int argc, char * const argv[], SCNetworkReachabilityContext *context)
{
	const char			*ip_address		= argv[0];
	char				*ip_addressN		= NULL;
	const char			*interface;
	CFMutableDictionaryRef		options			= NULL;
	const char			*remote_address		= NULL;
	char				*remote_addressN	= NULL;
	const char			*remote_interface	= NULL;
	struct sockaddr_in		sin;
	struct sockaddr_in6		sin6;
	SCNetworkReachabilityRef	target			= NULL;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len    = sizeof(sin);
	sin.sin_family = AF_INET;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len    = sizeof(sin6);
	sin6.sin6_family = AF_INET6;

	interface = strchr(ip_address, '%');
	if (interface != NULL) {
		ip_addressN = strdup(ip_address);
		ip_addressN[interface - ip_address] = '\0';
		ip_address = ip_addressN;
		interface++;
	}

	if ((argc > 1) && (strlen(argv[1]) > 0)) {
		remote_address = argv[1];

		remote_interface = strchr(remote_address, '%');
		if (remote_interface != NULL) {
			remote_addressN = strdup(remote_address);
			remote_addressN[remote_interface - remote_address] = '\0';
			remote_address = remote_addressN;
			remote_interface++;
		}
	}

	if (inet_aton(ip_address, &sin.sin_addr) == 1) {
		struct sockaddr_in	r_sin;

		// if the first argument was an IP[v4] address
		argc--;
		argv++;

		if (argc > 0) {
			memset(&r_sin, 0, sizeof(r_sin));
			r_sin.sin_len    = sizeof(r_sin);
			r_sin.sin_family = AF_INET;
		}

		if ((argc == 0)
		    || (remote_address == NULL)
		    || (inet_aton(remote_address, &r_sin.sin_addr) == 0)) {
			if ((argc > 0) || (interface != NULL)) {
				options = _setupReachabilityOptions(argc, argv, interface);
			}
			if (options == NULL) {
				target = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&sin);
				if (context != NULL) {
					context->info = "by address";
				}
			} else {
				CFDataRef	data;

				data = CFDataCreate(NULL, (const UInt8 *)&sin, sizeof(sin));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionRemoteAddress, data);
				CFRelease(data);

				if (context != NULL) {
					if (CFDictionaryContainsKey(options,
								    kSCNetworkReachabilityOptionInterface)) {
						if (CFDictionaryGetCount(options) == 2) {
							context->info = "by address w/scope";
						} else {
							context->info = "by address w/scope and options";
						}
					} else {
						context->info = "by address w/options";
					}
				}
			}
		} else {
			// we have both a local and [possibly a] remote address
			argc--;
			argv++;

			if (remote_interface != NULL) {
				if ((interface != NULL) && (strcmp(interface, remote_interface) != 0)) {
					SCPrint(TRUE, stderr,
						CFSTR("Interface mismatch \"%s\" != \"%s\"\n"),
						interface,
						remote_interface);
					exit(1);
				}

				interface = remote_interface;
			}

			options = _setupReachabilityOptions(argc, argv, interface);
			if (options == NULL) {
				target = SCNetworkReachabilityCreateWithAddressPair(NULL,
										    (struct sockaddr *)&sin,
										    (struct sockaddr *)&r_sin);
				if (context != NULL) {
					context->info = "by address pair";
				}
			} else {
				CFDataRef	data;

				data = CFDataCreate(NULL, (const UInt8 *)&sin, sizeof(sin));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionLocalAddress, data);
				CFRelease(data);
				data = CFDataCreate(NULL, (const UInt8 *)&r_sin, sizeof(r_sin));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionRemoteAddress, data);
				CFRelease(data);

				if (context != NULL) {
					if (CFDictionaryContainsKey(options,
								    kSCNetworkReachabilityOptionInterface)) {
						if (CFDictionaryGetCount(options) == 3) {
							context->info = "by address pair w/scope";
						} else {
							context->info = "by address pair w/scope and options";
						}
					} else {
						context->info = "by address pair w/options";
					}
				}
			}
		}
	} else if (inet_pton(AF_INET6, ip_address, &sin6.sin6_addr) == 1) {
		struct sockaddr_in6	r_sin6;

		// if the first argument was an IP[v6] address
		argc--;
		argv++;

		if (interface != NULL) {
			sin6.sin6_scope_id = if_nametoindex(interface);
		}

		if (argc > 0) {
			memset(&r_sin6, 0, sizeof(r_sin6));
			r_sin6.sin6_len    = sizeof(r_sin6);
			r_sin6.sin6_family = AF_INET6;
		}

		if ((argc == 0)
		    || (remote_address == NULL)
		    || (inet_pton(AF_INET6, remote_address, &r_sin6.sin6_addr) == 0)) {
			if (argc > 0) {
				options = _setupReachabilityOptions(argc, argv, NULL);
			}
			if (options == NULL) {
				target = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&sin6);
				if (context != NULL) {
					context->info = "by (v6) address";
				}
			} else {
				CFDataRef	data;

				data = CFDataCreate(NULL, (const UInt8 *)&sin6, sizeof(sin6));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionRemoteAddress, data);
				CFRelease(data);

				if (context != NULL) {
					context->info = "by (v6) address w/options";
				}
			}
		} else {
			// we have both a local and [possibly a] remote address
			argc--;
			argv++;

			if (remote_interface != NULL) {
				r_sin6.sin6_scope_id = if_nametoindex(remote_interface);

				if ((interface != NULL) && (strcmp(interface, remote_interface) != 0)) {
					SCPrint(TRUE, stderr,
						CFSTR("Interface mismatch \"%s\" != \"%s\"\n"),
						interface,
						remote_interface);
					exit(1);
				}
			}

			options = _setupReachabilityOptions(argc, argv, NULL);
			if (options == NULL) {
				target = SCNetworkReachabilityCreateWithAddressPair(NULL,
										    (struct sockaddr *)&sin6,
										    (struct sockaddr *)&r_sin6);
				if (context != NULL) {
					context->info = "by (v6) address pair";
				}
			} else {
				CFDataRef	data;

				data = CFDataCreate(NULL, (const UInt8 *)&sin6, sizeof(sin6));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionLocalAddress, data);
				CFRelease(data);
				data = CFDataCreate(NULL, (const UInt8 *)&r_sin6, sizeof(r_sin6));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionRemoteAddress, data);
				CFRelease(data);

				if (context != NULL) {
					context->info = "by (v6) address pair w/options";
				}
			}
		}
	} else {
		if (argc == 1) {
			target = SCNetworkReachabilityCreateWithName(NULL, argv[0]);
			if (context != NULL) {
				context->info = "by name";
			}
		} else {
			options = _setupReachabilityOptions(argc - 1, argv + 1, NULL);
			if (options == NULL) {
				target = SCNetworkReachabilityCreateWithName(NULL, argv[0]);
				if (context != NULL) {
					context->info = "by name";
				}
			} else {
				CFStringRef	str;

				str  = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionNodeName, str);
				CFRelease(str);

				if (context != NULL) {
					context->info = "by name w/options";
				}
			}
		}
	}

	if (ip_addressN != NULL) {
		free(ip_addressN);
	}

	if (remote_addressN != NULL) {
		free(remote_addressN);
	}

	if ((target == NULL) && (options != NULL)) {
		if (CFDictionaryContainsKey(options, kSCNetworkReachabilityOptionPTRAddress)) {
			CFDataRef	address;

			address = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionRemoteAddress);
			if (address == NULL) {
				SCPrint(TRUE, stderr, CFSTR("No address\n"));
				exit(1);
			}
			CFDictionarySetValue(options, kSCNetworkReachabilityOptionPTRAddress, address);
			CFDictionaryRemoveValue(options, kSCNetworkReachabilityOptionRemoteAddress);

			if (context != NULL) {
				CFIndex	n	= CFDictionaryGetCount(options);

				if (n == 1) {
					context->info = "by PTR";
				} else if (CFDictionaryContainsKey(options,
								   kSCNetworkReachabilityOptionInterface)) {
					if (n == 2) {
						context->info = "by PTR w/scope";
					} else {
						context->info = "by PTR w/scope and options";
					}
				} else {
					context->info = "by PTR w/options";
				}
			}
		}

		target = SCNetworkReachabilityCreateWithOptions(NULL, options);
		CFRelease(options);
	}

	return target;
}


static void
_printReachability(SCNetworkReachabilityRef target)
{
	SCNetworkReachabilityFlags	flags;
	char				flags_str[100];
	Boolean				ok;

	ok = SCNetworkReachabilityGetFlags(target, &flags);
	if (!ok) {
		SCPrint(TRUE, stderr, CFSTR("    could not determine reachability, %s\n"), SCErrorString(SCError()));
		return;
	}

	__SCNetworkReachability_flags_string(flags, _sc_debug, flags_str, sizeof(flags_str));
	SCPrint(TRUE, stdout,
		_sc_debug ? CFSTR("flags = %s\n") : CFSTR("%s\n"),
		flags_str);

	if (resolver_bypass && _sc_debug) {
		int	if_index;

		if_index = SCNetworkReachabilityGetInterfaceIndex(target);
		SCPrint(TRUE, stdout, CFSTR("interface index = %d\n"), if_index);
	}

	return;
}


__private_extern__
void
do_checkReachability(int argc, char * const argv[])
{
	SCNetworkReachabilityRef	target;

	target = _setupReachability(argc, argv, NULL);
	if (target == NULL) {
		SCPrint(TRUE, stderr, CFSTR("  Could not determine status: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	_printReachability(target);
	CFRelease(target);
	return;
}


static void
do_printNWI(int argc, char * const argv[], nwi_state_t state)
{
	if (state == NULL) {
		SCPrint(TRUE, stdout, CFSTR("No network information\n"));
		return;
	}

	if (argc > 0) {
		nwi_ifstate_t	ifstate;

		ifstate = nwi_state_get_ifstate(state, argv[0]);
		if (ifstate != NULL) {
			nwi_ifstate_t	alias;
			int		alias_af;

			_nwi_ifstate_log(ifstate, _sc_debug, NULL);

			alias_af = (ifstate->af == AF_INET) ? AF_INET6 : AF_INET;
			alias = nwi_ifstate_get_alias(ifstate, alias_af);
			if (alias != NULL) {
				SCPrint(TRUE, stdout, CFSTR("\n"));
				_nwi_ifstate_log(alias, _sc_debug, NULL);
			}
		} else {
			SCPrint(TRUE, stdout, CFSTR("No network information (for %s)\n"), argv[0]);
		}
		return;
	}

	_nwi_state_log(state, _sc_debug, NULL);
	return;
}


__private_extern__
void
do_showNWI(int argc, char * const argv[])
{
	nwi_state_t	state;

	state = nwi_state_copy();
	do_printNWI(argc, argv, state);
	if (state != NULL) {
		nwi_state_release(state);
	} else {
		exit(1);
	}

	return;
}


__private_extern__
void
do_watchNWI(int argc, char * const argv[])
{
	nwi_state_t	state;
	int		status;
	int		token;

	state = nwi_state_copy();
	do_printNWI(argc, argv, state);
	if (state != NULL) {
		nwi_state_release(state);
	}

	status = notify_register_dispatch(nwi_state_get_notify_key(),
					  &token,
					  dispatch_get_main_queue(),
					  ^(int token){
#pragma unused(token)
						  nwi_state_t		state;
						  struct tm		tm_now;
						  struct timeval	tv_now;

						  (void)gettimeofday(&tv_now, NULL);
						  (void)localtime_r(&tv_now.tv_sec, &tm_now);
						  SCPrint(TRUE, stdout, CFSTR("\n*** %2d:%02d:%02d.%03d\n\n"),
							  tm_now.tm_hour,
							  tm_now.tm_min,
							  tm_now.tm_sec,
							  tv_now.tv_usec / 1000);

						  state = nwi_state_copy();
						  do_printNWI(argc, argv, state);
						  if (state != NULL) {
							  nwi_state_release(state);
						  }
					  });
	if (status != NOTIFY_STATUS_OK) {
		SCPrint(TRUE, stderr, CFSTR("notify_register_dispatch() failed for nwi changes, status=%u\n"), status);
		exit(1);
	}

	CFRunLoopRun();
}


static void
callout(SCNetworkReachabilityRef target, SCNetworkReachabilityFlags flags, void *info)
{
	static int	n = 3;
	struct tm	tm_now;
	struct timeval	tv_now;

	(void)gettimeofday(&tv_now, NULL);
	(void)localtime_r(&tv_now.tv_sec, &tm_now);

	SCPrint(TRUE, stdout, CFSTR("\n*** %2d:%02d:%02d.%03d\n\n"),
		tm_now.tm_hour,
		tm_now.tm_min,
		tm_now.tm_sec,
		tv_now.tv_usec / 1000);
	SCPrint(TRUE, stdout, CFSTR("%2d: callback w/flags=0x%08x (info=\"%s\")\n"), n++, flags, (char *)info);
	SCPrint(TRUE, stdout, CFSTR("    %@\n"), target);
	_printReachability(target);
	SCPrint(TRUE, stdout, CFSTR("\n"));
	return;
}


__private_extern__
void
do_watchReachability(int argc, char * const argv[])
{
	SCNetworkReachabilityContext	context	= { 0, NULL, NULL, NULL, NULL };
	SCNetworkReachabilityRef	target;
	SCNetworkReachabilityRef	target_async;

	target = _setupReachability(argc, argv, NULL);
	if (target == NULL) {
		SCPrint(TRUE, stderr, CFSTR("  Could not determine status: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	target_async = _setupReachability(argc, argv, &context);
	if (target_async == NULL) {
		SCPrint(TRUE, stderr, CFSTR("  Could not determine status: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	// Normally, we don't want to make any calls to SCNetworkReachabilityGetFlags()
	// until after the "target" has been scheduled on a run loop.  Otherwise, we'll
	// end up making a synchronous DNS request and that's not what we want.
	//
	// To test the case were an application first calls SCNetworkReachabilityGetFlags()
	// we provide the "CHECK_REACHABILITY_BEFORE_SCHEDULING" environment variable.
	if (getenv("CHECK_REACHABILITY_BEFORE_SCHEDULING") != NULL) {
		CFRelease(target_async);
		target_async = target;
		CFRetain(target);
	}

	// Direct check of reachability
	SCPrint(TRUE, stdout, CFSTR(" 0: direct\n"));
	SCPrint(TRUE, stdout, CFSTR("   %@\n"), target);
	_printReachability(target);
	CFRelease(target);
	SCPrint(TRUE, stdout, CFSTR("\n"));

	// schedule the target
	SCPrint(TRUE, stdout, CFSTR(" 1: start\n"));
	SCPrint(TRUE, stdout, CFSTR("   %@\n"), target_async);
//	_printReachability(target_async);
	SCPrint(TRUE, stdout, CFSTR("\n"));

	if (!SCNetworkReachabilitySetCallback(target_async, callout, &context)) {
		printf("SCNetworkReachabilitySetCallback() failed: %s\n", SCErrorString(SCError()));
		exit(1);
	}

	if (doDispatch) {
		if (!SCNetworkReachabilitySetDispatchQueue(target_async, dispatch_get_main_queue())) {
			printf("SCNetworkReachabilitySetDispatchQueue() failed: %s\n", SCErrorString(SCError()));
			exit(1);
		}
	} else {
		if (!SCNetworkReachabilityScheduleWithRunLoop(target_async, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode)) {
			printf("SCNetworkReachabilityScheduleWithRunLoop() failed: %s\n", SCErrorString(SCError()));
			exit(1);
		}
	}

	// Note: now that we are scheduled on a run loop we can call SCNetworkReachabilityGetFlags()
	//       to get the current status.  For "names", a DNS lookup has already been initiated.
	SCPrint(TRUE, stdout, CFSTR(" 2: on %s\n"), doDispatch ? "dispatch queue" : "runloop");
	SCPrint(TRUE, stdout, CFSTR("   %@\n"), target_async);
	_printReachability(target_async);
	SCPrint(TRUE, stdout, CFSTR("\n"));

	CFRunLoopRun();
	return;
}


static void
do_printDNSConfiguration(int argc, char * const argv[], dns_config_t *dns_config)
{
#pragma unused(argc)
#pragma unused(argv)
	int	_sc_log_save;

	if (dns_config == NULL) {
		SCPrint(TRUE, stdout, CFSTR("No DNS configuration available\n"));
		return;
	}

	_sc_log_save = _sc_log;
	_sc_log = kSCLogDestinationFile;
	_dns_configuration_log(dns_config, _sc_debug, NULL);
	_sc_log = _sc_log_save;

	if (_sc_debug) {
		SCPrint(TRUE, stdout, CFSTR("\ngeneration = %llu\n"), dns_config->generation);
	}

	return;
}


__private_extern__
void
do_showDNSConfiguration(int argc, char * const argv[])
{
	dns_config_t	*dns_config;

	dns_config = dns_configuration_copy();
	do_printDNSConfiguration(argc, argv, dns_config);
	if (dns_config != NULL) {
		dns_configuration_free(dns_config);
	} else {
		exit(1);
	}

	return;
}


__private_extern__
void
do_watchDNSConfiguration(int argc, char * const argv[])
{
	dns_config_t	*dns_config;
	int		status;
	int		token;

	dns_config = dns_configuration_copy();
	do_printDNSConfiguration(argc, argv, dns_config);
	if (dns_config != NULL) {
		dns_configuration_free(dns_config);
	}

	status = notify_register_dispatch(dns_configuration_notify_key(),
					  &token,
					  dispatch_get_main_queue(),
					  ^(int token){
#pragma unused(token)
						  dns_config_t		*dns_config;
						  struct tm		tm_now;
						  struct timeval	tv_now;

						  (void)gettimeofday(&tv_now, NULL);
						  (void)localtime_r(&tv_now.tv_sec, &tm_now);
						  SCPrint(TRUE, stdout, CFSTR("\n*** %2d:%02d:%02d.%03d\n\n"),
							  tm_now.tm_hour,
							  tm_now.tm_min,
							  tm_now.tm_sec,
							  tv_now.tv_usec / 1000);

						  dns_config = dns_configuration_copy();
						  do_printDNSConfiguration(argc, argv, dns_config);
						  if (dns_config != NULL) {
							  dns_configuration_free(dns_config);
						  }
					  });
	if (status != NOTIFY_STATUS_OK) {
		SCPrint(TRUE, stderr, CFSTR("notify_register_dispatch() failed for nwi changes, status=%u\n"), status);
		exit(1);
	}

	CFRunLoopRun();
}


static void
showProxy(CFDictionaryRef proxy)
{
	CFMutableDictionaryRef	cleaned	= NULL;

	if (!_sc_debug) {
		cleaned = CFDictionaryCreateMutableCopy(NULL, 0, proxy);
		CFDictionaryRemoveValue(cleaned, kSCPropNetProxiesScoped);
		CFDictionaryRemoveValue(cleaned, kSCPropNetProxiesServices);
		CFDictionaryRemoveValue(cleaned, kSCPropNetProxiesSupplemental);
		proxy = cleaned;
	}

	SCPrint(TRUE, stdout, CFSTR("%@\n"), proxy);
	if (cleaned != NULL) CFRelease(cleaned);
	return;
}


__private_extern__
void
do_showProxyConfiguration(int argc, char * const argv[])
{
	CFDictionaryRef		proxies;

	if (getenv("BYPASS_GLOBAL_PROXY") != NULL) {
		CFMutableDictionaryRef	options ;

		options = CFDictionaryCreateMutable(NULL, 0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
		CFDictionaryAddValue(options, kSCProxiesNoGlobal, kCFBooleanTrue);
		proxies = SCDynamicStoreCopyProxiesWithOptions(NULL, options);
		CFRelease(options);
	} else {
		proxies = SCDynamicStoreCopyProxies(NULL);
	}

	if (proxies != NULL) {
		CFStringRef	interface	= NULL;
		CFStringRef	server		= NULL;

		while (argc > 0) {
			if (strcasecmp(argv[0], "interface") == 0) {
				argv++;
				argc--;

				if (argc < 1) {
					SCPrint(TRUE, stderr, CFSTR("No interface\n"));
					exit(1);
				}

				if (if_nametoindex(argv[0]) == 0) {
					SCPrint(TRUE, stderr, CFSTR("No interface: %s\n"), argv[0]);
					exit(1);
				}

				if (interface != NULL) {
					CFRelease(interface);
				}
				interface = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
				argv++;
				argc--;
			} else {
				if (server != NULL) {
					CFRelease(server);
				}
				server = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
				argv++;
				argc--;
			}
		}

		if ((server != NULL) || (interface != NULL)) {
			CFArrayRef	matching;

			matching = SCNetworkProxiesCopyMatching(proxies, server, interface);
			if (matching != NULL) {
				CFIndex	i;
				CFIndex	n;

				if (server != NULL) {
					if (interface != NULL) {
						SCPrint(TRUE, stdout,
							CFSTR("server = %@, interface = %@\n"),
							server,
							interface);
					} else {
						SCPrint(TRUE, stdout,
							CFSTR("server = %@\n"),
							server);
					}
				} else {
					SCPrint(TRUE, stdout,
						CFSTR("interface = %@\n"),
						interface);
				}

				n = CFArrayGetCount(matching);
				for (i = 0; i < n; i++) {
					CFDictionaryRef	proxy;

					proxy = CFArrayGetValueAtIndex(matching, i);
					SCPrint(TRUE, stdout, CFSTR("\nproxy #%ld\n"), i + 1);
					showProxy(proxy);
				}

				CFRelease(matching);
			} else {
				SCPrint(TRUE, stdout, CFSTR("No matching proxy configurations\n"));
			}
		} else {
			showProxy(proxies);
		}

		if (interface != NULL) CFRelease(interface);
		if (server != NULL) CFRelease(server);
		CFRelease(proxies);
	} else {
		SCPrint(TRUE, stdout, CFSTR("No proxy configuration available\n"));
	}

	return;
}


__private_extern__
void
do_snapshot(int argc, char * const argv[])
{
	if (argc == 1) {
		CFDictionaryRef		dict;
		int			fd;
		CFMutableArrayRef	patterns;

		fd = open(argv[0], O_WRONLY|O_CREAT|O_TRUNC|O_EXCL, 0644);
		if (fd == -1) {
			SCPrint(TRUE, stdout, CFSTR("open() failed: %s\n"), strerror(errno));
			return;
		}

		patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(patterns, CFSTR(".*"));
		dict = SCDynamicStoreCopyMultiple(store, NULL, patterns);
		CFRelease(patterns);
		if (dict != NULL) {
			CFDataRef	xmlData;

			xmlData = CFPropertyListCreateData(NULL, dict, kCFPropertyListXMLFormat_v1_0, 0, NULL);
			CFRelease(dict);
			if (xmlData != NULL) {
				(void) write(fd, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData));
				CFRelease(xmlData);
			} else {
				SC_log(LOG_NOTICE, "CFPropertyListCreateData() failed");
			}
		} else {
			if (SCError() == kSCStatusOK) {
				SCPrint(TRUE, stdout, CFSTR("No SCDynamicStore content\n"));
			} else {
				SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
			}
		}
		(void) close(fd);
	} else {
#if	!TARGET_OS_SIMULATOR
		if (geteuid() != 0) {
			SCPrint(TRUE, stdout, CFSTR("Need to be \"root\" to capture snapshot\n"));
		} else
#endif	// !TARGET_OS_SIMULATOR
		if (!SCDynamicStoreSnapshot(store)) {
			SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
		}
	}

	return;
}


__private_extern__
void
do_renew(char *if_name)
{
	CFArrayRef	services;
	Boolean		ok	= FALSE;

	if ((if_name == NULL) || (strlen(if_name) == 0)) {
		SCPrint(TRUE, stderr, CFSTR("No interface name\n"));
		exit(1);
	}

	if (getenv("ATTEMPT_DHCP_RENEW_WITH_SCDYNAMICSTORE") != NULL) {
		CFArrayRef	interfaces;

		interfaces = SCNetworkInterfaceCopyAll();
		if (interfaces != NULL) {
			CFIndex		i;
			CFStringRef	match_name;
			CFIndex		n;

			match_name = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingASCII);
			assert(match_name != NULL);

			n = CFArrayGetCount(interfaces);
			for (i = 0; i < n; i++) {
				CFStringRef		bsd_name;
				SCNetworkInterfaceRef	interface;

				interface = CFArrayGetValueAtIndex(interfaces, i);
				bsd_name = SCNetworkInterfaceGetBSDName(interface);
				if (_SC_CFEqual(bsd_name, match_name)) {
					// if match
					ok = SCNetworkInterfaceForceConfigurationRefresh(interface);
					if (!ok) {
						int	status;

						status = SCError();
						if (status != kSCStatusAccessError) {
							SCPrint(TRUE, stderr, CFSTR("%s\n"), SCErrorString(status));
							exit(1);
						}

						// ... and if can't write the SCDynamicStore, try w/prefs
					}

					break;
				}
			}

			CFRelease(match_name);
			CFRelease(interfaces);
		}

		if (ok) {
			return;
		}
	}

	do_prefs_init();	/* initialization */
	do_prefs_open(0, NULL);	/* open default prefs */

	services = SCNetworkServiceCopyAll(prefs);
	if (services != NULL) {
		CFIndex		i;
		CFStringRef	match_name;
		CFIndex		n;

		match_name = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingASCII);
		assert(match_name != NULL);

		n = CFArrayGetCount(services);
		for (i = 0; i < n; i++) {
			CFStringRef		bsd_name;
			SCNetworkInterfaceRef	interface;
			SCNetworkServiceRef	service;

			service = CFArrayGetValueAtIndex(services, i);
			interface = SCNetworkServiceGetInterface(service);
			if (interface == NULL) {
				// if no interface
				continue;
			}

			bsd_name = SCNetworkInterfaceGetBSDName(interface);
			if (_SC_CFEqual(bsd_name, match_name)) {
				// if match
				ok = SCNetworkInterfaceForceConfigurationRefresh(interface);
				if (!ok) {
					SCPrint(TRUE, stderr, CFSTR("%s\n"), SCErrorString(SCError()));
					exit(1);
				}

				break;
			}
		}

		CFRelease(match_name);
		CFRelease(services);
	}

	if (!ok) {
		SCPrint(TRUE, stderr, CFSTR("No interface\n"));
		exit(1);
	}

	_prefs_close();
	return;
}


static void
waitKeyFound()
{
	return;
}


static void __attribute__((noreturn))
waitTimeout(int sigraised)
{
#pragma unused(sigraised)
	exit(1);
}


__private_extern__
void
do_wait(char *waitKey, int timeout)
{
	struct itimerval	itv;
	CFStringRef		key;
	CFMutableArrayRef	keys;
	Boolean			ok;

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil (wait)"), waitKeyFound, NULL);
	if (store == NULL) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreCreate() failed: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	key  = CFStringCreateWithCString(NULL, waitKey, kCFStringEncodingUTF8);

	keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(keys, key);
	ok = SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
	if (!ok) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreSetNotificationKeys() failed: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	notifyRls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!notifyRls) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreCreateRunLoopSource() failed: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), notifyRls, kCFRunLoopDefaultMode);

	value = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (value) {
		/* if the key is already present */
		return;
	}

	if (timeout > 0) {
		signal(SIGALRM, waitTimeout);
		memset(&itv, 0, sizeof(itv));
		itv.it_value.tv_sec = timeout;
		if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
			SCPrint(TRUE, stderr,
				CFSTR("setitimer() failed: %s\n"), strerror(errno));
			exit(1);
		}
	}

	CFRunLoopRun();
}

/**
 ** "scutil --advisory <ifname> [ set | -W ]"
 **/

static void
store_watch_key(SCDynamicStoreCallBack callback, CFStringRef label,
		const void * info, CFStringRef key)
{
	SCDynamicStoreContext	context = { .info = (void *)info };
	CFMutableArrayRef	keys;
	Boolean			ok;

	store = SCDynamicStoreCreate(NULL, label, callback, &context);
	if (store == NULL) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreCreate() failed: %s\n"),
			SCErrorString(SCError()));
		exit(1);
	}
	keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(keys, key);
	CFRelease(key);
	ok = SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
	if (!ok) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreSetNotificationKeys failed: %s\n"),
			SCErrorString(SCError()));
		exit(1);
	}
	notifyRls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!notifyRls) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreCreateRunLoopSource() failed: %s\n"),
			SCErrorString(SCError()));
		exit(1);
	}
	CFRunLoopAddSource(CFRunLoopGetCurrent(), notifyRls,
			   kCFRunLoopDefaultMode);
	return;
}

static void __printflike(2, 3)
timestamp_fprintf(FILE * f, const char * message, ...)
{
	struct timeval	tv;
	struct tm       tm;
	time_t		t;
	va_list		ap;

	(void)gettimeofday(&tv, NULL);
	t = tv.tv_sec;
	(void)localtime_r(&t, &tm);

	va_start(ap, message);
	fprintf(f, "%04d/%02d/%02d %2d:%02d:%02d.%06d ",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		tv.tv_usec);
	vfprintf(f, message, ap);
	va_end(ap);
}

typedef struct {
	SCNetworkInterfaceRef	interface;
	SCNetworkInterfaceAdvisory advisory;
} advisoryContext, *advisoryContextRef;

static const char *
get_advisory_str(SCNetworkInterfaceAdvisory advisory)
{
	const char *	str = NULL;

	switch (advisory) {
	case kSCNetworkInterfaceAdvisoryLinkLayerIssue:
		str = "LinkLayer";
		break;
	case kSCNetworkInterfaceAdvisoryUplinkIssue:
		str = "Uplink";
		break;
	case kSCNetworkInterfaceAdvisoryBetterInterfaceAvailable:
		str = "BetterInterface";
		break;
	default:
		str = "<unknown>";
		break;
	}
	return (str);
}

static void
advisoryShow(SCNetworkInterfaceRef interface)
{
	CFIndex		count;
	CFArrayRef	list;

	list = SCNetworkInterfaceCopyAdvisoryInfo(interface);
	if (list == NULL) {
		return;
	}
	count = CFArrayGetCount(list);
	SCPrint(TRUE, stdout,
		CFSTR("%@: advisory count %d\n"),
		SCNetworkInterfaceGetBSDName(interface),
		(int)count);
	for (CFIndex i = 0; i < count; i++) {
		SCNetworkInterfaceAdvisory 		advisory;
		SCNetworkInterfaceAdvisoryInfoRef 	info;

		info = (SCNetworkInterfaceAdvisoryInfoRef)
			CFArrayGetValueAtIndex(list, i);
		advisory = SCNetworkInterfaceAdvisoryInfoGetAdvisory(info);
		SCPrint(TRUE, stdout,
			CFSTR("%ld. %@ [%d] %s [%u]\n"),
			i + 1,
			SCNetworkInterfaceAdvisoryInfoGetProcessName(info),
			SCNetworkInterfaceAdvisoryInfoGetProcessID(info),
			get_advisory_str(advisory), advisory);
	}
	CFRelease(list);
}

static void
advisoryCheck(SCNetworkInterfaceRef interface,
	      SCNetworkInterfaceAdvisory advisory,
	      Boolean show_timestamp)
{
	Boolean		is_set;

	is_set = SCNetworkInterfaceAdvisoryIsSpecificSet(interface, advisory);
	if (show_timestamp) {
		timestamp_fprintf(stdout, "");
	}
	if (advisory == kSCNetworkInterfaceAdvisoryNone) {
		if (is_set) {
			printf("At least one advisory is set\n");
		} else {
			printf("No advisories are set\n");
		}
	} else {
		printf("%s advisory is %sset\n",
		       get_advisory_str(advisory),
		       is_set ? "" : "not ");
	}
	if (is_set && _sc_verbose) {
		advisoryShow(interface);
	}
}

static void
advisoryChanged(SCDynamicStoreRef session, CFArrayRef changes,
		void * info)
{
#pragma unused(session, changes)
	advisoryContextRef	context = (advisoryContextRef)info;

	advisoryCheck(context->interface, context->advisory, TRUE);
	return;
}

static void
advisoryWatch(SCNetworkInterfaceRef interface,
	      SCNetworkInterfaceAdvisory advisory)
{
	advisoryContextRef	adv_context;
	CFStringRef		key;

	adv_context = malloc(sizeof(*adv_context));
	adv_context->interface = interface;
	adv_context->advisory = advisory;
	key = SCNetworkInterfaceCopyAdvisoryNotificationKey(interface);
	store_watch_key(advisoryChanged, CFSTR("scutil --advisory"),
			adv_context, key);
	CFRelease(key);
}

static void
advisoryShowAll(void)
{
	CFIndex		count;
	Boolean		first = TRUE;
	CFArrayRef	list;

	list = SCNetworkInterfaceAdvisoryCopyInterfaceNames();
	if (list == NULL) {
		printf("No advisories\n");
		return;
	}
	count = CFArrayGetCount(list);
	for (CFIndex i = 0; i < count; i++) {
		CFStringRef		ifname_cf;
		SCNetworkInterfaceRef	interface;

		ifname_cf = CFArrayGetValueAtIndex(list, i);
		interface = _SCNetworkInterfaceCreateWithBSDName(NULL,
								 ifname_cf,
								 kIncludeAllVirtualInterfaces);
		if (interface != NULL) {
			if (first) {
				first = FALSE;
			} else {
				printf("\n");
			}
			advisoryShow(interface);
			CFRelease(interface);
		}
	}
	CFRelease(list);
}

static void
advisoryUsage(void)
{
	fprintf(stderr,
		"usage:\n"
		"\tscutil --advisory \"\"\n"
		"\tscutil --advisory <ifname> "
		"( show | set ) "
		"[ linklayer | uplink | betterinterface ] "
		"[ -W ]\n");
}
__private_extern__
void
do_advisory(const char * ifname, Boolean watch, int argc, char * const argv[])
{
	SCNetworkInterfaceAdvisory advisory = kSCNetworkInterfaceAdvisoryNone;
	Boolean			do_set = FALSE;
	Boolean			show_all = FALSE;
	SCNetworkInterfaceRef	interface = NULL;

	if (ifname[0] == '\0') {
		show_all = TRUE;
	} else {
		CFStringRef		ifname_cf;

		ifname_cf = CFStringCreateWithCString(NULL, ifname, kCFStringEncodingUTF8);
		interface = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname_cf, kIncludeAllVirtualInterfaces);
		CFRelease(ifname_cf);
		if (interface == NULL) {
			fprintf(stderr, "Failed to instantiate SCNetworkInterfaceRef\n");
			exit(1);
		}
	}
	if (argc >= 1) {
		if (show_all) {
			advisoryUsage();
			exit(1);
		}
		if (strcasecmp(argv[0], "set") == 0) {
			do_set = TRUE;
			advisory = kSCNetworkInterfaceAdvisoryLinkLayerIssue;
		} else if (strcasecmp(argv[0], "get") == 0
			   || strcasecmp(argv[0], "show") == 0) {
		} else {
			advisoryUsage();
			exit(1);
		}
		if (argc >= 2) {
			if (strcasecmp(argv[1], "uplink") == 0) {
				advisory = kSCNetworkInterfaceAdvisoryUplinkIssue;
			} else if (strcasecmp(argv[1], "linklayer") == 0) {
				advisory = kSCNetworkInterfaceAdvisoryLinkLayerIssue;
			} else if (strcasecmp(argv[1], "betterinterface") == 0) {
				advisory = kSCNetworkInterfaceAdvisoryBetterInterfaceAvailable;
			} else {
				fprintf(stderr,
					"Bad advisory '%s', must be either 'uplink', 'linklayer', or 'betterinterface'\n",
					argv[1]);
				exit(1);
			}
		}
	}
	if (show_all) {
		advisoryShowAll();
	} else if (do_set) {
		if (!SCNetworkInterfaceSetAdvisory(interface, advisory,
						   CFSTR("scutil advisory"))) {
			fprintf(stderr,
				"SCNetworkInterfaceSetAdvisory failed %s\n",
				SCErrorString(SCError()));
			exit(1);
		}
		CFRunLoopRun();
	} else {
		if (watch) {
			advisoryWatch(interface, advisory);
		}
		advisoryCheck(interface, advisory, watch);
		if (watch) {
			CFRunLoopRun();
		}
	}

	return;
}

static const char *
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
	    str = "<unknown>";
	    break;
    }
    return (str);
}

static void
rankAssertionShow(SCNetworkInterfaceRef interface,
		  Boolean show_timestamp)
{
	CFIndex		count;
	CFArrayRef	list;

	if (show_timestamp) {
		timestamp_fprintf(stdout, "");
	}

	list = SCNetworkInterfaceCopyRankAssertionInfo(interface);
	if (list == NULL) {
		printf("No rank assertions\n");
		return;
	}
	count = CFArrayGetCount(list);
	SCPrint(TRUE, stdout,
		CFSTR("%@ rank assertion count %d\n"),
		SCNetworkInterfaceGetBSDName(interface),
		(int)count);
	for (CFIndex i = 0; i < count; i++) {
		SCNetworkInterfaceRankAssertionInfoRef 	info;
		SCNetworkServicePrimaryRank 		rank;

		info = (SCNetworkInterfaceRankAssertionInfoRef)
			CFArrayGetValueAtIndex(list, i);
		rank = SCNetworkInterfaceRankAssertionInfoGetPrimaryRank(info);
		SCPrint(TRUE, stdout,
			CFSTR("%ld. %@ [%d] %s [%u]\n"),
			i + 1,
			SCNetworkInterfaceRankAssertionInfoGetProcessName(info),
			SCNetworkInterfaceRankAssertionInfoGetProcessID(info),
			get_rank_str(rank), rank);
	}
	CFRelease(list);
}
static void
rankAssertionShowAll(void)
{
	CFIndex		count;
	Boolean		first = TRUE;
	CFArrayRef	list;

	list = SCNetworkInterfaceRankAssertionCopyInterfaceNames();
	if (list == NULL) {
		printf("No rank assertions\n");
		return;
	}
	count = CFArrayGetCount(list);
	for (CFIndex i = 0; i < count; i++) {
		CFStringRef		ifname_cf;
		SCNetworkInterfaceRef	interface;

		ifname_cf = CFArrayGetValueAtIndex(list, i);
		interface = _SCNetworkInterfaceCreateWithBSDName(NULL,
								 ifname_cf,
								 kIncludeAllVirtualInterfaces);
		if (interface != NULL) {
			if (first) {
				first = FALSE;
			} else {
				printf("\n");
			}
			rankAssertionShow(interface, FALSE);
			CFRelease(interface);
		}
	}
	CFRelease(list);
}

static void
rankAssertionChanged(SCDynamicStoreRef session, CFArrayRef changes,
		     void * info)
{
#pragma unused(session)
#pragma unused(changes)
	SCNetworkInterfaceRef	interface = (SCNetworkInterfaceRef)info;

	rankAssertionShow(interface, TRUE);
	return;
}

static void
rankAssertionWatch(SCNetworkInterfaceRef interface)
{
	CFStringRef		key;

	key = SCNetworkInterfaceCopyRankAssertionNotificationKey(interface);
	store_watch_key(rankAssertionChanged, CFSTR("scutil --rank"),
			interface, key);
	CFRelease(key);
}

static void
rankUsage(void)
{
	fprintf(stderr,
		"usage:\n"
		"\tscutil --rank \"\"\n"
		"\tscutil --rank <ifname> "
		"show | set (First | Last | Never | Scoped)\n");
}

__private_extern__
void
do_rank(const char * ifname, Boolean watch, int argc, char * const argv[])
{
	Boolean				do_set = FALSE;
	SCNetworkInterfaceRef		interface = NULL;
	SCNetworkServicePrimaryRank 	rank;
	Boolean				show_all = FALSE;

	if (ifname[0] == '\0') {
		show_all = TRUE;
	} else {
		CFStringRef			ifname_cf;

		ifname_cf = CFStringCreateWithCString(NULL, ifname,
						      kCFStringEncodingUTF8);
		interface = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname_cf,
								 kIncludeAllVirtualInterfaces);
		CFRelease(ifname_cf);
		if (interface == NULL) {
			fprintf(stderr,
				"Failed to instantiate SCNetworkInterfaceRef\n");
			exit(1);
		}
	}
	rank = kSCNetworkServicePrimaryRankDefault;
	if (argc >= 1) {
		if (show_all) {
			rankUsage();
			exit(1);
		}
		if (strcasecmp(argv[0], "set") == 0) {
			do_set = TRUE;
		} else if (strcasecmp(argv[0], "show") == 0
			   || strcasecmp(argv[0], "get") == 0) {
		} else {
			rankUsage();
			exit(1);
		}
		if (argc >= 2) {
			if (!get_rank_from_string(argv[1], &rank)) {
				fprintf(stderr,
					"Bad rank '%s', must be 'First', "
					"'Last', 'Never', or 'Scoped'\n",
					argv[1]);
				exit(1);
			}
		}
	}
	if (show_all) {
		rankAssertionShowAll();
	} else if (do_set) {
		if (!SCNetworkInterfaceSetPrimaryRank(interface, rank)) {
			fprintf(stderr,
				"SCNetworkInterfaceSetPrimaryRank failed, %s\n",
				SCErrorString(SCError()));
			exit(1);
		}
		CFRunLoopRun();
	} else {
		if (watch) {
			rankAssertionWatch(interface);
		}
		rankAssertionShow(interface, watch);
		if (watch) {
			CFRunLoopRun();
		}
	}
	return;
}


#ifdef	TEST_DNS_CONFIGURATION

Boolean			doDispatch	= FALSE;
CFRunLoopSourceRef	notifyRls	= NULL;
SCDynamicStoreRef	store		= NULL;
CFPropertyListRef	value		= NULL;

int
main(int argc, char * const argv[])
{
	dns_config_t	*dns_config;

fprintf(stdout, "copy configuration\n");
	dns_config = dns_configuration_copy();
	if (dns_config != NULL) {

fprintf(stdout, "sleeping for 120 seconds\n");
sleep(120);

fprintf(stdout, "sending ack\n");
		_dns_configuration_ack(dns_config, "TEST_DNS_CONFIGURATION");

fprintf(stdout, "sleeping for 120 seconds\n");
sleep(120);

		dns_configuration_free(dns_config);
	}

	do_showDNSConfiguration(argc, argv);
	exit(0);
}

#endif	// TEST_DNS_CONFIGURATION
