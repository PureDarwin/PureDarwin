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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <list>
#include <map>
#include <vector>

#include <variant>

#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFStream.h>

#include "kernel_collection_builder.h"
#include "ClosureFileSystemPhysical.h"
#include "FileUtils.h"
#include "JSONWriter.h"
#include "MachOAppCache.h"

using namespace dyld3::json;

using dyld3::closure::FileSystemPhysical;
using dyld3::closure::LoadedFileInfo;
using dyld3::GradedArchs;
using dyld3::MachOAnalyzer;
using dyld3::MachOAppCache;
using dyld3::Platform;
using dyld3::json::Node;

__attribute__((noreturn))
static void exit_usage(const char* missingOption = nullptr) {
    if ( missingOption != nullptr ) {
        fprintf(stderr, "Missing option '%s'\n", missingOption);
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "Usage: dyld_app_cache_util [-layout] [-entrypoint] [-fixups] [-symbols] [-kmod] [-uuid] [-fips] -arch arch -platform platform -app-cache app-cache-path\n");
    fprintf(stderr, "  -layout          print the layout of an existing app cache\n");
    fprintf(stderr, "  -entrypoint      print the entrypoint of an existing app cache\n");
    fprintf(stderr, "  -fixups          print the fixups of an existing app cache\n");
    fprintf(stderr, "  -symbols         print the symbols of an existing app cache\n");
    fprintf(stderr, "  -kmod            print the kmod_info of an existing app cache\n");
    fprintf(stderr, "  -uuid            print the UUID of an existing app cache\n");
    fprintf(stderr, "  -fips            print the FIPS section of an existing app cache\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Usage: dyld_app_cache_util -validate file-path -arch arch -platform platform\n");
    fprintf(stderr, "  -validate        the path to check is valid for inserting in to an app cache\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Usage: dyld_app_cache_util -list-bundles directory-path\n");
    fprintf(stderr, "  -list-bundles    the directory to index for bundles\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Usage: dyld_app_cache_util -create-kernel-collection kernel-collection -kernel kernel-path [-extensions path-to-extensions] [-bundle-id bundle-id]*\n");
    fprintf(stderr, "  -create-kernel-collection    create a kernel collection and write to the given path\n");
    fprintf(stderr, "  -kernel                      path to the kernel static executable\n");
    fprintf(stderr, "  -extensions                  path to the kernel extensions directory\n");
    fprintf(stderr, "  -bundle-id                   zero or more bundle-ids to link in to the kernel collection\n");
    fprintf(stderr, "  -sectcreate                  segment name, section name, and payload file path for more data to embed in the kernel\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Usage: dyld_app_cache_util -create-pageable-kernel-collection aux-kernel-collection -kernel-collection kernel-collection-path [-extensions path-to-extensions] [-bundle-id bundle-id]*\n");
    fprintf(stderr, "  -create-pageable-kernel-collection   create a pageable kernel collection and write to the given path\n");
    fprintf(stderr, "  -kernel-collection                   path to the kernel collection collection\n");
    fprintf(stderr, "  -extensions                          path to the kernel extensions directory\n");
    fprintf(stderr, "  -bundle-id                           zero or more bundle-ids to link in to the kernel collection\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Usage: dyld_app_cache_util -create-aux-kernel-collection aux-kernel-collection -kernel-collection kernel-collection-path [-extensions path-to-extensions] [-bundle-id bundle-id]*\n");
    fprintf(stderr, "  -create-aux-kernel-collection    create an aux kernel collection and write to the given path\n");
    fprintf(stderr, "  -kernel-collection               path to the kernel collection\n");
    fprintf(stderr, "  -pageable-collection             path to the pageable collection\n");
    fprintf(stderr, "  -extensions                      path to the kernel extensions directory\n");
    fprintf(stderr, "  -bundle-id                       zero or more bundle-ids to link in to the kernel collection\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Common options:\n");
    fprintf(stderr, "  -arch xxx        the arch to use to create the app cache\n");
    fprintf(stderr, "  -platform xxx    the platform to use to create the app cache\n");

    exit(1);
}

struct DumpOptions {
    bool printLayout            = false;
    bool printEntryPoint        = false;
    bool printFixups            = false;
    bool printSymbols           = false;
    bool printUUID              = false;
    bool printKModInfo          = false;
    bool printFIPS              = false;
};

struct ValidateOptions {
    const char* filePath    = nullptr;
};

struct ListBundlesOptions {
    const char* directoryPath    = nullptr;
};

// The payload of -sectcreate
struct SectionData {
    const char* segmentName         = nullptr;
    const char* sectionName         = nullptr;
    const char* payloadFilePath     = nullptr;
};

struct CreateKernelCollectionOptions {
    const char*                             outputCachePath         = nullptr;
    const char*                             kernelPath              = nullptr;
    const char*                             kernelCollectionPath    = nullptr;
    const char*                             pageableCollectionPath  = nullptr;
    const char*                             extensionsPath          = nullptr;
    const char*                             volumeRoot              = "";
    std::vector<const char*>                bundleIDs;
    bool                                    verbose                 = false;
    bool                                    printJSONErrors         = false;
    CollectionKind                          collectionKind          = unknownKC;
    StripMode                               stripMode               = unknownStripMode;
    std::vector<SectionData>                sections;
    const char*                             prelinkInfoExtraData    = nullptr;
};

typedef std::variant<std::monostate, DumpOptions, ValidateOptions, ListBundlesOptions, CreateKernelCollectionOptions> OptionsVariants;

struct CommonOptions {
    const char*                 appCachePath        = nullptr;
    std::vector<const char*>    archs;
    const char*                 platform            = nullptr;
};

CommonOptions gOpts;

template<typename T>
static T& exitOrGetState(OptionsVariants& options, const char* argv) {
    if (std::holds_alternative<std::monostate>(options)) {
        return options.emplace<T>();
    }
    if (std::holds_alternative<T>(options))
        return std::get<T>(options);
    exit_usage();
}

static bool parseArgs(int argc, const char* argv[], OptionsVariants& options) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (arg[0] != '-') {
            fprintf(stderr, "unknown option: %s\n", arg);
            exit_usage();
        }

        // Common options
        if (strcmp(arg, "-app-cache") == 0) {
            if (gOpts.appCachePath != nullptr)
                exit_usage();
            gOpts.appCachePath = argv[++i];
            continue;
        }
        if (strcmp(arg, "-arch") == 0) {
            gOpts.archs.push_back(argv[++i]);
            continue;
        }
        if (strcmp(arg, "-platform") == 0) {
            if (gOpts.platform != nullptr)
                exit_usage();
            gOpts.platform = argv[++i];
            continue;
        }

        // DumpOptions
        if (strcmp(arg, "-layout") == 0) {
            exitOrGetState<DumpOptions>(options, arg).printLayout = true;
            continue;
        }
        if (strcmp(arg, "-entrypoint") == 0) {
            exitOrGetState<DumpOptions>(options, arg).printEntryPoint = true;
            continue;
        }
        if (strcmp(arg, "-fixups") == 0) {
            exitOrGetState<DumpOptions>(options, arg).printFixups = true;
            continue;
        }
        if (strcmp(arg, "-symbols") == 0) {
            exitOrGetState<DumpOptions>(options, arg).printSymbols = true;
            continue;
        }
        if (strcmp(arg, "-uuid") == 0) {
            exitOrGetState<DumpOptions>(options, arg).printUUID = true;
            continue;
        }
        if (strcmp(arg, "-kmod") == 0) {
            exitOrGetState<DumpOptions>(options, arg).printKModInfo = true;
            continue;
        }
        if (strcmp(arg, "-fips") == 0) {
            exitOrGetState<DumpOptions>(options, arg).printFIPS = true;
            continue;
        }

        // ValidateOptions
        if (strcmp(arg, "-validate") == 0) {
            exitOrGetState<ValidateOptions>(options, arg).filePath = argv[++i];
            continue;
        }

        // ListBundlesOptions
        if (strcmp(arg, "-list-bundles") == 0) {
            exitOrGetState<ListBundlesOptions>(options, arg).directoryPath = argv[++i];
            continue;
        }

        // CreateKernelCollectionOptions
        if (strcmp(arg, "-create-kernel-collection") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).outputCachePath = argv[++i];
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).collectionKind = baseKC;
            continue;
        }
        if (strcmp(arg, "-kernel") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).kernelPath = argv[++i];
            continue;
        }
        if (strcmp(arg, "-create-pageable-kernel-collection") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).outputCachePath = argv[++i];
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).collectionKind = pageableKC;
            continue;
        }
        if (strcmp(arg, "-create-aux-kernel-collection") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).outputCachePath = argv[++i];
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).collectionKind = auxKC;
            continue;
        }
        if (strcmp(arg, "-kernel-collection") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).kernelCollectionPath = argv[++i];
            continue;
        }
        if (strcmp(arg, "-pageable-collection") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).pageableCollectionPath = argv[++i];
            continue;
        }
        if (strcmp(arg, "-extensions") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).extensionsPath = argv[++i];
            continue;
        }
        if (strcmp(arg, "-volume-root") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).volumeRoot = argv[++i];
            continue;
        }
        if (strcmp(arg, "-bundle-id") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).bundleIDs.push_back(argv[++i]);
            continue;
        }
        if (strcmp(arg, "-verbose") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).verbose = true;
            continue;
        }
        if (strcmp(arg, "-json-errors") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).printJSONErrors = true;
            continue;
        }
        if (strcmp(arg, "-strip-all") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).stripMode = stripAll;
            continue;
        }
        if (strcmp(arg, "-strip-all-kexts") == 0) {
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).stripMode = stripAllKexts;
            continue;
        }
        if (strcmp(arg, "-sectcreate") == 0) {
            const char* segmentName = argv[++i];
            const char* sectionName = argv[++i];
            const char* payloadFilePath = argv[++i];
            SectionData sectData = { segmentName, sectionName, payloadFilePath };
                exitOrGetState<CreateKernelCollectionOptions>(options, arg).sections.push_back(sectData);
            continue;
        }
        if (strcmp(arg, "-prelink-info-extra") == 0) {
            const char* payloadFilePath = argv[++i];
            exitOrGetState<CreateKernelCollectionOptions>(options, arg).prelinkInfoExtraData = payloadFilePath;
            continue;
        }


        fprintf(stderr, "unknown option: %s\n", arg);
        exit_usage();
    }

    return true;
}

