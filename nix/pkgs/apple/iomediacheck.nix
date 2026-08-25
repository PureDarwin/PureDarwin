{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, iokitHeaders
, xnuIokitHeaders ? ../../../src/Kernel/xnu/iokit
, storageHeaders ? ../../../src/Kernel/Extensions/IOStorageFamily/include
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-iomediacheck";
  version = "0.1";

  src = ../../../src/Userspace/iomediacheck;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -F$DARWIN_SDK_ROOT/System/Library/Frameworks -I${libSystem}/usr/include -I${corefoundation}/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lIOKitCF -lCoreFoundation -lSystem"

    # Real headers, not local redefinitions: IOKitLib.h from the IOKitUser port,
    # IOBSD.h from xnu's iokit tree, storage/IOMedia.h from IOStorageFamily.
    CFLAGS="$CFLAGS -I${iokitHeaders}/include -I${xnuIokitHeaders} -I${storageHeaders}"

    $CC $CFLAGS -c iomediacheck.c -o iomediacheck.o
    $CC $LDFLAGS -o iomediacheck iomediacheck.o

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/sbin
    cp iomediacheck $out/usr/sbin/iomediacheck
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Real apple-oss-distributions ioreg (IOKitTools), linked against real CoreFoundation + this project's MIG-backed IOKitLibCF";
    platforms = platforms.unix;
  };
}
