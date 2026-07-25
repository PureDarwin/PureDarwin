/*
TEST_ENTITLEMENTS preopt-caches.entitlements
TEST_CONFIG OS=iphoneos MEM=mrc
TEST_BUILD
    mkdir -p $T{OBJDIR}
    /usr/sbin/dtrace -h -s $DIR/../runtime/objc-probes.d -o $T{OBJDIR}/objc-probes.h
    $C{COMPILE} $DIR/preopt-caches.mm -std=gnu++17 -isystem $C{SDK_PATH}/System/Library/Frameworks/System.framework/PrivateHeaders -I$T{OBJDIR} -ldsc -o preopt-caches.exe
END
*/
//
//  check_preopt_caches.m
//  check-preopt-caches
//
//  Created by Thomas Deniau on 11/06/2020.
//

#define TEST_CALLS_OPERATOR_NEW

#include "test-defines.h"
#include "../runtime/objc-private.h"
#include <objc/objc-internal.h>

#include <dlfcn.h>
#include <dirent.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_process_info.h>
#include <mach-o/dyld_cache_format.h>
#include <mach-o/dsc_iterator.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <set>
#include <spawn.h>
#include <sys/poll.h>

#include "test.h"

int validate_dylib_in_forked_process(const char * const toolPath, const char * const dylib)
{
    int out_pipe[2] = {-1};
    int err_pipe[2] = {-1};
    int exit_code = -1;
    pid_t pid = 0;
    int rval = 0;

    std::string child_stdout;
    std::string child_stderr;

    posix_spawn_file_actions_t actions = NULL;
    const char * const args[] = {toolPath, dylib, NULL};
    int ret = 0;

    if (pipe(out_pipe)) {
        exit(3);
    }

    if (pipe(err_pipe)) {
        exit(3);
    }

    //Do-si-do the FDs
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclose(&actions, out_pipe[0]);
    posix_spawn_file_actions_addclose(&actions, err_pipe[0]);
    posix_spawn_file_actions_adddup2(&actions, out_pipe[1], 1);
    posix_spawn_file_actions_adddup2(&actions, err_pipe[1], 2);
    posix_spawn_file_actions_addclose(&actions, out_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, err_pipe[1]);

    // Fork so that we can dlopen the dylib in a clean context
    ret = posix_spawnp(&pid, args[0], &actions, NULL, (char * const *)args, NULL);

    if (ret != 0) {
        fail("posix_spawn for %s failed: returned %d, %s\n", dylib, ret, strerror(ret));
        exit(3);
    }

    posix_spawn_file_actions_destroy(&actions);
    close(out_pipe[1]);
    close(err_pipe[1]);

    std::string buffer(4096,' ');
    std::vector<pollfd> plist = { {out_pipe[0],POLLIN,0}, {err_pipe[0],POLLIN,0} };
    while (( (rval = poll(&plist[0],(nfds_t)plist.size(), 100000)) > 0 ) || ((rval < 0) && (errno == EINTR))) {
        if (rval < 0) {
            // EINTR
            continue;
        }

        ssize_t bytes_read = 0;

        if (plist[0].revents&(POLLERR|POLLHUP) || plist[1].revents&(POLLERR|POLLHUP)) {
            bytes_read = read(out_pipe[0], &buffer[0], buffer.length());
            bytes_read = read(err_pipe[0], &buffer[0], buffer.length());
            break;
        }

        if (plist[0].revents&POLLIN) {
            bytes_read = read(out_pipe[0], &buffer[0], buffer.length());
            child_stdout += buffer.substr(0, static_cast<size_t>(bytes_read));
        }
        else if ( plist[1].revents&POLLIN ) {
            bytes_read = read(err_pipe[0], &buffer[0], buffer.length());
            child_stderr += buffer.substr(0, static_cast<size_t>(bytes_read));
        }
        else break; // nothing left to read

        plist[0].revents = 0;
        plist[1].revents = 0;
    }
    if (rval == 0) {
        // Early timeout so try to clean up.
        fail("Failed to validate dylib %s: timeout!\n", dylib);
        return 1;
    }


    if (err_pipe[0] != -1) {
        close(err_pipe[0]);
    }

    if (out_pipe[0] != -1) {
        close(out_pipe[0]);
    }

    if (pid != 0) {
        if (waitpid(pid, &exit_code, 0) < 0) {
            fail("Could not wait for PID %d (dylib %s): err %s\n", pid, dylib, strerror(errno));
        }

        if (!WIFEXITED(exit_code)) {
            fail("PID %d (%s) did not exit: %d. stdout: %s\n stderr: %s\n", pid, dylib, exit_code, child_stdout.c_str(), child_stderr.c_str());
        }
        if (WEXITSTATUS(exit_code) != 0) {
            fail("Failed to validate dylib %s\nstdout: %s\nstderr: %s\n", dylib, child_stdout.c_str(), child_stderr.c_str());
        }
    }

    testprintf("%s", child_stdout.c_str());

    return 0;
}

