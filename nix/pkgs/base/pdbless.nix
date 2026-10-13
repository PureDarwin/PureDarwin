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

# pdbless, firmware boot entries through PDACPIPlatform's PDEFIVariables
stdenv.mkDerivation {
  pname = "puredarwin-pdbless";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src;
    fileset = lib.fileset.unions [
      ../../../src/Userspace/pdbless
      ../../../src/Kernel/Extensions/PDACPIPlatform/PDEFIVariables.h
    ];
  };

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    CFLAGS="-isysroot $DARWIN_SDK_ROOT -F$DARWIN_SDK_ROOT/System/Library/Frameworks -I${libSystem}/usr/include -I${corefoundation}/include -IKernel/Extensions/PDACPIPlatform -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -O2"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5"
    $CC $CFLAGS -c Userspace/pdbless/pdbless.c -o pdbless.o
    $CC $LDFLAGS -o pdbless pdbless.o -lIOKitCF -lCoreFoundation -lSystem
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/sbin
    cp pdbless $out/usr/sbin/pdbless
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Firmware boot entries for PureDarwin";
    platforms = platforms.unix;
  };
}
