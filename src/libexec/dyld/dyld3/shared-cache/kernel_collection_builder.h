/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
*
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

#ifndef kernel_collection_builder_h
#define kernel_collection_builder_h

#include <Availability.h>

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef CF_ENUM(uint32_t, CollectionKind) {
    unknownKC   = 0,
    baseKC      = 1,
    auxKC       = 2,
    pageableKC  = 3,
};

typedef CF_ENUM(uint32_t, FileResultKind) {
    unknownFileResult   = 0,
    kernelCollection    = 1,
};

typedef CF_ENUM(uint32_t, StripMode) {
    unknownStripMode     = 0,
    stripNone            = 1, // Don't strip any symbols
    stripAll             = 2, // Strip all symbols from all binaries
    stripAllKexts        = 3, // Don't strip xnu, but strip everything else
};

typedef CF_ENUM(uint32_t, BinaryStripMode) {
    binaryUnknownStripMode     = 0,
    binaryStripNone            = 1, // Don't strip any symbols
    binaryStripExports         = 2, // Strip all the exports, but leave the local symbols
    binaryStripLocals          = 3, // Strip all the locals, but leave the exported symbols
    binaryStripAll             = 4, // Strip all symbols
};

typedef CF_ENUM(uint32_t, ProgressKind) {
    unknownProgress     = 0,
    layout              = 1,
    generateHeader      = 2,
    copyInputs          = 3,
    applySplitSeg       = 4,
    generatePrelinkInfo = 5,
    processFixups       = 6,
    optimizeStubs       = 7,
    optimizeLinkedit    = 8,
    writeFixups         = 9,
    emitFile            = 10
};

struct BuildOptions_v1
{
    uint64_t                                    version;                        // Future proofing, set to 1
    CollectionKind                              collectionKind;
    StripMode                                   stripMode;
// Valid archs are one of: "arm64", "arm64e", "x86_64", "x86_64h"
    const CFStringRef                           arch;
    bool                                        verboseDiagnostics;
};


struct CollectionFileResult_v1
{
    uint64_t                                    version;            // Future proofing, set to 1
    FileResultKind                              fileKind;
    const CFDataRef                             data;               // Owned by the cache builder.  Destroyed by destroyKernelCollectionBuilder
    const CFArrayRef                            warnings;           // should this be per-result?
};

struct KextFileData_v1
{
    uint64_t                                    version;            // Future proofing, set to 1
    const CFStringRef                           kextpath;
    const CFDataRef                             kextdata;
    const CFArrayRef                            dependencies;
    const CFStringRef                           bundleID;
    const CFStringRef                           bundlePath;
    CFDictionaryRef                             plist;
    BinaryStripMode                             stripMode;
};

typedef void (*ProgressCallback)(const char* message, ProgressKind kind);

struct KernelCollectionBuilder;

// What is the version of this builder dylib.  Can be used to determine what API is available.
__API_AVAILABLE(macos(10.14))
void getVersion(uint32_t *major, uint32_t *minor);

// Returns a valid object on success, or NULL on failure.
__API_AVAILABLE(macos(10.14))
struct KernelCollectionBuilder* createKernelCollectionBuilder(const struct BuildOptions_v1* options);

// Add a kernel static executable file.  Returns true on success.
__API_AVAILABLE(macos(10.14))
bool addKernelFile(struct KernelCollectionBuilder* builder, const CFStringRef path, const CFDataRef data);

// Add kext mach-o and plist files.  Returns true on success.
__API_AVAILABLE(macos(10.14))
bool addKextDataFile(struct KernelCollectionBuilder* builder, const struct KextFileData_v1* fileData);

// Add interface plist file.  Returns true on success.
__API_AVAILABLE(macos(10.14))
bool addInterfaceFile(struct KernelCollectionBuilder* builder, const CFStringRef path, const CFDataRef data);

// Add collection file.  Returns true on success.
__API_AVAILABLE(macos(10.14))
bool addCollectionFile(struct KernelCollectionBuilder* builder, const CFStringRef path, const CFDataRef data, CollectionKind kind);

// Add data to the given segment of the final file.  Note the section can be null (or "") if desired
__API_AVAILABLE(macos(10.14))
bool addSegmentData(struct KernelCollectionBuilder* builder, const CFStringRef segmentName, const CFStringRef sectionName, const CFDataRef data);

// Add entries to the prelink info dictionary.  Note this can only be used once at this time.
__API_AVAILABLE(macos(10.14))
bool addPrelinkInfo(struct KernelCollectionBuilder* builder, const CFDictionaryRef extraData);

// Set a handler to be called at various points during the build to notify the user of progress.
__API_AVAILABLE(macos(10.14))
void setProgressCallback(const ProgressCallback callback);

// Returns true on success.
__API_AVAILABLE(macos(10.14))
bool runKernelCollectionBuilder(struct KernelCollectionBuilder* builder);

// Gets the list of errors we have produced.  These may be from incorrect input values, or failures in the build itself
__API_AVAILABLE(macos(10.14))
const char* const* getErrors(const struct KernelCollectionBuilder* builder, uint64_t* errorCount);

// Gets the errors on each kext.  This returns null if there are no such errors.
// A non-null result is a dictionary where the keys are bundle-ids and the values are
// arrays of strings represeting error messages.
__API_AVAILABLE(macos(10.14))
CFDictionaryRef getKextErrors(const struct KernelCollectionBuilder* builder);

// Returns an array of the resulting files.  These may be new collections, or other files required later
__API_AVAILABLE(macos(10.14))
const struct CollectionFileResult_v1* const* getCollectionFileResults(struct KernelCollectionBuilder* builder, uint64_t* resultCount);

__API_AVAILABLE(macos(10.14))
void destroyKernelCollectionBuilder(struct KernelCollectionBuilder* builder);

#ifdef __cplusplus
}
#endif

#endif /* kernel_collection_builder_h */
