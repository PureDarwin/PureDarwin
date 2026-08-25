{ stdenv, lib, darwinCrossToolchain, nativeLd, libSystem, libobjc, corefoundation, pdsurface, coregraphics
, appleSdk, src ? ../../../src/Userspace/cg-screen-demo
, targetTriple ? "x86_64-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-cg-screen-demo";
  version = "0.1";
  inherit src;
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;
  buildPhase = ''
    export SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=11.0 -D_DARWIN_C_SOURCE -x objective-c \
      -fno-stack-protector -I${pdsurface}/usr/include -I${coregraphics}/usr/include \
      -I${corefoundation}/include \
      -I${libSystem}/usr/include -I${libobjc}/usr/include \
      -L${pdsurface}/usr/lib -L${coregraphics}/usr/lib -L${libSystem}/usr/lib \
      -L${libobjc}/usr/lib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 cg-screen-demo.c \
      -lCoreGraphics -lPDSurface -lobjc -lSystem -o cg-screen-demo
  '';
  installPhase = ''
    install -Dm755 cg-screen-demo $out/usr/bin/cg-screen-demo
  '';
  meta = with lib; {
    description = "CoreGraphics bitmap-context scanout demo for PureDarwin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
