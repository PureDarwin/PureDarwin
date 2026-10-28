{ stdenv
, lib
, cmake
, ninja
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, llvmPackages
, libSystem
, icu
, src
, pdCompatInclude
, libobjc
  # 32-bit ARM: the SDK stubs have no armv6 slice, so CMake's link test cannot
  # produce an executable, and chained fixups are 64-bit only.
, isArmv6 ? lib.hasPrefix "armv6-" targetTriple
, compilerRt ? null
, foundationSrc
, appleSdk
}:

let
  # classes and constants clients built against the macOS SDK bind to CoreFoundation,
  # implemented in Foundation and re-exported from here
  nsReexportClasses = [
    "NSArray" "NSMutableArray" "NSDictionary" "NSMutableDictionary" "NSSet" "NSMutableSet"
    "NSOrderedSet" "NSMutableOrderedSet" "NSData" "NSMutableData" "NSDate" "NSLocale"
    "NSRunLoop" "NSURL" "NSUserDefaults"
    # the classes the macOS SDK places in CoreFoundation too, apps bind them there
    "NSTimer" "NSTimeZone" "NSCalendar" "NSDateComponents" "NSEnumerator" "NSException"
    "NSInvocation" "NSMethodSignature" "NSPort" "NSMachPort" "NSStream" "NSInputStream" "NSOutputStream"
    "NSConstantArray" "NSConstantDictionary"
  ];
  nsReexportSymbols = [
    "___NSArray0__" "___NSDictionary0__" "_NSDefaultRunLoopMode" "_NSRunLoopCommonModes"
    "_NSInternalInconsistencyException" "_NSInvalidArgumentException" "_NSRangeException"
    "_NSSystemTimeZoneDidChangeNotification" "___CFConstantStringClassReference"
  ];
  tbdArch = if lib.hasPrefix "arm64" targetTriple then "arm64" else "x86_64";
  reexportList = lib.concatStringsSep "\n" (nsReexportSymbols
    ++ map (c: "_OBJC_CLASS_$_" + c) nsReexportClasses
    ++ map (c: "_OBJC_METACLASS_$_" + c) nsReexportClasses
    ++ [ "_OBJC_EHTYPE_$_NSException" ]);
