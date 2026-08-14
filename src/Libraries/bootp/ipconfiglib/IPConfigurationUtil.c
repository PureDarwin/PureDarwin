/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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
 * IPConfigurationUtil.c
 * - API to communicate with IPConfiguration agent to perform various tasks
 */

/* 
 * Modification History
 *
 * March 29, 2018 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CoreFoundation.h>
#include "IPConfigurationUtil.h"
#include "IPConfigurationUtilPrivate.h"
#include "ipconfig_types.h"
#include "ipconfig_ext.h"
#include "ipconfig.h"
#include "cfutil.h"
#include "IPConfigurationLog.h"
#include "symbol_scope.h"

STATIC CFDictionaryRef
create_network_dict(CFStringRef ssid)
{
    const void *	key = kIPConfigurationForgetNetworkSSID;
    const void *	value = ssid;

    return (CFDictionaryCreate(NULL, &key, &value, 1,
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks));
}

Boolean
IPConfigurationForgetNetwork(CFStringRef interface_name, CFStringRef ssid)
{
    InterfaceName		ifname;
    kern_return_t		kret;
    CFDictionaryRef		network_dict;
    CFDataRef			network_data;
    mach_port_t			server = MACH_PORT_NULL;
    ipconfig_status_t		status;
    Boolean			success = FALSE;
    void *			xml_data_ptr = NULL;
    int				xml_data_len = 0;

    if (interface_name == NULL || ssid == NULL) {
	return (FALSE);
    }
    kret = ipconfig_server_port(&server);
    if (kret != BOOTSTRAP_SUCCESS) {
	IPConfigLog(LOG_NOTICE,
		    "ipconfig_server_port, %s",
		    mach_error_string(kret));
	return (FALSE);
    }
    InterfaceNameInitWithCFString(ifname, interface_name);
    network_dict = create_network_dict(ssid);
    network_data
	= CFPropertyListCreateData(NULL,
				   network_dict,
				   kCFPropertyListBinaryFormat_v1_0,
				   0,
				   NULL);
    CFRelease(network_dict);
    xml_data_ptr = (void *)CFDataGetBytePtr(network_data);
    xml_data_len = (int)CFDataGetLength(network_data);
    kret = ipconfig_forget_network(server, ifname,
				   xml_data_ptr, xml_data_len,
				   &status);
    CFRelease(network_data);
    if (kret != KERN_SUCCESS) {
	IPConfigLog(LOG_NOTICE,
		    "ipconfig_forget_network(%s) failed, %s",
		    ifname, mach_error_string(kret));
    }
    else if (status != ipconfig_status_success_e) {
	IPConfigLog(LOG_NOTICE,
		    "ipconfig_forget_network(%s) failed, %s",
		    ifname, ipconfig_status_string(status));
    }
    else {
	IPConfigLog(LOG_NOTICE,
		    "ipconfig_forget_network(%s) succeeded", ifname);
	success = TRUE;
    }
    return (success);
}
