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
  corefoundation-wasm = cfWasm;
  cf-smoke-wasm = cfSmokeWasm;
  dispatch-web-wasm = dispatchWebWasm;
  dispatch-smoke-wasm = dispatchSmokeWasm;
  icu-wasm = icuWasm;
  emscripten-cache-wasm = emCache;
  libobjc2-wasm = libobjc2Wasm;
  objc-smoke-wasm = objcSmokeWasm;
}