in
stdenv.mkDerivation {
  pname = "puredarwin-corefoundation";
  version = "1338";

  inherit src;

  nativeBuildInputs = [ cmake ninja llvmPackages.bintools ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export NIX_DARWIN_TOOLCHAIN_DIR="${darwinCrossToolchain}/bin"

    mkdir -p placeholder-libs
    echo 'static int puredarwin_placeholder;' > placeholder-libs/placeholder.c
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -isysroot "$DARWIN_SDK_ROOT" -c placeholder-libs/placeholder.c -o placeholder-libs/placeholder.o
    ${darwinCrossToolchain}/bin/${targetTriple}-ar crs placeholder-libs/libBlocksRuntime.a placeholder-libs/placeholder.o
    ${darwinCrossToolchain}/bin/${targetTriple}-ar crs placeholder-libs/libdispatch.a placeholder-libs/placeholder.o

    # Foundation links CoreFoundation, so CF links a text stub of it, upward, to re-export from
    printf '%b\n' '${reexportList}' > ns-reexports.txt
    # the locale keys are __attribute__((alias)) on linux and windows, mach-o takes them as linker aliases
    sed -n 's/^CF_EXPORT CFStringRef const \([A-Za-z0-9_]*\) __attribute__((alias ("\([A-Za-z0-9_]*\)")));/_\2 _\1/p' \
      Locale.subproj/CFLocaleKeys.c > cf-aliases.txt
    {
      echo '--- !tapi-tbd'
      echo 'tbd-version: 4'
      echo 'targets: [ ${tbdArch}-macos ]'
      echo 'install-name: /usr/lib/libFoundation.dylib'
      echo 'exports:'
      echo '  - targets: [ ${tbdArch}-macos ]'
      echo '    symbols: [ ${lib.concatStringsSep ", " (map (s: "'" + s + "'") nsReexportSymbols)} ]'
      echo '    objc-classes: [ ${lib.concatStringsSep ", " nsReexportClasses} ]'
      echo '    objc-eh-types: [ NSException ]'
      echo '...'
    } > Foundation-stub.tbd

    mkdir -p foundation-headers/Foundation
    find ${foundationSrc} -name '*.h' -exec cp {} foundation-headers/Foundation/ \;

    # -Wl,-fixup_chains: PD's dyld lazy-binding/stub-resolution path is
    # fragile (same reason launchd/SystemStarter/launchctl/notifyd
    # already eager-bind themselves) - without it, CoreFoundation's own
    # internal calls between its exported symbols go through dyld_stub_binder
    # and can fault (executing from a freshly-written RW page instead of a
    # resolved stub) the first time something loads this dylib early enough
    # in boot to hit it before anything else has masked it.
    cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=${../../../cmake/nix-toolchain.cmake} \
      -DNIX_DARWIN_TOOLCHAIN_DIR=${darwinCrossToolchain}/bin \
      -DNIX_DARWIN_HOST=${targetTriple} \
      -DNIX_DARWIN_SDK_ROOT=$DARWIN_SDK_ROOT \
      -DBUILD_SHARED_LIBS=ON \
      ${lib.optionalString isArmv6 "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"} \
      -DCMAKE_C_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include -I${pdCompatInclude} -I${lib.getDev icu}/include -I${libobjc}/usr/include -I$PWD/foundation-headers -DU_DISABLE_RENAMING=1 -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 ${lib.optionalString (!isArmv6) "-DPD_CF_STRING_CLASS_FROM_FOUNDATION=1"} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0" \
      -DCMAKE_OBJC_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include -I${pdCompatInclude} -I${lib.getDev icu}/include -I${libobjc}/usr/include -I$PWD/foundation-headers -fno-objc-arc -DU_DISABLE_RENAMING=1 -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 ${lib.optionalString (!isArmv6) "-DPD_CF_STRING_CLASS_FROM_FOUNDATION=1"} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0" \
      -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=${nativeLd}/bin/ld -nostdlib -L$PWD/placeholder-libs -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${icu}/usr/lib -lSystem -lobjc ${lib.optionalString (compilerRt != null) "${compilerRt}/lib/libcompiler_rt.a"} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-install_name,/usr/lib/libCoreFoundation.dylib -Wl,-alias_list,$PWD/cf-aliases.txt ${lib.optionalString (!isArmv6) "-Wl,-upward_library,$PWD/Foundation-stub.tbd -Wl,-reexported_symbols_list,$PWD/ns-reexports.txt"} ${lib.optionalString (!isArmv6) "-Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains"}"

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    find build -name '*.ninja' -exec \
      sed -i -E 's/[[:space:]]-Xlinker$//' {} +
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/include
    cp build/CoreFoundation.framework/libCoreFoundation.dylib $out/usr/lib/
    cp -a build/CoreFoundation.framework/Headers/. $out/include/
    cp -a build/CoreFoundation.framework/PrivateHeaders/. $out/include/

    # The headers are flattened into include/ for callers that spell
    # <CFString.h>, but Foundation's own headers use <CoreFoundation/CFString.h>.
    # A self-referential subdirectory makes both spellings work off one -I.
    ln -s . $out/include/CoreFoundation

    fwdir=$out/System/Library/Frameworks/CoreFoundation.framework
    mkdir -p "$fwdir/Versions/A/Resources"
    ln -s ../../../../../../usr/lib/libCoreFoundation.dylib "$fwdir/Versions/A/CoreFoundation"
    cp -a build/CoreFoundation.framework/Headers "$fwdir/Versions/A/Headers"
    cp -a build/CoreFoundation.framework/PrivateHeaders "$fwdir/Versions/A/PrivateHeaders"
    ln -sf A "$fwdir/Versions/Current"
    ln -sf Versions/Current/CoreFoundation "$fwdir/CoreFoundation"
    ln -sf Versions/Current/Headers "$fwdir/Headers"
    ln -sf Versions/Current/PrivateHeaders "$fwdir/PrivateHeaders"
    ln -sf Versions/Current/Resources "$fwdir/Resources"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin CoreFoundation (from apple/swift-corelibs-foundation), cross-built as a real dylib";
    platforms = platforms.unix;
  };
}