static Platform stringToPlatform(const std::string& str) {
    if (str == "unknown")
        return Platform::unknown;
    if (str == "macOS")
        return Platform::macOS;
    if (str == "iOS")
        return Platform::iOS;
    if (str == "tvOS")
        return Platform::tvOS;
    if (str == "watchOS")
        return Platform::watchOS;
    if (str == "bridgeOS")
        return Platform::bridgeOS;
    if (str == "iOSMac")
        return Platform::iOSMac;
    if (str == "UIKitForMac")
        return Platform::iOSMac;
    if (str == "iOS_simulator")
        return Platform::iOS_simulator;
    if (str == "tvOS_simulator")
        return Platform::tvOS_simulator;
    if (str == "watchOS_simulator")
        return Platform::watchOS_simulator;
    return Platform::unknown;
}

static int dumpAppCache(const DumpOptions& options) {
    // Verify any required options
    if (gOpts.archs.size() != 1)
        exit_usage("-arch");
    if (gOpts.platform == nullptr)
        exit_usage("-platform");

    if (gOpts.appCachePath == nullptr)
        exit_usage();

    FileSystemPhysical fileSystem;
    if (!fileSystem.fileExists(gOpts.appCachePath)) {
        fprintf(stderr, "App-cache path does not exist: %s\n", gOpts.appCachePath);
        return 1;
    }

    const GradedArchs& archs = GradedArchs::forName(gOpts.archs[0]);
    Platform platform = Platform::unknown;
    bool isKernelCollection = false;

    // HACK: Pass a real option for building a kernel app cache
    if (!strcmp(gOpts.platform, "kernel")) {
        isKernelCollection = true;
    } else {
        platform = stringToPlatform(gOpts.platform);
        if (platform == Platform::unknown) {
            fprintf(stderr, "Could not create app cache because: unknown platform '%s'\n", gOpts.platform);
            return 1;
        }
    }

    __block Diagnostics diag;
    char appCacheRealPath[MAXPATHLEN];
    LoadedFileInfo loadedFileInfo = MachOAnalyzer::load(diag, fileSystem, gOpts.appCachePath, archs, platform, appCacheRealPath);
    if (diag.hasError()) {
        fprintf(stderr, "Could not load app cache because: %s\n", diag.errorMessage().c_str());
        return 1;
    }

    MachOAppCache* appCacheMA = (MachOAppCache*)loadedFileInfo.fileContent;
    if (appCacheMA == nullptr) {
        fprintf(stderr, "Could not load app cache: %s\n", gOpts.appCachePath);
        return 1;
    }

    if (options.printLayout) {
        __block Node topNode;

        // Add the segments for the app cache
        __block Node segmentsNode;
        __block bool hasError = false;
        appCacheMA->forEachSegment(^(const dyld3::MachOFile::SegmentInfo &info, bool &stop) {
            Node segmentNode;
            segmentNode.map["name"] = makeNode(info.segName);
            segmentNode.map["vmAddr"] = makeNode(hex(info.vmAddr));
            segmentNode.map["vmSize"] = makeNode(hex(info.vmSize));
            segmentNode.map["vmEnd"] = makeNode(hex(info.vmAddr + info.vmSize));
            switch (info.protections) {
                case VM_PROT_READ:
                    segmentNode.map["permissions"] = makeNode("r--");
                    break;
                case VM_PROT_WRITE:
                    segmentNode.map["permissions"] = makeNode("-w-");
                    break;
                case VM_PROT_EXECUTE:
                    segmentNode.map["permissions"] = makeNode("--x");
                    break;
                case VM_PROT_READ | VM_PROT_WRITE:
                    segmentNode.map["permissions"] = makeNode("rw-");
                    break;
                case VM_PROT_READ | VM_PROT_EXECUTE:
                    segmentNode.map["permissions"] = makeNode("r-x");
                    break;
                case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
                    segmentNode.map["permissions"] = makeNode("rwx");
                    break;
                default:
                    fprintf(stderr, "Unknown permissions on segment '%s'\n", info.segName);
                    hasError = true;
                    stop = true;
            }

            __block Node sectionsNode;
            appCacheMA->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo &sectInfo, bool malformedSectionRange, bool &stop) {
                if ( strncmp(sectInfo.segInfo.segName, info.segName, 16) != 0 )
                    return;

                Node sectionNode;
                sectionNode.map["name"] = makeNode(sectInfo.sectName);
                sectionNode.map["vmAddr"] = makeNode(hex(sectInfo.sectAddr));
                sectionNode.map["vmSize"] = makeNode(hex(sectInfo.sectSize));
                sectionNode.map["vmEnd"] = makeNode(hex(sectInfo.sectAddr + sectInfo.sectSize));

                sectionsNode.array.push_back(sectionNode);
            });

            if ( !sectionsNode.array.empty() ) {
                segmentNode.map["sections"] = sectionsNode;
            }

            segmentsNode.array.push_back(segmentNode);
        });

        if (hasError)
            return 1;

        topNode.map["cache-segments"] = segmentsNode;

        // Map from name to relative path
        __block std::unordered_map<std::string, std::string> relativePaths;
        appCacheMA->forEachPrelinkInfoLibrary(diag, ^(const char *bundleName, const char* relativePath,
                                                      const std::vector<const char *> &deps) {
            if ( relativePath != nullptr )
                relativePaths[bundleName] = relativePath;
        });

        __block Node dylibsNode;
        appCacheMA->forEachDylib(diag, ^(const MachOAnalyzer *ma, const char *name, bool &stop) {
            __block Node segmentsNode;
            ma->forEachSegment(^(const dyld3::MachOFile::SegmentInfo &info, bool &stop) {
                Node segmentNode;
                segmentNode.map["name"] = makeNode(info.segName);
                segmentNode.map["vmAddr"] = makeNode(hex(info.vmAddr));
                segmentNode.map["vmSize"] = makeNode(hex(info.vmSize));
                segmentNode.map["vmEnd"] = makeNode(hex(info.vmAddr + info.vmSize));

                switch (info.protections) {
                    case VM_PROT_READ:
                        segmentNode.map["permissions"] = makeNode("r--");
                        break;
                    case VM_PROT_WRITE:
                        segmentNode.map["permissions"] = makeNode("-w-");
                        break;
                    case VM_PROT_EXECUTE:
                        segmentNode.map["permissions"] = makeNode("--x");
                        break;
                    case VM_PROT_READ | VM_PROT_WRITE:
                        segmentNode.map["permissions"] = makeNode("rw-");
                        break;
                    case VM_PROT_READ | VM_PROT_EXECUTE:
                        segmentNode.map["permissions"] = makeNode("r-x");
                        break;
                    default:
                        fprintf(stderr, "Unknown permissions on segment '%s'\n", info.segName);
                        hasError = true;
                        stop = true;
                }

                __block Node sectionsNode;
                ma->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo &sectInfo, bool malformedSectionRange, bool &stop) {
                    if ( strncmp(sectInfo.segInfo.segName, info.segName, 16) != 0 )
                        return;

                    Node sectionNode;
                    sectionNode.map["name"] = makeNode(sectInfo.sectName);
                    sectionNode.map["vmAddr"] = makeNode(hex(sectInfo.sectAddr));
                    sectionNode.map["vmSize"] = makeNode(hex(sectInfo.sectSize));
                    sectionNode.map["vmEnd"] = makeNode(hex(sectInfo.sectAddr + sectInfo.sectSize));

                    sectionsNode.array.push_back(sectionNode);
                });

                if ( !sectionsNode.array.empty() ) {
                    segmentNode.map["sections"] = sectionsNode;
                }

                segmentsNode.array.push_back(segmentNode);
            });

            Node dylibNode;
            dylibNode.map["name"] = makeNode(name);
            dylibNode.map["segments"] = segmentsNode;

            auto relativePathIt = relativePaths.find(name);
            if ( relativePathIt != relativePaths.end() )
                dylibNode.map["relativePath"] = makeNode(relativePathIt->second);

            dylibsNode.array.push_back(dylibNode);
        });

        topNode.map["dylibs"] = dylibsNode;

        printJSON(topNode, 0, std::cout);
    }

    if (options.printEntryPoint) {
        __block Node topNode;

        // add entry
        uint64_t    entryOffset;
        bool        usesCRT;
        Node        entryPointNode;
        if ( appCacheMA->getEntry(entryOffset, usesCRT) ) {
            entryPointNode.value = hex(appCacheMA->preferredLoadAddress() + entryOffset);
        }

        topNode.map["entrypoint"] = entryPointNode;

        printJSON(topNode, 0, std::cout);
    }

    if (options.printFixups) {
        __block Node topNode;

        __block uint64_t baseAddress = ~0ULL;
        appCacheMA->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo& info, bool& stop) {
            baseAddress = std::min(baseAddress, info.vmAddr);
        });
        uint64_t cacheBaseAddress = baseAddress;
        uint64_t textSegVMAddr = appCacheMA->preferredLoadAddress();

        auto getFixupsNode = [cacheBaseAddress, textSegVMAddr](const dyld3::MachOAnalyzer* ma) {
            __block Node fixupsNode;

            if (!ma->hasChainedFixups()) {
                return makeNode("none");
            }

            // Keep track of the fixups seen by chained fixups.  The remainder might be
            // classic relocs if we are the x86_64 kernel collection
            __block std::set<uint64_t> seenFixupVMOffsets;

            __block Diagnostics diag;
            ma->withChainStarts(diag, 0, ^(const dyld_chained_starts_in_image* starts) {
                ma->forEachFixupInAllChains(diag, starts, false, ^(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, const dyld_chained_starts_in_segment* segInfo, bool& stop) {
                    uint64_t vmOffset = (uint8_t*)fixupLoc - (uint8_t*)ma;
                    seenFixupVMOffsets.insert(vmOffset);

                    // Correct for __DATA being before __TEXT, in which case the offset
                    // is from __DATA, not a mach header offset
                    vmOffset += (textSegVMAddr - cacheBaseAddress);

                    fixupsNode.map[hex(vmOffset)] = makeNode("fixup");
                    switch (segInfo->pointer_format) {
                        case DYLD_CHAINED_PTR_64_KERNEL_CACHE:
                        case DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE: {
                            uint64_t targetVMOffset = fixupLoc->kernel64.target;
                            uint64_t targetVMAddr = targetVMOffset + cacheBaseAddress;
                            std::string level = "kc(" + decimal(fixupLoc->kernel64.cacheLevel) + ")";
                            std::string fixup = level + " + " + hex(targetVMAddr);
                            if (fixupLoc->kernel64.isAuth) {
                                fixup += " auth(";
                                fixup += fixupLoc->kernel64.keyName();
                                fixup += " ";
                                fixup += fixupLoc->kernel64.addrDiv ? "addr" : "!addr";
                                fixup += " ";
                                fixup += decimal(fixupLoc->kernel64.diversity);
                                fixup += ")";
                            }
                            fixupsNode.map[hex(vmOffset)] = makeNode(fixup);
                            break;
                        }
                        default:
                            diag.error("unknown pointer type %d", segInfo->pointer_format);
                            break;
                    }
                 });
            });
            diag.assertNoError();

            ma->forEachRebase(diag, ^(const char *opcodeName, const dyld3::MachOAnalyzer::LinkEditInfo &leInfo,
                                      const dyld3::MachOAnalyzer::SegmentInfo *segments,
                                      bool segIndexSet, uint32_t pointerSize, uint8_t segmentIndex,
                                      uint64_t segmentOffset, dyld3::MachOAnalyzer::Rebase kind, bool &stop) {
                uint64_t rebaseVmAddr  = segments[segmentIndex].vmAddr + segmentOffset;
                uint64_t runtimeOffset = rebaseVmAddr - textSegVMAddr;
                const uint8_t* fixupLoc = (const uint8_t*)ma + runtimeOffset;

                // Correct for __DATA being before __TEXT, in which case the offset
                // is from __DATA, not a mach header offset
                runtimeOffset += (textSegVMAddr - cacheBaseAddress);

                std::string fixup = "kc(0) + ";
                switch ( kind ) {
                    case dyld3::MachOAnalyzer::Rebase::unknown:
                        fixup += " : unhandled";
                        break;
                    case dyld3::MachOAnalyzer::Rebase::pointer32: {
                        uint32_t value = *(uint32_t*)(fixupLoc);
                        fixup += hex(value) + " : pointer32";
                        break;
                    }
                    case dyld3::MachOAnalyzer::Rebase::pointer64: {
                        uint64_t value = *(uint64_t*)(fixupLoc);
                        fixup += hex(value) + " : pointer64";
                        break;
                    }
                    case dyld3::MachOAnalyzer::Rebase::textPCrel32:
                        fixup += " : pcrel32";
                        break;
                    case dyld3::MachOAnalyzer::Rebase::textAbsolute32:
                        fixup += " : absolute32";
                        break;
                }
                fixupsNode.map[hex(runtimeOffset)] = makeNode(fixup);
            });
            diag.assertNoError();

            return fixupsNode;
        };

        topNode.map["fixups"] = getFixupsNode(appCacheMA);

        __block Node dylibsNode;
        appCacheMA->forEachDylib(diag, ^(const MachOAnalyzer *ma, const char *name, bool &stop) {
            Node dylibNode;
            dylibNode.map["name"] = makeNode(name);
            dylibNode.map["fixups"] = getFixupsNode(ma);

            dylibsNode.array.push_back(dylibNode);
        });

        topNode.map["dylibs"] = dylibsNode;

        printJSON(topNode, 0, std::cout);
    }

    if (options.printSymbols) {
        __block Node topNode;

        __block Node dylibsNode;
        appCacheMA->forEachDylib(diag, ^(const MachOAnalyzer *ma, const char *name, bool &stop) {
            __block Node globalSymbolsNode;
            ma->forEachGlobalSymbol(diag, ^(const char *symbolName, uint64_t n_value, uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool &stop) {
                Node symbolNode;
                symbolNode.map["name"] = makeNode(symbolName);
                symbolNode.map["vmAddr"] = makeNode(hex(n_value));

                globalSymbolsNode.array.push_back(symbolNode);
            });

            __block Node localSymbolsNode;
            ma->forEachLocalSymbol(diag, ^(const char *symbolName, uint64_t n_value, uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool &stop) {
                Node symbolNode;
                symbolNode.map["name"] = makeNode(symbolName);
                symbolNode.map["vmAddr"] = makeNode(hex(n_value));

                localSymbolsNode.array.push_back(symbolNode);
            });

            if (globalSymbolsNode.array.empty())
                globalSymbolsNode = makeNode("none");
            if (localSymbolsNode.array.empty())
                localSymbolsNode = makeNode("none");

            Node dylibNode;
            dylibNode.map["name"] = makeNode(name);
            dylibNode.map["global-symbols"] = globalSymbolsNode;
            dylibNode.map["local-symbols"] = localSymbolsNode;

            dylibsNode.array.push_back(dylibNode);
        });

        topNode.map["dylibs"] = dylibsNode;

        printJSON(topNode, 0, std::cout);
    }

    if (options.printUUID) {
        __block Node topNode;

        // add uuid
        Node uuidNode;
        uuid_t uuid = {};
        if ( appCacheMA->getUuid(uuid) ) {
            uuid_string_t uuidString;
            uuid_unparse_upper(uuid, uuidString);
            uuidNode.value = uuidString;
        }

        topNode.map["uuid"] = uuidNode;

        auto getPlistUUID = ^(const char* jsonNodeName, CFStringRef keyName) {
            uuid_t uuid = {};
            const uint8_t* prelinkInfoBuffer = nullptr;
            uint64_t prelinkInfoBufferSize = 0;
            prelinkInfoBuffer = (const uint8_t*)appCacheMA->findSectionContent("__PRELINK_INFO", "__info", prelinkInfoBufferSize);
            if ( prelinkInfoBuffer != nullptr ) {
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
                CFDataRef uuidDataRef = (CFDataRef)CFDictionaryGetValue((CFDictionaryRef)plistRef, keyName);
                if ( uuidDataRef != nullptr ) {
                    CFDataGetBytes(uuidDataRef, CFRangeMake(0, CFDataGetLength(uuidDataRef)), uuid);

                    Node uuidNode;
                    uuid_string_t uuidString;
                    uuid_unparse_upper(uuid, uuidString);
                    uuidNode.value = uuidString;
                    topNode.map[jsonNodeName] = uuidNode;
                }
                CFRelease(plistRef);
                CFRelease(readStreamRef);
            }
        };

        getPlistUUID("prelink-info-uuid", CFSTR("_PrelinkKCID"));

        // If we are an auxKC, then we should also have a reference to the baseKC UUID
        getPlistUUID("prelink-info-base-uuid", CFSTR("_BootKCID"));

        // If we are an pageableKC, then we should also have a reference to the pageableKC UUID
        getPlistUUID("prelink-info-pageable-uuid", CFSTR("_PageableKCID"));

        printJSON(topNode, 0, std::cout);
    }

    if (options.printKModInfo) {
        __block Node topNode;

        appCacheMA->forEachDylib(diag, ^(const MachOAnalyzer *ma, const char *name, bool &stop) {
            Node dylibNode;
            dylibNode.map["name"] = makeNode(name);

            // Check for a global first
            __block uint64_t kmodInfoVMOffset = 0;
            __block bool found = false;
            {
                dyld3::MachOAnalyzer::FoundSymbol foundInfo;
                found = ma->findExportedSymbol(diag, "_kmod_info", true, foundInfo, nullptr);
                if ( found ) {
                    kmodInfoVMOffset = foundInfo.value;
                }
            }
            // And fall back to a local if we need to
            if ( !found ) {
                ma->forEachLocalSymbol(diag, ^(const char* aSymbolName, uint64_t n_value, uint8_t n_type,
                                               uint8_t n_sect, uint16_t n_desc, bool& stop) {
                    if ( strcmp(aSymbolName, "_kmod_info") == 0 ) {
                        kmodInfoVMOffset = n_value - ma->preferredLoadAddress();
                        found = true;
                        stop = true;
                    }
                });
            }

            if ( found ) {
                dyld3::MachOAppCache::KModInfo64_v1* kmodInfo = (dyld3::MachOAppCache::KModInfo64_v1*)((uint8_t*)ma + kmodInfoVMOffset);
                Node kmodInfoNode;
                kmodInfoNode.map["info-version"] = makeNode(decimal(kmodInfo->info_version));
                kmodInfoNode.map["name"] = makeNode((const char*)&kmodInfo->name[0]);
                kmodInfoNode.map["version"] = makeNode((const char*)&kmodInfo->version[0]);
                kmodInfoNode.map["address"] = makeNode(hex(kmodInfo->address));
                kmodInfoNode.map["size"] = makeNode(hex(kmodInfo->size));

                dylibNode.map["kmod_info"] = kmodInfoNode;
            } else {
                dylibNode.map["kmod_info"] = makeNode("none");
            }

            topNode.array.push_back(dylibNode);
        });

        printJSON(topNode, 0, std::cout);
    }

    if (options.printFIPS) {
        __block Node topNode;

        appCacheMA->forEachDylib(diag, ^(const MachOAnalyzer *ma, const char *name, bool &stop) {
            if ( strcmp(name, "com.apple.kec.corecrypto") != 0 )
                return;

            uint64_t hashStoreSize;
            const void* hashStoreLocation = ma->findSectionContent("__TEXT", "__fips_hmacs", hashStoreSize);
            assert(hashStoreLocation != nullptr);

            const uint8_t* hashBuffer = (const uint8_t*)hashStoreLocation;
            std::string hashString;

            for (int i = 0; i < hashStoreSize; ++i) {
                uint8_t byte = hashBuffer[i];
                uint8_t nibbleL = byte & 0x0F;
                uint8_t nibbleH = byte >> 4;
                if ( nibbleH < 10 ) {
                    hashString += '0' + nibbleH;
                } else {
                    hashString += 'a' + (nibbleH-10);
                }
                if ( nibbleL < 10 ) {
                    hashString += '0' + nibbleL;
                } else {
                    hashString += 'a' + (nibbleL-10);
                }
            }

            stop = true;

            topNode.map["fips"] = makeNode(hashString);
        });

        printJSON(topNode, 0, std::cout);
    }

    return 0;
}

