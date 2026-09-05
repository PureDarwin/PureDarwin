{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, src
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# epoll/timerfd/signalfd/eventfd over kqueue, installed as <sys/epoll.h> plus a
# pkg-config file named epoll-shim - the same name the BSD ports use, so meson
# projects that do dependency('epoll-shim') find it without patching.
#
# The implementation is the shim already vendored in Wayland's tree, which has
# been carrying libwayland-server's event loop (and therefore sway/wlroots)
# since the Wayland port landed.

let
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
in
stdenv.mkDerivation {
  pname = "puredarwin-epoll-shim";
  version = "1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -D_DARWIN_C_SOURCE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -fno-stack-protector -fPIC -O2 \
      -I. -I${libSystem}/usr/include \
      -c epoll.c -o epoll.o

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -D_DARWIN_C_SOURCE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -fno-stack-protector -fPIC -O2 \
      -I. -I${libSystem}/usr/include \
      -c sem.c -o sem.o

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,/usr/lib/libepoll-shim.dylib \
      -Wl,-fixup_chains \
      epoll.o sem.o -lSystem -o libepoll-shim.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    # Ports include these under four different names; all four resolve to the
    # one shim.
    install -Dm644 epoll.h "$out/include/sys/epoll.h"
    for alias in timerfd eventfd signalfd; do
      cat > "$out/include/sys/$alias.h" <<ALIASEOF
#ifndef PD_SYS_''${alias}_H
#define PD_SYS_''${alias}_H
#include <sys/epoll.h>
#endif
ALIASEOF
    done
    install -Dm755 libepoll-shim.dylib "$out/usr/lib/libepoll-shim.dylib"

    mkdir -p "$out/lib"
    ln -s ../usr/lib/libepoll-shim.dylib "$out/lib/libepoll-shim.dylib"

    mkdir -p "$out/lib/pkgconfig"
    cat > "$out/lib/pkgconfig/epoll-shim.pc" <<EOF
prefix=$out
includedir=\''${prefix}/include
libdir=\''${prefix}/usr/lib

Name: epoll-shim
Description: epoll/timerfd/signalfd/eventfd over kqueue for PureDarwin
Version: 1
Cflags: -I\''${includedir}
Libs: -L\''${libdir} -lepoll-shim
EOF

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "epoll/timerfd/signalfd/eventfd compatibility layer over kqueue";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
