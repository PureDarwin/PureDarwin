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
, at-spi2-core
, glib
, libxml2
, dbus
, pcre2
, libffi
, zlib
, libiconv
}:

let
  deps = [ glib libxml2 dbus pcre2 libffi zlib libiconv ];
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
  pname = "puredarwin-at-spi2-core";
  inherit (at-spi2-core) version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .
    # No gettext/libintl port exists yet; this header is included but the
    # _()/N_() macros it defines are never actually called in this file.
    sed -i '/^#include <libintl.h>$/d' atspi/atspi-gmain.c
    sed -i "/subdir('tests')/d" meson.build
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
c = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang'
cpp = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang++'
ar = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar'
strip = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-install_name_tool'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-undefined,dynamic_lookup', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
needs_exe_wrapper = true
EOF

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dx11=disabled \
      -Dintrospection=disabled \
      -Ddocs=false \
      -Dgtk2_atk_adaptor=false \
      -Ddbus_glib=disabled \
      -Duse_systemd=false \
      -Ddefault_bus=dbus-daemon \
      -Ddbus_daemon=/bin/dbus-daemon \
      -Datk_only=false

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
    sed -i 's/ -DG_LOG_DOMAIN="dbind"//' "$out/lib/pkgconfig/atspi-2.pc"

    INSTALL_NAME_TOOL="${nativeMesonTools}/bin/install_name_tool"
    dylibs=$(find "$out/lib" -maxdepth 1 -name "*.dylib" -not -type l)
    for dylib in $dylibs; do
      base=$(basename "$dylib")
      "$INSTALL_NAME_TOOL" -id "/lib/$base" "$dylib"
    done
    allfiles=$(
      [ ! -d "$out/bin" ] || find "$out/bin" -type f
      [ ! -d "$out/lib" ] || find "$out/lib" -type f
      [ ! -d "$out/libexec" ] || find "$out/libexec" -type f
    )
    load_paths="$out ${lib.concatStringsSep " " deps}"
    for f in $allfiles; do
      for dep in $load_paths; do
        for dylib in "$dep"/lib/*.dylib; do
          [ -e "$dylib" ] || continue
          base=$(basename "$dylib")
          "$INSTALL_NAME_TOOL" -change "@rpath/$base" "/lib/$base" "$f" 2>/dev/null || true
          "$INSTALL_NAME_TOOL" -change "$dep/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
        done
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "AT-SPI2 core (libatspi, ATK, atk-bridge-2.0, registryd), cross-built for PureDarwin (X11/introspection/systemd disabled)";
    platforms = platforms.linux;
  };
}
