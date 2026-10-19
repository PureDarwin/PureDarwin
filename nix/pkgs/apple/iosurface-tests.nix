{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, corefoundation
, iosurface
, openglFramework
, mesa
, glu
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# the IOSurface tests, run in a guest: sharing between processes, NV12 planes, and a GL texture from a surface
stdenv.mkDerivation {
  pname = "puredarwin-iosurface-tests";
  version = "0.1";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation
    mkdir -p gl-headers
    ln -s ${openglFramework}/System/Library/Frameworks/OpenGL.framework/Versions/A/Headers gl-headers/OpenGL
    common="-isysroot $SDK -mmacosx-version-min=26.5 -O1 -Wall -Icf-headers -I${libSystem}/usr/include
      -fuse-ld=${nativeLd}/bin/ld -nostdlib
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib -L${corefoundation}/usr/lib"
    $CC $common $src/iosurface-test.c \
      ${iosurface}/System/Library/Frameworks/IOSurface.framework/Versions/A/IOSurface \
      -lCoreFoundation -lSystem -o iosurface-test
    $CC $common -Igl-headers \
      -I${mesa}/usr/include $src/iosurface-gl-test.c \
      ${iosurface}/System/Library/Frameworks/IOSurface.framework/Versions/A/IOSurface \
      ${openglFramework}/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL \
      -Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib \
      -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib \
      -lCoreFoundation -lSystem -o iosurface-gl-test
    $CC $common -Igl-headers \
      -I${mesa}/usr/include $src/iosurface-yuv-test.c \
      ${iosurface}/System/Library/Frameworks/IOSurface.framework/Versions/A/IOSurface \
      ${openglFramework}/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL \
      -Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib \
      -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib \
      -lCoreFoundation -lSystem -o iosurface-yuv-test
    $CC $common -Igl-headers \
      -I${mesa}/usr/include $src/calayer-test.c \
      ${openglFramework}/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL \
      -Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib \
      -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib \
      -lCoreFoundation -lSystem -o calayer-test
    $CC $common -Igl-headers \
      -I${mesa}/usr/include $src/cgl-zink-test.c \
      ${iosurface}/System/Library/Frameworks/IOSurface.framework/Versions/A/IOSurface \
      ${openglFramework}/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL \
      -Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib \
      -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib \
      -lCoreFoundation -lSystem -o cgl-zink-test
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 iosurface-test $out/usr/local/bin/iosurface-test
    install -Dm755 iosurface-gl-test $out/usr/local/bin/iosurface-gl-test
    install -Dm755 iosurface-yuv-test $out/usr/local/bin/iosurface-yuv-test
    install -Dm755 calayer-test $out/usr/local/bin/calayer-test
    install -Dm755 cgl-zink-test $out/usr/local/bin/cgl-zink-test
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "IOSurface sharing, plane and GL tests for PureDarwin guests";
    platforms = lib.platforms.unix;
  };
}
