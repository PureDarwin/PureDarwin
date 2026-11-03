{ stdenv, lib, darwinCrossToolchain, nativeLd, libSystem, appleSdk
, targetTriple ? "armv6-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-pdadbd";
  version = "0.1";
  src = ../../../src/Tools/pdadbd;
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
      -Wl,-platform_version,macos,11.0,11.5 pdadbd.c -lSystem -o pdadbd
  '';
  installPhase = ''
    install -Dm755 pdadbd $out/usr/sbin/pdadbd
    # launchd keeps it running; it exits while /dev/adb is missing and is started again
    install -Dm644 org.puredarwin.pdadbd.plist $out/System/Library/LaunchDaemons/org.puredarwin.pdadbd.plist
  '';
  meta = with lib; {
    description = "ADB device daemon for PureDarwin's USB gadget";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
