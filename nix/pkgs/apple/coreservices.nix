{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, src
, appleSdk
}:

# CoreServices umbrella. Only the CarbonCore Multiprocessing entry points are
# implemented so far - see src/Libraries/CoreServices

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
      -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices \
      -Wl,-fixup_chains \
      src/Libraries/CoreServices/MultiprocessingCompat.c \
      -lSystem \
      -o CoreServices

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    fwdir=$out/System/Library/Frameworks/CoreServices.framework
    mkdir -p "$fwdir/Versions/A/Resources"
    cp CoreServices "$fwdir/Versions/A/CoreServices"
    ln -sf A "$fwdir/Versions/Current"
    ln -sf Versions/Current/CoreServices "$fwdir/CoreServices"
    ln -sf Versions/Current/Resources "$fwdir/Resources"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "CoreServices umbrella framework (CarbonCore Multiprocessing entry points)";
    platforms = platforms.unix;
  };
}
