/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef __PPPCONTROLLERPRIV_H__
#define __PPPCONTROLLERPRIV_H__

/*
 * Keys have moved to SystemConfiguration Framework
 *  SCSchemaDefinitions.h and SCSchemaDefinitionsPrivate.h 
 */

/* IPSec error codes */
enum {
	IPSEC_NO_ERROR = 0,
	IPSEC_GENERIC_ERROR = 1,
	IPSEC_NOSERVERADDRESS_ERROR = 2,
	IPSEC_NOSHAREDSECRET_ERROR = 3,
	IPSEC_NOCERTIFICATE_ERROR = 4,
	IPSEC_RESOLVEADDRESS_ERROR = 5,
	IPSEC_NOLOCALNETWORK_ERROR = 6,
	IPSEC_CONFIGURATION_ERROR = 7,
	IPSEC_RACOONCONTROL_ERROR = 8,
	IPSEC_CONNECTION_ERROR = 9,
	IPSEC_NEGOTIATION_ERROR = 10,
	IPSEC_SHAREDSECRET_ERROR = 11,
	IPSEC_SERVER_CERTIFICATE_ERROR = 12,
	IPSEC_CLIENT_CERTIFICATE_ERROR = 13,
	IPSEC_XAUTH_ERROR = 14,
	IPSEC_NETWORKCHANGE_ERROR = 15,
	IPSEC_PEERDISCONNECT_ERROR = 16,
	IPSEC_PEERDEADETECTION_ERROR = 17,
	IPSEC_EDGE_ACTIVATION_ERROR = 18,
	IPSEC_IDLETIMEOUT_ERROR = 19,
	IPSEC_CLIENT_CERTIFICATE_PREMATURE = 20,
	IPSEC_CLIENT_CERTIFICATE_EXPIRED = 21,
	IPSEC_SERVER_CERTIFICATE_PREMATURE = 22,
	IPSEC_SERVER_CERTIFICATE_EXPIRED = 23,
	IPSEC_SERVER_CERTIFICATE_INVALID_ID = 24
};

/* 
 * IPSEC state
 *
 */
enum {
    IPSEC_IDLE = 0,
    IPSEC_INITIALIZE,
    IPSEC_CONTACT,
    IPSEC_PHASE1,
    IPSEC_PHASE1AUTH,
    IPSEC_PHASE2,
    IPSEC_RUNNING,
    IPSEC_TERMINATE,
    IPSEC_WAITING
};



#endif


