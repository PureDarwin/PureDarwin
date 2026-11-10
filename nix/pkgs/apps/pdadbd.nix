# SPDX-License-Identifier: MIT
# pdadbd: the device side of adb over a usb gadget's /dev/adb (PDQcomUSB, PDRockchipDWC3), from PureDarwin-20.5
{ stdenv, lib, darwinCrossToolchain, nativeLd, libSystem, appleSdk
, targetTriple ? "arm64-apple-darwin20.4"
, isArmv6 ? lib.hasPrefix "armv6-" targetTriple }:

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
      -isysroot "$SDK" -D_DARWIN_C_SOURCE -O2 -Wall \
      -fno-stack-protector -I${libSystem}/usr/include -L${libSystem}/usr/lib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      ${if isArmv6 then "-mmacosx-version-min=11.0 -Wl,-platform_version,macos,11.0,11.5"
        else "-Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains"} \
      pdadbd.c -lSystem -o pdadbd
  '';
  installPhase = ''
    install -Dm755 pdadbd $out/usr/sbin/pdadbd
    # launchd keeps it running, it exits while /dev/adb is missing and is started again
    install -Dm644 org.puredarwin.pdadbd.plist $out/System/Library/LaunchDaemons/org.puredarwin.pdadbd.plist
  '';
  meta = with lib; {
    description = "ADB device daemon for PureDarwin's USB gadget";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
