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

#include "MachOAppCache.h"

#include <list>

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFString.h>

#ifndef LC_FILESET_ENTRY
#define LC_FILESET_ENTRY      (0x35 | LC_REQ_DYLD) /* used with fileset_entry_command */
struct fileset_entry_command {
    uint32_t        cmd;        /* LC_FILESET_ENTRY */
    uint32_t        cmdsize;    /* includes id string */
    uint64_t        vmaddr;     /* memory address of the dylib */
    uint64_t        fileoff;    /* file offset of the dylib */
    union lc_str    entry_id;   /* contained entry id */
    uint32_t        reserved;   /* entry_id is 32-bits long, so this is the reserved padding */
};
#endif

namespace dyld3 {

void MachOAppCache::forEachDylib(Diagnostics& diag, void (^callback)(const MachOAnalyzer* ma, const char* name, bool& stop)) const {
    const intptr_t slide = getSlide();
    forEachLoadCommand(diag, ^(const load_command *cmd, bool &stop) {
        if (cmd->cmd == LC_FILESET_ENTRY) {
            const fileset_entry_command* app_cache_cmd = (const fileset_entry_command*)cmd;
            const char* name = (char*)app_cache_cmd + app_cache_cmd->entry_id.offset;
            callback((const MachOAnalyzer*)(app_cache_cmd->vmaddr + slide), name, stop);
            return;
        }
    });
}

void MachOAppCache::forEachPrelinkInfoLibrary(Diagnostics& diags,
                                              void (^callback)(const char* bundleName, const char* relativePath,
                                                               const std::vector<const char*>& deps)) const  {

     __block std::list<std::string> nonASCIIStrings;
     auto getString = ^(Diagnostics& diags, CFStringRef symbolNameRef) {
         const char* symbolName = CFStringGetCStringPtr(symbolNameRef, kCFStringEncodingUTF8);
         if ( symbolName != nullptr )
             return symbolName;

         CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(symbolNameRef), kCFStringEncodingUTF8);
         char buffer[len + 1];
         if ( !CFStringGetCString(symbolNameRef, buffer, len, kCFStringEncodingUTF8) ) {
             diags.error("Could not convert string to ASCII");
             return (const char*)nullptr;
         }
         buffer[len] = '\0';
         nonASCIIStrings.push_back(buffer);
         return nonASCIIStrings.back().c_str();
     };

    const uint8_t* prelinkInfoBuffer = nullptr;
    uint64_t prelinkInfoBufferSize = 0;
    prelinkInfoBuffer = (const uint8_t*)findSectionContent("__PRELINK_INFO", "__info", prelinkInfoBufferSize);
    if ( prelinkInfoBuffer == nullptr )
        return;

    CFReadStreamRef readStreamRef = CFReadStreamCreateWithBytesNoCopy(kCFAllocatorDefault, prelinkInfoBuffer, prelinkInfoBufferSize, kCFAllocatorNull);
    if ( !CFReadStreamOpen(readStreamRef) ) {
        fprintf(stderr, "Could not open plist stream\n");
        exit(1);
    }
    CFErrorRef errorRef = nullptr;
    CFPropertyListRef plistRef = CFPropertyListCreateWithStream(kCFAllocatorDefault, readStreamRef, prelinkInfoBufferSize, kCFPropertyListImmutable, nullptr, &errorRef);
    if ( errorRef != nullptr ) {
        CFStringRef stringRef = CFErrorCopyFailureReason(errorRef);
        fprintf(stderr, "Could not read plist because: %s\n", CFStringGetCStringPtr(stringRef, kCFStringEncodingASCII));
        CFRelease(stringRef);
        exit(1);
    }
    assert(CFGetTypeID(plistRef) == CFDictionaryGetTypeID());

    // Get the "_PrelinkInfoDictionary" array
    CFArrayRef prelinkInfoDictionaryArrayRef = (CFArrayRef)CFDictionaryGetValue((CFDictionaryRef)plistRef, CFSTR("_PrelinkInfoDictionary"));
    assert(CFGetTypeID(prelinkInfoDictionaryArrayRef) == CFArrayGetTypeID());

    for (CFIndex i = 0; i != CFArrayGetCount(prelinkInfoDictionaryArrayRef); ++i) {
        CFDictionaryRef kextInfoDictionary = (CFDictionaryRef)CFArrayGetValueAtIndex(prelinkInfoDictionaryArrayRef, i);
        assert(CFGetTypeID(kextInfoDictionary) == CFDictionaryGetTypeID());

        CFStringRef bundleIdentifierStringRef = (CFStringRef)CFDictionaryGetValue((CFDictionaryRef)kextInfoDictionary, CFSTR("CFBundleIdentifier"));
        assert(CFGetTypeID(bundleIdentifierStringRef) == CFStringGetTypeID());

        const char* bundleID = getString(diags, bundleIdentifierStringRef);
        if ( bundleID == nullptr )
            return;

        const char* relativePath = nullptr;
        CFStringRef relativePathStringRef = (CFStringRef)CFDictionaryGetValue((CFDictionaryRef)kextInfoDictionary, CFSTR("_PrelinkExecutableRelativePath"));
        if ( relativePathStringRef != nullptr ) {
            assert(CFGetTypeID(relativePathStringRef) == CFStringGetTypeID());
            relativePath = getString(diags, relativePathStringRef);
            if ( relativePath == nullptr )
                return;
        }

        std::vector<const char*> dependencies;

        CFDictionaryRef bundleLibrariesDictionaryRef = (CFDictionaryRef)CFDictionaryGetValue((CFDictionaryRef)kextInfoDictionary, CFSTR("OSBundleLibraries"));
        if (bundleLibrariesDictionaryRef != nullptr) {
            // Add the libraries to the dependencies
            // If we didn't have bundle libraries then a placeholder was added
            assert(CFGetTypeID(bundleLibrariesDictionaryRef) == CFDictionaryGetTypeID());

            struct ApplyContext {
                Diagnostics*                diagnostics;
                std::vector<const char*>*   dependencies                                    = nullptr;
                const char* (^getString)(Diagnostics& diags, CFStringRef symbolNameRef)     = nullptr;
            };

            CFDictionaryApplierFunction callback = [](const void *key, const void *value, void *context) {
                CFStringRef keyStringRef = (CFStringRef)key;
                assert(CFGetTypeID(keyStringRef) == CFStringGetTypeID());

                ApplyContext* applyContext = (ApplyContext*)context;
                const char* depString = applyContext->getString(*applyContext->diagnostics, keyStringRef);
                if ( !depString )
                    return;

                applyContext->dependencies->push_back(depString);
            };

            ApplyContext applyContext = { &diags, &dependencies, getString };
            CFDictionaryApplyFunction(bundleLibrariesDictionaryRef, callback, &applyContext);

            if ( diags.hasError() )
                return;
        }
        callback(bundleID, relativePath, dependencies);
    }

    CFRelease(plistRef);
    CFRelease(readStreamRef);
}

} // namespace dyld3
