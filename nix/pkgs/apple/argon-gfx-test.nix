{ stdenv, lib, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, carbon, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, src ? ../../../tests/argon-gfx
, targetTriple ? "x86_64-apple-darwin20.4" }:

# argon damage and present measurements
stdenv.mkDerivation {
  pname = "puredarwin-argon-gfx-test";
  version = "0.1";
  inherit src;
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cc=${darwinCrossToolchain}/bin/${targetTriple}-clang
    $cc -isysroot $SDK -mmacosx-version-min=26.5 -D_DARWIN_C_SOURCE -O1 \
      -x objective-c -fno-objc-arc -fblocks -Wall -Wno-deprecated-declarations -Wno-multichar \
      -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
      -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
      -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
      -I${coregraphics}/usr/include -I${coreservices}/usr/include \
      -I${windowserver}/usr/include -I${mesa}/usr/include \
      -F${appkit}/System/Library/Frameworks -F${carbon}/System/Library/Frameworks \
      -F${coregraphics}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks \
      -c gfxtest.m -o gfxtest.o
    $cc -isysroot "$SDK" -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${appkit}/usr/lib -L${coregraphics}/usr/lib \
      -F${appkit}/System/Library/Frameworks -F${carbon}/System/Library/Frameworks \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      gfxtest.o -lAppKit -lCoreGraphics -lFoundation -lCoreFoundation -lobjc -lSystem -o gfxtest
    runHook postBuild
  '';

  installPhase = ''
    install -Dm755 gfxtest $out/usr/local/bin/gfxtest
  '';

  meta.platforms = lib.platforms.unix;
}
