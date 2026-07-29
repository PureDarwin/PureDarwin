{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

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

  installName = "/System/Library/Frameworks/Security.framework/Versions/A/Security";
in
stdenv.mkDerivation {
  pname = "puredarwin-security";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -I${src}/include -I${corefoundation}/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,${installName} \
      -lCoreFoundation -lSystem \
      ${src}/Security.c \
      -o Security

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/Security.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers"
    cp Security "$frameworkDir/Versions/A/Security"
    cp -a ${src}/include/Security/. "$frameworkDir/Versions/A/Headers/"

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/Security "$frameworkDir/Security"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"

    # Also drop a flat dylib under /usr/lib, matching every other
    # PureDarwin library (CoreFoundation, IOKitCF) - some consumers link
    # -lSecurity / -L.../usr/lib rather than -F.../Frameworks.
    mkdir -p "$out/usr/lib"
    ln -s "../../System/Library/Frameworks/Security.framework/Versions/A/Security" \
      "$out/usr/lib/libSecurity.dylib"

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Minimal PureDarwin Security.framework (real SecRandomCopyBytes, stub SecItem*)";
    platforms = platforms.linux;
  };
}
