{ stdenv
, lib
, meson
, ninja
, pkg-config
, python3
, glibNative
, libxslt
, docbook-xsl-nons
, libxml2
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, version
, src
, gtk3
, vte
, libxfce4ui
, libxfce4util
, xfconf
, glib
, pcre2
, libffi
, zlib
, libiconv
, cairo
, cairoGobject
, pixman
, pango
, fribidi
, harfbuzz
, freetype2
, fontconfig
, expat
, gdkPixbuf
, libepoxy
, atspi2Core
, dbus
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
, libpng
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  deps = [
    glib pcre2 libffi zlib libiconv
    cairo cairoGobject pixman
    pango fribidi harfbuzz freetype2 fontconfig expat
    gdkPixbuf
    libepoxy
    atspi2Core
    dbus
    libX11 libxcb libXau libXdmcp libXext libXi libXrender libXrandr libXfixes libXcursor
    xorgproto libpng
    gtk3 vte libxfce4ui libxfce4util xfconf
  ];
  depPcPaths = map lib.getDev deps;
in
stdenv.mkDerivation {
  pname = "puredarwin-xfce4-terminal";
  inherit version src;

  patches = [ ./xfce4-terminal/no-layer-shell-finalize.patch ];

    # xsltproc builds the man page; docbook-xsl + an XML catalog keep it offline.
  nativeBuildInputs = [ meson ninja pkg-config python3 glibNative libxslt libxml2 ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"
    export XML_CATALOG_FILES="${docbook-xsl-nons}/xml/xsl/docbook/catalog.xml"

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    export PATH="${glibNative}/bin:$PATH"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
cpp = '${darwinCrossToolchain}/bin/${targetTriple}-clang++'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool'
glib-compile-resources = '${glibNative}/bin/glib-compile-resources'
glib-compile-schemas = '${glibNative}/bin/glib-compile-schemas'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-DCAIRO_HAS_GOBJECT_FUNCTIONS=1', '-Ddngettext(Domain,Singular,Plural,N)=((N)==1?(Singular):(Plural))', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-undefined,dynamic_lookup', '-lSystem']
cpp_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-Wno-invalid-constexpr', '-DCAIRO_HAS_GOBJECT_FUNCTIONS=1', '-Ddngettext(Domain,Singular,Plural,N)=((N)==1?(Singular):(Plural))', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
cpp_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-undefined,dynamic_lookup', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    # --prefix=/ with DESTDIR at install, not --prefix=$out: xfce4-terminal
    # compiles its own datadir in, so a store prefix makes it look for
    # $out/share/applications/xfce4-terminal.desktop, a path that does not exist
    # in the guest ("Unable to open ..."). DESTDIR=$out keeps the staged layout.
    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=/ \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dx11=enabled \
      -Dwayland=disabled

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    DESTDIR="$out" ninja -C build install

    INSTALL_NAME_TOOL="${nativeMesonTools}/bin/install_name_tool"
    # a terminal ships no libraries; guard the template's dylib fixup
    dylibs=$([ -d "$out/lib" ] && find "$out/lib" -maxdepth 1 -name "*.dylib" -not -type l || true)
    for dylib in $dylibs; do
      base=$(basename "$dylib")
      "$INSTALL_NAME_TOOL" -id "/lib/$base" "$dylib"
    done
    allfiles=$(
      [ ! -d "$out/bin" ] || find "$out/bin" -type f
      [ ! -d "$out/lib" ] || find "$out/lib" -type f
    )
    load_paths="$out ${lib.concatStringsSep " " deps}"
    for f in $allfiles; do
      for dep in $load_paths; do
        for dylib in "$dep"/lib/*.dylib; do
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
        done
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "xfce4-terminal, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
