/*
 * Copyright (c) 2008-2017 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
 
#ifndef __OS_AVAILABILITY__
#define __OS_AVAILABILITY__

/* 
 * API_TO_BE_DEPRECATED is used as a version number in API that will be deprecated 
 * in an upcoming release. This soft deprecation is an intermediate step before formal 
 * deprecation to notify developers about the API before compiler warnings are generated.
 * You can find all places in your code that use soft deprecated API by redefining the 
 * value of this macro to your current minimum deployment target, for example:
 * (macOS)
 *   clang -DAPI_TO_BE_DEPRECATED=10.12 <other compiler flags>
 * (iOS)
 *   clang -DAPI_TO_BE_DEPRECATED=11.0 <other compiler flags>
 */
 
#ifndef API_TO_BE_DEPRECATED
#define API_TO_BE_DEPRECATED 100000
#endif

#ifndef API_TO_BE_DEPRECATED_MACOS
#define API_TO_BE_DEPRECATED_MACOS 100000
#endif

#ifndef API_TO_BE_DEPRECATED_IOS
#define API_TO_BE_DEPRECATED_IOS 100000
#endif

#ifndef API_TO_BE_DEPRECATED_TVOS
#define API_TO_BE_DEPRECATED_TVOS 100000
#endif

#ifndef API_TO_BE_DEPRECATED_WATCHOS
#define API_TO_BE_DEPRECATED_WATCHOS 100000
#endif

#ifndef __API_TO_BE_DEPRECATED_BRIDGEOS
#define __API_TO_BE_DEPRECATED_BRIDGEOS 100000
#endif

#ifndef __API_TO_BE_DEPRECATED_MACCATALYST
#define __API_TO_BE_DEPRECATED_MACCATALYST 100000
#endif

#ifndef API_TO_BE_DEPRECATED_DRIVERKIT
#define API_TO_BE_DEPRECATED_DRIVERKIT 100000
#endif

#ifndef API_TO_BE_DEPRECATED_VISIONOS
#define API_TO_BE_DEPRECATED_VISIONOS 100000
#endif


#include <AvailabilityInternal.h>
#include <AvailabilityInternalLegacy.h>
#if __has_include(<AvailabilityInternalPrivate.h>)
  #include <AvailabilityInternalPrivate.h>
#endif

/*
 Macros for defining which versions/platform a given symbol can be used.
 
 @see http://clang.llvm.org/docs/AttributeReference.html#availability

 * Note that these macros are only compatible with clang compilers that
 * support the following target selection options:
 *
 * -mmacosx-version-min
 * -miphoneos-version-min
 * -mwatchos-version-min
 * -mtvos-version-min
 * -mbridgeos-version-min
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
     *   watchOSApplicationExtension, driverkit, visionos, visionOSApplicationExtension
     *
     * Examples:
     *    API_AVAILABLE(macos(10.10))
     *    API_AVAILABLE(macos(10.9), ios(10.0))
     *    API_AVAILABLE(macos(10.4), ios(8.0), watchos(2.0), tvos(10.0))
     *    API_AVAILABLE(driverkit(19.0))
     */

// @@AVAILABILITY_MACRO_INTERFACE(API_AVAILABLE,__API_AVAILABLE)@@
// @@AVAILABILITY_MACRO_INTERFACE(API_AVAILABLE,__API_AVAILABLE_BEGIN,scoped_availablity=TRUE)@@

    /*
     * API Deprecations
     *
     * Use to specify the release that a particular API became deprecated.
     *
     * Platform names:
     *   macos, macOSApplicationExtension, macCatalyst, macCatalystApplicationExtension,
     *   ios, iOSApplicationExtension, tvos, tvOSApplicationExtension, watchos,
     *   watchOSApplicationExtension, driverkit, visionos, visionOSApplicationExtension
     *
     *   Within each platform a tuple of versions will represent the version the API was
     *   introduced in, followed by the version it was deperecated in.
     *
     * Examples:
     *
     *    API_DEPRECATED("Deprecated", macos(10.4, 10.8))
     *    API_DEPRECATED("Deprecated", macos(10.4, 10.8), ios(2.0, 3.0), watchos(2.0, 3.0), tvos(9.0, 10.0))
     *
     *    API_DEPRECATED_WITH_REPLACEMENT("-setName:", tvos(10.0, 10.4), ios(9.0, 10.0))
     *    API_DEPRECATED_WITH_REPLACEMENT("SomeClassName", macos(10.4, 10.6), watchos(2.0, 3.0))
     */
     
// @@AVAILABILITY_MACRO_INTERFACE(API_DEPRECATED,__API_DEPRECATED_MSG,argCount=1)@@
// @@AVAILABILITY_MACRO_INTERFACE(API_DEPRECATED_WITH_REPLACEMENT,__API_DEPRECATED_REP,argCount=1)@@

// @@AVAILABILITY_MACRO_INTERFACE(API_DEPRECATED,__API_DEPRECATED_BEGIN,argCount=1,scoped_availablity=TRUE)@@

