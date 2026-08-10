/*
 * Copyright (c) 2003-2007,2013 Apple Inc. All Rights Reserved.
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
 *
 * keychain_find.h
 */

#ifndef _KEYCHAIN_FINDINTERNETPASSWORD_H_
#define _KEYCHAIN_FINDINTERNETPASSWORD_H_  1

#ifdef __cplusplus
extern "C" {
#endif

extern int keychain_item(int argc, char * const *argv);

extern int keychain_find_internet_password(int argc, char * const *argv);

extern int keychain_find_generic_password(int argc, char * const *argv);

extern int keychain_find_certificate(int argc, char * const *argv);

extern int keychain_delete_internet_password(int argc, char * const *argv);

extern int keychain_delete_generic_password(int argc, char * const *argv);

extern int keychain_dump(int argc, char * const *argv);

extern int keychain_show_certificates(int argc, char * const *argv);

#ifdef __cplusplus
}
#endif

#endif /* _KEYCHAIN_FINDINTERNETPASSWORD_H_ */