static int validateFile(const ValidateOptions& options) {
    // Verify any required options
    if (gOpts.archs.size() != 1)
        exit_usage("-arch");
    if (gOpts.platform == nullptr)
        exit_usage("-platform");
    if (options.filePath == nullptr)
        exit_usage();

    const GradedArchs& archs = GradedArchs::forName(gOpts.archs[0]);
    Platform platform = Platform::unknown;

    // HACK: Pass a real option for building a kernel app cache
    if (strcmp(gOpts.platform, "kernel")) {
        fprintf(stderr, "Could not create app cache because: unknown platform '%s'\n", gOpts.platform);
        return 1;
    }

    __block Diagnostics diag;

    std::string file = options.filePath;
    {
        FileSystemPhysical fileSystem;
        char fileRealPath[MAXPATHLEN];
        LoadedFileInfo loadedFileInfo = MachOAnalyzer::load(diag, fileSystem, file.c_str(), archs, platform, fileRealPath);
        if (diag.hasError()) {
            fprintf(stderr, "Could not load file '%s' because: %s\n", file.c_str(), diag.errorMessage().c_str());
            diag.clearError();
            return 1;
        }

        MachOAnalyzer* ma = (MachOAnalyzer*)loadedFileInfo.fileContent;
        if (ma == nullptr) {
            fprintf(stderr, "Could not load file: %s\n", file.c_str());
            return 1;
        }

        auto errorHandler = ^(const char* msg) {
            diag.warning("File '%s' cannot be placed in kernel collection because: %s", file.c_str(), msg);
        };
        if (ma->canBePlacedInKernelCollection(file.c_str(), errorHandler)) {
            return 0;
        } else {
            fileSystem.unloadFile(loadedFileInfo);
        }
    }

    {
        // Since we found no files, print warnings for the ones we tried
        if (diag.warnings().empty()) {
            fprintf(stderr, "File '%s' was not valid for app-cache\n", file.c_str());
        } else {
            for (const std::string& msg : diag.warnings()) {
                fprintf(stderr, "  %s\n", msg.c_str());
            }
        }
        return 1;
    }

    return 0;
}

