/*
 * Copyright (c) 2011-2019 Apple Inc. All rights reserved.
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
 * IPConfigurationService.c
 * - API to communicate with IPConfiguration to instantiate services
 */
/* 
 * Modification History
 *
 * April 14, 2011 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <pthread.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <libkern/OSAtomic.h>
#include "ipconfig_types.h"
#include "ipconfig_ext.h"
#include "symbol_scope.h"
#include "cfutil.h"
#include "ipconfig.h"
#include "IPConfigurationLog.h"
#include "IPConfigurationServiceInternal.h"
#include "IPConfigurationService.h"

const CFStringRef 
kIPConfigurationServiceOptionEnableDAD = _kIPConfigurationServiceOptionEnableDAD;

const CFStringRef
kIPConfigurationServiceOptionEnableCLAT46 = _kIPConfigurationServiceOptionEnableCLAT46;

const CFStringRef
kIPConfigurationServiceOptionMTU = _kIPConfigurationServiceOptionMTU;

const CFStringRef
kIPConfigurationServiceOptionPerformNUD = _kIPConfigurationServiceOptionPerformNUD;

const CFStringRef
kIPConfigurationServiceOptionIPv6Entity = _kIPConfigurationServiceOptionIPv6Entity;

const CFStringRef
kIPConfigurationServiceOptionIPv6LinkLocalAddress = CFSTR("IPv6LinkLocalAddress");

const CFStringRef
kIPConfigurationServiceOptionAPNName = _kIPConfigurationServiceOptionAPNName;

#ifndef kSCPropNetIPv6LinkLocalAddress
STATIC const CFStringRef kSCPropNetIPv6LinkLocalAddress = CFSTR("LinkLocalAddress");
#define kSCPropNetIPv6LinkLocalAddress	kSCPropNetIPv6LinkLocalAddress
#endif /* kSCPropNetIPv6LinkLocalAddress */


/**
 ** ObjectWrapper
 **/

typedef struct {
    const void *	obj;
    int32_t		retain_count;
} ObjectWrapper, * ObjectWrapperRef;

STATIC const void *
ObjectWrapperRetain(const void * info)
{
    ObjectWrapperRef 	wrapper = (ObjectWrapperRef)info;

    (void)OSAtomicIncrement32(&wrapper->retain_count);
#ifdef DEBUG
    printf("wrapper retain (%d)\n", new_val);
#endif
    return (info);
}

STATIC ObjectWrapperRef
ObjectWrapperAlloc(const void * obj)
{
    ObjectWrapperRef	wrapper;

    wrapper = (ObjectWrapperRef)malloc(sizeof(*wrapper));
    wrapper->obj = obj;
    wrapper->retain_count = 1;
    return (wrapper);
}

STATIC void
ObjectWrapperRelease(const void * info)
{
    int32_t		new_val;
    ObjectWrapperRef 	wrapper = (ObjectWrapperRef)info;

    new_val = OSAtomicDecrement32(&wrapper->retain_count);
#ifdef DEBUG
    printf("wrapper release (%d)\n", new_val);
#endif
    if (new_val == 0) {
#ifdef DEBUG
	printf("wrapper free\n");
#endif
	free(wrapper);
    }
    else if (new_val < 0) {
	IPConfigLogFL(LOG_NOTICE,
		      "IPConfigurationService: retain count already zero");
	abort();
    }
    return;
}

/**
 ** IPConfigurationService
 **/
struct __IPConfigurationService {
    CFRuntimeBase		cf_base;

    InterfaceName		ifname;
    mach_port_t			server;
    SCDynamicStoreRef		store;
    dispatch_queue_t		queue;
    ServiceID			service_id;
    CFStringRef			store_key;
    ObjectWrapperRef		wrapper;
    CFDictionaryRef		config_dict;
    Boolean			need_reconnect;
};

/**
 ** CF object glue code
 **/
STATIC CFStringRef	__IPConfigurationServiceCopyDebugDesc(CFTypeRef cf);
STATIC void		__IPConfigurationServiceDeallocate(CFTypeRef cf);

