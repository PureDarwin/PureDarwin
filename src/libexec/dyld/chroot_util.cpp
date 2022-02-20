/*
* Copyright (c) 2019 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
* Reserved.  This file contains Original Code and/or Modifications of
* Original Code as defined in and that are subject to the Apple Public
* Source License Version 1.0 (the 'License').  You may not use this file
* except in compliance with the License.  Please obtain a copy of the
* License at http://www.apple.com/publicsource and read it before using
* this file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
* License for the specific language governing rights and limitations
* under the License."
*
* @APPLE_LICENSE_HEADER_END@
*/

#include <cassert>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/attr.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <copyfile.h>

#include <set>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>

#include "StringUtils.h"
#include "MachOFile.h"

std::set<std::string> scanForDependencies(const std::string& path) {
    __block std::set<std::string> result;
    struct stat stat_buf;
    int fd = open(path.c_str(), O_RDONLY, 0);
    if (fd == -1) {
        return result;
    }

    if (fstat(fd, &stat_buf) == -1) {
        close(fd);
        return result;
    }

    const void* buffer = mmap(NULL, (size_t)stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buffer == MAP_FAILED) {
        close(fd);
        return result;
    }

    auto scanner = ^(const char *loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool &stop) {
        if (isWeak) { return; } // We explicily avoid LC_LOAD_WEAK_DYLIB since we are trying to build a minimal chroot
        if (loadPath[0] != '/') { return; } // Only include absolute dependencies
        result.insert(loadPath);
    };
    Diagnostics diag;
    if ( dyld3::FatFile::isFatFile(buffer) ) {
        const dyld3::FatFile* ff = (dyld3::FatFile*)buffer;
        ff->forEachSlice(diag, stat_buf.st_size, ^(uint32_t sliceCpuType, uint32_t sliceCpuSubType, const void* sliceStart, uint64_t sliceSize, bool& stop) {
            const dyld3::MachOFile* mf = (dyld3::MachOFile*)sliceStart;
            mf->forEachDependentDylib(scanner);
         });
    } else {
        const dyld3::MachOFile* mf = (dyld3::MachOFile*)buffer;
        if (mf->isMachO(diag, stat_buf.st_size)) {
            mf->forEachDependentDylib(scanner);
        }
    }
    close(fd);
    return result;
}

std::string withoutPrefixPath(const std::string& path, const std::string& prefix ) {
    std::string result = path;
    size_t pos = result.find(prefix);
    result.erase(pos, prefix.length());
    return result;
}

void add_symlinks_to_dylib(const std::string path) {
    static std::set<std::string> alreadyMatched;
    size_t pos = path.rfind(".framework/Versions/");
    auto prefixPath = path.substr(0, pos);
    if (alreadyMatched.find(prefixPath) != alreadyMatched.end()) { return; }

    if (pos == std::string::npos) { return; }
//    fprintf(stderr, "PATH: %s\n", path.c_str());
    size_t versionStart = pos+20;
    size_t versionEnd = versionStart;
    while (path[versionEnd] != '/') {
        ++versionEnd;
    }
    size_t frameworkNameBegin = pos;
    while (path[frameworkNameBegin-1] != '/') {
        --frameworkNameBegin;
    }
    auto frameworkName = path.substr(frameworkNameBegin, pos-frameworkNameBegin);
    auto version = path.substr(versionStart, versionEnd-versionStart);
    std::string mainLinkPath = prefixPath + ".framework/" + frameworkName;
    std::string mainLinkTarget = "Versions/Current/" + frameworkName;
    std::string versionLinkPath =  prefixPath + ".framework/Versions/Current";
    std::string versionLinkTarget =  version;;
    alreadyMatched.insert(prefixPath);
    if (!std::filesystem::exists(versionLinkPath)) {
        std::filesystem::create_symlink(version, versionLinkPath);
    }
    if (!std::filesystem::exists(mainLinkPath)) {
        std::filesystem::create_symlink(mainLinkTarget, mainLinkPath);
    }
}

void add_symlink(const std::string& target, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_symlink(target, path);
    }
}

void buildChroot(const std::string& chroot, const std::string& fallback, const std::vector<std::string>& binaries) {
    auto chrootPath = std::filesystem::path(chroot);
    auto fallbackPath = std::filesystem::path(fallback);

    for (const auto& binary : binaries) {
        if (std::filesystem::exists(chroot + binary)) { continue; }
        std::filesystem::create_directories(std::filesystem::path(chroot + binary).parent_path());
        std::filesystem::copy(fallback + binary, chroot + binary);
    }
    bool foundNewEntries = true;
    std::set<std::string> scannedFiles;
    std::string devfsPath = chroot + "/dev";
    while (foundNewEntries) {
        foundNewEntries = false;
        for(auto file = std::filesystem::recursive_directory_iterator(chroot);
            file != std::filesystem::recursive_directory_iterator();
            ++file ) {
            auto filePath = file->path().string();
            if (filePath == devfsPath) {
                file.disable_recursion_pending();
                continue;
            }
            if (scannedFiles.find(filePath) != scannedFiles.end()) { continue; }
            scannedFiles.insert(filePath);
            auto candidates = scanForDependencies(filePath);
            for (const auto& candidate : candidates) {
                if (std::filesystem::exists(chroot + candidate)) { continue; }
                if (!std::filesystem::exists(fallback + candidate)) { continue; }
                std::filesystem::create_directories(std::filesystem::path(chroot + candidate).parent_path());
                std::filesystem::copy(fallback + candidate, chroot + candidate);
                add_symlinks_to_dylib(chroot + candidate);
                foundNewEntries = true;
            }
        }
    }
    add_symlink("libSystem.B.dylib", chroot + "/usr/lib/libSystem.dylib");
    add_symlink("libSystem.dylib", chroot + "/usr/lib/libc.dylib");
}

int main(int argc, const char * argv[]) {
    std::vector<std::string> binaries;
    std::vector<std::string> overlays;
    std::string fallback;
    std::string chroot;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (arg[0] == '-') {
            if (strcmp(arg, "-chroot") == 0) {
                chroot = argv[++i];
            } else if (strcmp(arg, "-fallback") == 0) {
                fallback = argv[++i];
            } else if (strcmp(arg, "-add_file") == 0) {
                binaries.push_back(argv[++i]);
            } else {
                fprintf(stderr, "unknown option: %s\n", arg);
                exit(-1);
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", arg);
            exit(-1);
        }
    }

    if (chroot.length() == 0) {
        fprintf(stderr, "No -chroot <dir>\n");
        exit(-1);
    }
    if (fallback.length() == 0) {
        fprintf(stderr, "No -fallback <dir>\n");
        exit(-1);
    }
    buildChroot(chroot, fallback, binaries);
    // insert code here...
    return 0;
}
