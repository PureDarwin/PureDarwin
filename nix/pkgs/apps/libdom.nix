{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libdom
, libwapcaplet
, libparserutils
, libhubbub
, expat
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  deps = [ libwapcaplet libparserutils libhubbub expat ];
in
stdenv.mkDerivation {
  pname = "puredarwin-libdom";
  inherit (libdom) version src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -Iinclude -Isrc -Ibindings/hubbub -Ibindings/xml -I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${dep}/include") deps}"

    srcs=$(find src -name '*.c')
    srcs="$srcs bindings/hubbub/parser.c bindings/xml/expat_xmlparser.c"
    for f in $srcs; do
      obj=$(echo "$f" | tr '/' '_').o
      "$CC" $CFLAGS -c "$f" -o "$obj"
    done

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") deps} \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
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
