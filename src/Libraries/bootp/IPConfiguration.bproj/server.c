/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <syslog.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPropertyList.h>
#include <SystemConfiguration/SCPrivate.h>
#include <bsm/libbsm.h>
#include <TargetConditionals.h>

#include "symbol_scope.h"
#include "cfutil.h"
#include "ipconfigServer.h"
#include "ipconfigd.h"
#include "ipconfig_ext.h"
#include "globals.h"
#include "IPConfigurationUtilPrivate.h"

static uid_t S_uid = -1;
static pid_t S_pid = -1;


#if TARGET_OS_OSX
#define	kIPConfigurationServiceEntitlement	NULL

static boolean_t
S_has_entitlement(audit_token_t token, CFStringRef entitlement)
{
    return (FALSE);
}

#else /* TARGET_OS_OSX */
#define	kIPConfigurationServiceEntitlement	CFSTR("com.apple.IPConfigurationService")	/* boolean */

#include <Security/SecTask.h>
static boolean_t
S_has_entitlement(audit_token_t token, CFStringRef entitlement)
{
    boolean_t		ret = FALSE;
    SecTaskRef		task;

    task = SecTaskCreateWithAuditToken(NULL, token);
    if (task != NULL) {
	CFBooleanRef	allow;

	allow = SecTaskCopyValueForEntitlement(task, entitlement, NULL);
	if (allow != NULL) {
	    if (isA_CFBoolean(allow) != NULL) {
		ret = CFBooleanGetValue(allow);
	    }
	    CFRelease(allow);
	}
	CFRelease(task);
    }
    return (ret);
}
#endif /* TARGET_OS_OSX */

static void
S_process_audit_token(audit_token_t audit_token)
{
    S_uid = -1;
    S_pid = -1;
    audit_token_to_au32(audit_token,
			NULL,		/* auidp */
			&S_uid,		/* euid */
			NULL,		/* egid */
			NULL,		/* ruid */
			NULL,		/* rgid */
			&S_pid,		/* pid */
			NULL,		/* asid */
			NULL);		/* tid */
    return;
}

static boolean_t
S_IPConfigurationServiceOperationAllowed(audit_token_t audit_token)
{
    S_process_audit_token(audit_token);
    if (S_uid == 0
	|| S_has_entitlement(audit_token,
			     kIPConfigurationServiceEntitlement)) {
	return (TRUE);
    }
    return (FALSE);
}