// @@AVAILABILITY_MACRO_INTERFACE(API_DEPRECATED_WITH_REPLACEMENT,__API_DEPRECATED_WITH_REPLACEMENT_BEGIN,argCount=1,scoped_availablity=TRUE)@@

    /*
     * API Obsoletions
     *
     * Use to specify the release that a particular API became unavailable.
     *
     * Platform names:
     *   macos, macOSApplicationExtension, macCatalyst, macCatalystApplicationExtension,
     *   ios, iOSApplicationExtension, tvos, tvOSApplicationExtension, watchos,
     *   watchOSApplicationExtension, driverkit, visionos, visionOSApplicationExtension
     *
     *   Within each platform a tuple of versions will represent the version the API was
     *   introduced in, followed by the version it was deperecated in, and finally the version it
     *   was removed in.
     *
     * Examples:
     *
     *    API_OBSOLETED("No longer supported", macos(10.4, 10.8, 11.0))
     *    API_OBSOLETED("No longer supported", macos(10.4, 10.8, 11.0), ios(2.0, 3.0, 4.0), watchos(2.0, 3.0, 4.0), tvos(9.0, 10.0, 11.0))
     *
     *    API_OBSOLETED_WITH_REPLACEMENT("-setName:", tvos(10.0, 10.4, 12.0), ios(9.0, 10.0, 11.0))
     *    API_OBSOLETED_WITH_REPLACEMENT("SomeClassName", macos(10.4, 10.6, 11.0), watchos(2.0, 3.0, 4.0))
     */

// @@AVAILABILITY_MACRO_INTERFACE(API_OBSOLETED,__API_OBSOLETED_MSG,argCount=1)@@
// @@AVAILABILITY_MACRO_INTERFACE(API_OBSOLETED_WITH_REPLACEMENT,__API_OBSOLETED_REP,argCount=1)@@

// @@AVAILABILITY_MACRO_INTERFACE(API_OBSOLETED,__API_OBSOLETED_BEGIN,argCount=1,scoped_availablity=TRUE)@@

// @@AVAILABILITY_MACRO_INTERFACE(API_OBSOLETED_WITH_REPLACEMENT,__API_OBSOLETED_WITH_REPLACEMENT_BEGIN,argCount=1,scoped_availablity=TRUE)@@

    /*
     * API Unavailability
     * Use to specify that an API is unavailable for a particular platform.
     *
     * Example:
     *    API_UNAVAILABLE(macos)
     *    API_UNAVAILABLE(watchos, tvos)
     */

// @@AVAILABILITY_MACRO_INTERFACE(API_UNAVAILABLE,__API_UNAVAILABLE)@@

// @@AVAILABILITY_MACRO_INTERFACE(API_UNAVAILABLE,__API_UNAVAILABLE_BEGIN,scoped_availablity=TRUE)@@
 #endif /* __has_attribute(availability) */
#endif /* #if defined(__has_feature) && defined(__has_attribute) */

/* 
 * Evaluate to nothing for compilers that don't support clang language extensions.
 */

#ifndef API_AVAILABLE
  #define API_AVAILABLE(...)
#endif

#ifndef API_AVAILABLE_BEGIN
  #define API_AVAILABLE_BEGIN(...)
#endif

#ifndef API_AVAILABLE_END
  #define API_AVAILABLE_END
#endif

#ifndef API_DEPRECATED
  #define API_DEPRECATED(...)
#endif

#ifndef API_DEPRECATED_BEGIN
  #define API_DEPRECATED_BEGIN(...)
#endif

#ifndef API_DEPRECATED_END
  #define API_DEPRECATED_END
#endif

#ifndef API_DEPRECATED_WITH_REPLACEMENT
  #define API_DEPRECATED_WITH_REPLACEMENT(...)
#endif

#ifndef API_DEPRECATED_WITH_REPLACEMENT_BEGIN
  #define API_DEPRECATED_WITH_REPLACEMENT_BEGIN(...)
#endif

#ifndef API_DEPRECATED_WITH_REPLACEMENT_END
  #define API_DEPRECATED_WITH_REPLACEMENT_END
#endif

#ifndef API_OBSOLETED
  #define API_OBSOLETED(...)
#endif

#ifndef API_OBSOLETED_BEGIN
  #define API_OBSOLETED_BEGIN(...)
#endif

#ifndef API_OBSOLETED_END
  #define API_OBSOLETED_END
#endif

#ifndef API_OBSOLETED_WITH_REPLACEMENT
  #define API_OBSOLETED_WITH_REPLACEMENT(...)
#endif

#ifndef API_OBSOLETED_WITH_REPLACEMENT_BEGIN
  #define API_OBSOLETED_WITH_REPLACEMENT_BEGIN(...)
#endif

#ifndef API_OBSOLETED_WITH_REPLACEMENT_END
  #define API_OBSOLETED_WITH_REPLACEMENT_END
#endif

#ifndef API_UNAVAILABLE
  #define API_UNAVAILABLE(...)
#endif

#ifndef API_UNAVAILABLE_BEGIN
  #define API_UNAVAILABLE_BEGIN(...)
#endif

#ifndef API_UNAVAILABLE_END
  #define API_UNAVAILABLE_END
#endif

#if __has_include(<AvailabilityProhibitedInternal.h>)
  #include <AvailabilityProhibitedInternal.h>
#endif

/*
 * If SPI decorations have not been defined elsewhere, disable them.
 */

#ifndef SPI_AVAILABLE
  #define SPI_AVAILABLE(...)
#endif

#ifndef SPI_AVAILABLE_BEGIN
  #define SPI_AVAILABLE_BEGIN(...)
#endif

#ifndef SPI_AVAILABLE_END
  #define SPI_AVAILABLE_END
#endif

#ifndef SPI_DEPRECATED
  #define SPI_DEPRECATED(...)
#endif

#ifndef SPI_DEPRECATED_WITH_REPLACEMENT
  #define SPI_DEPRECATED_WITH_REPLACEMENT(...)
#endif

#endif /* __OS_AVAILABILITY__ */

