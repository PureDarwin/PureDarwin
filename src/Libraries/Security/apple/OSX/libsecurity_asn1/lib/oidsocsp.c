/*
 * Copyright (c) 2009-2010,2012 Apple Inc. All Rights Reserved.
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
 File:      oidsocsp.cpp

 Contains:  Object Identifiers for OCSP
 */

#include <Security/oidsbase.h>
#include "oidsocsp.h"

SECASN1OID_DEF(OID_PKIX_OCSP,                   OID_AD_OCSP);
SECASN1OID_DEF(OID_PKIX_OCSP_BASIC,             OID_AD_OCSP, 1);
SECASN1OID_DEF(OID_PKIX_OCSP_NONCE,             OID_AD_OCSP, 2);
SECASN1OID_DEF(OID_PKIX_OCSP_CRL,               OID_AD_OCSP, 3);
SECASN1OID_DEF(OID_PKIX_OCSP_RESPONSE,          OID_AD_OCSP, 4);
SECASN1OID_DEF(OID_PKIX_OCSP_NOCHECK,           OID_AD_OCSP, 5);
SECASN1OID_DEF(OID_PKIX_OCSP_ARCHIVE_CUTOFF,    OID_AD_OCSP, 6);
SECASN1OID_DEF(OID_PKIX_OCSP_SERVICE_LOCATOR,   OID_AD_OCSP, 7);

SECASN1OID_DEF(OID_GOOGLE_OCSP_SCT, GOOGLE_OCSP_SCT_OID);
