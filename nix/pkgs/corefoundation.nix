{ stdenv
, lib
, requireFile
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
, foundationSrc
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-corefoundation";
  version = "1338";

  inherit src;

  nativeBuildInputs = [ cmake ninja llvmPackages.bintools ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export NIX_DARWIN_TOOLCHAIN_DIR="${darwinCrossToolchain}/bin"

    mkdir -p placeholder-libs
    echo 'static int puredarwin_placeholder;' > placeholder-libs/placeholder.c
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -isysroot "$DARWIN_SDK_ROOT" -c placeholder-libs/placeholder.c -o placeholder-libs/placeholder.o
    ${darwinCrossToolchain}/bin/${targetTriple}-ar crs placeholder-libs/libBlocksRuntime.a placeholder-libs/placeholder.o
    ${darwinCrossToolchain}/bin/${targetTriple}-ar crs placeholder-libs/libdispatch.a placeholder-libs/placeholder.o

    # Foundation is not built as a library here (CF only needs its headers to
    # compile Bridging.subproj/__NSCFType.m, which is the ObjC toll-free
    # bridge glue living inside CF itself) - stage a "Foundation/" include
    # directory from the vendored Foundation source tree so
    # `#import <Foundation/NSObject.h>` resolves the way a real framework
    # search path would.
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
      -DCMAKE_TOOLCHAIN_FILE=${../../cmake/nix-toolchain.cmake} \
      -DNIX_DARWIN_TOOLCHAIN_DIR=${darwinCrossToolchain}/bin \
      -DNIX_DARWIN_HOST=${targetTriple} \
      -DNIX_DARWIN_SDK_ROOT=$DARWIN_SDK_ROOT \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_C_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include -I${pdCompatInclude} -I${lib.getDev icu}/include -I${libobjc}/usr/include -I$PWD/foundation-headers -DU_DISABLE_RENAMING=1 -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0" \
      -DCMAKE_OBJC_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include -I${pdCompatInclude} -I${lib.getDev icu}/include -I${libobjc}/usr/include -I$PWD/foundation-headers -fno-objc-arc -DU_DISABLE_RENAMING=1 -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0" \
      -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=${nativeLd}/bin/ld -nostdlib -L$PWD/placeholder-libs -L${libSystem}/usr/lib -L${libobjc}/usr/lib -lSystem -lobjc -Wl,-install_name,/usr/lib/libCoreFoundation.dylib -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains"

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
    platforms = platforms.linux;
  };
}
