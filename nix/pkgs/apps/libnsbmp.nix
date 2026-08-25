{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libnsbmp
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-libnsbmp";
  inherit (libnsbmp) version src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -Iinclude -I${libSystem}/usr/include"

    "$CC" $CFLAGS -c src/libnsbmp.c -o libnsbmp.c.o

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=11.0 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -install_name /lib/libnsbmp.dylib \
      -lSystem \
      libnsbmp.c.o \
      -o libnsbmp.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/lib/pkgconfig" "$out/include"
    install -m755 libnsbmp.dylib "$out/lib/libnsbmp.dylib"
    install -m644 include/libnsbmp.h "$out/include/libnsbmp.h"

    cat > "$out/lib/pkgconfig/libnsbmp.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include

Name: libnsbmp
Description: BMP/ICO decoding library for NetSurf
Version: ${libnsbmp.version}
Libs: -L\''${libdir} -lnsbmp
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libnsbmp (NetSurf BMP/ICO decoder), cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