STATIC CFTypeID __kIPConfigurationServiceTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __IPConfigurationServiceClass = {
    0,						/* version */
    "IPConfigurationService",			/* className */
    NULL,					/* init */
    NULL,					/* copy */
    __IPConfigurationServiceDeallocate,		/* deallocate */
    NULL,					/* equal */
    NULL,					/* hash */
    NULL,					/* copyFormattingDesc */
    __IPConfigurationServiceCopyDebugDesc	/* copyDebugDesc */
};

STATIC CFStringRef
__IPConfigurationServiceCopyDebugDesc(CFTypeRef cf)
{
    CFAllocatorRef		allocator = CFGetAllocator(cf);
    CFMutableStringRef		result;
    IPConfigurationServiceRef	service = (IPConfigurationServiceRef)cf;

    result = CFStringCreateMutable(allocator, 0);
    CFStringAppendFormat(result, NULL,
			 CFSTR("<IPConfigurationService %p [%p]> {"),
			 cf, allocator);
    CFStringAppendFormat(result, NULL, CFSTR("ifname = %s, serviceID = %s"),
			 service->ifname, service->service_id);
    CFStringAppend(result, CFSTR("}"));
    return (result);
}


STATIC void
__IPConfigurationServiceDeallocate(CFTypeRef cf)
{
    IPConfigurationServiceRef 	service = (IPConfigurationServiceRef)cf;

    if (service->wrapper != NULL) {
	if (service->queue != NULL) {
	    /* ensure disconnect code won't run anymore */
	    dispatch_sync(service->queue,
			  ^{
			      service->wrapper->obj = NULL;
			  });
	}
	ObjectWrapperRelease(service->wrapper);
	service->wrapper = NULL;
    }
    if (service->store != NULL) {
	SCDynamicStoreSetDispatchQueue(service->store, NULL);
	my_CFRelease(&service->store);
    }
    if (service->queue != NULL) {
	dispatch_release(service->queue);
	service->queue = NULL;
    }
    if (service->server != MACH_PORT_NULL) {
	kern_return_t		kret;
	ipconfig_status_t	status;

	kret = ipconfig_remove_service_on_interface(service->server,
						    service->ifname,
						    service->service_id,
						    &status);
	if (kret != KERN_SUCCESS) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_remove_service_on_interface(%s %s) "
			  "failed, %s",
			  service->ifname, service->service_id,
			  mach_error_string(kret));
	}
	else if (status != ipconfig_status_success_e) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_remove_service_on_interface(%s %s)"
			  " failed: %s",
			  service->ifname, service->service_id,
			  ipconfig_status_string(status));
	}
	mach_port_deallocate(mach_task_self(), service->server);
	service->server = MACH_PORT_NULL;
    }
    my_CFRelease(&service->config_dict);
    my_CFRelease(&service->store_key);

    return;
}

STATIC void
__IPConfigurationServiceInitialize(void)
{
    /* initialize runtime */
    __kIPConfigurationServiceTypeID 
	= _CFRuntimeRegisterClass(&__IPConfigurationServiceClass);
    return;
}

STATIC void
__IPConfigurationServiceRegisterClass(void)
{
    STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

    pthread_once(&initialized, __IPConfigurationServiceInitialize);
    return;
}

STATIC IPConfigurationServiceRef
__IPConfigurationServiceAllocate(CFAllocatorRef allocator)
{
    IPConfigurationServiceRef	service;
    int				size;

    __IPConfigurationServiceRegisterClass();

    size = sizeof(*service) - sizeof(CFRuntimeBase);
    service = (IPConfigurationServiceRef)
	_CFRuntimeCreateInstance(allocator,
				 __kIPConfigurationServiceTypeID, size, NULL);
    bzero(((void *)service) + sizeof(CFRuntimeBase), size);
    return (service);
}

/**
 ** Utility functions
 **/
