{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, coregraphics
, corefoundation
, wayland
, xkbcommon
, waylandProtocols
, waylandScanner
, src
, appleSdk
}:

stdenv.mkDerivation {
  pname = "puredarwin-windowserver";
  version = "0.1";

  inherit src;

  nativeBuildInputs = [ waylandScanner ];

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # Sources #import <WindowServer/X.h>, so stage the headers under that name.
    mkdir -p staged/WindowServer
    cp *.h staged/WindowServer/

    protocol=${waylandProtocols}/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
    wayland-scanner client-header "$protocol" xdg-shell-client-protocol.h
    wayland-scanner private-code "$protocol" xdg-shell-protocol.c

    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    cflags="
      -isysroot $DARWIN_SDK_ROOT
      -mmacosx-version-min=26.5
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
      -fno-stack-protector
      -I$PWD/staged
      -I$PWD
      -I${libSystem}/usr/include
      -I${coregraphics}/usr/include
      -I${corefoundation}/include
      -I${wayland}/include
      -I${xkbcommon}/include
    "

    $cc $cflags -c rpc_wayland.c -o rpc_wayland.o
    $cc $cflags -c xdg-shell-protocol.c -o xdg-shell-protocol.o

    $cc \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -L${coregraphics}/usr/lib \
      -L${corefoundation}/usr/lib \
      -L${wayland}/lib \
      -L${xkbcommon}/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,/usr/lib/libWindowServer.dylib \
      rpc_wayland.o xdg-shell-protocol.o \
      -lwayland-client -lxkbcommon -lCoreGraphics -lCoreFoundation -lSystem \
      -o libWindowServer.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/usr/lib $out/usr/include/WindowServer
    cp libWindowServer.dylib $out/usr/lib/
    cp *.h $out/usr/include/WindowServer/

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "PureDarwin window-server RPC, serviced in process against Wayland";
    platforms = platforms.linux;
  };
}
