/*
 * Copyright (c) 2007-2016 by Apple Inc.. All rights reserved.
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
    File:       AvailabilityInternal.h
 
    Contains:   implementation details of __OSX_AVAILABLE_* macros from <Availability.h>

*/
#ifndef __AVAILABILITY_INTERNAL__
#define __AVAILABILITY_INTERNAL__

#include <AvailabilityVersions.h>

// @@AVAILABILITY_MIN_MAX_DEFINES()@@


#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
    /* make sure a default max version is set */
    #ifndef __IPHONE_OS_VERSION_MAX_ALLOWED
        #define __IPHONE_OS_VERSION_MAX_ALLOWED     __IPHONE_17_0
    #endif
    /* make sure a valid min is set */
    #if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_2_0
        #undef __IPHONE_OS_VERSION_MIN_REQUIRED
        #define __IPHONE_OS_VERSION_MIN_REQUIRED    __IPHONE_2_0
    #endif
#endif

#define __AVAILABILITY_INTERNAL_DEPRECATED            __attribute__((deprecated))
#ifdef __has_feature
    #if __has_feature(attribute_deprecated_with_message)
        #define __AVAILABILITY_INTERNAL_DEPRECATED_MSG(_msg)  __attribute__((deprecated(_msg)))
    #else
        #define __AVAILABILITY_INTERNAL_DEPRECATED_MSG(_msg)  __attribute__((deprecated))
    #endif
#elif defined(__GNUC__) && ((__GNUC__ >= 5) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 5)))
    #define __AVAILABILITY_INTERNAL_DEPRECATED_MSG(_msg)  __attribute__((deprecated(_msg)))
#else
    #define __AVAILABILITY_INTERNAL_DEPRECATED_MSG(_msg)  __attribute__((deprecated))
#endif
#define __AVAILABILITY_INTERNAL_UNAVAILABLE           __attribute__((unavailable))
#define __AVAILABILITY_INTERNAL_WEAK_IMPORT           __attribute__((weak_import))
#define __AVAILABILITY_INTERNAL_REGULAR            

// @@AVAILABILITY_PLATFORM_DEFINES()@@


#if defined(__has_feature) && defined(__has_attribute)
 #if __has_attribute(availability)
  #define __API_APPLY_TO any(record, enum, enum_constant, function, objc_method, objc_category, objc_protocol, objc_interface, objc_property, type_alias, variable, field)
  #define __API_RANGE_STRINGIFY(x) __API_RANGE_STRINGIFY2(x)
  #define __API_RANGE_STRINGIFY2(x) #x
 #endif /* __has_attribute(availability) */
#endif /* defined(__has_feature) && defined(__has_attribute) */
/*
 Macros for defining which versions/platform a given symbol can be used.
 
 @see http://clang.llvm.org/docs/AttributeReference.html#availability
 */

#if defined(__has_feature) && defined(__has_attribute)
 #if __has_attribute(availability)

    /*
     * API Introductions
     *
     * Use to specify the release that a particular API became available.
     *
     * Platform names:
     *   macos, macOSApplicationExtension, macCatalyst, macCatalystApplicationExtension,
     *   ios, iOSApplicationExtension, tvos, tvOSApplicationExtension, watchos,
     *   watchOSApplicationExtension, bridgeos, driverkit, visionos, visionOSApplicationExtension
     *
     * Examples:
     *    __API_AVAILABLE(macos(10.10))
     *    __API_AVAILABLE(macos(10.9), ios(10.0))
     *    __API_AVAILABLE(macos(10.4), ios(8.0), watchos(2.0), tvos(10.0))
     *    __API_AVAILABLE(driverkit(19.0))
     */

    #define __API_A(x) __attribute__((availability(__API_AVAILABLE_PLATFORM_##x)))
    
// @@AVAILABILITY_MACRO_IMPL(__API_AVAILABLE,__API_A)@@
    
    #define __API_A_BEGIN(x) _Pragma(__API_RANGE_STRINGIFY (clang attribute (__attribute__((availability(__API_AVAILABLE_PLATFORM_##x))), apply_to = __API_APPLY_TO)))
    
