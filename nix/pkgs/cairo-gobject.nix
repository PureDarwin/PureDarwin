{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, cairo
, cairoReal
, glib
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  deps = [ cairo glib ];
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
  pname = "puredarwin-cairo-gobject";
  inherit (cairoReal) version;
  src = cairoReal.src;

  # cairo.nix (the real cairo build we depend on) strips subdir('util')
  # entirely, so cairo-gobject (util/cairo-gobject upstream) never gets
  # built there. It's only 2 small .c files plus a header, so compile it
  # standalone here instead of complicating cairo.nix's own build.
  sourceRoot = ".";

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    SRC=$(echo */util/cairo-gobject)
    touch "$SRC/config.h"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -DCAIRO_COMPILATION -DCAIRO_HAS_GOBJECT_FUNCTIONS=1 -I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${dep}/include -I${dep}/include/cairo -I${dep}/include/glib-2.0 -I${dep}/lib/glib-2.0/include") deps}"

    for f in "$SRC"/cairo-gobject-enums.c "$SRC"/cairo-gobject-structs.c; do
      "$CC" $CFLAGS -c "$f" -o "$(basename "$f").o"
    done

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=11.0 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -L${cairo}/lib -L${glib}/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -install_name /lib/libcairo-gobject.dylib \
      -lcairo -lglib-2.0 -lgobject-2.0 \
      -lSystem \
      cairo-gobject-enums.c.o cairo-gobject-structs.c.o \
      -o libcairo-gobject.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/lib/pkgconfig" "$out/include/cairo"
    install -m755 libcairo-gobject.dylib "$out/lib/libcairo-gobject.dylib"
    SRC=$(echo */util/cairo-gobject)
    install -m644 "$SRC/cairo-gobject.h" "$out/include/cairo/cairo-gobject.h"

    cat > "$out/lib/pkgconfig/cairo-gobject.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include/cairo

Name: cairo-gobject
Description: cairo-gobject for cairo graphics library
Version: ${cairoReal.version}
Requires: cairo glib-2.0 gobject-2.0
Libs: -L\''${libdir} -lcairo-gobject
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "cairo-gobject, cross-built standalone for PureDarwin";
    platforms = platforms.linux;
  };
}
