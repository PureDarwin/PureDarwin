{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, corefoundation
, src
, appleSdk
}:

# CoreServices umbrella: the CarbonCore Multiprocessing entry points and a
# LaunchServices built on a directory scan of the application folders.
# See src/Libraries/CoreServices.

let
in
stdenv.mkDerivation {
  pname = "puredarwin-coreservices";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib \
      -I${corefoundation}/include \
      -Isrc/Libraries/CoreServices/include \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices \
      -Wl,-fixup_chains \
      src/Libraries/CoreServices/MultiprocessingCompat.c \
      src/Libraries/CoreServices/LaunchServices.c \
      -lCoreFoundation -lSystem \
      -o CoreServices

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    fwdir=$out/System/Library/Frameworks/CoreServices.framework
    mkdir -p "$fwdir/Versions/A/Resources"
    cp CoreServices "$fwdir/Versions/A/CoreServices"
    mkdir -p "$fwdir/Versions/A/Headers"
    cp src/Libraries/CoreServices/include/CoreServices/CoreServices.h \
       src/Libraries/CoreServices/include/CoreServices/LaunchServices.h \
       "$fwdir/Versions/A/Headers/"
    mkdir -p "$out/usr/include/CoreServices"
    cp src/Libraries/CoreServices/include/CoreServices/*.h "$out/usr/include/CoreServices/"
    ln -sf A "$fwdir/Versions/Current"
    ln -sf Versions/Current/CoreServices "$fwdir/CoreServices"
    ln -sf Versions/Current/Resources "$fwdir/Resources"
    ln -sf Versions/Current/Headers "$fwdir/Headers"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "CoreServices umbrella framework (CarbonCore Multiprocessing entry points)";
    platforms = platforms.unix;
  };
}
