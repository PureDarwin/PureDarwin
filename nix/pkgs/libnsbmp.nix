{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, libnsbmp
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-libnsbmp";
  inherit (libnsbmp) version src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    CC="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang"

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
