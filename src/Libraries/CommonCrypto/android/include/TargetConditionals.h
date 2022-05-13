/*	TargetConditionals.h
	Copyright (c) 2010-2013, Apple Inc. All rights reserved.
	For CF on Linux ONLY
*/

#ifndef __TARGETCONDITIONALS__
#define __TARGETCONDITIONALS__

#if !defined(__clang_major__) || __clang_major__ < 7
#define __nullable
#include <signal.h>
#endif

#define TARGET_OS_MAC               0
#define TARGET_OS_WIN32             0
#define TARGET_OS_UNIX              0
#define TARGET_OS_EMBEDDED          0 
#define TARGET_OS_IPHONE            0 
#define TARGET_IPHONE_SIMULATOR     0 
#define TARGET_OS_LINUX             0
#define TARGET_OS_ANDROID           1
#define TARGET_RT_LITTLE_ENDIAN     __LITTLE_ENDIAN_BITFIELD
#define TARGET_RT_BIG_ENDIAN        __BIG_ENDIAN_BITFIELD

#endif  /* __TARGETCONDITIONALS__ */
