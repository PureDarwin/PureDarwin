/*
 * Copyright (c) 1999-2002, 2005-2008 Apple Inc.  All Rights Reserved.
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

#ifndef _OBJC_CONFIG_H_
#define _OBJC_CONFIG_H_

#include <TargetConditionals.h>

// Avoid the !NDEBUG double negative.
#if !NDEBUG
#   define DEBUG 1
#else
#   define DEBUG 0
#endif

// Define SUPPORT_GC_COMPAT=1 to enable compatibility where GC once was.
// OBJC_NO_GC and OBJC_NO_GC_API in objc-api.h mean something else.
#if !TARGET_OS_OSX
#   define SUPPORT_GC_COMPAT 0
#else
#   define SUPPORT_GC_COMPAT 1
#endif

// Define SUPPORT_ZONES=1 to enable malloc zone support in NXHashTable.
#if !(TARGET_OS_OSX || TARGET_OS_MACCATALYST)
#   define SUPPORT_ZONES 0
#else
#   define SUPPORT_ZONES 1
#endif

// Define SUPPORT_MOD=1 to use the mod operator in NXHashTable and objc-sel-set
#if defined(__arm__)
#   define SUPPORT_MOD 0
#else
#   define SUPPORT_MOD 1
#endif

// Define SUPPORT_PREOPT=1 to enable dyld shared cache optimizations
#if TARGET_OS_WIN32
#   define SUPPORT_PREOPT 0
#else
#   define SUPPORT_PREOPT 1
#endif

// Define SUPPORT_TAGGED_POINTERS=1 to enable tagged pointer objects
// Be sure to edit tagged pointer SPI in objc-internal.h as well.
#if !__LP64__
#   define SUPPORT_TAGGED_POINTERS 0
#else
#   define SUPPORT_TAGGED_POINTERS 1
#endif

// Define SUPPORT_MSB_TAGGED_POINTERS to use the MSB 
// as the tagged pointer marker instead of the LSB.
// Be sure to edit tagged pointer SPI in objc-internal.h as well.
#if !SUPPORT_TAGGED_POINTERS  ||  ((TARGET_OS_OSX || TARGET_OS_MACCATALYST) && __x86_64__)
#   define SUPPORT_MSB_TAGGED_POINTERS 0
#else
#   define SUPPORT_MSB_TAGGED_POINTERS 1
#endif

// Define SUPPORT_INDEXED_ISA=1 on platforms that store the class in the isa 
// field as an index into a class table.
// Note, keep this in sync with any .s files which also define it.
// Be sure to edit objc-abi.h as well.
#if __ARM_ARCH_7K__ >= 2  ||  (__arm64__ && !__LP64__)
#   define SUPPORT_INDEXED_ISA 1
#else
#   define SUPPORT_INDEXED_ISA 0
#endif

// Define SUPPORT_PACKED_ISA=1 on platforms that store the class in the isa 
// field as a maskable pointer with other data around it.
#if (!__LP64__  ||  TARGET_OS_WIN32  ||  \
     (TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST && !__arm64__))
#   define SUPPORT_PACKED_ISA 0
#else
#   define SUPPORT_PACKED_ISA 1
#endif

// Define SUPPORT_NONPOINTER_ISA=1 on any platform that may store something
// in the isa field that is not a raw pointer.
#if !SUPPORT_INDEXED_ISA  &&  !SUPPORT_PACKED_ISA
#   define SUPPORT_NONPOINTER_ISA 0
#else
#   define SUPPORT_NONPOINTER_ISA 1
#endif

// Define SUPPORT_FIXUP=1 to repair calls sites for fixup dispatch.
// Fixup messaging itself is no longer supported.
// Be sure to edit objc-abi.h as well (objc_msgSend*_fixup)
#if !(defined(__x86_64__) && (TARGET_OS_OSX || TARGET_OS_SIMULATOR))
#   define SUPPORT_FIXUP 0
#else
#   define SUPPORT_FIXUP 1
#endif

// Define SUPPORT_ZEROCOST_EXCEPTIONS to use "zero-cost" exceptions for OBJC2.
// Be sure to edit objc-exception.h as well (objc_add/removeExceptionHandler)
#if defined(__arm__)  &&  __USING_SJLJ_EXCEPTIONS__
#   define SUPPORT_ZEROCOST_EXCEPTIONS 0
#else
#   define SUPPORT_ZEROCOST_EXCEPTIONS 1
#endif

// Define SUPPORT_ALT_HANDLERS if you're using zero-cost exceptions 
// but also need to support AppKit's alt-handler scheme
// Be sure to edit objc-exception.h as well (objc_add/removeExceptionHandler)
#if !SUPPORT_ZEROCOST_EXCEPTIONS  ||  !TARGET_OS_OSX
#   define SUPPORT_ALT_HANDLERS 0
#else
#   define SUPPORT_ALT_HANDLERS 1
#endif

// Define SUPPORT_RETURN_AUTORELEASE to optimize autoreleased return values
#if TARGET_OS_WIN32
#   define SUPPORT_RETURN_AUTORELEASE 0
#else
#   define SUPPORT_RETURN_AUTORELEASE 1
#endif

// Define SUPPORT_STRET on architectures that need separate struct-return ABI.
#if defined(__arm64__)
#   define SUPPORT_STRET 0
#else
#   define SUPPORT_STRET 1
#endif

// Define SUPPORT_MESSAGE_LOGGING to enable NSObjCMessageLoggingEnabled
#if !TARGET_OS_OSX
#   define SUPPORT_MESSAGE_LOGGING 0
#else
#   define SUPPORT_MESSAGE_LOGGING 1
#endif

// Define SUPPORT_AUTORELEASEPOOL_DEDDUP_PTRS to combine consecutive pointers to the same object in autorelease pools
#if !__LP64__
#   define SUPPORT_AUTORELEASEPOOL_DEDUP_PTRS 0
#else
#   define SUPPORT_AUTORELEASEPOOL_DEDUP_PTRS 1
#endif

// Define HAVE_TASK_RESTARTABLE_RANGES to enable usage of
// task_restartable_ranges_synchronize()
// PureDarwin: the task_restartable_ranges_register/synchronize mach APIs are
// not implemented in PD's libSystem yet, so force this off. objc-cache then
// uses the classic thread-scanning _collecting_in_critical() fallback (plain
// mach task_threads/thread_get_state), which PD does provide.
#if defined(__PUREDARWIN__) || TARGET_OS_SIMULATOR || defined(__i386__) || defined(__arm__) || !TARGET_OS_MAC
#   define HAVE_TASK_RESTARTABLE_RANGES 0
#else
#   define HAVE_TASK_RESTARTABLE_RANGES 1
#endif

// OBJC_INSTRUMENTED controls whether message dispatching is dynamically
// monitored.  Monitoring introduces substantial overhead.
// NOTE: To define this condition, do so in the build command, NOT by
// uncommenting the line here.  This is because objc-class.h heeds this
// condition, but objc-class.h can not #include this file (objc-config.h)
// because objc-class.h is public and objc-config.h is not.
//#define OBJC_INSTRUMENTED

// The runtimeLock is a mutex always held hence the cache lock is
// redundant and can be elided.
//
// If the runtime lock ever becomes a rwlock again,
// the cache lock would need to be used again
#define CONFIG_USE_CACHE_LOCK 0

// Determine how the method cache stores IMPs.
#define CACHE_IMP_ENCODING_NONE 1 // Method cache contains raw IMP.
#define CACHE_IMP_ENCODING_ISA_XOR 2 // Method cache contains ISA ^ IMP.
#define CACHE_IMP_ENCODING_PTRAUTH 3 // Method cache contains ptrauth'd IMP.

#if __PTRAUTH_INTRINSICS__
// Always use ptrauth when it's supported.
#define CACHE_IMP_ENCODING CACHE_IMP_ENCODING_PTRAUTH
#elif defined(__arm__)
// 32-bit ARM uses no encoding.
#define CACHE_IMP_ENCODING CACHE_IMP_ENCODING_NONE
#else
// Everything else uses ISA ^ IMP.
#define CACHE_IMP_ENCODING CACHE_IMP_ENCODING_ISA_XOR
#endif

#define CACHE_MASK_STORAGE_OUTLINED 1
#define CACHE_MASK_STORAGE_HIGH_16 2
#define CACHE_MASK_STORAGE_LOW_4 3
#define CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS 4

#if defined(__arm64__) && __LP64__
#if TARGET_OS_OSX || TARGET_OS_SIMULATOR
#define CACHE_MASK_STORAGE CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS
#else
#define CACHE_MASK_STORAGE CACHE_MASK_STORAGE_HIGH_16
#endif
#elif defined(__arm64__) && !__LP64__
#define CACHE_MASK_STORAGE CACHE_MASK_STORAGE_LOW_4
#else
#define CACHE_MASK_STORAGE CACHE_MASK_STORAGE_OUTLINED
#endif

// Constants used for signing/authing isas. This doesn't quite belong
// here, but the asm files can't import other headers.
#define ISA_SIGNING_DISCRIMINATOR 0x6AE1
#define ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS 0xB5AB

#define ISA_SIGNING_KEY ptrauth_key_process_independent_data

// ISA signing authentication modes. Set ISA_SIGNING_AUTH_MODE to one
// of these to choose how ISAs are authenticated.
#define ISA_SIGNING_STRIP 1 // Strip the signature whenever reading an ISA.
#define ISA_SIGNING_AUTH  2 // Authenticate the signature on all ISAs.


// ISA signing modes. Set ISA_SIGNING_SIGN_MODE to one of these to
// choose how ISAs are signed.
#define ISA_SIGNING_SIGN_NONE       1 // Sign no ISAs.
#define ISA_SIGNING_SIGN_ONLY_SWIFT 2 // Only sign ISAs of Swift objects.
#define ISA_SIGNING_SIGN_ALL        3 // Sign all ISAs.

#if __has_feature(ptrauth_objc_isa_strips) || __has_feature(ptrauth_objc_isa_signs) || __has_feature(ptrauth_objc_isa_authenticates)
#   if __has_feature(ptrauth_objc_isa_authenticates)
#       define ISA_SIGNING_AUTH_MODE ISA_SIGNING_AUTH
#   else
#       define ISA_SIGNING_AUTH_MODE ISA_SIGNING_STRIP
#   endif
#   if __has_feature(ptrauth_objc_isa_signs)
#       define ISA_SIGNING_SIGN_MODE ISA_SIGNING_SIGN_ALL
#   else
#       define ISA_SIGNING_SIGN_MODE ISA_SIGNING_SIGN_NONE
#   endif
#else
#   if __has_feature(ptrauth_objc_isa)
#       define ISA_SIGNING_AUTH_MODE ISA_SIGNING_AUTH
#       define ISA_SIGNING_SIGN_MODE ISA_SIGNING_SIGN_ALL
#   else
#       define ISA_SIGNING_AUTH_MODE ISA_SIGNING_STRIP
#       define ISA_SIGNING_SIGN_MODE ISA_SIGNING_SIGN_NONE
#   endif
#endif

// When set, an unsigned superclass pointer is treated as Nil, which
// will treat the class as if its superclass was weakly linked and
// not loaded, and cause uses of the class to resolve to Nil.
#define SUPERCLASS_SIGNING_TREAT_UNSIGNED_AS_NIL 0

#if defined(__arm64__) && TARGET_OS_IOS && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
#define CONFIG_USE_PREOPT_CACHES 1
#else
#define CONFIG_USE_PREOPT_CACHES 0
#endif

// When set to 1, small methods in the shared cache have a direct
// offset to a selector. When set to 0, small methods in the shared
// cache have the same format as other small methods, with an offset
// to a selref.
#define CONFIG_SHARED_CACHE_RELATIVE_DIRECT_SELECTORS 1

#endif
