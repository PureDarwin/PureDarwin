// objc4's NSObjCRuntime.h for Argon on Linux: the integer types and limits Foundation builds on
#pragma once
#include <stdint.h>
#include <limits.h>
#include <objc/objc.h>

typedef long NSInteger;
typedef unsigned long NSUInteger;
#define NSIntegerMax LONG_MAX
#define NSIntegerMin LONG_MIN
#define NSUIntegerMax ULONG_MAX
#define NSINTEGER_DEFINED 1
