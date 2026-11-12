{ stdenv, lib, darwinCrossToolchain, nativeLd, libSystem, libobjc, corefoundation, pdsurface, coregraphics
, appleSdk, src ? ../../../src/Userspace/pdres
, targetTriple ? "x86_64-apple-darwin20.4" }:

# pdres: the main display's modes, listed and switched through CoreGraphics as Settings does
stdenv.mkDerivation {
  pname = "puredarwin-pdres";
  version = "0.1";
  inherit src;
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;
  buildPhase = ''
    export SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -D_DARWIN_C_SOURCE \
      -fno-stack-protector -I${coregraphics}/usr/include -I${corefoundation}/include \
      -I${libSystem}/usr/include -I${libobjc}/usr/include \
      -L${pdsurface}/usr/lib -L${coregraphics}/usr/lib -L${corefoundation}/usr/lib -L${libSystem}/usr/lib \
      -L${libobjc}/usr/lib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 pdres.c \
      -lCoreGraphics -lCoreFoundation -lobjc -lSystem -o pdres
  '';
  installPhase = ''
    install -Dm755 pdres $out/usr/bin/pdres
  '';
  meta = with lib; {
    description = "Display mode listing and switching for PureDarwin";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.unix;
  };
}