STATIC CFDictionaryRef
config_dict_create(CFStringRef serviceID,
		   CFDictionaryRef requested_ipv6_config,
		   CFBooleanRef no_publish,
		   CFNumberRef mtu, CFBooleanRef perform_nud,
		   CFStringRef ipv6_ll, CFBooleanRef enable_dad,
		   CFStringRef apn_name, CFBooleanRef enable_clat46)
{
#define N_KEYS_VALUES	9
    int			count;
    CFDictionaryRef	config_dict;
    CFDictionaryRef 	ipv6_dict;
    const void *	keys[N_KEYS_VALUES];
    CFDictionaryRef	options;
    const void *	values[N_KEYS_VALUES];

    /* monitor pid */    
    count = 0;
    keys[count] = _kIPConfigurationServiceOptionMonitorPID;
    values[count] = kCFBooleanTrue;
    count++;

    /* no publish */
    if (no_publish == NULL) {
	no_publish = kCFBooleanTrue;
    }
    keys[count] = _kIPConfigurationServiceOptionNoPublish;
    values[count] = no_publish;
    count++;

    /* mtu */
    if (mtu != NULL) {
	keys[count] = kIPConfigurationServiceOptionMTU;
	values[count] = mtu;
	count++;
    }
    
    /* perform NUD */
    if (perform_nud != NULL) {
	keys[count] = kIPConfigurationServiceOptionPerformNUD;
	values[count] = perform_nud;
	count++;
    }

    /* enable DAD */
    if (enable_dad != NULL) {
	keys[count] = kIPConfigurationServiceOptionEnableDAD;
	values[count] = enable_dad;
	count++;
    }

    /* enable CLAT46 */
    if (enable_clat46 != NULL) {
	keys[count] = kIPConfigurationServiceOptionEnableCLAT46;
	values[count] = enable_clat46;
	count++;
    }

    /* serviceID */
    keys[count] = _kIPConfigurationServiceOptionServiceID;
    values[count] = serviceID;
    count++;

    /* clear state */
    keys[count] = _kIPConfigurationServiceOptionClearState;
    values[count] = kCFBooleanTrue;
    count++;

    /* APN name */
    if (apn_name != NULL) {
	keys[count] = kIPConfigurationServiceOptionAPNName;
	values[count] = apn_name;
	count++;
    }

    options
	= CFDictionaryCreate(NULL, keys, values, count,
			     &kCFTypeDictionaryKeyCallBacks,
			     &kCFTypeDictionaryValueCallBacks);

    if (requested_ipv6_config != NULL) {
	if (ipv6_ll != NULL
	    && !CFDictionaryContainsKey(requested_ipv6_config,
					kSCPropNetIPv6LinkLocalAddress)) {
	    CFMutableDictionaryRef	temp;

	    temp = CFDictionaryCreateMutableCopy(NULL, 0,
						 requested_ipv6_config);
	    CFDictionarySetValue(temp,
				 kSCPropNetIPv6LinkLocalAddress,
				 ipv6_ll);
	    ipv6_dict = temp;
	}
	else {
	    ipv6_dict = requested_ipv6_config;
	}
    }
    else {
	count = 0;
	keys[count] = kSCPropNetIPv6ConfigMethod;
	values[count] = kSCValNetIPv6ConfigMethodAutomatic;
	count++;

	/* add the IPv6 link-local address if specified */
	if (ipv6_ll != NULL) {
	    keys[count] = kSCPropNetIPv6LinkLocalAddress;
	    values[count] = ipv6_ll;
	    count++;
	}

	/* create the default IPv6 dictionary */
	ipv6_dict
	    = CFDictionaryCreate(NULL, keys, values, count,
				 &kCFTypeDictionaryKeyCallBacks,
				 &kCFTypeDictionaryValueCallBacks);
    }

    count = 0;
    keys[count] = kIPConfigurationServiceOptions;
    values[count] = options;
    count++;

    keys[count] = kSCEntNetIPv6;
    values[count] = ipv6_dict;
    count++;

    config_dict
	= CFDictionaryCreate(NULL, keys, values, count,
			     &kCFTypeDictionaryKeyCallBacks,
			     &kCFTypeDictionaryValueCallBacks);
    CFRelease(options);
    if (ipv6_dict != requested_ipv6_config) {
	CFRelease(ipv6_dict);
    }
    return (config_dict);
}

