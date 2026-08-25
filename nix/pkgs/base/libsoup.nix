{ stdenv
, lib
, meson
, ninja
, pkg-config
, python3
, glibNative
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, libsoup
, glib
, pcre2
, libffi
, zlib
, libiconv
, sqlite
, libpsl
, nghttp2
, libxml2
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  deps = [ glib pcre2 libffi zlib libiconv sqlite libpsl nghttp2 libxml2 ];
  depPcPaths = map lib.getDev deps;
in
stdenv.mkDerivation {
  pname = "puredarwin-libsoup";
  inherit (libsoup) version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 glibNative ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:${glibNative}/bin:$PATH"

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool'
glib-compile-resources = '${glibNative}/bin/glib-compile-resources'
glib-compile-schemas = '${glibNative}/bin/glib-compile-schemas'
glib-mkenums = '${glibNative}/bin/glib-mkenums'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem']

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
      -Dintrospection=disabled \
      -Dvapi=disabled \
      -Ddocs=disabled \
      -Ddoc_tests=false \
      -Dtests=false \
      -Dinstalled_tests=false \
      -Dsysprof=disabled \
      -Dgssapi=disabled \
      -Dntlm=disabled \
      -Dbrotli=disabled \
      -Dtls_check=false

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
        "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libsoup 3, cross-built for PureDarwin (WebKitGTK's network backend)";
    platforms = platforms.linux;
  };
}