// @@AVAILABILITY_MACRO_IMPL(__API_AVAILABLE_BEGIN,__API_A_BEGIN)@@

    /*
     * API Deprecations
     *
     * Use to specify the release that a particular API became deprecated.
     *
     * Platform names:
     *   macos, macOSApplicationExtension, macCatalyst, macCatalystApplicationExtension,
     *   ios, iOSApplicationExtension, tvos, tvOSApplicationExtension, watchos,
     *   watchOSApplicationExtension, bridgeos, driverkit, visionos, visionOSApplicationExtension
     *
     * Examples:
     *
     *    __API_DEPRECATED("Deprecated", macos(10.4, 10.8))
     *    __API_DEPRECATED("Deprecated", macos(10.4, 10.8), ios(2.0, 3.0), watchos(2.0, 3.0), tvos(9.0, 10.0))
     *
     *    __API_DEPRECATED_WITH_REPLACEMENT("-setName:", tvos(10.0, 10.4), ios(9.0, 10.0))
     *    __API_DEPRECATED_WITH_REPLACEMENT("SomeClassName", macos(10.4, 10.6), watchos(2.0, 3.0))
     */

    #define __API_D(msg,x) __attribute__((availability(__API_DEPRECATED_PLATFORM_##x,message=msg)))
  
// @@AVAILABILITY_MACRO_IMPL(__API_DEPRECATED_MSG,__API_D,args=msg)@@

    #define __API_D_BEGIN(msg, x) _Pragma(__API_RANGE_STRINGIFY (clang attribute (__attribute__((availability(__API_DEPRECATED_PLATFORM_##x,message=msg))), apply_to = __API_APPLY_TO)))

// @@AVAILABILITY_MACRO_IMPL(__API_DEPRECATED_BEGIN,__API_D_BEGIN,args=msg)@@

    #if __has_feature(attribute_availability_with_replacement)
        #define __API_DR(rep,x) __attribute__((availability(__API_DEPRECATED_PLATFORM_##x,replacement=rep)))
    #else
        #define __API_DR(rep,x) __attribute__((availability(__API_DEPRECATED_PLATFORM_##x)))
    #endif

// @@AVAILABILITY_MACRO_IMPL(__API_DEPRECATED_REP,__API_DR,args=msg)@@

    #if __has_feature(attribute_availability_with_replacement)
        #define __API_DR_BEGIN(rep,x) _Pragma(__API_RANGE_STRINGIFY (clang attribute (__attribute__((availability(__API_DEPRECATED_PLATFORM_##x,replacement=rep))), apply_to = __API_APPLY_TO)))
    #else
        #define __API_DR_BEGIN(rep,x) _Pragma(__API_RANGE_STRINGIFY (clang attribute (__attribute__((availability(__API_DEPRECATED_PLATFORM_##x))), apply_to = __API_APPLY_TO)))
    #endif

// @@AVAILABILITY_MACRO_IMPL(__API_DEPRECATED_WITH_REPLACEMENT_BEGIN,__API_DR_BEGIN,args=msg)@@

    /*
     * API Obsoletions
     *
     * Use to specify the release that a particular API became unavailable.
     *
     * Platform names:
     *   macos, macOSApplicationExtension, macCatalyst, macCatalystApplicationExtension,
     *   ios, iOSApplicationExtension, tvos, tvOSApplicationExtension, watchos,
     *   watchOSApplicationExtension, bridgeos, driverkit, visionos, visionOSApplicationExtension
     *
     * Examples:
     *
     *    __API_OBSOLETED("No longer supported", macos(10.4, 10.8, 11.0))
     *    __API_OBSOLETED("No longer supported", macos(10.4, 10.8, 11.0), ios(2.0, 3.0, 4.0), watchos(2.0, 3.0, 4.0), tvos(9.0, 10.0, 11.0))
     *
     *    __API_OBSOLETED_WITH_REPLACEMENT("-setName:", tvos(10.0, 10.4, 12.0), ios(9.0, 10.0, 11.0))
     *    __API_OBSOLETED_WITH_REPLACEMENT("SomeClassName", macos(10.4, 10.6, 11.0), watchos(2.0, 3.0, 4.0))
     */

