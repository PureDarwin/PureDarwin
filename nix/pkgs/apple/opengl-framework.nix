{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, mesa
, glu
, libX11 ? null
, xorgproto ? null
, libXext ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, src
  # CGL is offscreen-only, so the EGL backend uses Mesa's surfaceless platform
  # and needs no window system at all. The GLX backend needs an X server.
, withX11 ? true
, appleSdk
}:

let

  installName = "/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL";
in
stdenv.mkDerivation {
  pname = "puredarwin-opengl-framework${lib.optionalString (!withX11) "-nox"}";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -I${src}/include -I${mesa}/usr/include \
      ${lib.optionalString withX11 "-I${libX11}/include -I${xorgproto}/include"} \
      ${lib.optionalString (!withX11) "-DPD_CGL_USE_EGL=1"} \
      -I${libSystem}/usr/include \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${mesa}/usr/lib -L${glu}/usr/lib \
      ${lib.optionalString withX11 "-L${libX11}/lib -L${libXext}/lib -L${libxcb}/lib -L${libXau}/lib -L${libXdmcp}/lib"} \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,${installName} \
      -Wl,-reexport-lGL \
      -Wl,-reexport-lGLU \
      ${lib.optionalString withX11 "-lX11 -lXext -lxcb -lXau -lXdmcp"} \
      ${lib.optionalString (!withX11) "-lEGL"} \
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
    # cp -a carried the store's read-only mode onto Headers/.
    chmod u+w "$frameworkDir/Versions/A/Headers"
    # Real OpenGL.framework ships glu.h too, and ports include <OpenGL/glu.h>.
    # Mesa's copy includes <GL/gl.h> rather than <OpenGL/gl.h>; every consumer
    # here already has Mesa's include dir on the search path.
    cp ${glu}/usr/include/GL/glu.h "$frameworkDir/Versions/A/Headers/glu.h"

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
    description = "PureDarwin OpenGL.framework: CGL over GLX pbuffers, re-exporting the GL and GLU APIs";
    platforms = platforms.unix;
  };
}
