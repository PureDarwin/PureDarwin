{ stdenv
, lib
, requireFile
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
, libX11
, libxcb
, libXau
, libXdmcp
, libXext
, libXi
, libXrender
, libXrandr
, libXfixes
, libXcursor
, xorgproto
, expat
, pcre2
, libffi
, fribidi
, harfbuzz
, freetype2
, fontconfig
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  deps = [
    gtk3 glib cairo cairoGobject pango gdkPixbuf libepoxy atspi2Core dbus
    libcurl openssl zlib libpng libiconv
    libwapcaplet libparserutils libhubbub libcss libdom
    libnsgif libnsbmp libnsutils libutf8proc
    libX11 libxcb libXau libXdmcp libXext libXi libXrender libXrandr libXfixes libXcursor
    xorgproto expat pcre2 libffi fribidi harfbuzz freetype2 fontconfig
  ];
  depPcPaths = map lib.getDev deps;

  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };

  src = fetchgit {
    url = "https://github.com/netsurf-browser/netsurf.git";
    rev = "140e56912639449673dbc301408e38817b8723a2"; # release/3.11
    hash = "sha256-/+H9xBla4FH93XVg3uPjbpb8TqIdWj+vXr9xMfqnvgc=";
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-netsurf";
  version = "3.11";
  inherit src;

  nativeBuildInputs = [ perl pkg-config nsgenbind glibNative gdkPixbufNative inetutils ];

  postPatch = ''
    patchShebangs .
    substituteInPlace Makefile --replace-fail 'LDFLAGS += -Wl,--trace' '# LDFLAGS += -Wl,--trace (removed for PureDarwin: lld does not support it)'
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

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