#define __API_O(msg,x) __attribute__((availability(__API_OBSOLETED_PLATFORM_##x,message=msg)))

// @@AVAILABILITY_MACRO_IMPL(__API_OBSOLETED_MSG,__API_O,args=msg)@@

#define __API_O_BEGIN(msg, x, y) _Pragma(__API_RANGE_STRINGIFY (clang attribute (__attribute__((availability(__API_OBSOLETED_PLATFORM_##x,message=msg))), apply_to = __API_APPLY_TO)))

// @@AVAILABILITY_MACRO_IMPL(__API_OBSOLETED_BEGIN,__API_O_BEGIN,args=msg)@@

#if __has_feature(attribute_availability_with_replacement)
    #define __API_OR(rep,x) __attribute__((availability(__API_OBSOLETED_PLATFORM_##x,replacement=rep)))
#else
    #define __API_OR(rep,x) __attribute__((availability(__API_OBSOLETED_PLATFORM_##x)))
#endif

// @@AVAILABILITY_MACRO_IMPL(__API_OBSOLETED_REP,__API_OR,args=msg)@@

#if __has_feature(attribute_availability_with_replacement)
    #define __API_OR_BEGIN(rep,x) _Pragma(__API_RANGE_STRINGIFY (clang attribute (__attribute__((availability(__API_OBSOLETED_PLATFORM_##x,replacement=rep))), apply_to = __API_APPLY_TO)))
#else
    #define __API_OR_BEGIN(rep,x) _Pragma(__API_RANGE_STRINGIFY (clang attribute (__attribute__((availability(__API_OBSOLETED_PLATFORM_##x))), apply_to = __API_APPLY_TO)))
#endif

// @@AVAILABILITY_MACRO_IMPL(__API_OBSOLETED_WITH_REPLACEMENT_BEGIN,__API_R_BEGIN,args=msg)@@

    /*
     * API Unavailability
     * Use to specify that an API is unavailable for a particular platform.
     *
     * Example:
     *    __API_UNAVAILABLE(macos)
     *    __API_UNAVAILABLE(watchos, tvos)
     */

    #define __API_U(x) __attribute__((availability(__API_UNAVAILABLE_PLATFORM_##x)))

// @@AVAILABILITY_MACRO_IMPL(__API_UNAVAILABLE,__API_U)@@

    #define __API_U_BEGIN(x) _Pragma(__API_RANGE_STRINGIFY (clang attribute (__attribute__((availability(__API_UNAVAILABLE_PLATFORM_##x))), apply_to = __API_APPLY_TO)))

// @@AVAILABILITY_MACRO_IMPL(__API_UNAVAILABLE_BEGIN,__API_U_BEGIN)@@

 #endif /* __has_attribute(availability) */
#endif /* #if defined(__has_feature) && defined(__has_attribute) */

/*
 * Swift compiler version
 * Allows for project-agnostic "epochs" for frameworks imported into Swift via the Clang importer, like #if _compiler_version for Swift
 * Example:
 *
 *  #if __swift_compiler_version_at_least(800, 2, 20)
 *  - (nonnull NSString *)description;
 *  #else
 *  - (NSString *)description;
 *  #endif
 */
 
#ifdef __SWIFT_COMPILER_VERSION
    #define __swift_compiler_version_at_least_impl(X, Y, Z, a, b, ...) \
    __SWIFT_COMPILER_VERSION >= ((X * UINT64_C(1000) * 1000 * 1000) + (Z * 1000 * 1000) + (a * 1000) + b)
    #define __swift_compiler_version_at_least(...) __swift_compiler_version_at_least_impl(__VA_ARGS__, 0, 0, 0, 0)
#else
    #define __swift_compiler_version_at_least(...) 1
#endif

#endif /* __AVAILABILITY_INTERNAL__ */


