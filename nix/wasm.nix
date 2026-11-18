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
    embuilder build libc libc++ libc++abi libc++-noexcept libc++abi-noexcept libcompiler_rt libc-mt libdlmalloc \
      libsockets libGL libstubs libnoexit libhtml5 libal libGL-getprocaddr
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
  # nil messages make typed functions at run time (libobjc2-web), which needs a table that can grow
  emLinkFlags = "-sGLOBAL_BASE=16777216 -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1 -sALLOW_TABLE_GROWTH";

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
      # objc_get_class too, for a class sent a message by name
      sed -i 's/^id objc_get_class(const char \*name)$/id objc_get_class(const char *name, ...)/' class_table.c
      sed -i 's/^id objc_get_class(const char \*name);$/id objc_get_class(const char *name, ...);/' objc/runtime-deprecated.h
      grep -q 'objc_get_class(const char \*name, \.\.\.)$' class_table.c
      sed -i 's/^void objc_resolve_class(Class);$/BOOL objc_resolve_class(Class);/' dtable.c
      sed -i 's/^PRIVATE void objc_resolve_class(Class);$/PRIVATE BOOL objc_resolve_class(Class);/' runtime.c
      grep -q '^PRIVATE BOOL objc_resolve_class(Class);' runtime.c
      grep -q '__objc_exec_class(struct objc_module_abi_8 \*module, \.\.\.)' loader.c
      grep -q 'objc_lookup_class(const char \*name, \.\.\.)$' class_table.c
      grep -q '^BOOL objc_resolve_class(Class);' dtable.c
      # the runtime calls +load, +initialize and the C++ ivar hooks through IMP, which is variadic in
      # libobjc2's headers. Those methods are void (id, SEL), so each call goes through that type instead
      sed -i 's/m->imp((id)class, loadSel);/((void (*)(id, SEL))m->imp)((id)class, loadSel);/' class_table.c
      sed -i 's/initializeSlot->imp((id)class, initializeSel);/((void (*)(id, SEL))initializeSlot->imp)((id)class, initializeSel);/' dtable.c
      sed -i 's/currentClass->cxx_destruct(obj, cxx_destruct);/((void (*)(id, SEL))currentClass->cxx_destruct)(obj, cxx_destruct);/' runtime.c
      sed -i 's/cls->cxx_construct(obj, cxx_construct);/((void (*)(id, SEL))cls->cxx_construct)(obj, cxx_construct);/' runtime.c
      grep -q '((void (\*)(id, SEL))m->imp)' class_table.c
      grep -q '((void (\*)(id, SEL))initializeSlot->imp)' dtable.c
      [ $(grep -c '((void (\*)(id, SEL))c[a-zA-Z]*->cxx_' runtime.c) = 2 ]
      # messages to nil answer with a zero method of the call's own wasm type (src/Libraries/libobjc2-web)
      cp ${../src/Libraries/libobjc2-web/nil_wasm.c} nil_wasm.c
      sed -i 's/^\tsendmsg2.c$/\tsendmsg2.c\n\tnil_wasm.c/' CMakeLists.txt
      grep -q '^	nil_wasm.c$' CMakeLists.txt
      python3 ${../src/Libraries/libobjc2-web/patch_sendmsg.py}
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

  # ICU for CoreFoundation's strings, locales and calendars. ICU cross-builds in two steps: its
  # tools natively first, then the library with emscripten against them (--with-cross-build).
  # Static libraries. The data stays a separate .dat file, loaded at run time from emscripten's file system,
  # since genccode can only write ELF or gcc assembly. A trimmed data file can replace it later.
  icuWasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-icu-wasm";
    inherit (pkgs.icu) version src;
    nativeBuildInputs = [ emcc pkgs.python3 ];
    dontStrip = true;
    dontConfigure = true;
    buildPhase = ''
      runHook preBuild
      ${emSetup}
      top=$PWD
      mkdir native && cd native
      ../source/configure --disable-tests --disable-samples --disable-extras > configure.log
      make -j$NIX_BUILD_CORES > make.log
      cd $top
      # configure does not know emscripten, the linux makefile fragment fits it, as ICU's own error suggests
      cp source/config/mh-linux source/config/mh-unknown
      mkdir wasm && cd wasm
      emconfigure ../source/configure --host=wasm32-unknown-emscripten --with-cross-build=$top/native \
        --enable-static --disable-shared --disable-tests --disable-samples --disable-extras --disable-tools \
        --with-data-packaging=archive --prefix=$out > configure.log
      emmake make -j$NIX_BUILD_CORES > make.log 2>&1 || { grep -v 'warning:\|note:' make.log | tail -40; exit 1; }
      runHook postBuild
    '';
    installPhase = ''
      runHook preInstall
      # the build phase left the shell in the wasm build directory
      emmake make install > install.log
      runHook postInstall
    '';
  };

  # libdispatch for the page: one thread, queues drained from the browser's event loop (src/Libraries/libdispatch-web)
  dispatchWebWasm = pkgs.runCommand "puredarwin-dispatch-web-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    src=${../src/Libraries/libdispatch-web}
    emcc -O2 -std=gnu11 -fblocks -Wall -Werror -I$src/include -I${libobjc2Wasm}/include \
      -c $src/dispatch_web.c -o dispatch_web.o
    mkdir -p $out/lib
    emar rcs $out/lib/libdispatch.a dispatch_web.o
    cp -r $src/include $out/include
  '';

  # once, ordered async work on a serial queue, a sync, a group, a one-shot timer and an after, run under node
  dispatchSmokeWasm = pkgs.runCommand "puredarwin-dispatch-smoke-wasm" { nativeBuildInputs = [ emcc pkgs.nodejs pkgs.python3 ]; } ''
    ${emSetup}
    cat > smoke.c <<'EOF'
    #include <dispatch/dispatch.h>
    #include <stdio.h>
    #include <stdlib.h>
    static char line[256];
    static void add(const char *s) { strcat(line, s); }
    int main(void)
    {
        static dispatch_once_t once;
        for (int i = 0; i < 3; i++)
            dispatch_once(&once, ^{ add("once "); });
        dispatch_queue_t q = dispatch_queue_create("smoke", DISPATCH_QUEUE_SERIAL);
        dispatch_async(q, ^{ add("a1 "); });
        dispatch_async(q, ^{ add("a2 "); });
        dispatch_sync(q, ^{ add("sync "); });
        dispatch_group_t g = dispatch_group_create();
        dispatch_group_async(g, q, ^{ add("g1 "); });
        dispatch_group_notify(g, q, ^{ add("notify "); });
        dispatch_source_t t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, q);
        dispatch_source_set_event_handler(t, ^{ add("timer "); dispatch_source_cancel(t); });
        dispatch_source_set_timer(t, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_MSEC), DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(t);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_MSEC), q, ^{ add("after"); puts(line); exit(0); });
        dispatch_main();
    }
    EOF
    sed -i '1i #include <string.h>' smoke.c
    emcc -O1 -fblocks -I${dispatchWebWasm}/include -I${libobjc2Wasm}/include smoke.c \
      ${dispatchWebWasm}/lib/libdispatch.a ${libobjc2Wasm}/lib/libobjc.a ${emLinkFlags} -o smoke.js
    res=$(node smoke.js) || { node smoke.js 2>&1 | head -30; exit 1; }
    echo "$res"
    [ "$res" = "once a1 a2 sync g1 notify timer after" ]
    mkdir -p $out; cp smoke.js smoke.wasm $out/
  '';

  # CoreFoundation (Apache 2.0, swift-corelibs) for the page, built from its sources directly: its CMake wants
  # libxml2 and libcurl off Darwin. The old XML parser stays out, as in CF's own non-Darwin build.
  cfWasm = pkgs.runCommand "puredarwin-corefoundation-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    cf=${../src/Frameworks/CoreFoundation}
    mkdir -p inc/CoreFoundation obj $out/lib $out/include/CoreFoundation
    for h in $cf/*/*.h $cf/Base.subproj/SwiftRuntime/*.h; do ln -sf $h inc/CoreFoundation/; done
    cp $cf/Base.subproj/SwiftRuntime/TargetConditionals.h inc/
    flags="-O2 -x c -std=gnu11 -fblocks -fno-common -fdollars-in-identifiers -D_GNU_SOURCE -DCF_BUILDING_CF=1 \
      -DU_SHOW_DRAFT_API=1 -include $cf/Base.subproj/CoreFoundation_Prefix.h -Iinc -Iinc/CoreFoundation \
      -I${icuWasm}/include -I${libobjc2Wasm}/include -I${dispatchWebWasm}/include -Wno-everything"
    # every file is tried, so one build shows every error left
    bad=0
    for f in $cf/*/*.c; do
      # the old XML parser, and PD's bookmark and resource code, which is written to Darwin's file system calls
      case $f in */CFXMLNode.c|*/CFXMLParser.c|*/CFURLBookmark_PureDarwin.c|*/CFURLResources_PureDarwin.c) continue ;; esac
      emcc $flags -c $f -o obj/$(basename $(dirname $f))_$(basename $f .c).o 2> err.log || { bad=1; grep error: err.log; }
    done
    [ $bad = 0 ]
    emar rcs $out/lib/libCoreFoundation.a obj/*.o
    cp -L inc/CoreFoundation/*.h $out/include/CoreFoundation/
    cp inc/TargetConditionals.h $out/include/
  '';

  # a CFString, a CFArray, a CFDictionary and a run loop timer, printed under node
  cfSmokeWasm = pkgs.runCommand "puredarwin-cf-smoke-wasm" { nativeBuildInputs = [ emcc pkgs.nodejs pkgs.python3 ]; } ''
    ${emSetup}
    cat > smoke.c <<'EOF'
    #include <CoreFoundation/CoreFoundation.h>
    #include <stdio.h>
    static int fired;
    static void tick(CFRunLoopTimerRef t, void *info) { fired++; }
    int main(void)
    {
        CFStringRef s = CFStringCreateWithFormat(NULL, NULL, CFSTR("hello %d from %s"), 42, "CF");
        char buf[128];
        CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8);
        CFStringRef up = CFStringCreateMutableCopy(NULL, 0, s);
        CFStringUppercase((CFMutableStringRef)up, NULL);
        char ubuf[128];
        CFStringGetCString(up, ubuf, sizeof(ubuf), kCFStringEncodingUTF8);
        const void *vals[] = { s, up };
        CFArrayRef a = CFArrayCreate(NULL, vals, 2, &kCFTypeArrayCallBacks);
        CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(d, CFSTR("key"), a);
        CFRunLoopTimerRef t = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent(), 0, 0, 0, tick, NULL);
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), t, kCFRunLoopDefaultMode);
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
        printf("%s | %s | %ld %ld | timer %d\n", buf, ubuf, (long)CFArrayGetCount(CFDictionaryGetValue(d, CFSTR("key"))),
            (long)CFDictionaryGetCount(d), fired);
        return 0;
    }
    EOF
    # the checks below are explicit, a filter that finds nothing must not end the build
    set +e +o pipefail
    # ICU's data is a file in the page's file system, ICU_DATA says where
    echo "Module.preRun = (Module.preRun || []).concat(() => { ENV.ICU_DATA = '/icu'; });" > icu.js
    emcc -O1 -fblocks -I${cfWasm}/include -I${cfWasm}/include/CoreFoundation -I${dispatchWebWasm}/include \
      -I${libobjc2Wasm}/include smoke.c ${cfWasm}/lib/libCoreFoundation.a ${dispatchWebWasm}/lib/libdispatch.a \
      ${libobjc2Wasm}/lib/libobjc.a ${icuWasm}/lib/libicui18n.a ${icuWasm}/lib/libicuuc.a ${icuWasm}/lib/libicudata.a \
      --embed-file ${icuWasm}/share/icu/${icuWasm.version}/icudt76l.dat@/icu/icudt76l.dat --pre-js icu.js \
      ${emLinkFlags} -sERROR_ON_UNDEFINED_SYMBOLS=1 -o smoke.js 2>&1 | tee link.log | grep -v "^warning" | head -60
    [ -f smoke.js ] || exit 1
    res=$(node smoke.js 2>&1) || true
    echo "$res" | head -60
    [ "$res" = "hello 42 from CF | HELLO 42 FROM CF | 2 1 | timer 1" ] || exit 1
    mkdir -p $out; cp smoke.js smoke.wasm $out/
  '';

  # libffi (MIT) for NSInvocation. Its wasm32 port calls through emscripten's JS glue, 64-bit values as BigInt
  ffiWasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-libffi-wasm";
    inherit (pkgs.libffi) version src;
    nativeBuildInputs = [ emcc pkgs.python3 ];
    dontStrip = true;
    configurePhase = ''
      ${emSetup}
      emconfigure ./configure --host=wasm32-unknown-emscripten --prefix=$out --enable-static --disable-shared \
        --disable-dependency-tracking --disable-builddir --disable-multi-os-directory --disable-raw-api --disable-docs \
        CFLAGS="-O2 -DWASM_BIGINT"
    '';
    buildPhase = "emmake make -j$NIX_BUILD_CORES";
    installPhase = "emmake make install";
  };

  # Foundation (MPL-2.0) on the Argon recipe for the page, as three static libraries: the objc compat layer,
  # CoreFoundation built as Objective-C so its objects bridge to Foundation classes, and Foundation itself
  foundationWasm = pkgs.runCommand "puredarwin-foundation-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    top=${lib.fileset.toSource { root = ../.; fileset = lib.fileset.unions [ ../src/Frameworks/CoreFoundation
      ../src/Frameworks/Foundation ../linux/objc-compat ../src/Libraries/libweb-compat ../nix/pkgs/apple/foundation.nix ]; }}
    cf=$top/src/Frameworks/CoreFoundation
    compat=$top/linux/objc-compat
    web=$top/src/Libraries/libweb-compat
    mkdir -p inc/CoreFoundation inc/Foundation $out/lib $out/include
    for h in $cf/*/*.h $cf/Base.subproj/SwiftRuntime/*.h; do ln -sf $h inc/CoreFoundation/; done
    find $top/src/Frameworks/Foundation -name '*.h' -exec ln -sf {} inc/Foundation/ \;
    cp $cf/Base.subproj/SwiftRuntime/TargetConditionals.h inc/
    objc="-O2 ${objcFlags} -fno-objc-arc -D__PUREDARWIN__=1 -DARGON_LIBOBJC2=1 -D_GNU_SOURCE -DDEPLOYMENT_RUNTIME_OBJC=1 \
      -DINCLUDE_OBJC=1 -I${libobjc2Wasm}/include -I${dispatchWebWasm}/include -I${icuWasm}/include -I${ffiWasm}/include \
      -I$web/include -Wno-everything"
    # every file is tried, so one build shows every error left
    bad=0
    cc() { lib=$1; lang=$2; f=$3; shift 3; mkdir -p obj/$lib
      emcc -x $lang $objc "$@" -c $f -o obj/$lib/$(basename $(dirname $f))_$(basename $f).o 2> err.log || { bad=1; grep error: err.log; }; }
    for f in $compat/NSObject.m $compat/weak-compat.m; do cc compat objective-c $f -I$compat/include; done
    for f in $compat/darwin-compat.c $web/web_compat.c; do cc compat c $f -I$compat/include; done
    # the .m files are the toll-free bridge's classes (__NSCFType, NSNull, the CFNumber and CFBoolean classes)
    for f in $cf/*/*.c $cf/Bridging.subproj/*.m; do
      case $f in */CFXMLNode.c|*/CFXMLParser.c|*/CFURLBookmark_PureDarwin.c|*/CFURLResources_PureDarwin.c) continue ;; esac
      cc cf objective-c $f -std=gnu11 -fdollars-in-identifiers -fno-common -DCF_BUILDING_CF=1 -DU_SHOW_DRAFT_API=1 \
        -include $cf/Base.subproj/CoreFoundation_Prefix.h -Iinc -Iinc/CoreFoundation -I$compat/include
    done
    srcs=$(sed -n '/mmSrcs = \[/,/\];/p' $top/nix/pkgs/apple/foundation.nix | grep -o '"[^"]*"' | tr -d '"')
    for s in $srcs String.subproj/NSConstantString; do
      case $s in RunLoop.subproj/NSPort|RunLoop.subproj/NSPortMessage|URL.subproj/NSNetServices|XML.subproj/*|*/NSAppleEvent*) continue ;; esac
      cc foundation objective-c $top/src/Frameworks/Foundation/$s.m -include argon-darwin.h -I$compat/include \
        -Iinc -Iinc/Foundation -Iinc/CoreFoundation
    done
    [ $bad = 0 ]
    emar rcs $out/lib/libargonobjc.a obj/compat/*.o
    emar rcs $out/lib/libCoreFoundation.a obj/cf/*.o
    emar rcs $out/lib/libFoundation.a obj/foundation/*.o
    mkdir -p $out/include/CoreFoundation $out/include/Foundation
    cp -L inc/CoreFoundation/*.h $out/include/CoreFoundation/
    cp -L inc/Foundation/*.h $out/include/Foundation/
    cp inc/TargetConditionals.h $out/include/
    cp -r $compat/include $out/include/argon-compat
    chmod -R u+w $out/include/argon-compat
    # the compat copy reaches into the source tree, the installed one is the header itself (as Argon does)
    cp inc/TargetConditionals.h $out/include/argon-compat/TargetConditionals.h
    cp -r $web/include/. $out/include/
  '';

  # everything an Objective-C program for the page links. The 1.9 ABI finds classes by name at run time, so no
  # symbol pulls a class's object out of an archive: the frameworks go in whole, as -ObjC does on Darwin
  foundationLibs = "-Wl,--whole-archive ${foundationWasm}/lib/libFoundation.a ${foundationWasm}/lib/libCoreFoundation.a"
    + " ${foundationWasm}/lib/libargonobjc.a -Wl,--no-whole-archive ${dispatchWebWasm}/lib/libdispatch.a ${libobjc2Wasm}/lib/libobjc.a"
    + " ${ffiWasm}/lib/libffi.a ${icuWasm}/lib/libicui18n.a ${icuWasm}/lib/libicuuc.a ${icuWasm}/lib/libicudata.a"
    # libffi's call glue is JavaScript and works on the wasm stack through these emscripten helpers
    + " -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=\\$stackSave,\\$stackRestore,\\$stackAlloc,\\$addFunction";

  # ICU's data is a file in the page's file system, ICU_DATA says where
  icuPreJs = pkgs.writeText "icu-pre.js" "Module.preRun = (Module.preRun || []).concat(() => { ENV.ICU_DATA = '/icu'; });";
  icuEmbed = "--embed-file ${icuWasm}/share/icu/${icuWasm.version}/icudt76l.dat@/icu/icudt76l.dat --pre-js ${icuPreJs}";

  # literals, formatting, collections, CF bridging, NSInvocation and a timer on the run loop, under node
  foundationSmokeWasm = pkgs.runCommand "puredarwin-foundation-smoke-wasm" { nativeBuildInputs = [ emcc pkgs.nodejs pkgs.python3 ]; } ''
    ${emSetup}
    set +e +o pipefail
    cat > smoke.m <<'EOF'
    #import <Foundation/Foundation.h>
    #include <stdio.h>
    @interface Probe : NSObject
    @property int fired;
    - (int)add:(int)a to:(int)b;
    - (void)tick:(NSTimer *)t;
    @end
    @implementation Probe
    - (int)add:(int)a to:(int)b { return a + b; }
    - (void)tick:(NSTimer *)t { self.fired++; }
    @end
    int main(void)
    {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSString *s = [NSString stringWithFormat:@"%@ %d", @"hello", 42];
        NSArray *a = [NSArray arrayWithObjects:@"b", @"a", @"c", nil];
        NSString *sorted = [[a sortedArrayUsingSelector:@selector(compare:)] componentsJoinedByString:@","];
        NSDictionary *d = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:7] forKey:@"k"];
        CFStringRef cf = CFStringCreateWithCString(NULL, "bridged", kCFStringEncodingUTF8);
        NSString *ns = (NSString *)cf;
        Probe *p = [[Probe alloc] init];
        NSMethodSignature *sig = [p methodSignatureForSelector:@selector(add:to:)];
        NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
        int x = 40, y = 2, r = 0;
        [inv setTarget:p]; [inv setSelector:@selector(add:to:)];
        [inv setArgument:&x atIndex:2]; [inv setArgument:&y atIndex:3];
        [inv invoke]; [inv getReturnValue:&r];
        [NSTimer scheduledTimerWithTimeInterval:0 target:p selector:@selector(tick:) userInfo:nil repeats:NO];
        // timers fire from the page's event loop, which runs once main hands it back: report from a later turn
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
            printf("%s | %s | %d | %s %lu | invoke %d | timer %d\n", [s UTF8String], [sorted UTF8String],
                [[d objectForKey:@"k"] intValue], [[ns uppercaseString] UTF8String], (unsigned long)[ns length], r, p.fired);
            exit(0);
        });
        dispatch_main();
    }
    EOF
    emcc -O1 ${objcFlags} -fno-objc-arc -DARGON_LIBOBJC2=1 -include argon-darwin.h -I${foundationWasm}/include \
      -I${foundationWasm}/include/argon-compat -I${foundationWasm}/include/Foundation -I${foundationWasm}/include/CoreFoundation \
      -I${libobjc2Wasm}/include -I${dispatchWebWasm}/include -Wno-everything smoke.m ${foundationLibs} \
      ${icuEmbed} ${emLinkFlags} -fwasm-exceptions -sWASM_BIGINT -sERROR_ON_UNDEFINED_SYMBOLS=1 --profiling-funcs -g -gsource-map -o smoke.js 2>&1 | grep -v "^warning" | head -80
    [ -f smoke.js ] || exit 1
    res=$(node --enable-source-maps smoke.js 2>&1)
    echo "$res" | head -60
    [ "$res" = "hello 42 | a,b,c | 7 | BRIDGED 7 | invoke 42 | timer 1" ] || exit 1
    mkdir -p $out; cp smoke.js smoke.wasm $out/
  '';

  # Foundation on the Argon recipe (linux/nix/packages.nix): its objc-compat shim, CF built as Objective-C
  # so CF objects bridge to Foundation classes, then Foundation's own sources. Every file is tried and every
  # error kept, so one build maps the port. Never fails, it is a measurement.
  foundationSurveyWasm = pkgs.runCommand "puredarwin-foundation-survey-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    set +e +o pipefail
    # only what the survey reads, not the whole tree
    top=${lib.fileset.toSource { root = ../.; fileset = lib.fileset.unions [ ../src/Frameworks/CoreFoundation
      ../src/Frameworks/Foundation ../linux/objc-compat ../src/Libraries/libweb-compat ../nix/pkgs/apple/foundation.nix ]; }}
    web=$top/src/Libraries/libweb-compat
    cf=$top/src/Frameworks/CoreFoundation
    compat=$top/linux/objc-compat
    mkdir -p inc/CoreFoundation inc/Foundation logs $out
    for h in $cf/*/*.h $cf/Base.subproj/SwiftRuntime/*.h; do ln -sf $h inc/CoreFoundation/; done
    find $top/src/Frameworks/Foundation -name '*.h' -exec ln -sf {} inc/Foundation/ \;
    cp $cf/Base.subproj/SwiftRuntime/TargetConditionals.h inc/
    objc="-fobjc-runtime=gnustep-1.9 -fblocks -fno-objc-arc -D__PUREDARWIN__=1 -DARGON_LIBOBJC2=1 -D_GNU_SOURCE \
      -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 -I${libobjc2Wasm}/include -I${dispatchWebWasm}/include \
      -I${icuWasm}/include -I${ffiWasm}/include -Wno-everything"
    try() {
      layer=$1; lang=$2; f=$3; shift 3
      log=logs/$layer-$(basename $f).log
      if emcc -x $lang $objc "$@" -c $f -o /dev/null > $log 2>&1; then echo "ok   $layer $(basename $f)" >> $out/files.txt
      else echo "FAIL $layer $(basename $f): $(grep -m1 'error:' $log | sed 's/^[^ ]* //')" >> $out/files.txt; fi
    }
    # Argon's inotify notify(3) and $ARGON_ROOT redirection have no meaning in a page, libweb-compat stands in
    for f in $compat/NSObject.m $compat/weak-compat.m; do try compat objective-c $f -I$web/include -I$compat/include; done
    for f in $compat/darwin-compat.c $web/web_compat.c; do try compat c $f -I$web/include -I$compat/include; done
    cfflags="-std=gnu11 -fdollars-in-identifiers -fno-common -DCF_BUILDING_CF=1 -DU_SHOW_DRAFT_API=1 \
      -include $cf/Base.subproj/CoreFoundation_Prefix.h -Iinc -Iinc/CoreFoundation -I$web/include -I$compat/include"
    for f in $cf/*/*.c; do
      case $f in */CFXMLNode.c|*/CFXMLParser.c|*/CFURLBookmark_PureDarwin.c|*/CFURLResources_PureDarwin.c) continue ;; esac
      try cf objective-c $f $cfflags
    done
    srcs=$(sed -n '/mmSrcs = \[/,/\];/p' $top/nix/pkgs/apple/foundation.nix | grep -o '"[^"]*"' | tr -d '"')
    for s in $srcs String.subproj/NSConstantString; do
      # Argon's Darwin-only skips, plus XML (libxml2 is not built for wasm yet) and Apple events
      case $s in RunLoop.subproj/NSPort|RunLoop.subproj/NSPortMessage|URL.subproj/NSNetServices|XML.subproj/*|*/NSAppleEvent*) continue ;; esac
      try foundation objective-c $top/src/Frameworks/Foundation/$s.m -include argon-darwin.h -I$web/include -I$compat/include \
        -Iinc -Iinc/Foundation -Iinc/CoreFoundation
    done
    {
      for l in compat cf foundation; do
        echo "$l: $(grep -c "^ok   $l " $out/files.txt) ok, $(grep -c "^FAIL $l " $out/files.txt) failed"
      done
      echo; echo "all errors by cause:"
      cat logs/*.log | grep 'error:' | sed 's/^[^ ]* error: //' | sed "s/'[^']*'/'X'/g" | sort | uniq -c | sort -rn | head -40
      echo; echo "missing headers:"
      cat logs/*.log | grep "fatal error: '" | sed "s/.*fatal error: '\([^']*\)'.*/\1/" | sort | uniq -c | sort -rn
      echo; echo "names:"
      cat logs/*.log | grep -o "unknown type name '[^']*'\|undeclared identifier '[^']*'\|undeclared function '[^']*'\|no member named '[^']*' in '[^']*'" | sort | uniq -c | sort -rn | head -50
    } > $out/summary.txt
    cp -r logs $out/
  '';

  # the image and font libraries Onyx2D and CoreText use, static for wasm from nixpkgs' own sources.
  # emscripten's ports fetch at build time, which the sandbox does not allow.
  zlibWasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-zlib-wasm";
    inherit (pkgs.zlib) version src;
    nativeBuildInputs = [ emcc pkgs.python3 ];
    dontStrip = true;
    configurePhase = "${emSetup}\nemconfigure ./configure --static --prefix=$out";
    buildPhase = "emmake make -j$NIX_BUILD_CORES libz.a";
    installPhase = "emmake make install";
  };

  # a CMake library as static wasm, FLAGS its CMake options
  emCMake = { pname, pkg, flags ? "", deps ? [ ] }: pkgs.stdenv.mkDerivation {
    inherit pname;
    inherit (pkg) version src;
    nativeBuildInputs = [ emcc pkgs.cmake pkgs.python3 ];
    dontStrip = true;
    dontUseCmakeConfigure = true;
    configurePhase = ''
      ${emSetup}
      emcmake cmake -S . -B wasm-build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_INSTALL_PREFIX=$out -DCMAKE_PREFIX_PATH="${lib.concatStringsSep ";" deps}" ${flags}
    '';
    buildPhase = "cmake --build wasm-build -j$NIX_BUILD_CORES";
    installPhase = "cmake --install wasm-build";
  };

  pngWasm = emCMake {
    pname = "puredarwin-libpng-wasm"; pkg = pkgs.libpng; deps = [ zlibWasm ];
    # emscripten's toolchain searches only its sysroot, so dependencies are named outright
    flags = "-DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF"
      + " -DZLIB_LIBRARY=${zlibWasm}/lib/libz.a -DZLIB_INCLUDE_DIR=${zlibWasm}/include";
  };

  jpegWasm = emCMake {
    pname = "puredarwin-libjpeg-turbo-wasm"; pkg = pkgs.libjpeg;
    flags = "-DENABLE_SHARED=OFF -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF -DWITH_JAVA=OFF";
  };

  freetypeWasm = emCMake {
    pname = "puredarwin-freetype-wasm"; pkg = pkgs.freetype; deps = [ zlibWasm pngWasm ];
    flags = "-DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON"
      + " -DZLIB_LIBRARY=${zlibWasm}/lib/libz.a -DZLIB_INCLUDE_DIR=${zlibWasm}/include"
      + " -DPNG_LIBRARY=${pngWasm}/lib/libpng.a -DPNG_PNG_INCLUDE_DIR=${pngWasm}/include";
  };

  tiffWasm = emCMake {
    pname = "puredarwin-libtiff-wasm"; pkg = pkgs.libtiff;
    flags = "-Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF -Dtiff-docs=OFF -Dlzma=OFF -Dzstd=OFF -Dwebp=OFF"
      + " -Djbig=OFF -Dlerc=OFF -Dlibdeflate=OFF -Dcxx=OFF"
      + " -DZLIB_LIBRARY=${zlibWasm}/lib/libz.a -DZLIB_INCLUDE_DIR=${zlibWasm}/include"
      + " -DJPEG_LIBRARY=${jpegWasm}/lib/libjpeg.a -DJPEG_INCLUDE_DIR=${jpegWasm}/include";
  };

  expatWasm = emCMake {
    pname = "puredarwin-expat-wasm"; pkg = pkgs.expat;
    flags = "-DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_DOCS=OFF"
      + " -DEXPAT_SHARED_LIBS=OFF";
  };

  # meson's view of emscripten, for the libraries that build with meson
  emMesonCross = pkgs.writeText "emscripten-cross.ini" ''
    [binaries]
    c = 'emcc'
    cpp = 'em++'
    ar = 'emar'
    ranlib = 'emranlib'
    pkg-config = 'pkg-config'

    [host_machine]
    system = 'emscripten'
    cpu_family = 'wasm32'
    cpu = 'wasm32'
    endian = 'little'
  '';

  # fribidi (LGPL-2.1), the bidirectional text algorithm CoreText's typesetter runs
  fribidiWasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-fribidi-wasm";
    inherit (pkgs.fribidi) version src;
    nativeBuildInputs = [ emcc pkgs.meson pkgs.ninja pkgs.pkg-config pkgs.python3 ];
    dontStrip = true;
    dontUseMesonConfigure = true;
    dontUseNinjaBuild = true;
    dontUseNinjaInstall = true;
    configurePhase = ''
      ${emSetup}
      patchShebangs .
      meson setup wasm-build --cross-file ${emMesonCross} --prefix=$out --default-library=static --wrap-mode=nodownload \
        --buildtype=release -Ddocs=false -Dtests=false -Dbin=false
    '';
    buildPhase = "ninja -C wasm-build -j$NIX_BUILD_CORES";
    installPhase = "ninja -C wasm-build install";
  };

  # fontconfig (MIT-style) over the freetype and expat above. Its configuration and the fonts are files in
  # the page's file system, the frameworks match fonts through it as they do on Linux.
  fontconfigWasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-fontconfig-wasm";
    inherit (pkgs.fontconfig) version src;
    nativeBuildInputs = [ emcc pkgs.meson pkgs.ninja pkgs.pkg-config pkgs.gperf pkgs.python3 pkgs.gettext ];
    dontStrip = true;
    dontUseMesonConfigure = true;
    dontUseNinjaBuild = true;
    dontUseNinjaInstall = true;
    configurePhase = ''
      ${emSetup}
      # the generators fontconfig runs at build time say /usr/bin/env, which the sandbox has not got
      patchShebangs .
      # freetype's .pc requires libpng's and zlib's
      export PKG_CONFIG_PATH=${freetypeWasm}/lib/pkgconfig:${expatWasm}/lib/pkgconfig:${pngWasm}/lib/pkgconfig:${zlibWasm}/lib/pkgconfig
      export PKG_CONFIG_LIBDIR=$PKG_CONFIG_PATH
      meson setup wasm-build --cross-file ${emMesonCross} --prefix=$out --default-library=static --wrap-mode=nodownload \
        --buildtype=release -Ddoc=disabled -Dtests=disabled -Dtools=disabled -Dcache-build=disabled -Dnls=disabled \
        -Dxml-backend=expat -Dbaseconfig-dir=/etc/fonts -Dconfig-dir=/etc/fonts/conf.d -Dcache-dir=/var/cache/fontconfig \
        -Ddefault-fonts-dirs=/System/Library/Fonts
    '';
    buildPhase = "ninja -C wasm-build -j$NIX_BUILD_CORES";
    # /etc/fonts and the cache directory are paths inside the page, they land under $out/page for its file system
    installPhase = ''
      DESTDIR=$TMPDIR/dest ninja -C wasm-build install
      cp -r $TMPDIR/dest$out/. $out/
      mkdir -p $out/page
      # conf.d links into conf.avail, already in $out: the page gets the files themselves
      cp -rL $TMPDIR/dest/etc $out/page/
    '';
  };

  # harfbuzz builds as one translation unit, harfbuzz.cc, over the freetype above
  harfbuzzWasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-harfbuzz-wasm";
    inherit (pkgs.harfbuzz) version src;
    nativeBuildInputs = [ emcc pkgs.python3 ];
    dontStrip = true;
    dontConfigure = true;
    buildPhase = ''
      ${emSetup}
      em++ -O2 -std=c++17 -fno-exceptions -fno-rtti -DHAVE_FREETYPE=1 -DHB_NO_MT \
        -I${freetypeWasm}/include/freetype2 -c src/harfbuzz.cc -o harfbuzz.o
      emar rcs libharfbuzz.a harfbuzz.o
    '';
    installPhase = ''
      mkdir -p $out/lib $out/include/harfbuzz
      cp libharfbuzz.a $out/lib/
      cp src/hb*.h $out/include/harfbuzz/
    '';
  };

  # the graphics frameworks and AppKit for the page on Argon's graphics recipe (linux/nix/packages.nix), as static
  # libraries. WindowServer is rpc_web.c, a canvas per window. CGL and the Wayland pieces have no page counterpart.
  appkitWasm = pkgs.runCommand "puredarwin-appkit-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    top=${lib.fileset.toSource { root = ../.; fileset = lib.fileset.unions [ ../src/Frameworks/Onyx2D ../src/Frameworks/CoreGraphics
      ../src/Frameworks/CoreText ../src/Frameworks/QuartzCore ../src/Frameworks/AppKit ../src/Frameworks/WindowServer
      ../src/Frameworks/OpenGL ../src/Frameworks/CoreVideo ../src/Frameworks/ApplicationServices ../src/Libraries/CoreServices
      ../linux/iokit ]; }}
    fw=$top/src/Frameworks
    F=${foundationWasm}/include
    mkdir -p inc/OpenGL inc/CoreVideo inc/CoreServices inc/IOKit inc/WindowServer $out/lib
    cp $fw/OpenGL/include/OpenGL/*.h inc/OpenGL/
    cp $fw/CoreVideo/*.h inc/CoreVideo/
    cp $top/src/Libraries/CoreServices/include/CoreServices/*.h inc/CoreServices/
    cp $top/linux/iokit/include/IOKit/*.h inc/IOKit/
    for f in Onyx2D CoreGraphics CoreText QuartzCore AppKit WindowServer ApplicationServices; do
      mkdir -p inc/$f; find $fw/$f -name '*.h' -exec cp {} inc/$f/ \;
    done
    flags="-O2 ${objcFlags} -fno-objc-arc -D__PUREDARWIN__=1 -DARGON_LIBOBJC2=1 -D_GNU_SOURCE -DDEPLOYMENT_RUNTIME_OBJC=1 \
      -DINCLUDE_OBJC=1 -include argon-darwin.h -I$F/argon-compat -I$F -I$F/Foundation -I$F/CoreFoundation -Iinc -Iinc/AppKit \
      -Iinc/Onyx2D -Iinc/CoreGraphics -I${libobjc2Wasm}/include -I${dispatchWebWasm}/include \
      -I${freetypeWasm}/include/freetype2 -I${harfbuzzWasm}/include/harfbuzz -I${pngWasm}/include -I${jpegWasm}/include \
      -I${zlibWasm}/include -I${fontconfigWasm}/include -I${fontconfigWasm}/include/fontconfig -I${tiffWasm}/include \
      -I${fribidiWasm}/include/fribidi -I${icuWasm}/include -Wno-everything"
    # every file is tried, so one build shows every error left
    bad=0
    cc() { lib=$1; dir=$2; f=$3; mkdir -p obj/$lib
      lang=objective-c; case $f in *.c) lang=c ;; esac
      emcc -x $lang $flags -c $dir/$f -o obj/$lib/$(echo $f | tr / _).o 2> err.log || { bad=1; echo "== $lib/$f"; grep error: err.log | head -5; }; }
    for f in $(cd $fw/Onyx2D && ls *.m); do cc Onyx2D $fw/Onyx2D $f; done
    for f in $(cd $fw/CoreGraphics && ls *.m | grep -vxE 'CGLPixelSurface.m|CGDirectDisplay.m|KTFont\+PDF.m'); do cc CoreGraphics $fw/CoreGraphics $f; done
    # every CoreText source: the font registry and descriptors live outside the two files Argon lists
    for f in $(cd $fw/CoreText && ls *.m); do cc CoreText $fw/CoreText $f; done
    for f in $(cd $fw/QuartzCore && ls *.m); do cc QuartzCore $fw/QuartzCore $f; done
    cc WindowServer $fw/WindowServer rpc_web.c
    cc CGL $fw/OpenGL CGL_web.c
    cc CoreServices $top/src/Libraries/CoreServices LaunchServices.c
    cc CoreServices $top/src/Libraries/CoreServices MultiprocessingCompat.c
    for f in $(cd $fw/AppKit && find . -name '*.m' | sed 's|^\./||' | grep -v '^CoreData_/' | sort); do cc AppKit $fw/AppKit $f; done
    [ $bad = 0 ]
    for l in Onyx2D CoreGraphics CoreText QuartzCore WindowServer CGL CoreServices AppKit; do emar rcs $out/lib/lib$l.a obj/$l/*.o; done
    cp -r inc $out/include
    mkdir -p $out/Resources
    cp -a $fw/AppKit/*.lproj $out/Resources/ 2>/dev/null || true
  '';

  systemFonts = pkgs.callPackage ./pkgs/base/system-fonts.nix { };

  # everything an AppKit program for the page links, frameworks whole (classes are found by name)
  appkitLibs = "-Wl,--whole-archive ${appkitWasm}/lib/libAppKit.a ${appkitWasm}/lib/libQuartzCore.a"
    + " ${appkitWasm}/lib/libCoreText.a ${appkitWasm}/lib/libCoreGraphics.a ${appkitWasm}/lib/libOnyx2D.a"
    + " ${appkitWasm}/lib/libWindowServer.a ${appkitWasm}/lib/libCGL.a ${appkitWasm}/lib/libCoreServices.a"
    + " -Wl,--no-whole-archive"
    + " ${fontconfigWasm}/lib/libfontconfig.a ${expatWasm}/lib/libexpat.a ${harfbuzzWasm}/lib/libharfbuzz.a"
    + " ${fribidiWasm}/lib/libfribidi.a"
    + " ${freetypeWasm}/lib/libfreetype.a ${tiffWasm}/lib/libtiff.a ${pngWasm}/lib/libpng.a ${jpegWasm}/lib/libjpeg.a"
    + " ${zlibWasm}/lib/libz.a ${foundationLibs}";

  # the page's file system: the fonts, fontconfig's configuration and ICU's data, preloaded before main runs
  pageFiles = pkgs.runCommand "puredarwin-web-files" { } ''
    mkdir -p $out/System/Library/Fonts $out/etc $out/icu $out/var/cache/fontconfig
    cp -r ${systemFonts}/System/Library/Fonts/Inter $out/System/Library/Fonts/
    cp -r ${fontconfigWasm}/page/etc/fonts $out/etc/
    cp ${icuWasm}/share/icu/${icuWasm.version}/icudt76l.dat $out/icu/
  '';

  # an AppKit program from src/Tests/web/NAME as a page: open index.html from a web server
  webPage = name: pkgs.runCommand "puredarwin-web-${name}" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    set +e +o pipefail
    src=${../src/Tests/web + "/${name}"}
    A=${appkitWasm}/include
    F=${foundationWasm}/include
    emcc -O1 ${objcFlags} -fno-objc-arc -DARGON_LIBOBJC2=1 -include argon-darwin.h -I$F/argon-compat -I$F -I$F/Foundation \
      -I$F/CoreFoundation -I$A -I$A/AppKit -I$A/Onyx2D -I$A/CoreGraphics -I${libobjc2Wasm}/include -I${dispatchWebWasm}/include \
      -Wno-everything $src/main.m ${appkitLibs} --preload-file ${pageFiles}@/ --pre-js ${icuPreJs} \
      ${emLinkFlags} -fexceptions -sLEGACY_GL_EMULATION=1 -sWASM_BIGINT -sERROR_ON_UNDEFINED_SYMBOLS=1 --profiling-funcs \
      -sEXPORTED_RUNTIME_METHODS=UTF8ToString -Wl,--error-limit=0 -o ${name}.js 2>&1 | grep -v "^warning" | head -400
    [ -f ${name}.js ] || exit 1
    mkdir -p $out
    cp ${name}.js ${name}.wasm ${name}.data $src/index.html $out/
  '';

  webHello = webPage "hello";
  webGL = webPage "gl";

  # the hello page in headless Chromium: it must come up and see a click on its button (src/Tests/web/check.mjs).
  # The screenshots before and after the click are the output, for a person reading CI.
  webCheck = pkgs.runCommand "puredarwin-web-check" { nativeBuildInputs = [ pkgs.chromium pkgs.nodejs pkgs.python3 ]; } ''
    export HOME=$TMPDIR
    mkdir -p $out
    node ${../src/Tests/web/check.mjs} ${pkgs.chromium}/bin/chromium ${webHello} $out 2>&1 | tee $out/log.txt
    grep -q '^PASS$' $out/log.txt
  '';

  # the GL page: an NSOpenGLView's triangle through CGL on WebGL, read back into the window
  webCheckGL = pkgs.runCommand "puredarwin-web-check-gl" { nativeBuildInputs = [ pkgs.chromium pkgs.nodejs pkgs.python3 ]; } ''
    export HOME=$TMPDIR
    mkdir -p $out
    node ${../src/Tests/web/check.mjs} ${pkgs.chromium}/bin/chromium ${webGL} $out gl 2>&1 | tee $out/log.txt
    grep -q '^PASS$' $out/log.txt
  '';

  # LLVM 21's middle end for wasm, the libraries air2spv reads AIR and runs its passes with (no backends). The cross
  # build makes a native llvm-tblgen of its own first
  llvmWasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-llvm-wasm";
    inherit (pkgs.llvmPackages_21.llvm) version;
    src = pkgs.llvmPackages_21.llvm.monorepoSrc;
    nativeBuildInputs = [ emcc pkgs.cmake pkgs.ninja pkgs.python3 ];
    dontStrip = true;
    dontUseCmakeConfigure = true;
    dontUseNinjaBuild = true;
    dontUseNinjaInstall = true;
    configurePhase = ''
      ${emSetup}
      emcmake cmake -S llvm -B wasm-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$out \
        -DLLVM_TARGETS_TO_BUILD= -DLLVM_ENABLE_PROJECTS= -DLLVM_ENABLE_THREADS=OFF -DLLVM_ENABLE_ZLIB=OFF \
        -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_TERMINFO=OFF -DLLVM_ENABLE_LIBEDIT=OFF \
        -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_UTILS=OFF \
        -DLLVM_BUILD_TOOLS=OFF -DLLVM_INCLUDE_TOOLS=OFF -DLLVM_ENABLE_BACKTRACES=OFF -DLLVM_ENABLE_PIC=OFF \
        -DLLVM_DEFAULT_TARGET_TRIPLE=wasm32-unknown-emscripten -DLLVM_HOST_TRIPLE=wasm32-unknown-emscripten \
        -DCMAKE_C_FLAGS=-fexceptions -DCMAKE_CXX_FLAGS=-fexceptions
    '';
    buildPhase = "ninja -C wasm-build -j$NIX_BUILD_CORES LLVMPasses LLVMBitReader";
    installPhase = ''
      mkdir -p $out/lib $out/include
      cp wasm-build/lib/*.a $out/lib/
      cp -r llvm/include/llvm llvm/include/llvm-c $out/include/
      cp -r wasm-build/include/llvm/. $out/include/llvm/
    '';
  };

  # air2spv's command line for wasm on the LLVM above, run under node with the real file system
  air2spvWasm = pkgs.runCommand "puredarwin-air2spv-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    src=${lib.fileset.toSource { root = ../src/Tools; fileset = lib.fileset.unions [ ../src/Tools/air2spv
      ../src/Tools/airjitd/bridge.h ../src/Tools/airjitd/ir.h ../src/Tools/airjitd/ir.c ]; }}
    cd $src/air2spv
    mkdir -p $out/bin
    for f in spv.c translate.c iface.c air.c cfg.c a2s.c ../airjitd/ir.c main.c; do
      emcc -O2 -std=gnu11 -Wno-everything -I${llvmWasm}/include -I${pkgs.spirv-headers}/include -c $f \
        -o $TMPDIR/$(basename $f .c).o
    done
    # linked as C++: LLVM brings the C++ runtime
    em++ $TMPDIR/*.o $(ls ${llvmWasm}/lib/libLLVM*.a) -fexceptions -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 \
      -sSTACK_SIZE=8388608 -o $out/bin/air2spv.js
  '';

  # Indium's tests (0BSD), whose shaders are Apple sample code precompiled with Apple's metal
  indiumSrc = pkgs.fetchFromGitHub {
    owner = "darlinghq";
    repo = "indium";
    rev = "8423a7d2f053167030d2bc4a227f96243c740667";
    hash = "sha256-yrGSbR7ZllQnHMN98YMkcn+IF2oOPvuDqP3cIU7BYVg=";
  };

  # every Indium test shader through the native air2spv and the wasm one: the SPIR-V and reflection must match
  air2spvWasmCheck = pkgs.runCommand "puredarwin-air2spv-wasm-check" { nativeBuildInputs = [ pkgs.nodejs ]; } ''
    native=${pkgs.callPackage ./pkgs/gpu/air2spv.nix { }}/bin/air2spv
    mkdir -p $out
    n=0 bad=0
    for m in $(find ${indiumSrc}/test -name '*.metallib' | sort); do
      t=$(basename $(dirname $m))-$(basename $m .metallib)
      $native $m -o native.spv -r native.json > native.log 2>&1 || { echo "native failed: $t"; continue; }
      node ${air2spvWasm}/bin/air2spv.js $m -o wasm.spv -r wasm.json > wasm.log 2>&1 || { echo "wasm failed: $t"; cat wasm.log | head -5; bad=$((bad+1)); continue; }
      if cmp -s native.spv wasm.spv && cmp -s native.json wasm.json; then echo "same: $t ($(stat -c %s wasm.spv) bytes)"
      else echo "DIFFERENT: $t"; bad=$((bad+1)); fi
      n=$((n+1))
    done | tee $out/report.txt
    # no shaders found is a failure too, never an empty pass
    grep -q "^same: " $out/report.txt
    ! grep -q "DIFFERENT\|wasm failed" $out/report.txt
  '';

  # Dawn (BSD-3-Clause) at the commit the WebGPU package above was made from, without its submodules
  dawnSrc = pkgs.fetchFromGitHub {
    owner = "google";
    repo = "dawn";
    rev = "01940842b667a7812d0e4ca0ef4367fbec294241";
    hash = "sha256-31kSvVPTwqv+R0W2bFu9hTAoOUg+TpimOegrNHQEIik=";
  };

  # Tint, Dawn's shader compiler, for wasm: SPIR-V in, WGSL out, the step air2spv's output takes to WebGPU.
  # Only tint_api and what it needs are built, over nixpkgs' SPIRV-Tools, SPIRV-Headers and Abseil sources
  tintWasm = pkgs.stdenv.mkDerivation {
    pname = "puredarwin-tint-wasm";
    version = "0194084";
    src = dawnSrc;
    # Dawn's generators template with jinja2, its third_party copy is a submodule left out
    nativeBuildInputs = [ emcc pkgs.cmake pkgs.ninja (pkgs.python3.withPackages (ps: [ ps.jinja2 ps.markupsafe ])) ];
    dontStrip = true;
    dontUseCmakeConfigure = true;
    dontUseNinjaBuild = true;
    dontUseNinjaInstall = true;
    # Dawn's emscripten build is for its webgpu.h alone and stops adding third party code early, before the
    # SPIR-V headers and tools Tint's SPIR-V reader is built on
    postPatch = ''
      grep -q '^if (EMSCRIPTEN)$' third_party/CMakeLists.txt
      sed -i '/^if (EMSCRIPTEN)$/{N;/\n    return()$/{N;d}}' third_party/CMakeLists.txt
      ! grep -A1 '^if (EMSCRIPTEN)$' third_party/CMakeLists.txt | grep -q 'return()'
    '';
    configurePhase = ''
      ${emSetup}
      emcmake cmake -S . -B wasm-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DDAWN_SPIRV_TOOLS_DIR=${pkgs.spirv-tools.src} -DDAWN_SPIRV_HEADERS_DIR=${pkgs.spirv-headers.src} \
        -DDAWN_ABSEIL_DIR=${pkgs.abseil-cpp.src} -DDAWN_FETCH_DEPENDENCIES=OFF -DDAWN_BUILD_MONOLITHIC_LIBRARY=OFF \
        -DDAWN_JINJA2_DIR=${pkgs.python3Packages.jinja2}/${pkgs.python3.sitePackages}/jinja2 \
        -DDAWN_MARKUPSAFE_DIR=${pkgs.python3Packages.markupsafe}/${pkgs.python3.sitePackages}/markupsafe \
        -DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_TESTS=OFF -DDAWN_BUILD_PROTOBUF=OFF -DDAWN_ENABLE_INSTALL=OFF \
        -DDAWN_ENABLE_D3D11=OFF -DDAWN_ENABLE_D3D12=OFF -DDAWN_ENABLE_METAL=OFF -DDAWN_ENABLE_NULL=OFF \
        -DDAWN_ENABLE_DESKTOP_GL=OFF -DDAWN_ENABLE_OPENGLES=OFF -DDAWN_ENABLE_VULKAN=OFF \
        -DDAWN_ENABLE_WEBGPU_ON_WEBGPU=OFF -DDAWN_ENABLE_SPIRV_VALIDATION=OFF -DDAWN_USE_GLFW=OFF \
        -DTINT_BUILD_SPV_READER=ON -DTINT_BUILD_WGSL_WRITER=ON -DTINT_BUILD_WGSL_READER=ON \
        -DTINT_BUILD_SPV_WRITER=OFF -DTINT_BUILD_MSL_WRITER=OFF -DTINT_BUILD_HLSL_WRITER=OFF \
        -DTINT_BUILD_GLSL_WRITER=OFF -DTINT_BUILD_GLSL_VALIDATOR=OFF -DTINT_BUILD_CMD_TOOLS=OFF \
        -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_IR_BINARY=OFF -DTINT_BUILD_FUZZERS=OFF -DTINT_BUILD_BENCHMARKS=OFF \
        "-DCMAKE_CXX_FLAGS=-fexceptions -Wno-error -Wno-c2y-extensions -Wno-deprecated-pragma -I${pkgs.spirv-headers.src}/include -I$PWD/wasm-build/third_party/spirv-tools" \
        "-DCMAKE_C_FLAGS=-fexceptions -Wno-error -Wno-deprecated-pragma -I${pkgs.spirv-headers.src}/include"
    '';
    # Tint's SPIR-V reader includes tables SPIRV-Tools generates, so those come first
    buildPhase = ''
      ninja -C wasm-build -j$NIX_BUILD_CORES SPIRV-Tools-opt
      ninja -C wasm-build -j$NIX_BUILD_CORES tint_api
    '';
    installPhase = ''
      mkdir -p $out/lib $out/include
      find wasm-build -name '*.a' -exec cp {} $out/lib/ \;
      # Tint's headers include each other as src/tint/..., src/utils/... and include/...
      mkdir -p $out/include/src
      cp -r src/tint src/utils $out/include/src/
      cp -r include/. $out/include/
      cp -r wasm-build/gen $out/include/gen 2>/dev/null || true
    '';
  };

  # spv2wgsl for wasm: airjitd's SPIR-V to WGSL step (webgpu/wgsl.cc over Tint) as a node command, for checking
  spv2wgslWasm = pkgs.runCommand "puredarwin-spv2wgsl-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    w=${../src/Tools/airjitd/webgpu}
    T=${tintWasm}/include
    mkdir -p $out/bin
    em++ -O2 -std=c++20 -fexceptions -Wno-everything -I$T -I$T/gen \
      -c $w/wgsl.cc -o $TMPDIR/wgsl.o
    emcc -O2 -Wno-everything -c $w/spv2wgsl.c -o $TMPDIR/spv2wgsl.o
    # the archives depend on each other in no fixed order, the group resolves them all
    em++ $TMPDIR/wgsl.o $TMPDIR/spv2wgsl.o -Wl,--start-group $(ls ${tintWasm}/lib/*.a) -Wl,--end-group -fexceptions \
      -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8388608 -o $out/bin/spv2wgsl.js
  '';

  # every Indium test shader the way the page will take it: air2spv with the push constants as a uniform buffer, then
  # Tint to WGSL. Each WGSL is kept for reading
  wgslCheck = pkgs.runCommand "puredarwin-wgsl-check" { nativeBuildInputs = [ pkgs.nodejs ]; } ''
    mkdir -p $out
    for m in $(find ${indiumSrc}/test -name '*.metallib' | sort); do
      t=$(basename $(dirname $m))-$(basename $m .metallib)
      if ! node ${air2spvWasm}/bin/air2spv.js -W -U 31 $m -o $t.spv -r $out/$t.json > $t.log 2>&1; then
        echo "air2spv failed: $t: $(head -3 $t.log)"; continue
      fi
      if node ${spv2wgslWasm}/bin/spv2wgsl.js $t.spv $out/$t.wgsl > $t.log 2>&1; then echo "wgsl: $t ($(wc -l < $out/$t.wgsl) lines)"
      else echo "TINT FAILED: $t: $(head -c 600 $t.log)"; fi
    done | tee $out/report.txt
    grep -q "^wgsl: " $out/report.txt
  '';

  # Dawn's webgpu.h for emscripten, the release emscripten 5.0.6 pins (its remote port), fetched with that port's hash
  emdawnwebgpu = pkgs.runCommand "puredarwin-emdawnwebgpu" { nativeBuildInputs = [ pkgs.unzip ]; } ''
    mkdir -p $out
    unzip -q ${pkgs.fetchurl {
      url = "https://github.com/google/dawn/releases/download/v20251002.162335/emdawnwebgpu_pkg-v20251002.162335.zip";
      sha512 = "ed15672c2c495a77c764929e6979f4e155bf8b9c46dee5b0f234f3208a708bc2b846d89eef345b725d03454b56d549531f48fc84ff2afe7627d14115893b0fb0";
    }} -d $out
  '';

  # the graphics frameworks and AppKit compiled for wasm file by file on Argon's graphics recipe, every error
  # kept. The image and font libraries' headers are the native ones here, only to measure. Never fails.
  graphicsSurveyWasm = pkgs.runCommand "puredarwin-graphics-survey-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    set +e +o pipefail
    top=${lib.fileset.toSource { root = ../.; fileset = lib.fileset.unions [ ../src/Frameworks/Onyx2D ../src/Frameworks/CoreGraphics
      ../src/Frameworks/CoreText ../src/Frameworks/QuartzCore ../src/Frameworks/AppKit ../src/Frameworks/WindowServer
      ../src/Frameworks/OpenGL ../src/Frameworks/CoreVideo ../src/Frameworks/ApplicationServices ../src/Libraries/CoreServices
      ../linux/iokit ]; }}
    fw=$top/src/Frameworks
    F=${foundationWasm}/include
    mkdir -p inc/OpenGL inc/CoreVideo inc/CoreServices inc/IOKit logs $out
    cp $fw/OpenGL/include/OpenGL/*.h inc/OpenGL/
    cp $fw/CoreVideo/*.h inc/CoreVideo/
    cp $top/src/Libraries/CoreServices/include/CoreServices/*.h inc/CoreServices/
    cp $top/linux/iokit/include/IOKit/*.h inc/IOKit/
    for f in Onyx2D CoreGraphics CoreText QuartzCore AppKit WindowServer ApplicationServices; do
      mkdir -p inc/$f; find $fw/$f -name '*.h' -exec cp {} inc/$f/ \;
    done
    native="-idirafter ${pkgs.freetype.dev}/include/freetype2 -idirafter ${pkgs.fontconfig.dev}/include \
      -idirafter ${pkgs.libpng.dev}/include -idirafter ${pkgs.libjpeg.dev}/include -idirafter ${pkgs.libtiff.dev}/include \
      -idirafter ${pkgs.zlib.dev}/include -idirafter ${pkgs.fontconfig.dev}/include/fontconfig"
    flags="${objcFlags} -fno-objc-arc -D__PUREDARWIN__=1 -DARGON_LIBOBJC2=1 -D_GNU_SOURCE -DDEPLOYMENT_RUNTIME_OBJC=1 \
      -DINCLUDE_OBJC=1 -include argon-darwin.h -I$F/argon-compat -I$F -I$F/Foundation -I$F/CoreFoundation -Iinc -Iinc/AppKit \
      -Iinc/Onyx2D -Iinc/CoreGraphics -I${libobjc2Wasm}/include -I${dispatchWebWasm}/include -Wno-everything $native \
      -Wobjc-method-access"
    try() {
      layer=$1; f=$2
      lang=objective-c; case $f in *.c) lang=c ;; esac
      log=logs/$layer-$(echo $f | tr / _).log
      if emcc -x $lang $flags -c $fw/$layer/$f -o /dev/null > $log 2>&1; then echo "ok   $layer $f" >> $out/files.txt
      else echo "FAIL $layer $f: $(grep -m1 'error:' $log | sed 's/^[^ ]* //')" >> $out/files.txt; fi
    }
    for f in $(cd $fw/Onyx2D && ls *.m); do try Onyx2D $f; done
    for f in $(cd $fw/CoreGraphics && ls *.m | grep -vxE 'CGLPixelSurface.m|CGDirectDisplay.m|KTFont\+PDF.m'); do try CoreGraphics $f; done
    for f in CTFont.m KTFont.m; do try CoreText $f; done
    for f in $(cd $fw/QuartzCore && ls *.m); do try QuartzCore $f; done
    for f in $(cd $fw/AppKit && find . -name '*.m' | sed 's|^\./||' | grep -v '^CoreData_/' | sort); do try AppKit $f; done
    {
      for l in Onyx2D CoreGraphics CoreText QuartzCore AppKit; do
        echo "$l: $(grep -c "^ok   $l " $out/files.txt) ok, $(grep -c "^FAIL $l " $out/files.txt) failed"
      done
      echo; echo "first errors by cause:"
      for l in logs/*.log; do grep -m1 'error:' $l; done | sed 's/^[^ ]* error: //' | sed "s/'[^']*'/'X'/g" | sort | uniq -c | sort -rn | head -40
      echo; echo "missing headers:"
      cat logs/*.log | grep "fatal error: '" | sed "s/.*fatal error: '\([^']*\)'.*/\1/" | sort | uniq -c | sort -rn
      echo; echo "names:"
      cat logs/*.log | grep -o "unknown type name '[^']*'\|undeclared identifier '[^']*'\|undeclared function '[^']*'\|no member named '[^']*' in '[^']*'" | sort | uniq -c | sort -rn | head -50
      # a message to a method no header declares: clang guesses id (id, SEL, ...), which wasm traps if the method differs
      echo; echo "undeclared methods:"
      cat logs/*.log | grep -h "not found (return type defaults to" | sed 's/^[^ ]*\/src\/Frameworks\///' | sort -u
    } > $out/summary.txt
    cp -r logs $out/
  '';

  # every CoreFoundation source compiled for wasm on its own, to see the whole port at once: the files that
  # build, and for the rest the first error of each, counted by cause. Never fails, it is a measurement.
  cfSurveyWasm = pkgs.runCommand "puredarwin-cf-survey-wasm" { nativeBuildInputs = [ emcc pkgs.python3 ]; } ''
    ${emSetup}
    # a grep that finds nothing is an answer here, not a failure
    set +e +o pipefail
    cf=${../src/Frameworks/CoreFoundation}
    # CF includes its headers as <CoreFoundation/X.h>, from whichever subproject they live in
    mkdir -p inc/CoreFoundation logs $out
    for h in $cf/*/*.h $cf/Base.subproj/SwiftRuntime/*.h; do ln -sf $h inc/CoreFoundation/; done
    cp $cf/Base.subproj/SwiftRuntime/TargetConditionals.h inc/
    flags="-x c -std=gnu11 -fblocks -fno-common -fdollars-in-identifiers -D_GNU_SOURCE -DCF_BUILDING_CF=1 \
      -DU_SHOW_DRAFT_API=1 -include $cf/Base.subproj/CoreFoundation_Prefix.h -Iinc -Iinc/CoreFoundation \
      -I${icuWasm}/include -I${libobjc2Wasm}/include -I${dispatchWebWasm}/include -Wno-everything"
    ok=0; bad=0
    for f in $cf/*/*.c; do
      name=$(basename $(dirname $f))/$(basename $f)
      log=logs/$(echo $name | tr / _).log
      if emcc $flags -c $f -o /dev/null > $log 2>&1; then ok=$((ok+1)); echo "ok   $name" >> $out/files.txt
      else bad=$((bad+1)); echo "FAIL $name: $(grep -m1 'error:' $log | sed 's/^[^ ]* //')" >> $out/files.txt; fi
    done
    {
      echo "compiled $ok, failed $bad"
      echo; echo "first errors by cause:"
      grep -h -m1 'error:' logs/*.log | sed 's/^[^ ]* error: //' | sed "s/'[^']*'/'X'/g" | sort | uniq -c | sort -rn
      echo; echo "missing headers:"
      grep -h "fatal error: '" logs/*.log | sed "s/.*fatal error: '\([^']*\)'.*/\1/" | sort | uniq -c | sort -rn
      echo; echo "undeclared or unknown names:"
      grep -h -o "unknown type name '[^']*'\|undeclared identifier '[^']*'\|undeclared function '[^']*'" logs/*.log | sort | uniq -c | sort -rn | head -40
    } > $out/summary.txt
    cp -r logs $out/
  '';
in
{
  cf-survey-wasm = cfSurveyWasm;
  zlib-wasm = zlibWasm;
  libpng-wasm = pngWasm;
  libjpeg-wasm = jpegWasm;
  freetype-wasm = freetypeWasm;
  harfbuzz-wasm = harfbuzzWasm;
  expat-wasm = expatWasm;
  libtiff-wasm = tiffWasm;
  fontconfig-wasm = fontconfigWasm;
  fribidi-wasm = fribidiWasm;
  graphics-survey-wasm = graphicsSurveyWasm;
  appkit-wasm = appkitWasm;
  llvm-wasm = llvmWasm;
  air2spv-wasm = air2spvWasm;
  air2spv-check-wasm = air2spvWasmCheck;
  emdawnwebgpu-wasm = emdawnwebgpu;
  tint-wasm = tintWasm;
  spv2wgsl-wasm = spv2wgslWasm;
  wgsl-check-wasm = wgslCheck;
  web-hello-wasm = webHello;
  web-check-wasm = webCheck;
  web-gl-wasm = webGL;
  web-check-gl-wasm = webCheckGL;
  foundation-wasm = foundationWasm;
  foundation-smoke-wasm = foundationSmokeWasm;
  libffi-wasm = ffiWasm;
  foundation-survey-wasm = foundationSurveyWasm;
  corefoundation-wasm = cfWasm;
  cf-smoke-wasm = cfSmokeWasm;
  dispatch-web-wasm = dispatchWebWasm;
  dispatch-smoke-wasm = dispatchSmokeWasm;
  icu-wasm = icuWasm;
  emscripten-cache-wasm = emCache;
  libobjc2-wasm = libobjc2Wasm;
  objc-smoke-wasm = objcSmokeWasm;
}
