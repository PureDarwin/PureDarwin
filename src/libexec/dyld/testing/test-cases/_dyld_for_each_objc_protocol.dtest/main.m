
// BUILD:  $CC linked1.m -dynamiclib -o $BUILD_DIR/liblinked1.dylib -install_name $RUN_DIR/liblinked1.dylib -lobjc
// BUILD:  $CC linked2.m -dynamiclib -o $BUILD_DIR/liblinked2.dylib -install_name $RUN_DIR/liblinked2.dylib -lobjc
// BUILD:  $CC main.m -o $BUILD_DIR/_dyld_for_each_objc_protocol.exe $BUILD_DIR/liblinked1.dylib $BUILD_DIR/liblinked2.dylib -lobjc

// RUN:  ./_dyld_for_each_objc_protocol.exe

// The preoptimized objc protocol information is available via _dyld_for_each_objc_protocol().
// This test ensures that we match the objc behaviour when there are duplicates.
// For objc today, it walks the images in reverse load order, so the deepest library will be
// the canonical definition of a protocol.

#include <mach-o/dyld_priv.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "test_support.h"

static bool objcOptimizedByDyld() {
    extern const uint32_t objcInfo[]  __asm("section$start$__DATA_CONST$__objc_imageinfo");
    return (objcInfo[1] & 0x80);
}

static bool haveDyldCache() {
    size_t unusedCacheLen;
    return (_dyld_get_shared_cache_range(&unusedCacheLen) != NULL);
}

// All the libraries have a copy of DyldProtocol
@protocol DyldProtocol
@end

// Only the main executable has DyldMainProtocol
@protocol DyldMainProtocol
@end

__attribute__((used))
static void* useDyldProtocol() {
  return (void*)@protocol(DyldProtocol);
}

__attribute__((used))
static void* useDyldMainProtocol() {
  return (void*)@protocol(DyldMainProtocol);
}

extern id objc_getProtocol(const char *name);

static bool gotDyldProtocolMain = false;
static bool gotDyldProtocolLinked = false;
static bool gotDyldProtocolLinked2 = false;

static bool isInImage(void* ptr, const char* name) {
  Dl_info info;
  if ( dladdr(ptr, &info) == 0 ) {
    FAIL("dladdr(protocol, xx) failed");
    return false;
  }
  return strstr(info.dli_fname, name) != NULL;
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
   if (!objcOptimizedByDyld() || !haveDyldCache()) {
    __block bool sawClass = false;
    _dyld_for_each_objc_class("DyldClass", ^(void* classPtr, bool isLoaded, bool* stop) {
      sawClass = true;
    });
    if (sawClass) {
      FAIL("dyld2 shouldn't see any classes");
    }
    PASS("no shared cache or no dyld optimized objc");
  }

  // Check that DyldProtocol comes from liblinked2 as it is last in load order
  id runtimeDyldProtocol = objc_getProtocol("DyldProtocol");
  if (!isInImage(runtimeDyldProtocol, "liblinked2")) {
    FAIL("DyldProtocol should have come from liblinked2");
  }

  // Check that DyldLinkedProtocol comes from liblinked2 as it is last in load order
  id runtimeDyldLinkedProtocol = objc_getProtocol("DyldLinkedProtocol");
  if (!isInImage(runtimeDyldLinkedProtocol, "liblinked2")) {
    FAIL("DyldLinkedProtocol should have come from liblinked2");
  }

  // Walk all the implementations of "DyldProtocol"
  _dyld_for_each_objc_protocol("DyldProtocol", ^(void* protocolPtr, bool isLoaded, bool* stop) {
    // We should walk these in the order liblinked2, liblinked, main exe
    if (!gotDyldProtocolLinked2) {
      if (!isInImage(protocolPtr, "liblinked2")) {
        FAIL("Optimized DyldProtocol should have come from liblinked2");
      }
      if (!isLoaded) {
        FAIL("Optimized DyldProtocol isLoaded should have been set on liblinked2");
      }
      gotDyldProtocolLinked2 = true;
      return;
    }
    if (!gotDyldProtocolLinked) {
      if (!isInImage(protocolPtr, "liblinked1")) {
        FAIL("Optimized DyldProtocol should have come from liblinked");
      }
      if (!isLoaded) {
        FAIL("Optimized DyldProtocol isLoaded should have been set on liblinked");
      }
      gotDyldProtocolLinked = true;
      return;
    }
    if (!gotDyldProtocolMain) {
      if (!isInImage(protocolPtr, "_dyld_for_each_objc_protocol.exe")) {
        FAIL("Optimized DyldProtocol should have come from main exe");
      }
      if (!isLoaded) {
        FAIL("Optimized DyldProtocol isLoaded should have been set on main exe");
      }
      gotDyldProtocolMain = true;
      return;
    }
    FAIL("Unexpected Optimized DyldProtocol");
    return;
  });

  // Visit again, and return liblinked2's DyldProtocol
  __block void* DyldProtocolImpl = nil;
  _dyld_for_each_objc_protocol("DyldProtocol", ^(void* protocolPtr, bool isLoaded, bool* stop) {
    DyldProtocolImpl = protocolPtr;
    *stop = true;
  });
  if (!isInImage(DyldProtocolImpl, "liblinked2")) {
    FAIL("_dyld_for_each_objc_protocol should have returned DyldProtocol from liblinked2");
  }

  // Visit DyldMainProtocol and make sure it makes the callback for just the result from main.exe
  __block void* DyldMainProtocolImpl = nil;
  _dyld_for_each_objc_protocol("DyldMainProtocol", ^(void* protocolPtr, bool isLoaded, bool* stop) {
    DyldMainProtocolImpl = protocolPtr;
    *stop = true;
  });
  if (!isInImage(DyldMainProtocolImpl, "_dyld_for_each_objc_protocol.exe")) {
    FAIL("_dyld_for_each_objc_protocol should have returned DyldMainProtocol from main.exe");
  }

#if __has_feature(ptrauth_calls)
  // Check the ISA was signed correctly on arm64e
  id dyldMainProtocol = @protocol(DyldMainProtocol);
  void* originalISA = *(void **)dyldMainProtocol;
  void* strippedISA = __builtin_ptrauth_strip(originalISA, ptrauth_key_asda);
  uint64_t discriminator = __builtin_ptrauth_blend_discriminator((void*)dyldMainProtocol, 27361);
  void* signedISA = __builtin_ptrauth_sign_unauthenticated((void*)strippedISA, 2, discriminator);
  if ( originalISA != signedISA ) {
    FAIL("_dyld_for_each_objc_protocol DyldMainProtocol ISA is not signed correctly: %p vs %p",
         originalISA, signedISA);
  }
#endif

  PASS("Success");
}
