{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, libdom
, libwapcaplet
, libparserutils
, libhubbub
, expat
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  deps = [ libwapcaplet libparserutils libhubbub expat ];
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
  pname = "puredarwin-libdom";
  inherit (libdom) version src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -Iinclude -Isrc -Ibindings/hubbub -Ibindings/xml -I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${dep}/include") deps}"

    srcs=$(find src -name '*.c')
    srcs="$srcs bindings/hubbub/parser.c bindings/xml/expat_xmlparser.c"
    for f in $srcs; do
      obj=$(echo "$f" | tr '/' '_').o
      "$CC" $CFLAGS -c "$f" -o "$obj"
    done

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=11.0 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") deps} \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -install_name /lib/libdom.dylib \
      -lwapcaplet -lparserutils -lhubbub -lexpat -lSystem \
      *.o \
      -o libdom.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/lib/pkgconfig" "$out/include"
    install -m755 libdom.dylib "$out/lib/libdom.dylib"
    cp -r include/dom "$out/include/dom"
    mkdir -p "$out/include/dom/bindings/hubbub" "$out/include/dom/bindings/xml"
    install -m644 bindings/hubbub/errors.h bindings/hubbub/parser.h "$out/include/dom/bindings/hubbub/"
    install -m644 bindings/xml/xmlerror.h bindings/xml/xmlparser.h "$out/include/dom/bindings/xml/"

    cat > "$out/lib/pkgconfig/libdom.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include

Name: libdom
Description: Document Object Model library for NetSurf
Version: ${libdom.version}
Requires: libparserutils libwapcaplet libhubbub
Libs: -L\''${libdir} -ldom
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libdom (NetSurf DOM implementation), cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
