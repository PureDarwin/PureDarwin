{ stdenv
, lib
, requireFile
, meson
, ninja
, pkg-config
, darwinCrossToolchain
, nativeLd
, nativeMesonTools
, libSystem
, cairo
, glib
, fribidi
, freetype
, harfbuzz
, jsonc
, libdrm
, pango
, pcre2
, pixman
, wayland
, waylandProtocols
, waylandScanner
, wlroots
, xkbcommon
, xcb ? null
, xcbWm ? null
, src
  # Matches wlroots' withXwayland; a Wayland-only image has no xcb at all.
, withXwayland ? true
, targetTriple ? "x86_64-apple-darwin20.4"
}:

assert withXwayland -> (xcb != null && xcbWm != null);

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
  deps = [ cairo fribidi freetype glib harfbuzz jsonc libdrm pango pcre2 pixman wayland waylandProtocols wlroots xkbcommon ]
    ++ lib.optionals withXwayland [ xcb xcbWm ];
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
  pname = "puredarwin-sway${lib.optionalString (!withXwayland) "-nox"}";
  version = "1.12";
  inherit src;

  nativeBuildInputs = [ meson ninja pkg-config waylandScanner ];
  buildInputs = deps;

  postPatch = ''
    # PureDarwin provides realtime APIs from libSystem, not a separate librt.
    substituteInPlace meson.build \
      --replace "rt = cc.find_library('rt')" "rt = cc.find_library('rt', required: false)"
  '';

  configurePhase = ''
    runHook preConfigure
    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
    c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-D_DARWIN_C_SOURCE', '-fno-stack-protector', '-I${libSystem}/usr/include', '-I${waylandProtocols}/include', '-I$PWD/puredarwin']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    export PATH="${waylandScanner}/bin:${nativeMesonTools}/bin:$PATH"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" deps}:${lib.makeSearchPath "share/pkgconfig" deps}:${lib.makeSearchPath "usr/lib/pkgconfig" deps}:${lib.makeSearchPath "usr/share/pkgconfig" deps}:${waylandScanner}/lib/pkgconfig:${waylandScanner}/share/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Dauto_features=disabled \
      -Ddefault-wallpaper=false \
      -Dswaybar=true \
      -Dswaynag=false \
      -Dtray=disabled \
      -Dgdk-pixbuf=disabled \
      -Dman-pages=disabled \
      -Dzsh-completions=false \
      -Dbash-completions=false \
      -Dfish-completions=false
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
    # The generated sample config embeds Nix store paths.  Keep the guest
    # config self-contained; PureDarwin does not install Sway's wallpaper or
    # config.d tree.
    sed -i \
      -e '/^# Default wallpaper /c\\# PureDarwin does not select a default wallpaper.' \
      -e '/^output .* bg /d' \
      -e '/^    position top$/i\\    swaybar_command \/bin\/swaybar' \
      -e '/^include .*config\.d\/\*$/c\\# PureDarwin has no system config.d overrides.' \
      "$out/etc/sway/config"
    cat > $out/bin/puredarwin-sway <<'EOF'
#!/bin/sh
:
: "''${WLR_BACKENDS:=puredarwin}"
: "''${WLR_RENDERER:=pixman}"
export WLR_BACKENDS WLR_RENDERER
if [ -f /etc/sway/config ]; then
    exec sway -c /etc/sway/config "$@"
fi
exec sway "$@"
EOF
    chmod +x $out/bin/puredarwin-sway
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Sway Wayland compositor for PureDarwin's wlroots backend";
    homepage = "https://swaywm.org/";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
