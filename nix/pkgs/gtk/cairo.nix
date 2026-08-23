{ stdenv
, lib
, requireFile
, meson
, ninja
, pkg-config
, python3
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, cairo
, pixman
, zlib
, xorgproto ? null
, libX11 ? null
, libXext ? null
, libXrender ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, freetype
, fontconfig
, expat
, libpng
  # Wayland-only images have no X11 at all, so the xlib/xcb surface backends
  # have to be compiled out - otherwise libcairo bakes in an libX11 path.
, withX11 ? true
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  deps = [ pixman zlib freetype fontconfig expat libpng ]
    ++ lib.optionals withX11 [ xorgproto libX11 libXext libXrender libxcb libXau libXdmcp ];
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
  pname = "puredarwin-cairo${lib.optionalString (!withX11) "-nox"}";
  version = cairo.version;

  src = cairo.src;

  nativeBuildInputs = [ meson ninja pkg-config python3 ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs version.py
    sed -i "/subdir('util')/d" meson.build
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-DHAVE_UINT64_T=1', '-DHAVE___UINT128_T=1', '-DHAVE_XRENDERCREATESOLIDFILL=1', '-DHAVE_XRENDERCREATELINEARGRADIENT=1', '-DHAVE_XRENDERCREATERADIALGRADIENT=1', '-DHAVE_XRENDERCREATECONICALGRADIENT=1', '-DFC_RGBA_UNKNOWN=0', '-DFC_RGBA_RGB=1', '-DFC_RGBA_BGR=2', '-DFC_RGBA_VRGB=3', '-DFC_RGBA_VBGR=4', '-DFC_RGBA_NONE=5', '-DFC_HINT_NONE=0', '-DFC_HINT_SLIGHT=1', '-DFC_HINT_MEDIUM=2', '-DFC_HINT_FULL=3', '-DFC_LCD_NONE=0', '-DFC_LCD_DEFAULT=1', '-DFC_LCD_LIGHT=2', '-DFC_LCD_LEGACY=3', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, ${lib.optionalString withX11 "'-Wl,-force_load,${libXau}/lib/libXau.a', '-Wl,-force_load,${libXdmcp}/lib/libXdmcp.a', "}'-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem']

[host_machine]
system = 'darwin'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'
EOF

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dtests=disabled \
      -Dgtk_doc=false \
      -Dgtk2-utils=disabled \
      -Dglib=disabled \
      -Dlzo=disabled \
      -Dpng=enabled \
      -Dquartz=disabled \
      -Dtee=disabled \
      -Dfontconfig=enabled \
      -Dfreetype=enabled \
      -Dxlib=${if withX11 then "enabled" else "disabled"} \
      -Dxcb=${if withX11 then "enabled" else "disabled"} \
      -Dxlib-xcb=disabled \
      -Dzlib=enabled

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
    platforms = platforms.linux;
  };
}
