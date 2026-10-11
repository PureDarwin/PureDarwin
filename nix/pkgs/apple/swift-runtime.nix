{ stdenv
, lib
, fetchFromGitHub
, cmake
, ninja
, python3
, swiftToolchain
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, libobjc
, libcxxDylib
, libcxxabiDylib
, corefoundation
, foundation
, targetTriple ? "x86_64-apple-darwin20.4"
  # src/Libraries/SwiftOverlays
, overlaySrc
  # objc4's ObjectiveC.apinotes, libdispatch's darwin module map, CF's module map
, objcApinotes
, dispatchModulemap
, cfModulemap
}:

# the Swift 6.3.3 runtime (stdlib, Concurrency, Onone) built from swift.org
# sources with ObjC interop, plus the Darwin-platform overlays, as /usr/lib/swift.
let
  arch = if lib.hasPrefix "arm64" targetTriple then "arm64" else "x86_64";
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  tools = "${darwinCrossToolchain}/bin/${targetTriple}";
  # arm64 PD headers drop the x86 $INODE64 suffixes only with this, as the cross clang wrapper does
  platdef = lib.optionalString (arch == "arm64") "-DXNU_PLATFORM_MacOSX=1";

  swiftSrc = fetchFromGitHub {
    owner = "swiftlang";
    repo = "swift";
    rev = "swift-6.3.3-RELEASE";
    hash = "sha256-7/jT/U0sOsr4iWnCluyUWjnD/XH5CnPyAG9EQ+Nq+tI=";
  };
  # the last releases that still carried the open-source Darwin overlays
  swift54 = fetchFromGitHub {
    owner = "swiftlang";
    repo = "swift";
    rev = "swift-5.4.3-RELEASE";
    sparseCheckout = [ "stdlib/public/Darwin" "stdlib/public/SwiftShims" ];
    hash = "sha256-9hXo8I4QAGWJ4ZXIfBtTExQ9+IBmVxeKuyC6guDW9t0=";
  };
  swift59 = fetchFromGitHub {
    owner = "swiftlang";
    repo = "swift";
    rev = "swift-5.9-RELEASE";
    sparseCheckout = [ "stdlib/public/Platform" ];
    hash = "sha256-wpMJ9jS/SYrYOyoEj/vIedDPF4urM4cG0TKBUqL3K9w=";
  };

  # strong /usr/lib/swift dependencies of real apps that only need to exist for now
  placeholders = [
    "AVFoundation" "CoreAudio" "CoreImage" "CoreMIDI" "CoreMedia" "DataDetection"
    "IOKit" "Metal" "OSLog" "QuartzCore" "XPC" "_Builtin_float" "_errno" "_math"
    "_signal" "_stdio" "_time" "simd" "sys_time" "unistd"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-swift-runtime";
  version = "6.3.3";

  dontUnpack = true;
  outputs = [ "out" "dev" ];
  nativeBuildInputs = [ cmake ninja python3 ];
  dontUseCmakeConfigure = true;
  dontFixup = true;

  buildPhase = ''
    runHook preBuild
    # swiftc keeps its clang module cache under $HOME
    export HOME=$TMPDIR
    W=$TMPDIR/w
    R=$W/sysroot
    mkdir -p $W

    # sysroot: the PD SDK plus the PD dylibs and framework headers Swift imports
    cp -rL ${sdk} $R
    chmod -R u+w $R
    cp -rL ${libSystem}/usr/lib/. ${libobjc}/usr/lib/. ${libcxxDylib}/usr/lib/. ${libcxxabiDylib}/usr/lib/. $R/usr/lib/
    cp -rL ${corefoundation}/usr/lib/. ${foundation}/usr/lib/. $R/usr/lib/
    chmod -R u+w $R
    rm -f $R/usr/lib/libSystem.tbd $R/usr/lib/libc++.tbd $R/usr/lib/libc++abi.tbd
    cp -rL ${foundation}/usr/include/Foundation $R/usr/include/
    F=$R/System/Library/Frameworks
    rm -rf $F/CoreFoundation.framework
    cp -rL ${corefoundation}/System/Library/Frameworks/CoreFoundation.framework $F/
    chmod -R u+w $F
    mkdir -p $F/Foundation.framework/Modules $F/CoreFoundation.framework/Modules
    ln -sfn ../../../../usr/include/Foundation $F/Foundation.framework/Headers
    ln -sfn ../../../../usr/lib/libFoundation.dylib $F/Foundation.framework/Foundation
    chmod -R u+w $R
    cp ${cfModulemap} $F/CoreFoundation.framework/Modules/module.modulemap
    cp ${overlaySrc}/modules/Foundation.modulemap $F/Foundation.framework/Modules/module.modulemap
    cp ${overlaySrc}/modules/Foundation.apinotes $R/usr/include/Foundation/
    cp ${overlaySrc}/modules/ObjectiveC.modulemap $R/usr/include/objc/module.modulemap
    cp ${objcApinotes} $R/usr/include/objc/ObjectiveC.apinotes
    cp ${dispatchModulemap} $R/usr/include/dispatch/module.modulemap
    # PD Foundation header gaps the overlays need, implemented at runtime already
    sed -i 's/^@interface NSDate : NSObject$/@interface NSDate : NSObject <NSCopying>/' $R/usr/include/Foundation/NSDate.h
    sed -i '/^@interface NSString : NSObject/a @property(readonly, copy) NSString *decomposedStringWithCanonicalMapping;\n@property(readonly, copy) NSString *precomposedStringWithCanonicalMapping;' $R/usr/include/Foundation/NSString.h
    # below 15.0 clang joins builtin headers to the system modules, which this SDK's maps need
    echo '{"CanonicalName":"macosx14.5","Version":"14.5","MaximumDeploymentTarget":"26.5.99","DefaultDeploymentTarget":"14.5"}' > $R/SDKSettings.json
    sh ${overlaySrc}/gen-darwin-modulemap.sh $R ${swiftToolchain}/bin/clang ${arch}-apple-macosx13.0

    # resource dir: the toolchain's clang headers, the stdlib being built, swift's apinotes
    S=$W/swift
    cp -r ${swiftSrc} $S
    chmod -R u+w $S
    cmake -P $S/Runtimes/Resync.cmake > /dev/null
    STAGE=$W/stage
    RES=$W/res
    mkdir -p $RES
    ln -s $STAGE/usr/lib/swift $RES/macosx
    ln -s $STAGE/usr/lib/swift/shims $RES/shims
    ln -s ${swiftToolchain}/lib/swift/clang $RES/clang
    ln -s $S/apinotes $RES/apinotes
    cp $S/stdlib/public/SwiftShims/swift/shims/parent.modulemap $RES/module.modulemap

    cat > $W/pd-darwin.cmake <<EOF
    set(CMAKE_SYSTEM_NAME Darwin)
    set(CMAKE_C_COMPILER_WORKS YES)
    set(CMAKE_CXX_COMPILER_WORKS YES)
    set(CMAKE_Swift_COMPILER_WORKS YES)
    set(CMAKE_C_COMPILER ${swiftToolchain}/bin/clang)
    set(CMAKE_CXX_COMPILER ${swiftToolchain}/bin/clang++)
    set(CMAKE_Swift_COMPILER ${swiftToolchain}/bin/swiftc)
    set(CMAKE_AR ${swiftToolchain}/bin/llvm-ar CACHE FILEPATH "")
    set(CMAKE_RANLIB ${swiftToolchain}/bin/llvm-ranlib CACHE FILEPATH "")
    set(CMAKE_INSTALL_NAME_TOOL ${tools}-install_name_tool CACHE FILEPATH "")
    set(CMAKE_OSX_SYSROOT $R CACHE PATH "")
    set(CMAKE_FIND_ROOT_PATH $R)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_C_FLAGS_INIT "-fuse-ld=${nativeLd}/bin/ld ${platdef}")
    set(CMAKE_CXX_FLAGS_INIT "-fuse-ld=${nativeLd}/bin/ld ${platdef}")
    set(CMAKE_Swift_FLAGS_INIT "-ld-path=${nativeLd}/bin/ld ${lib.optionalString (platdef != "") "-Xcc ${platdef}"}")
    EOF

    # no zippering, crash reporter, backtracer or os_signpost tracing on PD yet
    cmake -S $S/Runtimes/Core -B $W/core -G Ninja \
      --toolchain $W/pd-darwin.cmake \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
      -DCMAKE_C_COMPILER_TARGET=${arch}-apple-macosx13.0 \
      -DCMAKE_CXX_COMPILER_TARGET=${arch}-apple-macosx13.0 \
      -DCMAKE_Swift_COMPILER_TARGET=${arch}-apple-macosx13.0 \
      -DSwiftCore_ARCH_SUBDIR=${arch} -DSwiftCore_PLATFORM_SUBDIR=macosx \
      -C $S/Runtimes/Core/cmake/caches/Vendors/Apple/apple-common.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DCMAKE_INSTALL_NAME_DIR=/usr/lib/swift \
      -DCMAKE_BUILD_WITH_INSTALL_NAME_DIR=ON \
      -DSwiftCore_INSTALL_NESTED_SUBDIR=OFF \
      -DSwiftCore_ENABLE_CRASH_REPORTER_CLIENT=OFF \
      -DSwiftCore_ENABLE_BACKTRACING=OFF \
      -DSwiftCore_ENABLE_STDLIB_TRACING=OFF \
      -DSwiftCore_ENABLE_CONCURRENCY=ON \
      -DSwiftCore_OPTIMIZATION_REMARKS=OFF \
      -DSwiftCore_ENABLE_STRICT_AVAILABILITY=OFF \
      -DSwiftCore_COMPILER_VARIANT_TARGET= \
      -Ddispatch_IMPLIB=$R/usr/lib/libSystem.B.dylib
    ninja -C $W/core
    DESTDIR=$STAGE ninja -C $W/core install > /dev/null

    # overlays: Darwin from 5.9's Platform, ObjectiveC/Dispatch/CoreFoundation from 5.4.3
    mkdir -p $W/ovshims
    O54=${swift54}/stdlib/public
    cp $O54/SwiftShims/*OverlayShims.h $O54/SwiftShims/NS*Shims.h $O54/SwiftShims/FoundationShimSupport.h \
      $O54/SwiftShims/CFCharacterSetShims.h $O54/SwiftShims/CFHashingShims.h $W/ovshims/
    awk '/^module _Swift[A-Za-z]*OverlayShims/{p=1} p{print} /^}/{p=0}' $O54/SwiftShims/module.modulemap > $W/ovshims/module.modulemap
    export ARCH=${arch} SWIFTC=${swiftToolchain}/bin/swiftc CLANG=${swiftToolchain}/bin/clang
    export SYSROOT=$R RESDIR=$RES OUT=$STAGE/usr/lib/swift LD=${nativeLd}/bin/ld WORK=$W
    export SWIFTSRC=$S OVSHIMS=$W/ovshims OBJCFLAGS="${platdef}"
    B="sh ${overlaySrc}/build-overlay.sh"
    P=${swift59}/stdlib/public/Platform
    D=$O54/Darwin
    $B Darwin "$P/Platform.swift $P/POSIXError.swift $P/MachError.swift $P/TiocConstants.swift $P/Darwin.swift.gyb $P/tgmath.swift.gyb"
    $B ObjectiveC "$D/ObjectiveC/ObjectiveC.swift" -lobjc
    # CoreFoundation makes blocks Objective-C objects, which libdispatch messages, so Dispatch users load it as on macOS
    UNDERLYING=1 OBJCFLAGS="-fobjc-arc ${platdef}" $B Dispatch "$D/Dispatch/Dispatch.swift $D/Dispatch/Dispatch.mm $D/Dispatch/Block.swift $D/Dispatch/Data.swift $D/Dispatch/IO.swift $D/Dispatch/Private.swift $D/Dispatch/Queue.swift $D/Dispatch/Source.swift $D/Dispatch/Time.swift" \
      -framework CoreFoundation
    $B CoreFoundation "$D/CoreFoundation/CoreFoundation.swift" -framework CoreFoundation

    # foundation: a subset of the 5.4.3 overlay, patched for PD's headers, plus PD's own files
    FO=$W/foundation
    mkdir -p $FO
    for f in Foundation String Date ReferenceConvertible Boxing NSStringEncodings NSArray NSFastEnumeration; do
      cp $D/Foundation/$f.swift $FO/
    done
    chmod u+w $FO/*
    (cd $FO && patch -p1 < ${overlaySrc}/Foundation/swift-5.4.3-foundation.patch)
    cp ${overlaySrc}/Foundation/NSString.swift ${overlaySrc}/Foundation/NSDictionary.swift ${overlaySrc}/Foundation/PDSwiftStringBridge.m $FO/
    UNDERLYING=1 $B Foundation "$FO/Foundation.swift $FO/String.swift $FO/Date.swift $FO/ReferenceConvertible.swift $FO/Boxing.swift $FO/NSStringEncodings.swift $FO/NSString.swift $FO/NSArray.swift $FO/NSDictionary.swift $FO/NSFastEnumeration.swift $FO/PDSwiftStringBridge.m" \
      -framework Foundation -framework CoreFoundation

    for m in ${lib.concatStringsSep " " placeholders}; do
      echo '// placeholder overlay, exports the force-load symbol only' > $W/$m.swift
      $B $m "$W/$m.swift"
    done

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    # the Foundation.framework/Versions/C/Foundation, which re-exports libswiftFoundation, comes from framework-compat.
    # out is what an image carries, dev the modules for building against it
    mkdir -p $out/usr/lib/swift $dev/usr/lib/swift
    cp -a $W/stage/usr/lib/swift/*.dylib $out/usr/lib/swift/
    cp -a $W/stage/usr/lib/swift/. $dev/usr/lib/swift/
    rm -f $dev/usr/lib/swift/*.dylib
    runHook postInstall
  '';

  meta = {
    description = "Swift 6.3.3 runtime and Darwin overlays for PureDarwin (/usr/lib/swift)";
    license = lib.licenses.asl20;
    platforms = lib.platforms.unix;
  };
}
