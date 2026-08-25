{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, cairo
, cairoReal
, glib
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  deps = [ cairo glib ];
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
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

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

    # CAIRO_HAS_GOBJECT_FUNCTIONS must reach consumers, not just this build:
    # all of cairo-gobject.h sits behind "#if CAIRO_HAS_GOBJECT_FUNCTIONS", and
    # upstream cairo defines it in cairo-features.h when gobject support is built
    # in. PureDarwin builds cairo-gobject as its own derivation, so cairo never
    # defines it and the installed header is inert - including it succeeds and
    # then every CAIRO_GOBJECT_TYPE_* use fails as undeclared (first seen in
    # xfce4-appfinder).
    cat > "$out/lib/pkgconfig/cairo-gobject.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include/cairo

Name: cairo-gobject
Description: cairo-gobject for cairo graphics library
Version: ${cairoReal.version}
Requires: cairo glib-2.0 gobject-2.0
Libs: -L\''${libdir} -lcairo-gobject
Cflags: -I\''${includedir} -DCAIRO_HAS_GOBJECT_FUNCTIONS=1
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "cairo-gobject, cross-built standalone for PureDarwin";
    platforms = platforms.linux;
  };
}
