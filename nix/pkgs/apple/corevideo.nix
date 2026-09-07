{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, libobjc
, corefoundation
, foundation
, openglFramework
, mesa
, glu
, src
, appleSdk
}:

let
  installName = "/System/Library/Frameworks/CoreVideo.framework/Versions/A/CoreVideo";
in
stdenv.mkDerivation {
  pname = "puredarwin-corevideo";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    mkdir -p staged/CoreVideo cf-headers
    cp *.h staged/CoreVideo/
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
      -F${openglFramework}/System/Library/Frameworks
    "

    echo "  CC CVDisplayLink.m"
    $cc $cflags -c CVDisplayLink.m -o CVDisplayLink.o

    $cc \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -L${libobjc}/usr/lib \
      -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib \
      -L${openglFramework}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib \
      -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,${installName} \
      CVDisplayLink.o \
      -lOpenGL -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o CoreVideo

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/CoreVideo.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers" \
      "$frameworkDir/Versions/A/Resources"
    cp CoreVideo "$frameworkDir/Versions/A/CoreVideo"
    cp *.h "$frameworkDir/Versions/A/Headers/"
    cp Info.plist "$frameworkDir/Versions/A/Resources/"

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/CoreVideo "$frameworkDir/CoreVideo"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"
    ln -s Versions/Current/Resources "$frameworkDir/Resources"

    mkdir -p "$out/usr/lib" "$out/usr/include"
    ln -s ../../System/Library/Frameworks/CoreVideo.framework/Versions/A/CoreVideo \
      "$out/usr/lib/libCoreVideo.dylib"
    ln -s ../../System/Library/Frameworks/CoreVideo.framework/Versions/A/Headers \
      "$out/usr/include/CoreVideo"

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Cocotron CoreVideo compatibility framework";
    platforms = platforms.unix;
  };
}
