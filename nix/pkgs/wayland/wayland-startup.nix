{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, wayland
, waylandProtocols
, waylandScanner
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
in
stdenv.mkDerivation {
  pname = "puredarwin-wayland-startup";
  version = "1";
  src = ../../../src/Userspace/wayland-startup;

  dontConfigure = true;
  nativeBuildInputs = [ stdenv.cc waylandScanner ];

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    wayland-scanner client-header \
      ${waylandProtocols}/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
      xdg-shell-client-protocol.h
    wayland-scanner private-code \
      ${waylandProtocols}/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
      xdg-shell-protocol.c

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -D_DARWIN_C_SOURCE -I. -I${wayland}/include -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${wayland}/lib -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-fixup_chains \
      -lwayland-client -lSystem -o wayland-startup wayland-startup.c \
      xdg-shell-protocol.c
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 wayland-startup $out/usr/bin/wayland-startup
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Minimal visible Wayland startup client for PureDarwin";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