bool check_class(Class cls, unsigned & cacheCount) {
    // printf("%s %s\n", class_getName(cls), class_isMetaClass(cls) ? "(metaclass)" : "");

    // For the initialization of the cache so that we setup the constant cache if any
    class_getMethodImplementation(cls, @selector(initialize));

    if (objc_cache_isConstantOptimizedCache(&(cls->cache), true, (uintptr_t)&_objc_empty_cache)) {
        cacheCount++;
        // printf("%s has a preopt cache\n", class_getName(cls));

        // Make the union of all selectors until the preopt fallback class
        const class_ro_t * fallback = ((const objc_class *) objc_cache_preoptFallbackClass(&(cls->cache)))->data()->ro();

        std::unordered_map<SEL, IMP> methods;

        Method *methodList;
        unsigned count;
        Class currentClass = cls;
        unsigned dynamicCount = 0;
        while (currentClass->data()->ro() != fallback) {
            methodList = class_copyMethodList(currentClass, &count);
            // printf("%d methods in method list for %s\n", count, class_getName(currentClass));
            for (unsigned i = 0 ; i < count ; i++) {
                SEL sel = method_getName(methodList[i]);
                if (methods.find(sel) == methods.end()) {
                    const char *name = sel_getName(sel);
                    // printf("[dynamic] %s -> %p\n", name, method_getImplementation(methodList[i]));
                    methods[sel] = ptrauth_strip(method_getImplementation(methodList[i]), ptrauth_key_function_pointer);
                    if (   (currentClass == cls) ||
                        (   (strcmp(name, ".cxx_construct") != 0)
                         && (strcmp(name, ".cxx_destruct") != 0))) {
                        dynamicCount++;
                    }
                }
            }
            if (count > 0) {
                free(methodList);
            }
            currentClass = class_getSuperclass(currentClass);
        }

        // Check we have an equality between the two caches

        // Count the methods in the preopt cache
        unsigned preoptCacheCount = 0;
        unsigned capacity = objc_cache_preoptCapacity(&(cls->cache));
        const preopt_cache_entry_t *buckets = objc_cache_preoptCache(&(cls->cache))->entries;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-of-sel-type"
        const uint8_t *selOffsetsBase = (const uint8_t*)@selector(🤯);
#pragma clang diagnostic pop
        for (unsigned i = 0 ; i < capacity ; i++) {
            uint32_t selOffset = buckets[i].sel_offs;
            if (selOffset != 0xFFFFFFFF) {
                SEL sel = (SEL)(selOffsetsBase + selOffset);
                IMP imp = (IMP)((uint8_t*)cls - buckets[i].imp_offs);
                if (methods.find(sel) == methods.end()) {
                    fail("ERROR: %s: %s not found in dynamic method list\n", class_getName(cls), sel_getName(sel));
                    return false;
                }
                IMP dynamicImp = methods.at(sel);
                // printf("[static] %s -> %p\n", sel_getName(sel), imp);
                if (imp != dynamicImp) {
                    fail("ERROR: %s: %s has different implementations %p vs %p in static and dynamic caches", class_getName(cls), sel_getName(sel), imp, dynamicImp);
                    return false;
                }
                preoptCacheCount++;
            }
        }

        if (preoptCacheCount != dynamicCount) {
            testwarn("Methods in preopt cache:\n");

            for (unsigned i = 0 ; i < capacity ; i++) {
                uint32_t selOffset = buckets[i].sel_offs;
                if (selOffset != 0xFFFFFFFF) {
                    SEL sel = (SEL)(selOffsetsBase + selOffset);
                    testwarn("%s\n", sel_getName(sel));
                }
            }

            testwarn("Methods in dynamic cache:\n");

            for (const auto & [sel, imp] : methods) {
                testwarn("%s\n", sel_getName(sel));
            }

            fail("ERROR: %s's preoptimized cache is missing some methods\n", class_getName(cls));

            return false;
        }

    } else {
        // printf("%s does NOT have a preopt cache\n", class_getName(cls));
    }

    return true;
}

