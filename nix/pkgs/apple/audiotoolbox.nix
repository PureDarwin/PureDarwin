{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, corefoundation
, src
, coreAudioSrc
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# CoreAudio's HAL, a client of coreaudiod, and AudioToolbox over it: AudioQueue, the output audio
# units, graphs, PCM converters and format queries. AudioUnit.framework re-exports AudioToolbox
stdenv.mkDerivation {
  pname = "puredarwin-audiotoolbox";
  version = "0.2";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk

    mkdir -p include/CoreAudio include/AudioToolbox include/AudioUnit cf-headers
    # PDAudioServer.h and PDAudioClient.h stay private to the HAL
    for h in CoreAudio.h CoreAudioTypes.h AudioHardware.h; do cp ${coreAudioSrc}/$h include/CoreAudio/; done
    cp $src/AudioToolbox.h include/AudioToolbox/
    # the sub-headers Apple splits AudioToolbox and AudioUnit into, each one the whole API here
    for h in AudioFormat AudioConverter AudioQueue AUGraph MusicDevice AudioComponent; do
      echo "#include <AudioToolbox/AudioToolbox.h>" > include/AudioToolbox/$h.h
    done
    for h in AudioUnit AUComponent AudioOutputUnit AudioUnitProperties AudioUnitParameters MusicDevice AudioComponent; do
      echo "#include <AudioToolbox/AudioToolbox.h>" > include/AudioUnit/$h.h
    done
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    CFLAGS="-isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -Iinclude -Icf-headers -I${libSystem}/usr/include"
    LDFLAGS="-fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L. \
      -dynamiclib -compatibility_version 1 -current_version 1"
    fw() { echo "-install_name /System/Library/Frameworks/$1.framework/Versions/A/$1"; }

    $CC $CFLAGS -I${coreAudioSrc} -c ${coreAudioSrc}/AudioHardware.c -o AudioHardware.o
    $CC $CFLAGS -I${coreAudioSrc} -c ${coreAudioSrc}/AudioDeviceIO.c -o AudioDeviceIO.o
    $CC $LDFLAGS $(fw CoreAudio) AudioHardware.o AudioDeviceIO.o -lCoreFoundation -lSystem -o CoreAudio
    ln -s CoreAudio libCoreAudio.dylib

    for f in AudioQueue AudioConverter AudioUnit AudioFormat AUGraph; do
      $CC $CFLAGS -c $src/$f.c -o $f.o
    done
    $CC $LDFLAGS $(fw AudioToolbox) AudioQueue.o AudioConverter.o AudioUnit.o AudioFormat.o AUGraph.o \
      -lCoreAudio -lCoreFoundation -lSystem -o AudioToolbox

    echo "// re-exports only" > empty.c
    $CC $CFLAGS -c empty.c -o empty.o
    $CC $LDFLAGS $(fw AudioUnit) empty.o -Wl,-reexport_library,AudioToolbox -lSystem -o AudioUnit
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    for f in CoreAudio AudioToolbox AudioUnit; do
      F=$out/System/Library/Frameworks/$f.framework
      install -Dm755 $f $F/Versions/A/$f
      mkdir -p $F/Versions/A/Headers
      ln -s A $F/Versions/Current
      ln -s Versions/Current/$f $F/$f
      ln -s Versions/Current/Headers $F/Headers
    done
    cp include/CoreAudio/*.h $out/System/Library/Frameworks/CoreAudio.framework/Versions/A/Headers/
    cp include/AudioToolbox/*.h $out/System/Library/Frameworks/AudioToolbox.framework/Versions/A/Headers/
    cp include/AudioUnit/*.h $out/System/Library/Frameworks/AudioUnit.framework/Versions/A/Headers/
    mkdir -p $out/usr/include $out/usr/lib
    ln -s ../../System/Library/Frameworks/CoreAudio.framework/Versions/A/Headers $out/usr/include/CoreAudio
    ln -s ../../System/Library/Frameworks/AudioToolbox.framework/Versions/A/Headers $out/usr/include/AudioToolbox
    ln -s ../../System/Library/Frameworks/AudioUnit.framework/Versions/A/Headers $out/usr/include/AudioUnit
    for f in CoreAudio AudioToolbox; do
      ln -s ../../System/Library/Frameworks/$f.framework/Versions/A/$f $out/usr/lib/lib$f.dylib
    done
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "CoreAudio, AudioToolbox and AudioUnit for PureDarwin, played through coreaudiod";
    platforms = lib.platforms.unix;
  };
}
