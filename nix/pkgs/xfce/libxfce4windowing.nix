{ stdenv
, lib
, meson
, ninja
, pkg-config
, python3
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, version
, src
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
, libpng
, gtk3
, libwnck
, libdisplayInfo
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
, libXres
, startupNotification
, xorgproto
, wayland
, waylandProtocols
, waylandScanner
, xkbcommon
, mesa
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  deps = [
    glib pcre2 libffi zlib libiconv
    cairo cairoGobject pixman
    pango fribidi harfbuzz freetype2 fontconfig expat
    gdkPixbuf libepoxy atspi2Core dbus libpng
    gtk3 libwnck libdisplayInfo
    libX11 libxcb libXau libXdmcp libXext libXi libXrender libXrandr
    libXfixes libXcursor libXres
    startupNotification
    xorgproto
    wayland waylandProtocols xkbcommon
  ];
  depPcPaths = map lib.getDev deps;
in
stdenv.mkDerivation {
  pname = "puredarwin-libxfce4windowing";
  inherit version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 waylandScanner ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .
    source ${./no-symbol-aliases.sh}
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:${waylandScanner}/bin:$PATH"

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}:${waylandScanner}/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    # meson asks for wayland-scanner as a native dependency, so it resolves
    # through the build machine's pkg-config rather than the cross one above.
    export PKG_CONFIG_PATH_FOR_BUILD="${waylandScanner}/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR_FOR_BUILD="${waylandScanner}/lib/pkgconfig"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
cpp = '${darwinCrossToolchain}/bin/${targetTriple}-clang++'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', '-I${mesa}/usr/include', '-I${../wayland/pd-compat-include}', '-DWL_EGL_PLATFORM=1', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,26.5,26.5', '-Wl,-undefined,dynamic_lookup', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    # introspection/vala need the host to run the just-built library, and gtk-doc
    # needs the docs toolchain; none of it reaches the image.
    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dgtk-doc=false \
      -Dintrospection=false \
      -Dvala=disabled \
      -Dtests=false \
      -Dx11=enabled \
      -Dwayland=enabled \
      -Dvisibility=false

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
    for f in $allfiles; do
      for dylib in $dylibs; do
        base=$(basename "$dylib")
        "$INSTALL_NAME_TOOL" -change "@rpath/$base" "/lib/$base" "$f" 2>/dev/null || true
        # a sibling inside the same project can be recorded by absolute install path
        # rather than @rpath (libxfce4windowingui -> libxfce4windowing), which the
        # @rpath rewrite above never matches
        "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libxfce4windowing, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
