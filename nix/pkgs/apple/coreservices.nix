{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, src
}:

# CoreServices umbrella. Only the CarbonCore Multiprocessing entry points are
# implemented so far - see src/Libraries/CoreServices

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
  pname = "puredarwin-coreservices";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

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
