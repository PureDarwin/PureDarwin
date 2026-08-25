{ stdenv
, lib
, perl
, darwinCrossToolchain
, nativeLd
, libSystem
, libparserutils
, libiconv
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let

  srcs = [
    "src/input/filter.c"
    "src/input/inputstream.c"
    "src/utils/errors.c"
    "src/utils/buffer.c"
    "src/utils/vector.c"
    "src/utils/stack.c"
    "src/charset/aliases.c"
    "src/charset/codec.c"
    "src/charset/codecs/codec_utf16.c"
    "src/charset/codecs/codec_utf8.c"
    "src/charset/codecs/codec_ext8.c"
    "src/charset/codecs/codec_8859.c"
    "src/charset/codecs/codec_ascii.c"
    "src/charset/encodings/utf16.c"
    "src/charset/encodings/utf8.c"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-libparserutils";
  inherit (libparserutils) version src;

  nativeBuildInputs = [ perl ];

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    perl build/make-aliases.pl

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -Iinclude -Isrc -I${libSystem}/usr/include -I${libiconv}/usr/include"

    for f in ${lib.concatStringsSep " " srcs}; do
      obj=$(echo "$f" | tr '/' '_').o
      "$CC" $CFLAGS -c "$f" -o "$obj"
    done

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=11.0 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib -L${libiconv}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -install_name /lib/libparserutils.dylib \
      -liconv -lSystem \
      *.o \
      -o libparserutils.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/lib/pkgconfig" "$out/include"
    install -m755 libparserutils.dylib "$out/lib/libparserutils.dylib"
    cp -r include/parserutils "$out/include/parserutils"

    cat > "$out/lib/pkgconfig/libparserutils.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include

Name: libparserutils
Description: Parser building library for NetSurf
Version: ${libparserutils.version}
Libs: -L\''${libdir} -lparserutils
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libparserutils (NetSurf parser-building library), cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
