/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

/*!
 @header nameconstraints
 The functions provided in nameconstraints.h provide an interface to
 a name constraints implementation as specified in section 4.2.1.10 of rfc5280.
 */

#ifndef _SECURITY_NAMECONSTRAINTS_H_
#define _SECURITY_NAMECONSTRAINTS_H_

#include <stdbool.h>
#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>

OSStatus SecNameContraintsMatchSubtrees(SecCertificateRef certificate, CFArrayRef subtrees, bool *matched, bool permit);

void SecNameConstraintsIntersectSubtrees(CFMutableArrayRef subtrees_state, CFArrayRef subtrees_new);

#endif /* SECURITY_NAMECONSTRAINTS_H */
