{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, mesa
, src
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

  installName = "/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL";
in
stdenv.mkDerivation {
  pname = "puredarwin-opengl-framework";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -I${src}/include -I${mesa}/usr/include \
      -I${libSystem}/usr/include \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${mesa}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,${installName} \
      -Wl,-reexport-lOSMesa \
      -lSystem \
      ${src}/CGL.c \
      -o OpenGL

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/OpenGL.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers"
    cp OpenGL "$frameworkDir/Versions/A/OpenGL"
    cp -a ${src}/include/OpenGL/. "$frameworkDir/Versions/A/Headers/"

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/OpenGL "$frameworkDir/OpenGL"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"

    # Flat dylib alias under /usr/lib, as Security.framework does, for callers
    # that link -lOpenGL instead of -framework OpenGL.
    mkdir -p "$out/usr/lib"
    ln -s "../../System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL" \
      "$out/usr/lib/libOpenGL.dylib"

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "PureDarwin OpenGL.framework: CGL over Mesa OSMesa, re-exporting the GL API";
    platforms = platforms.linux;
  };
}
