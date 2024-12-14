/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2009-2011 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __MACHO_DYLIB_FILE_H__
#define __MACHO_DYLIB_FILE_H__

#include "ld.hpp"
#include "Options.h"

namespace mach_o {
namespace dylib {


extern bool isDylibFile(const uint8_t* fileContent, uint64_t fileLength, cpu_type_t* result, cpu_subtype_t* subResult, ld::Platform* platform, uint32_t* minOsVers);					

extern const char* archName(const uint8_t* fileContent);


extern ld::dylib::File* parse(const uint8_t* fileContent, uint64_t fileLength, const char* path,
							  time_t modTime, const Options& opts, ld::File::Ordinal ordinal,
							  bool bundleLoader, bool indirectDylib, bool fromSDK);

} // namespace dylib
} // namespace mach_o


#endif // __MACHO_DYLIB_FILE_H__
