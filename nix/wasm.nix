# wasm.nix - PureDarwin's userland frameworks built for the browser with emscripten, so AppKit can run in a web page
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
{ pkgs, lib }:

let
  emcc = pkgs.emscripten;

  # emscripten builds its own libc, libc++ and friends on first use, into a writable cache.
  # Build that once here, every wasm package starts from a copy of it.
  emCache = pkgs.runCommand "puredarwin-emscripten-cache" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    export HOME=$TMPDIR
    export EM_CACHE=$out
    cp -r ${emcc}/share/emscripten/cache/. $out/ 2>/dev/null || mkdir -p $out
    chmod -R u+w $out
    embuilder build libc libc++ libc++abi libcompiler_rt libc-mt libdlmalloc libsockets libGL libstubs
  '';

  # the shell setup every wasm build starts with, a private copy of the cache it may add to
  emSetup = ''
    export HOME=$TMPDIR
    export EM_CACHE=$TMPDIR/em-cache
    cp -r ${emCache} $EM_CACHE
    chmod -R u+w $EM_CACHE
  '';

  # how every Objective-C source for the browser is compiled, against libobjc2's 1.9 ABI
  objcFlags = "-fobjc-runtime=gnustep-1.9 -fblocks";

  # libobjc2 tells a registered selector (an index) from an unregistered one (a name pointer) by
  # comparing the word with the table size, so no data may sit at a low address. Static data and
  # the heap start at 16 MB instead of emscripten's 1 KB.
  emLinkFlags = "-sGLOBAL_BASE=16777216 -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1";

  # GNUstep's Objective-C runtime (MIT). Apple's objc4 needs Mach-O images and per-arch msgSend assembly,
  # neither of which wasm has. The GNUstep ABI looks a method up and calls it through a plain pointer.
  libobjc2Wasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-libobjc2-wasm";
    version = pkgs.gnustep-libobjc.version;
    src = pkgs.gnustep-libobjc.src;
    nativeBuildInputs = [ emcc pkgs.cmake pkgs.python3 ];
    # the header-only hash map libobjc2 would otherwise clone at configure time
    buildInputs = [ pkgs.robin-map ];
    # wasm has no executable trampolines and clang has the gnustep 2.x ABI only for ELF and COFF:
    # the 1.9 ABI registers classes through __objc_load calls, which work in any object format
    postPatch = ''
      sed -i -e 's/^\tblock_trampolines.S$//' -e 's/^\tobjc_msgSend.S)$/\t)/' \
        -e 's/^\tblock_to_imp.c$/\tblock_to_imp_wasm.c/' \
        -e 's/-fobjc-runtime=gnustep-2.0/-fobjc-runtime=gnustep-1.9/' \
        -e 's/add_library(objc SHARED/add_library(objc STATIC/' CMakeLists.txt
      # wasm traps on any call whose signature differs from the definition. clang's 1.9 ABI code calls these
      # two as variadic, and dtable.c declares objc_resolve_class with the wrong return type
      sed -i 's/^OBJC_PUBLIC void __objc_exec_class(struct objc_module_abi_8 \*module)$/OBJC_PUBLIC void __objc_exec_class(struct objc_module_abi_8 *module, ...)/' loader.c
      sed -i 's/^id objc_lookup_class(const char \*name)$/id objc_lookup_class(const char *name, ...)/' class_table.c
      sed -i 's/^id objc_lookup_class(const char \*name);$/id objc_lookup_class(const char *name, ...);/' objc/runtime-deprecated.h
      sed -i 's/^void objc_resolve_class(Class);$/BOOL objc_resolve_class(Class);/' dtable.c
      sed -i 's/^PRIVATE void objc_resolve_class(Class);$/PRIVATE BOOL objc_resolve_class(Class);/' runtime.c
      grep -q '^PRIVATE BOOL objc_resolve_class(Class);' runtime.c
      grep -q '__objc_exec_class(struct objc_module_abi_8 \*module, \.\.\.)' loader.c
      grep -q 'objc_lookup_class(const char \*name, \.\.\.)$' class_table.c
      grep -q '^BOOL objc_resolve_class(Class);' dtable.c
      cat > block_to_imp_wasm.c <<'EOF'
    // blocks cannot become methods without writable code, which wasm does not have
    #include "objc/runtime.h"
    IMP imp_implementationWithBlock(id block) { (void)block; return (IMP)0; }
    id imp_getBlock(IMP anImp) { (void)anImp; return (id)0; }
    BOOL imp_removeBlock(IMP anImp) { (void)anImp; return NO; }
    // the loader sets up trampoline pages here, there are none on wasm
    void init_trampolines(void) { }
    EOF
    '';
    dontUseCmakeConfigure = true;
    # native strip does not understand wasm objects
    dontStrip = true;
    configurePhase = ''
      runHook preConfigure
      ${emSetup}
      emcmake cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$out \
        -DBUILD_SHARED_LIBS=OFF \
        -DTESTS=OFF \
        -DGNUSTEP_INSTALL_TYPE=NONE \
        -DEMBEDDED_BLOCKS_RUNTIME=ON \
        -DOLDABI_COMPAT=ON \
        -Dtsl-robin-map_DIR=${pkgs.robin-map}/share/cmake/tsl-robin-map \
        -DCMAKE_FIND_ROOT_PATH=${pkgs.robin-map}
      runHook postConfigure
    '';
    buildPhase = ''
      runHook preBuild
      cmake --build build -j$NIX_BUILD_CORES
      runHook postBuild
    '';
    installPhase = ''
      runHook preInstall
      cmake --install build
      runHook postInstall
    '';
  };

  # a class, a category, a block and runtime introspection, compiled with emcc and run under node:
  # the build fails unless the output is exactly what the program should print
  objcSmokeWasm = pkgs.runCommand "puredarwin-objc-smoke-wasm" { nativeBuildInputs = [ emcc pkgs.nodejs pkgs.python3 ]; } ''
    ${emSetup}
    cat > smoke.m <<'EOF'
    #include <objc/runtime.h>
    #include <stdio.h>
    __attribute__((objc_root_class))
    @interface Root { Class isa; }
    + (id)new;
    - (int)value;
    @end
    @implementation Root
    + (id)new { return class_createInstance(self, 0); }
    - (int)value { return 41; }
    @end
    @interface Root (Extra)
    - (int)more;
    @end
    @implementation Root (Extra)
    - (int)more { return [self value] + 1; }
    @end
    int main(void)
    {
        Root *r = [Root new];
        int (^twice)(int) = ^(int x) { return x * 2; };
        printf("value %d more %d block %d class %s\n", [r value], [r more], twice(21),
            class_getName(object_getClass(r)));
        return 0;
    }
    EOF
    emcc -O1 --profiling-funcs ${objcFlags} -I${libobjc2Wasm}/include smoke.m \
      ${libobjc2Wasm}/lib/libobjc.a ${emLinkFlags} -o smoke.js
    # not $out, which is where the result goes
    res=$(node smoke.js) || { node smoke.js 2>&1 | head -30; exit 1; }
    echo "$res"
    [ "$res" = "value 41 more 42 block 42 class Root" ]
    mkdir -p $out; cp smoke.js smoke.wasm $out/
  '';
in
{
  emscripten-cache-wasm = emCache;
  libobjc2-wasm = libobjc2Wasm;
  objc-smoke-wasm = objcSmokeWasm;
}
