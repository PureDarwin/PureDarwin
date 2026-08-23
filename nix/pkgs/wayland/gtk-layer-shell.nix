{ stdenv
, lib
, requireFile
, meson
, ninja
, pkg-config
, python3
, glibNative
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, gtk-layer-shell
, gtk3
, wayland
, waylandProtocols
, waylandScanner
, xkbcommon
, mesa
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
  # Wayland-only image: gtk-layer-shell needs no X11 of its own; the deps were
  # only there to match the X11-enabled gtk3 it links.
, withX11 ? true
, libpng
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  xDeps = lib.optionals withX11 [
    libX11 libxcb libXau libXdmcp libXext libXi libXrender libXrandr libXfixes
    libXcursor xorgproto
  ];
  deps = xDeps ++ [
    glib pcre2 libffi zlib libiconv
    cairo cairoGobject pixman
    pango fribidi harfbuzz freetype2 fontconfig expat
    gdkPixbuf
    libepoxy
    atspi2Core
    dbus
    libpng
    gtk3 wayland waylandProtocols xkbcommon
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
in
stdenv.mkDerivation {
  pname = "puredarwin-gtk-layer-shell";
  inherit (gtk-layer-shell) version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 glibNative waylandScanner ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:${waylandScanner}/bin:$PATH"

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
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
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-DCAIRO_HAS_GOBJECT_FUNCTIONS=1', '-Ddngettext(Domain,Singular,Plural,N)=((N)==1?(Singular):(Plural))', '-I${libSystem}/usr/include', '-I${mesa}/usr/include', '-I${./pd-compat-include}', '-DWL_EGL_PLATFORM=1', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-undefined,dynamic_lookup', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dexamples=false \
      -Ddocs=false \
      -Dtests=false \
      -Dintrospection=false \
      -Dvapi=false

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    ninja -C build install

    INSTALL_NAME_TOOL="${nativeMesonTools}/bin/install_name_tool"
    dylibs=$(find "$out/lib" -maxdepth 1 -name "*.dylib" -not -type l)
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
          "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
          "$INSTALL_NAME_TOOL" -change "$dep/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
        done
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "gtk-layer-shell, cross-built for PureDarwin - lets GTK3 windows be docks and desktops under a wlr-layer-shell compositor";
    homepage = "https://github.com/wmww/gtk-layer-shell";
    license = licenses.lgpl3Plus;
    platforms = platforms.linux;
  };
}
