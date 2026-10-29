# uigallery.nix - the AppKit control gallery used to compare PureDarwin's drawing with macOS
{ stdenv, lib, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, src
, targetTriple ? "x86_64-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-uigallery";
  version = "0.1";
  inherit src;
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;

  buildPhase = ''
    runHook preBuild
    export SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cc=${darwinCrossToolchain}/bin/${targetTriple}-clang
    $cc -isysroot $SDK -mmacosx-version-min=26.5 -D_DARWIN_C_SOURCE -fno-stack-protector -O1 \
      -x objective-c -fno-objc-arc -fblocks -Wno-deprecated-declarations \
      -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
      -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
      -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
      -I${coregraphics}/usr/include -I${coreservices}/usr/include \
      -I${windowserver}/usr/include -I${mesa}/usr/include \
      -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks \
      -c uigallery.m -o uigallery.o
    $cc -isysroot "$SDK" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${appkit}/usr/lib -L${onyx2d}/usr/lib \
      -L${coretext}/usr/lib -L${quartzcore}/usr/lib -L${coregraphics}/usr/lib \
      -L${windowserver}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      uigallery.o \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o uigallery
    runHook postBuild
  '';

  installPhase = ''
    install -Dm755 uigallery $out/usr/local/bin/uigallery
  '';
}
