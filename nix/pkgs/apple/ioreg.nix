{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-ioreg";
  version = "0.1";

  src = ../../../src/Userspace/ioreg;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -F$DARWIN_SDK_ROOT/System/Library/Frameworks -Icompat -I${libSystem}/usr/include -I${corefoundation}/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lIOKitCF -lCoreFoundation -lSystem"

    $CC $CFLAGS -c ioreg.c -o ioreg.o
    $CC $CFLAGS -c term_stub.c -o term_stub.o
    $CC $LDFLAGS -o ioreg ioreg.o term_stub.o

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/sbin
    cp ioreg $out/usr/sbin/ioreg
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Real apple-oss-distributions ioreg (IOKitTools), linked against real CoreFoundation + this project's MIG-backed IOKitLibCF";
    platforms = platforms.unix;
  };
}
