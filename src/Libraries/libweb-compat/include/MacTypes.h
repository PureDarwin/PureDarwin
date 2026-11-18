// MacTypes.h - the Carbon scalar types the frameworks name, on CoreFoundation's own definitions
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __MACTYPES__
#define __MACTYPES__

// CFBase.h already defines the sized integers, Boolean, OSStatus, UniChar and OSType, Darwin's copy would clash
#include <CoreFoundation/CFBase.h>

typedef SInt16 OSErr;
typedef unsigned long ItemCount;
typedef unsigned long ByteCount;
typedef unsigned long UniCharCount;
typedef char *Ptr;
typedef Ptr *Handle;
typedef long Size;
typedef unsigned char Str255[256];
typedef unsigned char *StringPtr;
typedef const unsigned char *ConstStr255Param;
typedef FourCharCode ResType;
typedef UInt32 TextEncoding;
typedef UInt32 OptionBits;
typedef void *LogicalAddress;
#ifndef noErr
#define noErr 0
#endif

#endif