bool check_library(const char *path) {
    std::set<std::string> blacklistedClasses {
        "PNPWizardScratchpadInkView", // Can only be +initialized on Pencil-capable devices
        "CACDisplayManager", // rdar://64929282 (CACDisplayManager does layout in +initialize!)
    };

    testprintf("Checking %s… ", path);

    __unused void *lib = dlopen(path, RTLD_NOW);
    extern uint32_t _dyld_image_count(void) __OSX_AVAILABLE_STARTING(__MAC_10_1, __IPHONE_2_0);
    unsigned outCount = 0;

    // Realize all classes first.
    Class *allClasses = objc_copyClassList(&outCount);
    if (allClasses != NULL) {
        free(allClasses);
    }

    allClasses = objc_copyClassesForImage(path, &outCount);
    if (allClasses != NULL) {
        unsigned classCount = 0;
        unsigned cacheCount = 0;

        for (const Class * clsPtr = allClasses ; *clsPtr != nil ; clsPtr++) {
            classCount++;
            Class cls = *clsPtr;

            if (blacklistedClasses.find(class_getName(cls)) != blacklistedClasses.end()) {
                continue;
            }

            if (!check_class(cls, cacheCount)) {
                return false;
            }

            if (!class_isMetaClass(cls)) {
                if (!check_class(object_getClass(cls), cacheCount)) {
                    return false;
                }
            }
        }
        testprintf("checked %d caches in %d classes\n", cacheCount, classCount);
        free(allClasses);
    } else {
        testprintf("could not find %s or no class names inside\n", path);
    }

    return true;
}

size_t size_of_shared_cache_with_uuid(uuid_t uuid) {
  DIR* dfd = opendir(IPHONE_DYLD_SHARED_CACHE_DIR);
  if (!dfd) {
    fail("Error: unable to open shared cache dir %s\n",
            IPHONE_DYLD_SHARED_CACHE_DIR);
    exit(1);
  }

  uint64_t shared_cache_size = 0;

  struct dirent *dp;
  while ((dp = readdir(dfd))) {
    char full_filename[512];
    snprintf(full_filename, sizeof(full_filename), "%s%s",
             IPHONE_DYLD_SHARED_CACHE_DIR, dp->d_name);

    struct stat stat_buf;
    if (stat(full_filename, &stat_buf) != 0)
      continue;

    if ((stat_buf.st_mode & S_IFMT) == S_IFDIR)
      continue;

    int fd = open(full_filename, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "Error: unable to open file %s\n", full_filename);
      continue;
    }

    struct dyld_cache_header header;
    if (read(fd, &header, sizeof(header)) != sizeof(header)) {
      fprintf(stderr, "Error: unable to read dyld shared cache header from %s\n",
              full_filename);
      close(fd);
      continue;
    }

    if (uuid_compare(header.uuid, uuid) == 0) {
      shared_cache_size = stat_buf.st_size;
      break;
    }
  }

  closedir(dfd);

  return shared_cache_size;
}

int main (int argc, const char * argv[])
{
    if (argc == 1) {
        int err = 0;
        dyld_process_info process_info = _dyld_process_info_create(mach_task_self(), 0, &err);
        if (NULL == process_info) {
            mach_error("_dyld_process_info_create", err);
            fail("_dyld_process_info_create");
            return 2;
        }
        dyld_process_cache_info cache_info;
        _dyld_process_info_get_cache(process_info, &cache_info);

        __block std::set<std::string> dylibsSet;
        size_t size = size_of_shared_cache_with_uuid(cache_info.cacheUUID);
        dyld_shared_cache_iterate((void*)cache_info.cacheBaseAddress, (uint32_t)size, ^(const dyld_shared_cache_dylib_info* dylibInfo, __unused const dyld_shared_cache_segment_info* segInfo) {
            if (dylibInfo->isAlias) return;
            std::string path(dylibInfo->path);
            dylibsSet.insert(path);
        });
        std::vector<std::string> dylibs(dylibsSet.begin(), dylibsSet.end());

        dispatch_apply(dylibs.size(), DISPATCH_APPLY_AUTO, ^(size_t idx) {
            validate_dylib_in_forked_process(argv[0], dylibs[idx].c_str());
        });
    } else {
        const char *libraryName = argv[1];
        if (!check_library(libraryName)) {
            fail("checking library %s\n", libraryName);
            return 1;
        }
    }

    succeed(__FILE__);
    return 0;
}
