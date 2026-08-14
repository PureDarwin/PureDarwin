#ifndef PD_SECURITY_PREFIX_H
#define PD_SECURITY_PREFIX_H

#include <os/availability.h>

#undef API_AVAILABLE
#undef API_AVAILABLE_BEGIN
#undef API_AVAILABLE_END
#undef API_DEPRECATED
#undef API_DEPRECATED_BEGIN
#undef API_DEPRECATED_END
#undef API_DEPRECATED_WITH_REPLACEMENT
#undef API_DEPRECATED_WITH_REPLACEMENT_BEGIN
#undef API_DEPRECATED_WITH_REPLACEMENT_END
#undef API_UNAVAILABLE
#undef API_UNAVAILABLE_BEGIN
#undef API_UNAVAILABLE_END
/* SPI_AVAILABLE does not exist in the 11.3 SDK at all. */
#undef SPI_AVAILABLE
#undef SPI_AVAILABLE_BEGIN
#undef SPI_AVAILABLE_END
#undef SPI_DEPRECATED
#undef SPI_DEPRECATED_WITH_REPLACEMENT

#define API_AVAILABLE(...)
#define API_AVAILABLE_BEGIN(...)
#define API_AVAILABLE_END
#define API_DEPRECATED(...)
#define API_DEPRECATED_BEGIN(...)
#define API_DEPRECATED_END
#define API_DEPRECATED_WITH_REPLACEMENT(...)
#define API_DEPRECATED_WITH_REPLACEMENT_BEGIN(...)
#define API_DEPRECATED_WITH_REPLACEMENT_END
#define API_UNAVAILABLE(...)
#define API_UNAVAILABLE_BEGIN(...)
#define API_UNAVAILABLE_END
#define SPI_AVAILABLE(...)
#define SPI_AVAILABLE_BEGIN(...)
#define SPI_AVAILABLE_END
#define SPI_DEPRECATED(...)
#define SPI_DEPRECATED_WITH_REPLACEMENT(...)

/*
 * The older __OSX_ and __IOS_ spellings take __MAC_10_12-style version macros,
 * which expand to 101200 rather than the 10.12 the availability attribute
 * wants. Apple's SDK reconciles the two through a table of
 * __AVAILABILITY_INTERNAL macros that our vendored AvailabilityVersions does
 * not carry, so drop them just like the API_ and SPI_ families above.
 */
#include <Availability.h>
/* Security's older headers still use AVAILABLE_MAC_OS_X_VERSION_*_AND_LATER,
 * which lives in AvailabilityMacros.h rather than Availability.h. */
#include <AvailabilityMacros.h>

/* The double-underscore forms are what libc's and pthread's own headers use,
 * and they name platforms (bridgeos, driverkit) our AvailabilityVersions does
 * not know, which turns the annotation into a parse error rather than a
 * warning. */
#undef __API_AVAILABLE
#undef __API_UNAVAILABLE
#undef __API_DEPRECATED
#undef __API_DEPRECATED_WITH_REPLACEMENT
#define __API_AVAILABLE(...)
#define __API_UNAVAILABLE(...)
#define __API_DEPRECATED(...)
#define __API_DEPRECATED_WITH_REPLACEMENT(...)

#undef __OSX_AVAILABLE
#undef __OSX_AVAILABLE_STARTING
#undef __OSX_AVAILABLE_BUT_DEPRECATED
#undef __OSX_AVAILABLE_BUT_DEPRECATED_MSG
#undef __OSX_DEPRECATED
#undef __IOS_AVAILABLE
#undef __IOS_DEPRECATED
#undef __TVOS_AVAILABLE
#undef __WATCHOS_AVAILABLE

#define __OSX_AVAILABLE(...)
#define __OSX_AVAILABLE_STARTING(...)
#define __OSX_AVAILABLE_BUT_DEPRECATED(...)
#define __OSX_AVAILABLE_BUT_DEPRECATED_MSG(...)
#define __OSX_DEPRECATED(...)
#define __IOS_AVAILABLE(...)
#define __IOS_DEPRECATED(...)
#define __TVOS_AVAILABLE(...)
#define __WATCHOS_AVAILABLE(...)

#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 1

#include <TargetConditionals.h>
#ifndef TARGET_OS_BRIDGE
#define TARGET_OS_BRIDGE 0
#endif

#endif /* PD_SECURITY_PREFIX_H */