STATIC Boolean
ipv6_config_is_valid(CFDictionaryRef config)
{
    CFStringRef		config_method;
    Boolean		is_valid = FALSE;

    config_method = CFDictionaryGetValue(config, kSCPropNetIPv6ConfigMethod);
    if (isA_CFString(config_method) == NULL) {
	goto done;
    }
    if (CFEqual(config_method, kSCValNetIPv6ConfigMethodManual)) {
	CFArrayRef	addresses;
	CFIndex		count;
	CFArrayRef	prefix_lengths;

	/* must contain Addresses, PrefixLength */
	addresses = CFDictionaryGetValue(config, kSCPropNetIPv6Addresses);
	prefix_lengths = CFDictionaryGetValue(config, 
					      kSCPropNetIPv6PrefixLength);
	if (isA_CFArray(addresses) == NULL
	    || isA_CFArray(prefix_lengths) == NULL) {
	    goto done;
	}
	count = CFArrayGetCount(addresses);
	if (count == 0 || count != CFArrayGetCount(prefix_lengths)) {
	    goto done;
	}
    }
    else if (CFEqual(config_method, kSCValNetIPv6ConfigMethodAutomatic)
	     || CFEqual(config_method, kSCValNetIPv6ConfigMethodLinkLocal)) {
	/* no other parameters are required */
    }
    else {
	goto done;
    }
    is_valid = TRUE;

 done:
    return (is_valid);
}

STATIC ipconfig_status_t
create_service(IPConfigurationServiceRef service, mach_port_t server)
{
    CFDataRef			config_data;
    kern_return_t		kret;
    ipconfig_status_t		status = ipconfig_status_success_e;
    boolean_t			tried_to_delete = FALSE;
    void *			xml_data_ptr = NULL;
    int				xml_data_len = 0;

    config_data
	= CFPropertyListCreateData(NULL,
				   service->config_dict,
				   kCFPropertyListBinaryFormat_v1_0,
				   0,
				   NULL);
    xml_data_ptr = (void *)CFDataGetBytePtr(config_data);
    xml_data_len = (int)CFDataGetLength(config_data);
    while (1) {
	kret = ipconfig_add_service(server, service->ifname,
				    xml_data_ptr, xml_data_len,
				    service->service_id,
				    &status);
	if (kret != KERN_SUCCESS) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_add_service(%s) failed, %s",
			  service->ifname, mach_error_string(kret));
	    break;
	}
	if (status == ipconfig_status_success_e) {
	    break;
	}
	if (status != ipconfig_status_duplicate_service_e
	    || tried_to_delete) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_add_service(%s) failed: %s",
			  service->ifname, ipconfig_status_string(status));
	    break;
	}
	tried_to_delete = TRUE;
	(void)ipconfig_remove_service(server, service->ifname,
				      xml_data_ptr, xml_data_len,
				      &status);
    }
    CFRelease(config_data);
    return (status);
}

STATIC void
remove_clear_state(IPConfigurationServiceRef service)
{
    CFDictionaryRef	options;

    options = CFDictionaryGetValue(service->config_dict,
				   kIPConfigurationServiceOptions);
    if (options != NULL
	&& CFDictionaryContainsKey(options,
				   _kIPConfigurationServiceOptionClearState)) {
	CFMutableDictionaryRef	config_dict;
	CFMutableDictionaryRef	new_options;

	new_options = CFDictionaryCreateMutableCopy(NULL, 0, options);
	CFDictionaryRemoveValue(new_options,
				_kIPConfigurationServiceOptionClearState);
	config_dict
	     = CFDictionaryCreateMutableCopy(NULL, 0, service->config_dict);
	CFDictionarySetValue(config_dict,
			     kIPConfigurationServiceOptions,
			     new_options);
	CFRelease(new_options);
	CFRelease(service->config_dict);
	service->config_dict = config_dict;
    }
    return;
}

STATIC void
store_reconnect(SCDynamicStoreRef store, void * info)
{
    IPConfigurationServiceRef	service;
    ObjectWrapperRef		wrapper = (ObjectWrapperRef)info;

    service = (IPConfigurationServiceRef)(wrapper->obj);
    if (service == NULL) {
	/* service has been deallocated */
	return;
    }

    /* not ready yet */
    if (service->server == MACH_PORT_NULL) {
	service->need_reconnect = TRUE;
	return;
    }

    /* on re-connect, make sure we don't clear the interface state */
    remove_clear_state(service);
    IPConfigLog(LOG_NOTICE,
		"IPConfigurationService: re-establishing service over %s",
		service->ifname);
    (void)create_service(service, service->server);
    return;
}

STATIC void
store_handle_changes(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    /* not used */
    return;
}

