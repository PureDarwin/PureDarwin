{ stdenv
, lib
, meson
, ninja
, pkg-config
, python3
, gperf
, perl
, darwinCrossToolchain
, nativeLd
, libSystem
, fontconfig
, freetype
, expat
, nativeMesonTools
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  deps = [ freetype expat ];
  depPcPaths = deps;
in
stdenv.mkDerivation {
  pname = "puredarwin-fontconfig";
  version = fontconfig.version;

  src = fontconfig.src;

  nativeBuildInputs = [ meson ninja pkg-config python3 gperf perl ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .
    sed -i "/subdir('test')/d" meson.build
    sed -i "/subdir('doc')/d" meson.build
  '';

  configurePhase = ''
    runHook preConfigure

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

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,26.5,26.5', '-lexpat', '-lSystem']

[properties]
needs_exe_wrapper = true

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
      -Ddoc=disabled \
      -Ddoc-txt=disabled \
      -Ddoc-man=disabled \
      -Ddoc-pdf=disabled \
      -Ddoc-html=disabled \
      -Dnls=disabled \
      -Dtests=disabled \
      -Dtools=disabled \
      -Dcache-build=disabled \
      -Dxml-backend=expat \
      -Dfontations=disabled

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
    {
      INT="${nativeMesonTools}/bin/install_name_tool"
      for dylib in "$out"/lib/*.dylib; do
        [ -L "$dylib" ] && continue
        "$INT" -id "/usr/lib/$(basename "$dylib")" "$dylib"
      done

      mkdir -p "$out/usr/lib"
      for dylib in "$out"/lib/*.dylib; do
        [ -e "$dylib" ] || continue
        ln -sf "../../lib/$(basename "$dylib")" "$out/usr/lib/$(basename "$dylib")"
      done
    }

    perl -0pi -e 's#<cachedir>.*?</cachedir>#<cachedir>/var/cache/fontconfig</cachedir>#' \
      "$out/etc/fonts/fonts.conf"
    perl -0pi -e 's#(<description>Default configuration file</description>\n\n)#\1\t<dir>/usr/share/fonts</dir>\n#' \
      "$out/etc/fonts/fonts.conf"

    patch_string() {
      local file="$1"
      local old="$2"
      local new="$3"
      OLD="$old" NEW="$new" perl -0pi -e '
        my $old = $ENV{OLD};
        my $new = $ENV{NEW};
        die "replacement is longer than original\n" if length($new) > length($old);
        my $padded = $new . ("\0" x (length($old) - length($new)));
        s/\Q$old\E/$padded/g;
      ' "$file"
    }

    fclib="$out/lib/libfontconfig.1.dylib"
    patch_string "$fclib" "$out/etc/fonts/conf.d" "/etc/fonts/conf.d"
    patch_string "$fclib" "$out/etc/fonts" "/etc/fonts"
    patch_string "$fclib" "$out/share/fontconfig/conf.avail" "/usr/share/fontconfig/conf.avail"
    patch_string "$fclib" "$out/var/cache/fontconfig" "/var/cache/fontconfig"

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