static void forEachBundle(const char* bundlesDirectoryPath,
                          void (^callback)(CFBundleRef bundleRef, const char* bundleName)) {
    CFStringRef sourcePath = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, bundlesDirectoryPath,
                                                             kCFStringEncodingASCII, kCFAllocatorNull);
    CFURLRef sourceURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, sourcePath,
                                                       kCFURLPOSIXPathStyle, true);
    CFArrayRef bundles = CFBundleCreateBundlesFromDirectory(kCFAllocatorDefault, sourceURL, nullptr);

    for (CFIndex i = 0, e = CFArrayGetCount(bundles); i != e; ++i) {
        CFBundleRef bundleRef = (CFBundleRef)CFArrayGetValueAtIndex(bundles, i);
        CFStringRef bundleID = CFBundleGetIdentifier(bundleRef);
        if (!bundleID)
            continue;
        const char* bundleName = CFStringGetCStringPtr(bundleID, kCFStringEncodingASCII);
        callback(bundleRef, bundleName);
    }

    CFRelease(sourcePath);
    CFRelease(sourceURL);
    CFRelease(bundles);
}

static int listBundles(const ListBundlesOptions& options) {
    // Verify any required options
    if (options.directoryPath == nullptr)
        exit_usage();

    forEachBundle(options.directoryPath, ^(CFBundleRef bundleRef, const char* bundleName){
        printf("Bundle: %s\n", bundleName);
    });

    return 0;
}

