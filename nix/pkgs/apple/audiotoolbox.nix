{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# AudioToolbox.framework: the AudioQueue output API over /dev/dsp0
stdenv.mkDerivation {
  pname = "puredarwin-audiotoolbox";
  version = "0.1";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -c $src/AudioQueue.c -o AudioQueue.o
    $CC -fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib \
      -dynamiclib -install_name /System/Library/Frameworks/AudioToolbox.framework/Versions/A/AudioToolbox \
      -compatibility_version 1 -current_version 1 \
      AudioQueue.o -lSystem -o AudioToolbox
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    F=$out/System/Library/Frameworks/AudioToolbox.framework
    install -Dm755 AudioToolbox $F/Versions/A/AudioToolbox
    ln -s A $F/Versions/Current
    ln -s Versions/Current/AudioToolbox $F/AudioToolbox
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "AudioToolbox AudioQueue output for PureDarwin";
    platforms = lib.platforms.unix;
  };
}
