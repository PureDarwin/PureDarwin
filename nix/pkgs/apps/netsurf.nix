{ stdenv
, lib
, fetchgit
, perl
, pkg-config
, nsgenbind
, glibNative
, gdkPixbufNative
, inetutils
, darwinCrossToolchain
, nativeLd
, libSystem
, hostOtool
, gtk3
, glib
, cairo
, cairoGobject
, pango
, gdkPixbuf
, libepoxy
, atspi2Core
, dbus
, libcurl
, openssl
, zlib
, libpng
, libiconv
, libwapcaplet
, libparserutils
, libhubbub
, libcss
, libdom
, libnsgif
, libnsbmp
, libnsutils
, libutf8proc
, libX11 ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, libXext ? null
, libXi ? null
, libXrender ? null
, libXrandr ? null
, libXfixes ? null
, libXcursor ? null
, xorgproto ? null
  # NetSurf's GTK frontend goes through GDK, so it needs no X11 of its own;
  # these matched the X11-enabled gtk3 it links.
, withX11 ? true
, expat
, pcre2
, libffi
, fribidi
, harfbuzz
, freetype2
, fontconfig
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  deps = [
    gtk3 glib cairo cairoGobject pango gdkPixbuf libepoxy atspi2Core dbus
    libcurl openssl zlib libpng libiconv
    libwapcaplet libparserutils libhubbub libcss libdom
    libnsgif libnsbmp libnsutils libutf8proc
    expat pcre2 libffi fribidi harfbuzz freetype2 fontconfig
  ] ++ lib.optionals withX11 [
    libX11 libxcb libXau libXdmcp libXext libXi libXrender libXrandr libXfixes
    libXcursor xorgproto
  ];
  depPcPaths = map lib.getDev deps;

  src = fetchgit {
    url = "https://github.com/netsurf-browser/netsurf.git";
    rev = "140e56912639449673dbc301408e38817b8723a2"; # release/3.11
    hash = "sha256-/+H9xBla4FH93XVg3uPjbpb8TqIdWj+vXr9xMfqnvgc=";
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-netsurf${lib.optionalString (!withX11) "-nox"}";
  version = "3.11";
  inherit src;

  nativeBuildInputs = [ perl pkg-config nsgenbind glibNative gdkPixbufNative inetutils ];

  postPatch = ''
    patchShebangs .
    substituteInPlace Makefile --replace-fail 'LDFLAGS += -Wl,--trace' '# LDFLAGS += -Wl,--trace'
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}:${libcurl}/usr/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    export PATH="${glibNative}/bin:$PATH"

    cat > Makefile.config <<EOF
override NETSURF_USE_DUKTAPE := YES
override NETSURF_USE_JPEG := NO
override NETSURF_USE_JPEGXL := NO
override NETSURF_USE_WEBP := NO
override NETSURF_USE_HARU_PDF := NO
override NETSURF_USE_NSPSL := NO
override NETSURF_USE_NSLOG := NO
override NETSURF_USE_VIDEO := NO
override CC := ${darwinCrossToolchain}/bin/${targetTriple}-clang
override AR := ${darwinCrossToolchain}/bin/${targetTriple}-ar
override RANLIB := ${darwinCrossToolchain}/bin/${targetTriple}-ranlib
override STRIP := ${darwinCrossToolchain}/bin/${targetTriple}-strip
override PKG_CONFIG := ${pkg-config}/bin/pkg-config
# NOT "override" here: unlike CC/AR/.../PKG_CONFIG (which nothing else
# in netsurf's Makefiles reassigns), CFLAGS/LDFLAGS accumulate further
# pkg-config-derived flags via plain `CFLAGS += ...` all over the real
# build (frontends/gtk/Makefile, the top-level pkg_config_find_and_add
# macro, etc). Once a variable is set via `override`, GNU Make silently
# drops every later *plain* (non-override) += on it - confirmed via a
# minimal two-line test - so a plain assignment must be used here for
# those later appends to actually take effect.
CFLAGS += -isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -D_DARWIN_C_SOURCE -fno-stack-protector -I${libSystem}/usr/include
LDFLAGS += -isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem
EOF

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild

    make -j$NIX_BUILD_CORES

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    make install PREFIX=$out

    INSTALL_NAME_TOOL="${hostOtool}/bin/install_name_tool"
    load_paths="$out ${lib.concatStringsSep " " deps}"
    for f in $(find "$out/bin" -type f); do
      for dep in $load_paths; do
        for dylib in "$dep"/lib/*.dylib "$dep"/usr/lib/*.dylib; do
          [ -e "$dylib" ] || continue
          base=$(basename "$dylib")
          "$INSTALL_NAME_TOOL" -change "@rpath/$base" "/lib/$base" "$f" 2>/dev/null || true
          # a sibling inside the same project can be recorded by absolute install
          # path rather than @rpath (libxfce4windowingui -> libxfce4windowing), which
          # the @rpath rewrite above never matches
          "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
          # siblings inside one project can be recorded by absolute install path
          # rather than @rpath (libxfce4windowingui -> libxfce4windowing), which the
          # @rpath rewrite above never matches
          "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
          "$INSTALL_NAME_TOOL" -change "$dep/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
          "$INSTALL_NAME_TOOL" -change "$dep/usr/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
        done
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "NetSurf web browser (GTK3 frontend), cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