static CFDataRef
createKernelCollectionForArch(const CreateKernelCollectionOptions& options, const char* arch,
                              Diagnostics& diag) {
    const GradedArchs& archs = GradedArchs::forName(arch);
    Platform platform = Platform::unknown;


    KernelCollectionBuilder* kcb = nullptr;
    {
        CFStringRef archStringRef = CFStringCreateWithCString(kCFAllocatorDefault, arch, kCFStringEncodingASCII);
        BuildOptions_v1 buildOptions = { 1, options.collectionKind, options.stripMode, archStringRef, options.verbose };
        kcb = createKernelCollectionBuilder(&buildOptions);
        CFRelease(archStringRef);
    }

    FileSystemPhysical fileSystem;
    LoadedFileInfo kernelCollectionLoadedFileInfo;

    auto loadKernelCollection = ^(const char* kernelCollectionPath, CollectionKind collectionKind) {
        if (!fileSystem.fileExists(kernelCollectionPath)) {
            fprintf(stderr, "kernel collection path does not exist: %s\n", options.kernelPath);
            return false;
        }
        LoadedFileInfo info;
        char realerPath[MAXPATHLEN];
        bool loadedFile = fileSystem.loadFile(kernelCollectionPath, info, realerPath, ^(const char *format, ...) {
            va_list list;
            va_start(list, format);
            diag.error(format, list);
            va_end(list);
        });
        if ( !loadedFile )
            return false;
        CFStringRef pathStringRef = CFStringCreateWithCString(kCFAllocatorDefault, kernelCollectionPath, kCFStringEncodingASCII);
        CFDataRef dataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const uint8_t*)info.fileContent, info.fileContentLen, kCFAllocatorNull);
        if ( !addCollectionFile(kcb, pathStringRef, dataRef, collectionKind) ) {
            diag.error("Could not load kernel collection file");
            return false;
        }
        CFRelease(dataRef);
        CFRelease(pathStringRef);
        return true;
    };

    switch (options.collectionKind) {
        case unknownKC:
            fprintf(stderr, "Invalid kernel collection kind\n");
            exit(1);
        case baseKC: {
            if (!fileSystem.fileExists(options.kernelPath)) {
                fprintf(stderr, "Kernel path does not exist: %s\n", options.kernelPath);
                return {};
            }
            LoadedFileInfo info;
            char realerPath[MAXPATHLEN];
            bool loadedFile = fileSystem.loadFile(options.kernelPath, info, realerPath, ^(const char *format, ...) {
                va_list list;
                va_start(list, format);
                diag.error(format, list);
                va_end(list);
            });
            if ( !loadedFile )
                return {};
            CFStringRef pathStringRef = CFStringCreateWithCString(kCFAllocatorDefault, options.kernelPath, kCFStringEncodingASCII);
            CFDataRef dataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const uint8_t*)info.fileContent, info.fileContentLen, kCFAllocatorNull);
            if ( !addKernelFile(kcb, pathStringRef, dataRef) ) {
                uint64_t errorCount = 0;
                const char* const* errors = getErrors(kcb, &errorCount);
                for (uint64_t i = 0; i != errorCount; ++i)
                    diag.error("Could not load kernel file because: '%s'", errors[i]);
                return {};
            }
            CFRelease(dataRef);
            CFRelease(pathStringRef);
            break;
        }
        case pageableKC:
            if ( !loadKernelCollection(options.kernelCollectionPath, baseKC) )
                return {};
            break;
        case auxKC:
            if ( !loadKernelCollection(options.kernelCollectionPath, baseKC) )
                return {};
            // Pageable is optional
            if ( options.pageableCollectionPath != nullptr ) {
                if ( !loadKernelCollection(options.pageableCollectionPath, pageableKC) )
                    return {};
            }
            break;
    }

    if ( !options.bundleIDs.empty() ) {
        struct BundleData {
            std::string                 executablePath;
            std::string                 bundlePath;
            std::vector<std::string>    dependencies;
            CFDictionaryRef             infoPlist       = nullptr;
        };
        __block std::map<std::string, BundleData> foundBundles;

        // Look for bundles in the extensions directory and any PlugIns directories its kext's contain
        __block std::list<std::pair<std::string, bool>> kextDirectoriesToProcess;
        kextDirectoriesToProcess.push_back({ options.extensionsPath, true });
        while ( !kextDirectoriesToProcess.empty() ) {
            std::string kextDir = kextDirectoriesToProcess.front().first;
            bool lookForPlugins = kextDirectoriesToProcess.front().second;
            kextDirectoriesToProcess.pop_front();

            __block bool foundError = false;
            forEachBundle(kextDir.c_str(), ^(CFBundleRef bundleRef, const char* bundleName) {

                if (foundError)
                    return;

                // If the directory contains a PlugIns directory, then add it to the list to seach for kexts
                if (lookForPlugins) {
                    CFURLRef pluginsRelativeURL = CFBundleCopyBuiltInPlugInsURL(bundleRef);
                    if ( pluginsRelativeURL != nullptr ) {
                        CFURLRef pluginsAbsoluteURL = CFURLCopyAbsoluteURL(pluginsRelativeURL);
                        CFStringRef pluginString = CFURLCopyFileSystemPath(pluginsAbsoluteURL, kCFURLPOSIXPathStyle);
                        const char* pluginPath = CFStringGetCStringPtr(pluginString, kCFStringEncodingASCII);
                        kextDirectoriesToProcess.push_back({ pluginPath, false });

                        CFRelease(pluginString);
                        CFRelease(pluginsAbsoluteURL);
                        CFRelease(pluginsRelativeURL);
                    }
                }

#if 0
                // For now always load bundles as we don't require every bundle to be listed on the command line
                // but can instead bring them in on demand.

                // Once we've looked for plugins, if we don't want this bundle then we can skip validating it.
                if ( foundBundles.count(bundleName) == 0 )
                    return;
#endif

                BundleData bundleData;
                bundleData.infoPlist = CFBundleGetInfoDictionary(bundleRef);

                CFURLRef bundleExecutableRelativeURL = CFBundleCopyExecutableURL(bundleRef);

                // Its ok to be missing an executable.  We'll just skip this bundle
                if ( bundleExecutableRelativeURL == nullptr ) {
                    // FIXME: Its possibly not ok to be missing the executable if its actually listed
                    // as a CFBundleExecutable path in the plist
                    foundBundles[bundleName] = bundleData;
                    return;
                }

                CFURLRef bundleExecutableAbsoluteURL = CFURLCopyAbsoluteURL(bundleExecutableRelativeURL);
                CFStringRef bundleExecutableString = CFURLCopyFileSystemPath(bundleExecutableAbsoluteURL, kCFURLPOSIXPathStyle);
                const char* bundleExecutablePath = CFStringGetCStringPtr(bundleExecutableString, kCFStringEncodingASCII);

                // Check for an arch specific dependency list first
                std::string archBundleLibraries = std::string("OSBundleLibraries") + "_" + arch;
                CFStringRef archBundleLibrariesStringRef = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, archBundleLibraries.c_str(),
                                                                                           kCFStringEncodingASCII, kCFAllocatorNull);
                CFTypeRef depsRef = CFBundleGetValueForInfoDictionaryKey(bundleRef, archBundleLibrariesStringRef);
                if ( depsRef == nullptr ) {
                    // No arch specific deps, so try the defaults
                    depsRef = CFBundleGetValueForInfoDictionaryKey(bundleRef, CFSTR("OSBundleLibraries"));
                }
                if (depsRef != nullptr) {
                    if (CFGetTypeID(depsRef) != CFDictionaryGetTypeID()) {
                        fprintf(stderr, "Bad bundle '%s' (\"OSBundleLibraries\" is not a dictionary)\n", bundleName);
                        foundError = true;
                        return;
                    }

                    CFDictionaryRef dictRef = (CFDictionaryRef)depsRef;
                    CFDictionaryApplyFunction(dictRef, [](const void *key, const void *value, void *context) {
                        BundleData* bundleData = (BundleData*)context;
                        CFStringRef keyRef = (CFStringRef)key;
                        //CFStringRef valueRef = (CFStringRef)value;
                        bundleData->dependencies.push_back(CFStringGetCStringPtr(keyRef, kCFStringEncodingASCII));
                    }, &bundleData);
                }

                // Make sure no-one tries to link the kernel directly.  They must do so via symbol sets
                if ( !bundleData.dependencies.empty() ) {
                    for (const std::string& dep : bundleData.dependencies) {
                        if (dep == "com.apple.kernel") {
                            fprintf(stderr, "Rejecting bundle '%s' as it is trying to link directly to the kernel\n",
                                    bundleName);
                            foundError = true;
                            return;
                        }
                    }
                }

                bundleData.executablePath = bundleExecutablePath;

                CFURLRef bundleURLRef = CFBundleCopyBundleURL(bundleRef);
                CFStringRef bundleURLString = CFURLCopyFileSystemPath(bundleURLRef, kCFURLPOSIXPathStyle);
                const char* bundleURLPath = CFStringGetCStringPtr(bundleURLString, kCFStringEncodingASCII);
                if (strncmp(bundleURLPath, options.extensionsPath, strlen(options.extensionsPath)) != 0) {
                    fprintf(stderr, "Bundle path '%s' is not within extensions directory '%s'\n",
                            bundleURLPath, options.extensionsPath);
                }
                // Don't remove the whole extensions prefix, but instead the volume root, if we have one
                bundleData.bundlePath = bundleURLPath + strlen(options.volumeRoot);
                foundBundles[bundleName] = bundleData;

                CFRelease(bundleExecutableString);
                CFRelease(bundleExecutableAbsoluteURL);
                CFRelease(bundleExecutableRelativeURL);
            });

            if (foundError)
                return {};
        }

        __block std::set<std::string> existingBundles;
        auto addSymbolSetsBundleIDs = ^(const dyld3::MachOAnalyzer* kernelMA) {
            assert(kernelMA != nullptr);

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

            uint64_t symbolSetsSize = 0;
            const void* symbolSetsContent = kernelMA->findSectionContent("__LINKINFO", "__symbolsets", symbolSetsSize);
            if ( symbolSetsContent != nullptr ) {

                // A helper to automatically call CFRelease when we go out of scope
                struct AutoReleaseTypeRef {
                    AutoReleaseTypeRef() = default;
                    ~AutoReleaseTypeRef() {
                        if ( ref != nullptr ) {
                            CFRelease(ref);
                        }
                    }
                    void setRef(CFTypeRef typeRef) {
                        assert(ref == nullptr);
                        ref = typeRef;
                    }

                    CFTypeRef ref = nullptr;
                };

                AutoReleaseTypeRef dataRefReleaser;
                AutoReleaseTypeRef plistRefReleaser;

                CFDataRef dataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const uint8_t*)symbolSetsContent, symbolSetsSize, kCFAllocatorNull);
                if ( dataRef == nullptr ) {
                    diag.error("Could not create data ref for symbol sets");
                    return false;
                }
                dataRefReleaser.setRef(dataRef);

                CFErrorRef errorRef = nullptr;
                CFPropertyListRef plistRef = CFPropertyListCreateWithData(kCFAllocatorDefault, dataRef, kCFPropertyListImmutable, nullptr, &errorRef);
                if (errorRef != nullptr) {
                    CFStringRef errorString = CFErrorCopyDescription(errorRef);
                    diag.error("Could not load plist because :%s",
                               CFStringGetCStringPtr(errorString, kCFStringEncodingASCII));
                    CFRelease(errorRef);
                    return false;
                }
                if ( plistRef == nullptr ) {
                    diag.error("Could not create plist ref for symbol sets");
                    return false;
                }
                plistRefReleaser.setRef(plistRef);

                if ( CFGetTypeID(plistRef) != CFDictionaryGetTypeID() ) {
                    diag.error("Symbol set plist should be a dictionary");
                    return false;
                }
                CFDictionaryRef symbolSetsDictRef = (CFDictionaryRef)plistRef;
                CFArrayRef symbolSetArrayRef = (CFArrayRef)CFDictionaryGetValue(symbolSetsDictRef, CFSTR("SymbolsSets"));
                if ( symbolSetArrayRef != nullptr ) {
                    if ( CFGetTypeID(symbolSetArrayRef) != CFArrayGetTypeID() ) {
                        diag.error("SymbolsSets value should be an array");
                        return false;
                    }
                    for (CFIndex symbolSetIndex = 0; symbolSetIndex != CFArrayGetCount(symbolSetArrayRef); ++symbolSetIndex) {
                        CFDictionaryRef symbolSetDictRef = (CFDictionaryRef)CFArrayGetValueAtIndex(symbolSetArrayRef, symbolSetIndex);
                        if ( CFGetTypeID(symbolSetDictRef) != CFDictionaryGetTypeID() ) {
                            diag.error("Symbol set element should be a dictionary");
                            return false;
                        }

                        // CFBundleIdentifier
                        CFStringRef bundleIDRef = (CFStringRef)CFDictionaryGetValue(symbolSetDictRef, CFSTR("CFBundleIdentifier"));
                        if ( (bundleIDRef == nullptr) || (CFGetTypeID(bundleIDRef) != CFStringGetTypeID()) ) {
                            diag.error("Symbol set bundle ID should be a string");
                            return false;
                        }

                        const char* dylibID = getString(diag, bundleIDRef);
                        if ( dylibID == nullptr )
                            return false;
                        existingBundles.insert(dylibID);
                    }
                }
            }
            return true;
        };

        auto addExistingBundleIDs = ^(const char* path, bool isBaseKC) {
            char fileRealPath[MAXPATHLEN];
            auto kernelCollectionLoadedFileInfo = MachOAnalyzer::load(diag, fileSystem, path, archs, platform, fileRealPath);
            if ( diag.hasError() ) {
                fprintf(stderr, "Could not load file '%s' because: %s\n", path, diag.errorMessage().c_str());
                return false;
            }
            const MachOAppCache* appCacheMA = (const MachOAppCache*)kernelCollectionLoadedFileInfo.fileContent;
            if (appCacheMA == nullptr) {
                fprintf(stderr, "Could not load file: %s\n", path);
                return false;
            }
            if ( !appCacheMA->isFileSet() ) {
                fprintf(stderr, "kernel collection is not a cache file: %s\n", path);
                return false;
            }
            __block const dyld3::MachOAnalyzer* kernelMA = nullptr;
            appCacheMA->forEachDylib(diag, ^(const MachOAnalyzer *ma, const char *name, bool &stop) {
                if ( ma->isStaticExecutable() ) {
                    kernelMA = ma;
                }
                existingBundles.insert(name);
            });

            if ( isBaseKC ) {
                if ( !addSymbolSetsBundleIDs(kernelMA) )
                    return false;
            }
            fileSystem.unloadFile(kernelCollectionLoadedFileInfo);
            return true;
        };
        if ( options.collectionKind == baseKC ) {
            char fileRealPath[MAXPATHLEN];
            auto kernelLoadedFileInfo = MachOAnalyzer::load(diag, fileSystem, options.kernelPath, archs, platform, fileRealPath);
            if ( diag.hasError() ) {
                fprintf(stderr, "Could not load file '%s' because: %s\n", options.kernelPath, diag.errorMessage().c_str());
                return {};
            }
            const MachOAppCache* kernelMA = (const MachOAppCache*)kernelLoadedFileInfo.fileContent;
            if (kernelMA == nullptr) {
                fprintf(stderr, "Could not load file: %s\n", options.kernelPath);
                return {};
            }
            if ( !kernelMA->isStaticExecutable() ) {
                fprintf(stderr, "kernel is not a static executable: %s\n", options.kernelPath);
                return {};
            }

            if ( !addSymbolSetsBundleIDs(kernelMA) )
                return {};
            fileSystem.unloadFile(kernelLoadedFileInfo);
        }
        if ( (options.collectionKind == auxKC) || (options.collectionKind == pageableKC) ) {
            // Work out which bundle-ids are already in the base KC
            if ( !addExistingBundleIDs(options.kernelCollectionPath, true) )
                return {};
        }
        if ( options.pageableCollectionPath != nullptr ) {
            // Work out which bundle-ids are already in the pageable KC
            if ( !addExistingBundleIDs(options.pageableCollectionPath, false) )
                return {};
        }

        std::set<std::string> processedBundleIDs;
        std::list<const char*> bundleIDsToLoad;
        bundleIDsToLoad.insert(bundleIDsToLoad.end(), options.bundleIDs.begin(), options.bundleIDs.end());
        while (!bundleIDsToLoad.empty()) {
            std::string bundleID = bundleIDsToLoad.front();
            bundleIDsToLoad.pop_front();

            std::string stripModeString;
            if ( const char* colonPos = strchr(bundleID.c_str(), ':') ) {
                stripModeString = colonPos + 1;
                bundleID.erase(colonPos - bundleID.data());
            }

            // If we've seen this one already then skip it
            if (!processedBundleIDs.insert(bundleID).second)
                continue;

            // Find the bundle for this ID
            auto it = foundBundles.find(bundleID);
            if (it == foundBundles.end()) {
                fprintf(stderr, "[WARNING]: Could not find bundle with ID '%s'\n", bundleID.c_str());
                continue;
            }

            BundleData& bundleData = it->second;

            LoadedFileInfo info;

            // Codeless kexts don't have an executable path, but we still want to put their
            // plist in the prelink info
            bool isCodeless = bundleData.executablePath.empty();
            if ( !isCodeless ) {
                char realerPath[MAXPATHLEN];
                bool loadedFile = fileSystem.loadFile(bundleData.executablePath.c_str(), info, realerPath,
                                                      ^(const char *format, ...) {
                    va_list list;
                    va_start(list, format);
                    diag.error(format, list);
                    va_end(list);
                });
                if ( !loadedFile )
                    return {};
            }

            std::vector<const char*> deps;
            for (const std::string& dependency : bundleData.dependencies)
                deps.push_back(dependency.c_str());

            CFStringRef         kextPathStringRef   = nullptr;
            CFDataRef           kextDataRef         = nullptr;
            if ( !isCodeless) {
                kextPathStringRef = CFStringCreateWithCString(kCFAllocatorDefault, bundleData.executablePath.c_str(), kCFStringEncodingASCII);
                kextDataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const uint8_t*)info.fileContent, info.fileContentLen, kCFAllocatorNull);
            }

            CFMutableArrayRef   kextDepsArrayRef = CFArrayCreateMutable(kCFAllocatorDefault, bundleData.dependencies.size(), &kCFTypeArrayCallBacks);
            for (const std::string& dependency : bundleData.dependencies) {
                CFStringRef depStringRef = CFStringCreateWithCString(kCFAllocatorDefault, dependency.c_str(), kCFStringEncodingASCII);
                CFArrayAppendValue(kextDepsArrayRef, depStringRef);
                CFRelease(depStringRef);
            }
            CFStringRef         kextBundleIDStringRef = CFStringCreateWithCString(kCFAllocatorDefault, bundleID.c_str(), kCFStringEncodingASCII);
            CFStringRef         kextBundlePathStringRef = CFStringCreateWithCString(kCFAllocatorDefault, bundleData.bundlePath.c_str(), kCFStringEncodingASCII);

            BinaryStripMode stripMode = binaryStripNone;
            if ( !stripModeString.empty() ) {
                if ( stripModeString == "locals" ) {
                    stripMode = binaryStripLocals;
                } else if ( stripModeString == "exports" ) {
                    stripMode = binaryStripExports;
                } else if ( stripModeString == "all" ) {
                    stripMode = binaryStripAll;
                } else {
                    diag.error("Unknown strip mode: '%s'", stripModeString.c_str());
                    return {};
                }
            }

            KextFileData_v1 fileData = { 1, kextPathStringRef, kextDataRef,
                                         kextDepsArrayRef, kextBundleIDStringRef, kextBundlePathStringRef,
                                         bundleData.infoPlist, stripMode };

            if ( !addKextDataFile(kcb, &fileData) ) {
                uint64_t errorCount = 0;
                const char* const* errors = getErrors(kcb, &errorCount);
                for (uint64_t i = 0; i != errorCount; ++i)
                    diag.error("Could not load kext file because: '%s'", errors[i]);
                return {};
            }

            // Walk the dependencies and add any new ones to the list
            for (const std::string& dependency : bundleData.dependencies) {
                if ( existingBundles.find(dependency) == existingBundles.end() )
                    bundleIDsToLoad.push_back(dependency.c_str());
            }
        }

        // Filter dependencies to kext's with binaries
