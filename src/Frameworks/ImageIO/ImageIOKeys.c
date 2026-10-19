// ImageIOKeys.c - image source options CoreGraphics' CGImageSource does not define
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// CGImageSource reads neither, float decoding and the type hint are not implemented

#include <CoreFoundation/CoreFoundation.h>

const CFStringRef kCGImageSourceShouldAllowFloat = CFSTR("kCGImageSourceShouldAllowFloat");
const CFStringRef kCGImageSourceTypeIdentifierHint = CFSTR("kCGImageSourceTypeIdentifierHint");
