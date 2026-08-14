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

#ifndef SecProtocolTypesPriv_h
#define SecProtocolTypesPriv_h

#include <Security/SecProtocolTypes.h>

__BEGIN_DECLS

SEC_ASSUME_NONNULL_BEGIN

/*!
 * @function sec_identity_create_with_certificates_and_external_private_key
 *
 * @abstract
 *      Create an ARC-able `sec_identity_t` instance from an array of `SecCertificateRef`
 *      instances and blocks to be invoked for private key opertions. Callers may use this
 *      constructor to build a `sec_identity_t` instance with an external private key.
 *
 * @param certificates
 *      An array of `SecCertificateRef` instances.
 *
 * @param sign_block
 *      A `sec_protocol_private_key_sign_t` block.
 *
 * @param decrypt_block
 *      A `sec_protocol_private_key_decrypt_t` block.
 *
 * @param operation_queue
 *      The `dispatch_queue_t` queue on which each private key operation is invoked.
 *
 * @return a `sec_identity_t` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable sec_identity_t
sec_identity_create_with_certificates_and_external_private_key(CFArrayRef certificates,
                                                               sec_protocol_private_key_sign_t sign_block,
                                                               sec_protocol_private_key_decrypt_t decrypt_block,
                                                               dispatch_queue_t operation_queue);

/*!
 * @function sec_identity_copy_private_key_sign_block
 *
 * @abstract
 *      Copy a retained reference to the underlying `sec_protocol_private_key_sign_t` used by the identity.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `sec_protocol_private_key_sign_t` block, or nil.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable sec_protocol_private_key_sign_t
sec_identity_copy_private_key_sign_block(sec_identity_t identity);

/*!
 * @function sec_identity_copy_private_key_decrypt_block
 *
 * @abstract
 *      Copy a retained reference to the underlying `sec_protocol_private_key_decrypt_t` used by the identity.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `sec_protocol_private_key_decrypt_t` block, or nil.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable sec_protocol_private_key_decrypt_t
sec_identity_copy_private_key_decrypt_block(sec_identity_t identity);

/*!
 * @function sec_identity_copy_private_key_queue
 *
 * @abstract
 *      Copy a retained reference to the `dispatch_queue_t` to be used by external private key
 *      operations, if any.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `dispatch_queue_t` queue, or nil.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable dispatch_queue_t
sec_identity_copy_private_key_queue(sec_identity_t identity);

/*!
 * @function sec_identity_has_certificates
 *
 * @abstract
 *      Determine if the `sec_identity_t` has a list of certificates associated with it.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return True if the identity has certificates associated with it, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_identity_has_certificates(sec_identity_t identity);

SEC_ASSUME_NONNULL_END

__END_DECLS

#endif // SecProtocolTypesPriv_h