#if 0
        const std::map<std::string, BundleData>* foundBundlesPtr = &foundBundles;
        for (AppCacheBuilder::InputDylib& file : loadedFiles) {
            file.dylibDeps.erase(std::remove_if(file.dylibDeps.begin(), file.dylibDeps.end(),
                                                [&](const std::string& depName) {
                auto it = foundBundlesPtr->find(depName);
                assert(it != foundBundlesPtr->end());
                return it->second.executablePath.empty();
            }),file.dylibDeps.end());
        }
#endif
    }

#if 0
    for (AppCacheBuilder::InputDylib& file : loadedFiles) {
        char fileRealPath[MAXPATHLEN];
        const char* path = file.dylib.loadedFileInfo.path;
        LoadedFileInfo loadedFileInfo = MachOAnalyzer::load(diag, fileSystem, path, archs, platform, fileRealPath);
        if ( diag.hasError() ) {
            fprintf(stderr, "Could not load file '%s' because: %s\n", path, diag.errorMessage().c_str());
            return {};
        }

        MachOAnalyzer* ma = (MachOAnalyzer*)loadedFileInfo.fileContent;
        if (ma == nullptr) {
            fprintf(stderr, "Could not load file: %s\n", path);
            return {};
        }

        auto errorHandler = ^(const char* msg) {
            diag.error("Binary located at '%s' cannot be placed in kernel collection because: %s", path, msg);
        };
        if (ma->canBePlacedInKernelCollection(path, errorHandler)) {
            DyldSharedCache::MappedMachO mappedFile(path, ma, loadedFileInfo.sliceLen, false, false,
                                                    loadedFileInfo.sliceOffset, loadedFileInfo.mtime,
                                                    loadedFileInfo.inode);
            CacheBuilder::LoadedMachO loadedMachO = { mappedFile, loadedFileInfo, nullptr };
            file.dylib = loadedMachO;
        } else {
            fileSystem.unloadFile(loadedFileInfo);
        }
        if ( diag.hasError() ) {
            fprintf(stderr, "%s\n", diag.errorMessage().c_str());
            return {};
        }
    }
