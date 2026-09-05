{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libutf8proc
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-libutf8proc";
  inherit (libutf8proc) version src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -DUTF8PROC_EXPORTS -Iinclude/libutf8proc -I${libSystem}/usr/include"

    "$CC" $CFLAGS -c src/utf8proc.c -o utf8proc.c.o

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -install_name /lib/libutf8proc.dylib \
      -lSystem \
      utf8proc.c.o \
      -o libutf8proc.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/lib/pkgconfig" "$out/include/libutf8proc"
    install -m755 libutf8proc.dylib "$out/lib/libutf8proc.dylib"
    install -m644 include/libutf8proc/utf8proc.h "$out/include/libutf8proc/utf8proc.h"
    # Upstream installs the header at the include root and libutf8proc.pc's
    # Cflags points there, so ports that do #include <utf8proc.h> (fcft, foot)
    # need it in both places.
    install -m644 include/libutf8proc/utf8proc.h "$out/include/utf8proc.h"

    cat > "$out/lib/pkgconfig/libutf8proc.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include

Name: libutf8proc
Description: UTF-8 processing library
Version: ${libutf8proc.version}
Libs: -L\''${libdir} -lutf8proc
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libutf8proc, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
