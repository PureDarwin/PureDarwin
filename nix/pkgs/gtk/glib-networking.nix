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
, glibNetworking
, glib
, gnutls
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  deps = [ glib gnutls ];
  depPcPaths = map lib.getDev deps;
in
stdenv.mkDerivation {
  pname = "puredarwin-glib-networking";
  inherit (glibNetworking) version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .

    # gnome.post_install() runs the cross-built (Darwin) gio-querymodules on the
    # Linux builder, which cannot execute. It only writes giomodule.cache; GIO
    # scans the module directory and loads modules without it.
    sed -i '/gnome\.post_install(/d' meson.build
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
cpp = '${darwinCrossToolchain}/bin/${targetTriple}-clang++'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
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
      -Dgnutls=enabled \
      -Dopenssl=disabled \
      -Dlibproxy=disabled \
      -Dgnome_proxy=disabled \
      -Dinstalled_tests=false

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

    # Meson names a shared_module ".dylib" on darwin, so the module
    # would be installed under a name g_io_modules_scan_all_in_directory never
    # considers. Rename to match what GIO actually scans for
    moddir="$out/lib/gio/modules"
    if [ -d "$moddir" ]; then
      for mod in "$moddir"/*.dylib; do
        [ -e "$mod" ] || continue
        mv "$mod" "''${mod%.dylib}.so"
      done
      for mod in "$moddir"/*.so; do
        [ -e "$mod" ] || continue
        "$INSTALL_NAME_TOOL" -id "/lib/gio/modules/$(basename "$mod")" "$mod" 2>/dev/null || true
      done
    fi

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "glib-networking (GnuTLS GIO TLS backend), cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