#endif

#if 0
    if (loadedFiles.empty()) {
        fprintf(stderr, "Could not find any valid files to create kernel collection\n");

        // Since we found no files, print warnings for the ones we tried
        if (!diag.warnings().empty()) {
            fprintf(stderr, "Failed to use the following files:\n");
            for (const std::string& msg : diag.warnings()) {
                fprintf(stderr, "  %s\n", msg.c_str());
            }
        }
        return {};
    }

    if (options.verbose) {
        for (const AppCacheBuilder::InputDylib& loadedFile : loadedFiles)
            fprintf(stderr, "Building cache with file: %s\n", loadedFile.dylib.loadedFileInfo.path);
    }
#endif

    for (const SectionData& sectData : options.sections) {
        CFStringRef segmentName   = CFStringCreateWithCString(kCFAllocatorDefault, sectData.segmentName, kCFStringEncodingASCII);
        CFStringRef sectionName   = nullptr;
        if ( sectData.sectionName != nullptr )
            sectionName = CFStringCreateWithCString(kCFAllocatorDefault, sectData.sectionName, kCFStringEncodingASCII);

        CFDataRef sectionData = nullptr;
        {
            struct stat stat_buf;
            int fd = ::open(sectData.payloadFilePath, O_RDONLY, 0);
            if (fd == -1) {
                diag.error("can't open file '%s', errno=%d\n", sectData.payloadFilePath, errno);
                return {};
            }

            if (fstat(fd, &stat_buf) == -1) {
                diag.error("can't stat open file '%s', errno=%d\n", sectData.payloadFilePath, errno);
                ::close(fd);
                return {};
            }

            const void* buffer = mmap(NULL, (size_t)stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (buffer == MAP_FAILED) {
                diag.error("mmap() for file at %s failed, errno=%d\n", sectData.payloadFilePath, errno);
                ::close(fd);
                return {};
            }
            ::close(fd);

            sectionData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const uint8_t*)buffer, stat_buf.st_size, kCFAllocatorNull);
        }

        if ( !addSegmentData(kcb, segmentName, sectionName, sectionData) ) {
            uint64_t errorCount = 0;
            const char* const* errors = getErrors(kcb, &errorCount);
            for (uint64_t i = 0; i != errorCount; ++i)
                diag.error("Could not load section data file because: '%s'", errors[i]);
            return {};
        }
    }

    if ( options.prelinkInfoExtraData != nullptr ) {
        struct stat stat_buf;
        int fd = ::open(options.prelinkInfoExtraData, O_RDONLY, 0);
        if (fd == -1) {
            diag.error("can't open file '%s', errno=%d\n", options.prelinkInfoExtraData, errno);
            return {};
        }

        if (fstat(fd, &stat_buf) == -1) {
            diag.error("can't stat open file '%s', errno=%d\n", options.prelinkInfoExtraData, errno);
            ::close(fd);
            return {};
        }

        const void* buffer = mmap(NULL, (size_t)stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (buffer == MAP_FAILED) {
            diag.error("mmap() for file at %s failed, errno=%d\n", options.prelinkInfoExtraData, errno);
            ::close(fd);
            return {};
        }
        ::close(fd);

        CFDataRef prelinkInfoData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const uint8_t*)buffer, stat_buf.st_size, kCFAllocatorNull);

        CFErrorRef errorRef = nullptr;
        CFPropertyListRef plistRef = CFPropertyListCreateWithData(kCFAllocatorDefault, prelinkInfoData, kCFPropertyListImmutable, nullptr, &errorRef);
        if (errorRef != nullptr) {
            CFStringRef errorString = CFErrorCopyDescription(errorRef);
            diag.error("Could not load prelink info plist because :%s",
                       CFStringGetCStringPtr(errorString, kCFStringEncodingASCII));
            CFRelease(errorRef);
            return {};
        }
        if ( plistRef == nullptr ) {
            diag.error("Could not create plist ref for prelink info");
            return {};
        }
        if ( CFGetTypeID(plistRef) != CFDictionaryGetTypeID() ) {
            diag.error("Prelink info plist should be a dictionary");
            return {};
        }

        if ( !addPrelinkInfo(kcb, (CFDictionaryRef)plistRef) ) {
            uint64_t errorCount = 0;
            const char* const* errors = getErrors(kcb, &errorCount);
            for (uint64_t i = 0; i != errorCount; ++i)
                diag.error("Could not prelink data file because: '%s'", errors[i]);
            return {};
        }
    }

    bool success = runKernelCollectionBuilder(kcb);
    uint64_t errorCount = 0;
    const char* const* errors = getErrors(kcb, &errorCount);
    if ( errors != nullptr ) {
        if ( !options.printJSONErrors ) {
            for (uint64_t i = 0; i != errorCount; ++i) {
                fprintf(stderr, "Could not build kernel collection because '%s'\n", errors[i]);
            }
        }
        CFDictionaryRef errorDictRef = getKextErrors(kcb);
        if ( errorDictRef != nullptr ) {
            Node rootNode;

            CFDictionaryApplyFunction(errorDictRef, [](const void *key, const void *value, void *context) {
                Node* rootNode = (Node*)context;
                CFStringRef keyRef = (CFStringRef)key;
                CFArrayRef valueRef = (CFArrayRef)value;

                Node bundleNode;
                bundleNode.map["id"] = Node(CFStringGetCStringPtr(keyRef, kCFStringEncodingASCII));

                Node errorsNode;
                CFArrayApplyFunction(valueRef, CFRangeMake(0, CFArrayGetCount(valueRef)), [](const void *value, void *context) {
                    Node* errorsNode = (Node*)context;
                    CFStringRef valueRef = (CFStringRef)value;

                    errorsNode->array.push_back(Node(CFStringGetCStringPtr(valueRef, kCFStringEncodingASCII)));
                }, &errorsNode);

                bundleNode.map["errors"] = errorsNode;

                rootNode->array.push_back(bundleNode);
            }, &rootNode);

            // sort the nodes so that the output is reproducible
            std::sort(rootNode.array.begin(), rootNode.array.end(),
                      [](const Node& a, const Node&b) {
                return a.map.find("id")->second.value < b.map.find("id")->second.value;
            });

            printJSON(rootNode);
        } else {
            Node rootNode;
            for (uint64_t i = 0; i != errorCount; ++i) {
                rootNode.array.push_back(Node(errors[i]));
            }
            printJSON(rootNode);
        }
        return {};
    }

    if ( !success )
        return {};

    uint64_t fileResultCount = 0;
    const auto* fileResults = getCollectionFileResults(kcb, &fileResultCount);
    if ( fileResults == nullptr ) {
        fprintf(stderr, "Could not get file results\n");
        return {};
    }
    if ( fileResultCount != 1 ) {
        fprintf(stderr, "Unsupported file result count: %lld\n", fileResultCount);
        return {};
    }

    CFDataRef dataRef = fileResults[0]->data;
    CFRetain(dataRef);

    destroyKernelCollectionBuilder(kcb);

    return dataRef;
}

