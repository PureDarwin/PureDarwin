/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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



#ifndef __DYLD_POINTER_AUTH_H__
#define __DYLD_POINTER_AUTH_H__

#include <ptrauth.h>

#if __has_feature(ptrauth_calls)
#define __ptrauth_dyld_address_auth __ptrauth(ptrauth_key_process_dependent_data, 1, 0)
#define __ptrauth_dyld_function_ptr __ptrauth(ptrauth_key_process_dependent_code, 1, 0)
#else
#define __ptrauth_dyld_address_auth
#define __ptrauth_dyld_function_ptr
#endif

namespace dyld3 {

// On arm64e, signs the given pointer with the address of where it is stored.
// Other archs just have a regular pointer
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wptrauth-null-pointers"
template<typename T>
struct AuthenticatedValue {
    static_assert(sizeof(T) <= sizeof(uintptr_t));

    AuthenticatedValue() {
        this->value = ptrauth_sign_unauthenticated(nullptr, ptrauth_key_process_dependent_data, this);
    }
    ~AuthenticatedValue() = default;
    AuthenticatedValue(const AuthenticatedValue& other) {
        this->value = ptrauth_auth_and_resign(other.value,
                                              ptrauth_key_process_dependent_data, &other,
                                              ptrauth_key_process_dependent_data, this);
    }
    AuthenticatedValue(AuthenticatedValue&& other) {
        this->value = ptrauth_auth_and_resign(other.value,
                                              ptrauth_key_process_dependent_data, &other,
                                              ptrauth_key_process_dependent_data, this);
        other.value = ptrauth_sign_unauthenticated(nullptr, ptrauth_key_process_dependent_data, &other);
    }

    AuthenticatedValue& operator=(const AuthenticatedValue&) = delete;
    AuthenticatedValue& operator=(AuthenticatedValue&&) = delete;

    // Add a few convenience methods for interoperating with values of the given type
    AuthenticatedValue& operator=(const T& other) {
        this->value = ptrauth_sign_unauthenticated(other, ptrauth_key_process_dependent_data, this);
        return *this;
    }
    bool operator==(const T& other) const {
        return ptrauth_auth_data(this->value, ptrauth_key_process_dependent_data, this) == other;
    }
    bool operator!=(const T& other) const {
        return ptrauth_auth_data(this->value, ptrauth_key_process_dependent_data, this) != other;
    }

private:
    const void* value;
};


#pragma clang diagnostic pop

} // namespace dyld3

#endif // __DYLD_POINTER_AUTH_H__

