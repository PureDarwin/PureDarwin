{ stdenv, lib, darwinCrossToolchain, nativeLd, libSystem, appleSdk
, targetTriple ? "armv6-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-temp";
  version = "0.1";
  src = ../../../src/Tools/temp;
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;
  buildPhase = ''
    export SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=11.0 -D_DARWIN_C_SOURCE -O2 -Wall \
      -fno-stack-protector -I${libSystem}/usr/include -L${libSystem}/usr/lib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 temp.c -lSystem -o temp
  '';
  installPhase = ''
    install -Dm755 temp $out/usr/bin/temp
  '';
  meta = with lib; {
    description = "Print the SoC temperature";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
