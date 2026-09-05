{ stdenv
, lib
, fetchurl
, meson
, ninja
, pkg-config
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, fontconfig
, freetype
, pixman
, harfbuzz
, libutf8proc
, tllist
, expat
, zlib
, libpng
, libiconv
  # harfbuzz.pc has "Requires: freetype2, glib-2.0", so glib and its own
  # dependencies have to be resolvable or pkg-config cannot emit harfbuzz cflags.
, glib
, pcre2
, libffi
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# fcft is foot's font-loading and glyph-rasterising library: fontconfig for
# lookup, FreeType for rasterisation, HarfBuzz for shaping. No windowing
# dependency, so nothing here needs X11 or Wayland.

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
  deps = [ fontconfig freetype pixman harfbuzz libutf8proc tllist expat zlib
           libpng libiconv glib pcre2 libffi ];
in
stdenv.mkDerivation rec {
  pname = "puredarwin-fcft";
  version = "3.3.3";

  src = fetchurl {
    url = "https://codeberg.org/dnkl/fcft/archive/${version}.tar.gz";
    sha256 = "0sihl89vnlijpfw1nzzhfx81ki1p8fwbhrc5dirj6dzlk6jz9h5h";
  };

  nativeBuildInputs = [ meson ninja pkg-config ];
  buildInputs = deps;

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-D_DARWIN_C_SOURCE', '-D_USE_EXTENDED_LOCALES_', '-include', 'xlocale.h', '-fno-stack-protector', '-I${./pd-libc-compat}', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (d: "'-I${lib.getDev d}/include'") deps}, '-I${lib.getDev freetype}/include/freetype2']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (d: "'-L${d}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,26.5,26.5', '-Wl,-fixup_chains', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    export PATH="${nativeMesonTools}/bin:$PATH"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" deps}:${lib.makeSearchPath "share/pkgconfig" deps}:${lib.makeSearchPath "usr/lib/pkgconfig" deps}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=/usr \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dsvg-backend=none \
      -Dgrapheme-shaping=enabled \
      -Drun-shaping=enabled \
      -Dexamples=false \
      -Ddocs=disabled

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
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Font loading and glyph rasterisation library used by foot";
    homepage = "https://codeberg.org/dnkl/fcft";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
