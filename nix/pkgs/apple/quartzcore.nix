{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, libobjc
, corefoundation
, foundation
, onyx2d
, coregraphics
, coretext
, openglFramework
, corevideo
, applicationservices
, mesa
, glu
, src
, appleSdk
}:

let
  installName = "/System/Library/Frameworks/QuartzCore.framework/Versions/A/QuartzCore";
in
stdenv.mkDerivation {
  pname = "puredarwin-quartzcore";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    mkdir -p staged/QuartzCore cf-headers
    cp *.h staged/QuartzCore/
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    cflags="
      -isysroot $DARWIN_SDK_ROOT
      -mmacosx-version-min=26.5
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
      -fno-stack-protector
      -fobjc-exceptions -fblocks
      -Wno-deprecated-objc-isa-usage -Wno-objc-root-class
      -I$PWD/staged
      -I$PWD/cf-headers
      -I${libSystem}/usr/include
      -I${libobjc}/usr/include
      -I${corefoundation}/include
      -I${foundation}/usr/include
      -I${onyx2d}/usr/include
      -I${coregraphics}/usr/include
      -I${coretext}/usr/include
      -I${corevideo}/usr/include
      -I${applicationservices}/usr/include
      -I${mesa}/usr/include
      -F${openglFramework}/System/Library/Frameworks
    "

    objects=""
    for source in *.m; do
      object="''${source%.m}.o"
      echo "  CC $source"
      $cc $cflags -c "$source" -o "$object"
      objects="$objects $object"
    done

    $cc \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -L${libobjc}/usr/lib \
      -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib \
      -L${onyx2d}/usr/lib \
      -L${coregraphics}/usr/lib \
      -L${coretext}/usr/lib \
      -L${corevideo}/usr/lib \
      -L${openglFramework}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib \
      -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib \
      -Wl,-dylib_file,/usr/lib/libOnyx2D.dylib:${onyx2d}/usr/lib/libOnyx2D.dylib \
      -Wl,-dylib_file,/System/Library/Frameworks/CoreVideo.framework/Versions/A/CoreVideo:${corevideo}/System/Library/Frameworks/CoreVideo.framework/Versions/A/CoreVideo \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,${installName} \
      $objects \
      -lOpenGL -lCoreVideo -lCoreText -lCoreGraphics -lOnyx2D -lFoundation -lCoreFoundation \
      -lobjc -lSystem \
      -o QuartzCore

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/QuartzCore.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers" \
      "$frameworkDir/Versions/A/Resources"
    cp QuartzCore "$frameworkDir/Versions/A/QuartzCore"
    cp *.h "$frameworkDir/Versions/A/Headers/"
    cp Info.plist "$frameworkDir/Versions/A/Resources/"

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/QuartzCore "$frameworkDir/QuartzCore"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"
    ln -s Versions/Current/Resources "$frameworkDir/Resources"

    mkdir -p "$out/usr/lib" "$out/usr/include"
    ln -s ../../System/Library/Frameworks/QuartzCore.framework/Versions/A/QuartzCore \
      "$out/usr/lib/libQuartzCore.dylib"
    ln -s ../../System/Library/Frameworks/QuartzCore.framework/Versions/A/Headers \
      "$out/usr/include/QuartzCore"

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Cocotron QuartzCore compatibility framework";
    platforms = platforms.unix;
  };
}
