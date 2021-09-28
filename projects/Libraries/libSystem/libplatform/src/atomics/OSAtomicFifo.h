//
//  OSAtomicFifo.h
//  libatomics
//
//  Created by Rokhini Prabhu on 4/7/20.
//

#ifndef _OS_ATOMIC_FIFO_QUEUE_
#define _OS_ATOMIC_FIFO_QUEUE_

#if defined(__arm64e__) && __has_feature(ptrauth_calls)
#include <ptrauth.h>

#define COMMPAGE_PFZ_BASE_AUTH_KEY ptrauth_key_process_independent_code
#define COMMPAGE_PFZ_FN_AUTH_KEY ptrauth_key_function_pointer
#define COMMPAGE_PFZ_BASE_DISCRIMINATOR ptrauth_string_discriminator("pfz")

#define COMMPAGE_PFZ_BASE_PTR __ptrauth(COMMPAGE_PFZ_BASE_AUTH_KEY, 1, COMMPAGE_PFZ_BASE_DISCRIMINATOR)

#define SIGN_PFZ_FUNCTION_PTR(ptr) ptrauth_sign_unauthenticated(ptr, COMMPAGE_PFZ_FN_AUTH_KEY, 0)

#else /* defined(__arm64e__) && __has_feature(ptrauth_calls) */

#define COMMPAGE_PFZ_BASE_AUTH_KEY 0
#define COMMPAGE_PFZ_FN_AUTH_KEY 0
#define COMMPAGE_PFZ_BASE_DISCRIMINATOR 0

#define COMMPAGE_PFZ_BASE_PTR

#define SIGN_PFZ_FUNCTION_PTR(ptr) ptr
#endif /* defined(__arm64e__) && __has_feature(ptrauth_calls) */

extern void *COMMPAGE_PFZ_BASE_PTR commpage_pfz_base;

#endif /* _OS_ATOMIC_FIFO_QUEUE_ */