STATIC Boolean
store_init(IPConfigurationServiceRef service, dispatch_queue_t queue)
{
    SCDynamicStoreContext	context = {
	.version = 0,
	.info = NULL,
	.retain = ObjectWrapperRetain,
	.release = ObjectWrapperRelease,
	.copyDescription = NULL
    };
    SCDynamicStoreRef		store;
    ObjectWrapperRef			wrapper;

    wrapper = ObjectWrapperAlloc(service);
    context.info = wrapper;
    store = SCDynamicStoreCreate(NULL,
				 CFSTR("IPConfigurationService"),
				 store_handle_changes,
				 &context);
    if (store == NULL) {
	IPConfigLogFL(LOG_NOTICE,
		      "SCDynamicStoreCreate failed");
    }
    else if (!SCDynamicStoreSetDisconnectCallBack(store,
						  store_reconnect)) {
	IPConfigLogFL(LOG_NOTICE,
		      "SCDynamicStoreSetDisconnectCallBack failed");
    }
    else if (!SCDynamicStoreSetDispatchQueue(store, queue)) {
	IPConfigLogFL(LOG_NOTICE,
		      "SCDynamicStoreSetDispatchQueue failed");
    }
    else {
	service->store = store;
	service->wrapper = wrapper;
    }
    if (service->store == NULL) {
	if (wrapper != NULL) {
	    ObjectWrapperRelease(wrapper);
	}
	my_CFRelease(&store);
    }
    return (service->store != NULL);
}

STATIC void
IPConfigurationServiceSetServiceID(IPConfigurationServiceRef service,
				   CFStringRef serviceID,
				   Boolean no_publish)
{
    if (no_publish) {
	service->store_key = IPConfigurationServiceKey(serviceID);
    }
    else {
	service->store_key =
	    SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							kSCDynamicStoreDomainState,
							serviceID,
							kSCEntNetIPv6);
    }
    ServiceIDInitWithCFString(service->service_id, serviceID);
    return;
}

/**
 ** IPConfigurationService APIs
 **/

CFTypeID
IPConfigurationServiceGetTypeID(void)
{
    __IPConfigurationServiceRegisterClass();
    return (__kIPConfigurationServiceTypeID);
}

STATIC void
init_log(void)
{
    static os_log_t handle;

    if (handle == NULL) {
	handle = os_log_create(kIPConfigurationLogSubsystem,
			       kIPConfigurationLogCategoryLibrary);
	IPConfigLogSetHandle(handle);
    }
    return;
}

