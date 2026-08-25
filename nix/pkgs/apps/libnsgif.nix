{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libnsgif
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let

  srcs = [ "src/lzw.c" "src/gif.c" ];
in
stdenv.mkDerivation {
  pname = "puredarwin-libnsgif";
  inherit (libnsgif) version src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -Iinclude -I${libSystem}/usr/include"

    for f in ${lib.concatStringsSep " " srcs}; do
      obj=$(basename "$f").o
      "$CC" $CFLAGS -c "$f" -o "$obj"
    done

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=11.0 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -install_name /lib/libnsgif.dylib \
      -lSystem \
      *.o \
      -o libnsgif.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/lib/pkgconfig" "$out/include"
    install -m755 libnsgif.dylib "$out/lib/libnsgif.dylib"
    install -m644 include/nsgif.h "$out/include/nsgif.h"

    cat > "$out/lib/pkgconfig/libnsgif.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include

Name: libnsgif
Description: GIF decoding library for NetSurf
Version: ${libnsgif.version}
Libs: -L\''${libdir} -lnsgif
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libnsgif (NetSurf GIF decoder), cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
