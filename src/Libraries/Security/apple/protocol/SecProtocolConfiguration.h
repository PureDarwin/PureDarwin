/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#ifndef SecProtocolConfiguration_h
#define SecProtocolConfiguration_h

#include <Security/SecProtocolObject.h>
#include <Security/SecureTransport.h>

#include <dispatch/dispatch.h>
#include <xpc/xpc.h>

#ifndef SEC_OBJECT_IMPL
/*!
 * A `sec_protocol_configuration` is an object that encapsulates App Transport Security
 * information and vends `sec_protocol_options` to clients for creating new connections.
 * It may also be queried to determine for what domains TLS is required.
 */
SEC_OBJECT_DECL(sec_protocol_configuration);
#endif // !SEC_OBJECT_IMPL

__BEGIN_DECLS

SEC_ASSUME_NONNULL_BEGIN

/*!
 * @function sec_protocol_configuration_copy_singleton
 *
 * @abstract
 *      Copy the per-process `sec_protocol_configuration_t` object.
 *
 * @return A non-nil `sec_protocol_configuration_t` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED sec_protocol_configuration_t
sec_protocol_configuration_copy_singleton(void);

/*!
 * @function sec_protocol_configuration_set_ats_overrides
 *
 * @abstract
 *      Set ATS overrides
 *
 * @param config
 *      A `sec_protocol_configuration_t` instance.
 *
 * @param override_dictionary
 *      A `CFDictionaryRef` dictionary containing the ATS overrides as
 *      documented here: https://developer.apple.com/library/archive/documentation/General/Reference/InfoPlistKeyReference/Articles/CocoaKeys.html#//apple_ref/doc/uid/TP40009251-SW33
 *
 * @return True if successful, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_configuration_set_ats_overrides(sec_protocol_configuration_t config, CFDictionaryRef override_dictionary);

/*!
 * @function sec_protocol_configuration_copy_transformed_options
 *
 * @abstract
 *      Transform an existing `sec_protocol_options_t` instance with a `sec_protocol_configuration_t` instance.
 *
 * @param config
 *      A `sec_protocol_configuration_t` instance.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @return The transformed `sec_protocol_options` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED __nullable sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options(sec_protocol_configuration_t config, sec_protocol_options_t options);

/*!
 * @function sec_protocol_configuration_copy_transformed_options_for_host
 *
 * @abstract
 *      Transform an existing `sec_protocol_options_t` instance with a `sec_protocol_configuration_t` instance
 *      using a specific host endpoint. Note that the service (port) is omitted from this formula.
 *
 * @param config
 *      A `sec_protocol_configuration_t` instance.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param host
 *      A NULL-terminated C string containing the host in question.
 *
 * @return The transformed `sec_protocol_options` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED __nullable sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options_for_host(sec_protocol_configuration_t config, sec_protocol_options_t options, const char *host);

/*!
 * @function sec_protocol_configuration_tls_required
 *
 * @abstract
 *      Determine if TLS is required by policy for a generic connection. Note that the service (port) is omitted
 *      from this formula.
 *
 * @param config
 *      A `sec_protocol_configuration_t` instance.
 *
 * @return True if connections require TLS, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_configuration_tls_required(sec_protocol_configuration_t config);

/*!
 * @function sec_protocol_configuration_tls_required_for_host
 *
 * @abstract
 *      Determine if TLS is required -- by policy -- for the given host endpoint. Note that the service (port) is
 *      omitted from this formula.
 *
 * @param config
 *      A `sec_protocol_configuration_t` instance.
 *
 * @param host
 *      A NULL-terminated C string containing the host endpoint to examine.
 *
 * @param is_direct
 *      A flag which indicates if the given hostname is local (direct).
 *
 * @return True if connections to the endpoint require TLS, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_configuration_tls_required_for_host(sec_protocol_configuration_t config, const char *host, bool is_direct);

/*!
 * @function sec_protocol_configuration_tls_required_for_address
 *
 * @abstract
 *      Determine if TLS is required -- by policy -- for the given address endpoint.
 *
 * @param config
 *      A `sec_protocol_configuration_t` instance.
 *
 * @param address
 *      A NULL-terminated C string containing the address endpoint to examine.
 *
 * @return True if connections to the endpoint require TLS, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_protocol_configuration_tls_required_for_address(sec_protocol_configuration_t config, const char *address);

SEC_ASSUME_NONNULL_END

__END_DECLS

#endif // SecProtocolConfiguration_h
