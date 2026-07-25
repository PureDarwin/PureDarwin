{ stdenv
, lib
, requireFile
, perl
, darwinCrossToolchain
, nativeLd
, libSystem
, libparserutils
, libiconv
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
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    perl build/make-aliases.pl

    CC="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang"

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
