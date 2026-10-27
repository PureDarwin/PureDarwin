{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, ldid
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# coreaudiod, the audio server: the one process IOAudioFamily lets drive the engine, mixing every CoreAudio client
stdenv.mkDerivation {
  pname = "puredarwin-coreaudiod";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src;
    fileset = lib.fileset.unions [
      ../../../src/Userspace/coreaudiod
      ../../../src/Frameworks/CoreAudio/PDAudioServer.h
    ];
  };

  nativeBuildInputs = [ ldid ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    CFLAGS="-isysroot $DARWIN_SDK_ROOT -F$DARWIN_SDK_ROOT/System/Library/Frameworks -I${libSystem}/usr/include -I${corefoundation}/include -IFrameworks/CoreAudio -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -O2 -Wall"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -F${iokit}/System/Library/Frameworks -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5"
    $CC $CFLAGS -c Userspace/coreaudiod/coreaudiod.c -o coreaudiod.o
    $CC $LDFLAGS -o coreaudiod coreaudiod.o -framework IOKit -lCoreFoundation -lSystem
    # IOAudioFamily answers only a process holding the driver-host entitlement
    ldid -SUserspace/coreaudiod/coreaudiod.entitlements coreaudiod
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 coreaudiod $out/usr/sbin/coreaudiod
    install -Dm644 Userspace/coreaudiod/org.puredarwin.audio.coreaudiod.plist \
      $out/System/Library/LaunchDaemons/org.puredarwin.audio.coreaudiod.plist
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "The PureDarwin audio server over IOAudioFamily";
    platforms = platforms.unix;
  };
}
