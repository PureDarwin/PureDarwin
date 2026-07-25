/*
 * With OS_OBJECT_USE_OBJC=1 (see CMakeLists.txt), os/object.h and the
 * dispatch private headers exercise API_AVAILABLE(...)/API_UNAVAILABLE(...)
 * invocations (2, 4, and 5-argument forms) whose variadic-arg-count macro
 * dispatch (__API_AVAILABLE_GET_MACRO) mis-selects with this toolchain,
 * producing parse errors ("expected ','") instead of expanding to nothing.
 * A plain command-line -D doesn't survive: Availability.h's own #define
 * runs later (when a header first pulls it in) and unconditionally wins.
 * Force Availability.h to load now, then override its macros for the rest
 * of the translation unit - PureDarwin doesn't do availability checking, so
 * this is behavior-neutral.
 */
#ifndef PD_NEUTER_API_AVAILABLE_H
#define PD_NEUTER_API_AVAILABLE_H

#include <Availability.h>
#include <os/availability.h>

#undef API_AVAILABLE
#define API_AVAILABLE(...)
#undef API_UNAVAILABLE
#define API_UNAVAILABLE(...)
#undef __API_AVAILABLE
#define __API_AVAILABLE(...)
#undef __API_UNAVAILABLE
#define __API_UNAVAILABLE(...)
#undef DISPATCH_ENUM_API_AVAILABLE
#define DISPATCH_ENUM_API_AVAILABLE(...)

#endif /* PD_NEUTER_API_AVAILABLE_H */