static int createKernelCollection(const CreateKernelCollectionOptions& options) {
    // Verify any required options
    if (gOpts.archs.empty()) {
        exit_usage("-arch");
    } else {
        std::set<std::string_view> archs(gOpts.archs.begin(), gOpts.archs.end());
        if (archs.size() != gOpts.archs.size()) {
            fprintf(stderr, "Duplicate -arch specified\n");
            exit(1);
        }
    }
    if (options.outputCachePath == nullptr)
        exit_usage();

    switch (options.stripMode) {
        case unknownStripMode:
        case stripNone:
            break;
        case stripAll:
        case stripAllKexts:
            if ( options.collectionKind != baseKC ) {
                fprintf(stderr, "Cannot use -strip-all-kexts with auxKC.  Use strip-all instead\n");
                exit(1);
            }
            break;
    }

    switch (options.collectionKind) {
        case unknownKC:
            fprintf(stderr, "Invalid kernel collection kind\n");
            exit(1);
        case baseKC:
            if (options.kernelPath == nullptr)
                exit_usage("-kernel");
            break;
        case pageableKC:
        case auxKC:
            if (options.kernelCollectionPath == nullptr)
                exit_usage("-kernel-collection");
            break;
    }

    if ( !options.bundleIDs.empty() ) {
        if (options.extensionsPath == nullptr)
            exit_usage("-extensions");
    }

    // Volume root should be a prefix of extensions path
    if ( options.extensionsPath != nullptr ) {
        if ( strncmp(options.extensionsPath, options.volumeRoot, strlen(options.volumeRoot)) != 0 ) {
            fprintf(stderr, "Volume root '%s' is not a prefix of extensions path '%s'\n",
                    options.volumeRoot, options.extensionsPath);
        }
    }

    std::vector<CFDataRef> buffers;
    for (const char* arch : gOpts.archs) {
        Diagnostics diag;
        CFDataRef bufferRef = createKernelCollectionForArch(options, arch, diag);
        if ( diag.hasError() ) {
            fprintf(stderr, "%s\n", diag.errorMessage().c_str());
            return 1;
        }
        if ( bufferRef == nullptr ) {
            // If we  want errors then return 0
            if ( options.printJSONErrors )
                return 0;
            return 1;
        }
        buffers.push_back(bufferRef);
    }

    if (buffers.size() == 1) {
        // Single arch.  Just write the file directly
        CFDataRef bufferRef = buffers.front();
        if ( !safeSave(CFDataGetBytePtr(bufferRef), CFDataGetLength(bufferRef), options.outputCachePath) ) {
            fprintf(stderr, "Could not write app cache\n");
            return 1;
        }
        CFRelease(bufferRef);
    } else {
        // Multiple buffers.  Create a FAT file
        std::vector<uint8_t> fatBuffer;

        // Add the FAT header to the start of the buffer
        fatBuffer.resize(0x4000, 0);
        fat_header* header = (fat_header*)&fatBuffer.front();
        header->magic       = OSSwapHostToBigInt32(FAT_MAGIC);
        header->nfat_arch   = OSSwapHostToBigInt32((uint32_t)buffers.size());

        for (uint32_t i = 0; i != buffers.size(); ++i) {
            CFDataRef bufferRef = buffers[i];
            mach_header* mh = (mach_header*)CFDataGetBytePtr(bufferRef);

            uint32_t offsetInBuffer = (uint32_t)fatBuffer.size();

            fat_arch* archBuffer = (fat_arch*)(&fatBuffer.front() + sizeof(fat_header));
            archBuffer[i].cputype       = OSSwapHostToBigInt32(mh->cputype);
            archBuffer[i].cpusubtype    = OSSwapHostToBigInt32(mh->cpusubtype);
            archBuffer[i].offset        = OSSwapHostToBigInt32(offsetInBuffer);
            archBuffer[i].size          = OSSwapHostToBigInt32((uint32_t)CFDataGetLength(bufferRef));
            archBuffer[i].align         = OSSwapHostToBigInt32(14);

            auto align = [](uint64_t addr, uint8_t p2) {
                uint64_t mask = (1 << p2);
                return (addr + mask - 1) & (-mask);
            };

            uint32_t alignedSize = (uint32_t)align((uint32_t)CFDataGetLength(bufferRef), 14);
            fatBuffer.resize(fatBuffer.size() + alignedSize, 0);
            memcpy(&fatBuffer.front() + offsetInBuffer, CFDataGetBytePtr(bufferRef), CFDataGetLength(bufferRef));
        }

        if ( !safeSave(&fatBuffer.front(), fatBuffer.size(), options.outputCachePath) ) {
            fprintf(stderr, "Could not write app cache\n");
            return 1;
        }
    }

    return 0;
}

int main(int argc, const char* argv[]) {
    OptionsVariants options;
    if (!parseArgs(argc, argv, options))
        return 1;

    if (std::holds_alternative<DumpOptions>(options)) {
        return dumpAppCache(std::get<DumpOptions>(options));
    }

    if (std::holds_alternative<ValidateOptions>(options)) {
        return validateFile(std::get<ValidateOptions>(options));
    }

    if (std::holds_alternative<ListBundlesOptions>(options)) {
        return listBundles(std::get<ListBundlesOptions>(options));
    }

    if (std::holds_alternative<CreateKernelCollectionOptions>(options)) {
        return createKernelCollection(std::get<CreateKernelCollectionOptions>(options));
    }

    assert(std::holds_alternative<std::monostate>(options));

    exit_usage();
}
