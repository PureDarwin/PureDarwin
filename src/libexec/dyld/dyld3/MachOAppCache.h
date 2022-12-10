/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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

#ifndef MachOAppCache_h
#define MachOAppCache_h

#include "MachOAnalyzer.h"

namespace dyld3 {

struct MachOAppCache : public MachOAnalyzer {
    // Taken from kmod.h
    enum {
        kmodMaxName = 64
    };
    #pragma pack(push, 4)
    struct KModInfo64_v1 {
        uint64_t            next_addr;
        int32_t             info_version;
        uint32_t            id;
        uint8_t             name[kmodMaxName];
        uint8_t             version[kmodMaxName];
        int32_t             reference_count;
        uint64_t            reference_list_addr;
        uint64_t            address;
        uint64_t            size;
        uint64_t            hdr_size;
        uint64_t            start_addr;
        uint64_t            stop_addr;
    };
    #pragma pack(pop)

    void forEachDylib(Diagnostics& diag, void (^callback)(const MachOAnalyzer* ma, const char* name, bool& stop)) const;

    // Walk the __PRELINK_INFO dictionary and return each bundle and its libraries
    void forEachPrelinkInfoLibrary(Diagnostics& diags,
                                   void (^callback)(const char* bundleName, const char* relativePath,
                                                    const std::vector<const char*>& deps)) const;
};

} // namespace dyld3

#endif /* MachOAppCache_h */
