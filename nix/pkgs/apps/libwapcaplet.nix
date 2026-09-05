{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libwapcaplet
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-libwapcaplet";
  inherit (libwapcaplet) version src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -Iinclude -I${libSystem}/usr/include"

    "$CC" $CFLAGS -c src/libwapcaplet.c -o libwapcaplet.c.o

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -install_name /lib/libwapcaplet.dylib \
      -lSystem \
      libwapcaplet.c.o \
      -o libwapcaplet.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/lib/pkgconfig" "$out/include/libwapcaplet"
    install -m755 libwapcaplet.dylib "$out/lib/libwapcaplet.dylib"
    install -m644 include/libwapcaplet/libwapcaplet.h "$out/include/libwapcaplet/libwapcaplet.h"

    cat > "$out/lib/pkgconfig/libwapcaplet.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include

Name: libwapcaplet
Description: String internment library for NetSurf
Version: ${libwapcaplet.version}
Libs: -L\''${libdir} -lwapcaplet
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libwapcaplet (NetSurf string internment library), cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
