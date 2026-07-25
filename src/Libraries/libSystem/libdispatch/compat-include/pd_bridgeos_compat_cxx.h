#ifndef PUREDARWIN_LIBDISPATCH_BRIDGEOS_COMPAT_CXX_H
#define PUREDARWIN_LIBDISPATCH_BRIDGEOS_COMPAT_CXX_H

#include <Availability.h>

#ifndef __API_AVAILABLE_PLATFORM_bridgeos
 #define __API_AVAILABLE_PLATFORM_bridgeos(x) bridgeos,introduced=x
#endif
#ifndef __API_DEPRECATED_PLATFORM_bridgeos
 #define __API_DEPRECATED_PLATFORM_bridgeos(x,y) bridgeos,introduced=x,deprecated=y
#endif
#ifndef __API_UNAVAILABLE_PLATFORM_bridgeos
 #define __API_UNAVAILABLE_PLATFORM_bridgeos bridgeos,unavailable
#endif

#endif /* PUREDARWIN_LIBDISPATCH_BRIDGEOS_COMPAT_CXX_H */