static ipconfig_status_t
method_info_from_xml_data(xmlData_t xml_data,
			  mach_msg_type_number_t xml_data_len,
			  ipconfig_method_info_t info)
{
    CFPropertyListRef	plist = NULL;
    ipconfig_status_t	status;

    if (xml_data != NULL) {
	plist = my_CFPropertyListCreateWithBytePtrAndLength(xml_data,
							    xml_data_len);
    }
    status = ipconfig_method_info_from_plist(plist, info);
    if (plist != NULL) {
	CFRelease(plist);
    }
    return (status);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_if_addr(mach_port_t p, InterfaceName name,
		  u_int32_t * addr, ipconfig_status_t * status)
{
    *status = get_if_addr(InterfaceNameNulTerminate(name), addr);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_if_count(mach_port_t p, int * count)
{
    *count = get_if_count();
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_option(mach_port_t p, InterfaceName name, int option_code,
		     dataOut_t * option_data,
		     mach_msg_type_number_t * option_dataCnt,
		     ipconfig_status_t * status)

{
    *status = get_if_option(InterfaceNameNulTerminate(name), option_code,
			    option_data, option_dataCnt);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_packet(mach_port_t p, InterfaceName name,
		     dataOut_t * packet_data,
		     mach_msg_type_number_t * packet_dataCnt,
		     ipconfig_status_t * status)
{
    *status = get_if_packet(InterfaceNameNulTerminate(name),
			    packet_data, packet_dataCnt);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_v6_packet(mach_port_t p, InterfaceName name,
			dataOut_t * packet_data,
			mach_msg_type_number_t * packet_dataCnt,
			ipconfig_status_t * status)
{
    *status = get_if_v6_packet(InterfaceNameNulTerminate(name),
			       packet_data, packet_dataCnt);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_set(mach_port_t p, InterfaceName name,
	      xmlData_t xml_data,
	      mach_msg_type_number_t xml_data_len,
	      ipconfig_status_t * ret_status,
	      audit_token_t audit_token)
{
    ipconfig_method_info	info;
    ipconfig_status_t		status;

    S_process_audit_token(audit_token);
    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    ipconfig_method_info_init(&info);
    status = method_info_from_xml_data(xml_data, xml_data_len, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = set_if(InterfaceNameNulTerminate(name), &info);
    ipconfig_method_info_free(&info);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_set_verbose(mach_port_t p, int verbose,
		      ipconfig_status_t * status,
		      audit_token_t audit_token)
{
    *status = ipconfig_status_permission_denied_e;
    return (KERN_SUCCESS);
}

#ifdef IPCONFIG_TEST_NO_ENTRY
PRIVATE_EXTERN kern_return_t
_ipconfig_set_something(mach_port_t p, int verbose,
			ipconfig_status_t * status)
{
    return (KERN_SUCCESS);
}
#endif /* IPCONFIG_TEST_NO_ENTRY */

PRIVATE_EXTERN kern_return_t
_ipconfig_add_service(mach_port_t p, 
		      InterfaceName name,
		      xmlData_t xml_data,
		      mach_msg_type_number_t xml_data_len,
		      ServiceID service_id,
		      ipconfig_status_t * ret_status,
		      audit_token_t audit_token)
{
    ipconfig_method_info	info;
    CFPropertyListRef		plist = NULL;
    ipconfig_status_t		status;

    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    if (xml_data != NULL) {
	plist = my_CFPropertyListCreateWithBytePtrAndLength(xml_data,
							    xml_data_len);
    }
    ipconfig_method_info_init(&info);
    status = ipconfig_method_info_from_plist(plist, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    ServiceIDClear(service_id);
    status = add_service(InterfaceNameNulTerminate(name),
			 &info, service_id, plist, S_pid);
    ipconfig_method_info_free(&info);

 done:
    if (plist != NULL) {
	CFRelease(plist);
    }
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_set_service(mach_port_t p, 
		      InterfaceName name,
		      xmlData_t xml_data,
		      mach_msg_type_number_t xml_data_len,
		      ServiceID service_id,
		      ipconfig_status_t * ret_status,
		      audit_token_t audit_token)
{
    ipconfig_method_info	info;
    ipconfig_status_t		status;

    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    ipconfig_method_info_init(&info);
    status = method_info_from_xml_data(xml_data, xml_data_len, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    ServiceIDClear(service_id);
    status = set_service(InterfaceNameNulTerminate(name), &info, service_id);
    ipconfig_method_info_free(&info);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t 
_ipconfig_remove_service_on_interface(mach_port_t server,
				      InterfaceName name,
				      ServiceID service_id,
				      ipconfig_status_t * ret_status,
				      audit_token_t audit_token)
{
    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	*ret_status = ipconfig_status_permission_denied_e;
    }
    else {
	char *	ifname;

	if (name[0] == '\0') {
	    ifname = NULL;
	}
	else {
	    ifname = InterfaceNameNulTerminate(name);
	}
	*ret_status = remove_service_with_id(ifname, service_id);
    }
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t 
_ipconfig_find_service(mach_port_t server,
		       InterfaceName name,
		       boolean_t exact,
		       xmlData_t xml_data,
		       mach_msg_type_number_t xml_data_len,
		       ServiceID service_id,
		       ipconfig_status_t * ret_status)
{
    ipconfig_method_info	info;
    ipconfig_status_t		status;

    ServiceIDClear(service_id);
    ipconfig_method_info_init(&info);
    status = method_info_from_xml_data(xml_data, xml_data_len, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = find_service(InterfaceNameNulTerminate(name),
			  exact, &info, service_id);
    ipconfig_method_info_free(&info);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t 
_ipconfig_remove_service(mach_port_t server,
			 InterfaceName name,
			 xmlData_t xml_data,
			 mach_msg_type_number_t xml_data_len,
			 ipconfig_status_t * ret_status,
			 audit_token_t audit_token)
{
    ipconfig_method_info	info;
    ipconfig_status_t		status;

    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    ipconfig_method_info_init(&info);
    status = method_info_from_xml_data(xml_data, xml_data_len, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = remove_service(InterfaceNameNulTerminate(name), &info);
    ipconfig_method_info_free(&info);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t 
_ipconfig_refresh_service(mach_port_t server,
			  InterfaceName name,
			  ServiceID service_id,
			  ipconfig_status_t * ret_status,
			  audit_token_t audit_token)
{
    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	*ret_status = ipconfig_status_permission_denied_e;
    }
    else {
	*ret_status = refresh_service(InterfaceNameNulTerminate(name),
				      service_id);
    }
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_forget_network(mach_port_t server,
			 InterfaceName name,
			 xmlData_t xml_data,
			 mach_msg_type_number_t xml_data_len,
			 ipconfig_status_t * ret_status,
			 audit_token_t audit_token)
{
    CFDictionaryRef		dict = NULL;
    ipconfig_status_t		status;
    CFStringRef			ssid = NULL;

    S_process_audit_token(audit_token);
    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    if (xml_data != NULL) {
	dict = my_CFPropertyListCreateWithBytePtrAndLength(xml_data,
							   xml_data_len);
	if (isA_CFDictionary(dict) != NULL) {
	    ssid = CFDictionaryGetValue(dict,
					kIPConfigurationForgetNetworkSSID);
	    ssid = isA_CFString(ssid);
	}
    }
    status = forget_network(InterfaceNameNulTerminate(name), ssid);
    my_CFRelease(&dict);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_ra(mach_port_t p, InterfaceName name,
		 xmlDataOut_t * ra_data,
		 mach_msg_type_number_t * ra_data_cnt,
		 ipconfig_status_t * status)
{
    *status = get_if_ra(InterfaceNameNulTerminate(name), ra_data, ra_data_cnt);
    return (KERN_SUCCESS);
}

static void
S_ipconfig_server(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    uint64_t 		reply_buf[(2048 + 256)/sizeof(uint64_t)];
    mach_msg_options_t 	options = 0;
    mig_reply_error_t * request = (mig_reply_error_t *)msg;
    mig_reply_error_t *	reply;
    mach_msg_return_t 	r = MACH_MSG_SUCCESS;

    if (_ipconfig_subsystem.maxsize > sizeof(reply_buf)) {
	syslog(LOG_NOTICE, "IPConfiguration server: %d > %ld",
	       _ipconfig_subsystem.maxsize, sizeof(reply_buf));
	reply = (mig_reply_error_t *)
	    malloc(_ipconfig_subsystem.maxsize);
    }
    else {
	reply = (mig_reply_error_t *)(void *)reply_buf;
    }
    if (ipconfig_server(&request->Head, &reply->Head) == FALSE) {
	my_log(LOG_DEBUG, "IPConfiguration: unknown message ID (%d) received",
	       request->Head.msgh_id);
    }

    /* Copied from Libc/mach/mach_msg.c:mach_msg_server_once(): Start */
    if (!(reply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
	if (reply->RetCode == MIG_NO_REPLY)
	    reply->Head.msgh_remote_port = MACH_PORT_NULL;
	else if ((reply->RetCode != KERN_SUCCESS) &&
		 (request->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
	    /* destroy the request - but not the reply port */
	    request->Head.msgh_remote_port = MACH_PORT_NULL;
	    mach_msg_destroy(&request->Head);
	}
    }
    /*
     *	We don't want to block indefinitely because the client
     *	isn't receiving messages from the reply port.
     *	If we have a send-once right for the reply port, then
     *	this isn't a concern because the send won't block.
     *	If we have a send right, we need to use MACH_SEND_TIMEOUT.
     *	To avoid falling off the kernel's fast RPC path unnecessarily,
     *	we only supply MACH_SEND_TIMEOUT when absolutely necessary.
     */
    if (reply->Head.msgh_remote_port != MACH_PORT_NULL) {
	r = mach_msg(&reply->Head,
		     (MACH_MSGH_BITS_REMOTE(reply->Head.msgh_bits) ==
		      MACH_MSG_TYPE_MOVE_SEND_ONCE) ?
		     MACH_SEND_MSG|options :
		     MACH_SEND_MSG|MACH_SEND_TIMEOUT|options,
		     reply->Head.msgh_size, 0, MACH_PORT_NULL,
		     MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if ((r != MACH_SEND_INVALID_DEST) &&
	    (r != MACH_SEND_TIMED_OUT))
	    goto done_once;
	r = MACH_MSG_SUCCESS;
    }
    if (reply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)
	mach_msg_destroy(&reply->Head);
 done_once:
    /* Copied from Libc/mach/mach_msg.c:mach_msg_server_once(): End */

    /* ALIGN: reply_buf is aligned to at least sizeof(uint64_t) bytes */
    if (reply != (mig_reply_error_t *)(void *)reply_buf) {
	free(reply);
    }

    if (r != MACH_MSG_SUCCESS) {
	my_log(LOG_DEBUG, "IPConfiguration msg_send: %s", mach_error_string(r));
    }
    return;
}

PRIVATE_EXTERN void
server_init()
{
    CFRunLoopSourceRef	rls;
    CFMachPortRef	ipconfigd_port;
    mach_port_t		server_port;
    kern_return_t 	status;

    status = bootstrap_check_in(bootstrap_port, IPCONFIG_SERVER, 
				&server_port);
    if (status != BOOTSTRAP_SUCCESS) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: bootstrap_check_in failed, %s",
	       mach_error_string(status));
	return;
    }
    ipconfigd_port = _SC_CFMachPortCreateWithPort(NULL, server_port,
						  S_ipconfig_server,
						  NULL);
    rls = CFMachPortCreateRunLoopSource(NULL, ipconfigd_port, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    CFRelease(ipconfigd_port);
    return;
}
