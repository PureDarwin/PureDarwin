/*
 * Copyright (c) 2000-2001,2004,2011,2014 Apple Inc. All Rights Reserved.
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


//
// cspattachment - cryptographic plugin attachment type
//
#ifndef _H_CSPATTACHMENT
#define _H_CSPATTACHMENT

#include "attachment.h"
#include <Security/cssmcspi.h>

#ifdef _CPP_CSPATTACHMENT
# pragma export on
#endif


typedef StandardAttachment<CSSM_SERVICE_CSP, CSSM_SPI_CSP_FUNCS> CSPAttachment;

#ifdef _CPP_CSPATTACHMENT
# pragma export off
#endif

#endif  //_H_CSPATTACHMENT
