/*
 * The vendored Security is from macOS 11.4 but we build against the 11.3 SDK,
 * whose <os/availability.h> only knows the platforms macos/ios/tvos/watchos.
 * Security's headers annotate with `bridgeos` and `macCatalyst` as well, and an
 * unknown platform name makes the argument-counting API_* macros expand into a
 * syntax error ("expected ','").
 */
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

#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 1

#include <TargetConditionals.h>
#ifndef TARGET_OS_BRIDGE
#define TARGET_OS_BRIDGE 0
#endif

#endif /* PD_SECURITY_PREFIX_H */
