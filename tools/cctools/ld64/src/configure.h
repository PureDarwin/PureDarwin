#ifndef _CONFIGURE_H
#define _CONFIGURE_H
#include <sys/param.h>
#include <limits.h>
#include <unistd.h>
#include <stddef.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "strlcat.h"
#include "strlcpy.h"
#include "helper.h"

#ifdef __GLIBCXX__
#include <algorithm>
#endif

#define PROGRAM_PREFIX ""
#define LD_PAGE_SIZE 0x1000
#define CPU_SUBTYPE_X86_ALL     ((cpu_subtype_t)3)

#define SUPPORT_ARCH_armv6 1
#define SUPPORT_ARCH_armv7 1
#define SUPPORT_ARCH_armv7s 1
#define SUPPORT_ARCH_arm64 1
#define SUPPORT_ARCH_arm64e 1
#define SUPPORT_ARCH_arm64_32 1
#define SUPPORT_ARCH_i386 1
#define SUPPORT_ARCH_x86_64 1
#define SUPPORT_ARCH_x86_64h 1
#define SUPPORT_ARCH_armv6m 1
#define SUPPORT_ARCH_armv7k 1
#define SUPPORT_ARCH_armv7m 1
#define SUPPORT_ARCH_armv7em 1

#define SUPPORT_APPLE_TV 1

#define ALL_SUPPORTED_ARCHS  "armv6 armv7 armv7s arm64 arm64e arm64_32 i386 x86_64 x86_64h armv6m armv7k armv7m armv7em (tvOS) arm64e arm64_32"

#define BITCODE_XAR_VERSION "1.0"

#ifndef HW_NCPU
#define HW_NCPU 3
#endif

#ifndef CTL_HW
#define CTL_HW  6
#endif

#ifndef ARG_MAX
#define ARG_MAX 31072
#endif

#endif