IPConfigurationServiceRef
IPConfigurationServiceCreate(CFStringRef interface_name, 
			     CFDictionaryRef options)
{
    CFStringRef			apn_name = NULL;
    CFStringRef			ipv6_ll = NULL;
    CFBooleanRef		enable_dad = NULL;
    CFBooleanRef		enable_clat46 = NULL;
    kern_return_t		kret;
    CFNumberRef			mtu = NULL;
    Boolean			no_publish = TRUE;
    CFBooleanRef		no_publish_cf = NULL;
    CFBooleanRef		perform_nud = NULL;
    CFDictionaryRef		requested_ipv6_config = NULL;
    mach_port_t			server = MACH_PORT_NULL;
    IPConfigurationServiceRef	ret_service = NULL;
    IPConfigurationServiceRef	service;
    CFStringRef			serviceID;
    ipconfig_status_t		status = ipconfig_status_success_e;

    init_log();
    kret = ipconfig_server_port(&server);
    if (kret != BOOTSTRAP_SUCCESS) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_server_port, %s",
		      mach_error_string(kret));
	return (NULL);
    }

    /* allocate/return an IPConfigurationServiceRef */
    service = __IPConfigurationServiceAllocate(NULL);
    if (service == NULL) {
	goto done;

    }

    /* remember the interface name */
    InterfaceNameInitWithCFString(service->ifname, interface_name);

    /* create the configuration, encapsulated as XML plist data */
    if (options != NULL) {
	struct in6_addr		ip;

	no_publish_cf
	    = CFDictionaryGetValue(options,
				   _kIPConfigurationServiceOptionNoPublish);
	if (no_publish_cf != NULL) {
	    if (isA_CFBoolean(no_publish_cf) == NULL) {
		IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			      _kIPConfigurationServiceOptionNoPublish);
		no_publish_cf = NULL;
	    }
	    else {
		no_publish = CFBooleanGetValue(no_publish_cf);
	    }
	}
	mtu = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionMTU);
	if (mtu != NULL && isA_CFNumber(mtu) == NULL) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionMTU);
	    mtu = NULL;
	}
	perform_nud 
	    = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionPerformNUD);
	if (perform_nud != NULL && isA_CFBoolean(perform_nud) == NULL) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionPerformNUD);
	    perform_nud = NULL;
	}
	enable_dad
	    = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionEnableDAD);
	if (enable_dad != NULL && isA_CFBoolean(enable_dad) == NULL) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionEnableDAD);
	    enable_dad = NULL;
	}

	enable_clat46
	    = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionEnableCLAT46);
	if (enable_clat46 != NULL && isA_CFBoolean(enable_clat46) == NULL) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionEnableCLAT46);
	    enable_clat46 = NULL;
	}

	requested_ipv6_config
	    = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionIPv6Entity);
	if (requested_ipv6_config != NULL
	    && ipv6_config_is_valid(requested_ipv6_config) == FALSE) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionIPv6Entity);
	    requested_ipv6_config = NULL;
	}
	ipv6_ll
	    = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionIPv6LinkLocalAddress);
	if (ipv6_ll != NULL
	    && (my_CFStringToIPv6Address(ipv6_ll, &ip) == FALSE
		|| IN6_IS_ADDR_LINKLOCAL(&ip) == FALSE)) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionIPv6LinkLocalAddress);
	    ipv6_ll = NULL;
	}
	apn_name = CFDictionaryGetValue(options,
					kIPConfigurationServiceOptionAPNName);
	if (apn_name != NULL
	    && isA_CFString(apn_name) == NULL) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionAPNName);
	    apn_name = NULL;
	}
    }
    serviceID = my_CFUUIDStringCreate(NULL);
    service->config_dict = config_dict_create(serviceID,
					      requested_ipv6_config,
					      no_publish_cf,
					      mtu, perform_nud, ipv6_ll,
					      enable_dad, apn_name, enable_clat46);

    /* monitor for configd restart */
    service->queue = dispatch_queue_create("IPConfigurationService", NULL);
    if (service->queue == NULL) {
	IPConfigLogFL(LOG_NOTICE,
		      "dispatch_queue_create failed");
	goto done;
    }
    if (!store_init(service, service->queue)) {
	goto done;
    }

    /* create the service in IPConfiguration */
    status = create_service(service, server);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    IPConfigurationServiceSetServiceID(service, serviceID, no_publish);
    dispatch_sync(service->queue,
		  ^{
		      service->server = server;
		      if (service->need_reconnect) {
			  (void)create_service(service, server);
		      }
		  });
    server = MACH_PORT_NULL;
    ret_service = service;
    service = NULL;

 done:
    if (server != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), server);
    }
    my_CFRelease(&serviceID);
    my_CFRelease(&service);
    return (ret_service);
}

/*
 * Function: IPConfigurationServiceGetNotificationKey
 *
 * Purpose:
 *   Return the SCDynamicStoreKeyRef used to monitor the service using
 *   SCDynamicStoreSetNotificationKeys().
 *
 * Parameters:
 *   service			: the service to monitor
 */
CFStringRef
IPConfigurationServiceGetNotificationKey(IPConfigurationServiceRef service)
{
    return (service->store_key);
}

CFDictionaryRef
IPConfigurationServiceCopyInformation(IPConfigurationServiceRef service)
{
    CFDictionaryRef		info;

    info = SCDynamicStoreCopyValue(service->store, service->store_key);
    if (info != NULL
	&& isA_CFDictionary(info) == NULL) {
	my_CFRelease(&info);
    }
    return (info);
}

void
IPConfigurationServiceRefreshConfiguration(IPConfigurationServiceRef service)
{
    kern_return_t		kret;
    ipconfig_status_t		status;

    kret = ipconfig_refresh_service(service->server,
				    service->ifname,
				    service->service_id,
				    &status);
    if (kret != KERN_SUCCESS) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_refresh_service(%s %s) failed, %s",
		      service->ifname, service->service_id,
		      mach_error_string(kret));
    }
    else if (status != ipconfig_status_success_e) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_refresh_service(%s %s) failed: %s",
		      service->ifname, service->service_id,
		      ipconfig_status_string(status));
    }
    return;
}
