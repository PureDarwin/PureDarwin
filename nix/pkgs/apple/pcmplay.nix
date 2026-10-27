{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, corefoundation
, audiotoolbox
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# pcmplay: raw s16le 48 kHz stereo PCM through an AudioQueue, built here since AudioToolbox comes after libSystem
stdenv.mkDerivation {
  pname = "puredarwin-pcmplay";
  version = "0.2";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    FW=System/Library/Frameworks
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -Icf-headers -I${libSystem}/usr/include \
      -I${audiotoolbox}/usr/include -c $src/pcmplay.c -o pcmplay.o
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylib_file,/$FW/CoreAudio.framework/Versions/A/CoreAudio:${audiotoolbox}/$FW/CoreAudio.framework/Versions/A/CoreAudio \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib -L${corefoundation}/usr/lib \
      pcmplay.o ${audiotoolbox}/$FW/AudioToolbox.framework/Versions/A/AudioToolbox \
      ${audiotoolbox}/$FW/CoreAudio.framework/Versions/A/CoreAudio -lCoreFoundation -lSystem -o pcmplay
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 pcmplay $out/bin/pcmplay
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "Plays raw PCM through AudioToolbox";
    platforms = lib.platforms.unix;
  };
}
