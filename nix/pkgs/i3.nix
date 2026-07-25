{ stdenv
, lib
, requireFile
, meson
, ninja
, pkg-config
, perl
, bash
, darwinCrossToolchain
, nativeLd
, libSystem
, i3
, xorgproto
, startup-notification
, libX11
, libxcb
, libxcb-util
, libxcb-keysyms
, libxcb-wm
, libxcb-render-util
, libxcb-image
, libxcb-cursor
, xcb-util-xrm
, xkbcommon
, yajl
, pcre2
, cairo
, pango
, glib
, fribidi
, harfbuzz
, libev
, libiconv
, zlib
, libffi
, pixman
, fontconfig
, freetype
, expat
, libXau
, libXdmcp
, libpng
, libXext
, libXrender
}:

let
  deps = [
    startup-notification
    xorgproto
    libX11
    libxcb
    libxcb-util
    libxcb-keysyms
    libxcb-wm
    libxcb-render-util
    libxcb-image
    libxcb-cursor
    xcb-util-xrm
    xkbcommon
    yajl
    pcre2
    cairo
    pango
    glib
    fribidi
    harfbuzz
    libev
    libiconv
    zlib
    libffi
    pixman
    fontconfig
    freetype
    expat
    libXau
    libXdmcp
    libpng
    libXext
    libXrender
  ];
  depPcPaths = deps;
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
  pname = "puredarwin-i3";
  version = i3.version;

  src = i3.src;

  nativeBuildInputs = [ meson ninja pkg-config perl bash ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .

    perl -0pi -e "s/libsn_dep = dependency\\('libstartup-notification-1\\.0', method: 'pkg-config'\\)/libsn_dep = dependency('libstartup-notification-1.0', method: 'pkg-config')\\nx11_xcb_dep = dependency('x11-xcb', method: 'pkg-config')/" meson.build
    perl -0pi -e "s/xcb_util_cursor_dep = dependency\\('xcb-cursor', method: 'pkg-config'\\)/xcb_util_cursor_dep = dependency('xcb-cursor', method: 'pkg-config')\\nxcb_image_dep = dependency('xcb-image', method: 'pkg-config')\\nxcb_render_util_dep = dependency('xcb-renderutil', method: 'pkg-config')/" meson.build
    perl -0pi -e "s/  libsn_dep,\\n/  libsn_dep,\\n  x11_xcb_dep,\\n/" meson.build
    perl -0pi -e "s/  xcb_util_cursor_dep,\\n/  xcb_util_cursor_dep,\\n  xcb_image_dep,\\n  xcb_render_util_dep,\\n/" meson.build
    grep -q "x11_xcb_dep = dependency" meson.build
    grep -q "xcb_image_dep = dependency" meson.build
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang'
ar = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar'
strip = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylinker_install_name,/usr/lib/dyld', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem', '${libXau}/lib/libXau.a', '${libXdmcp}/lib/libXdmcp.a']

[properties]
needs_exe_wrapper = true
pkg_config_libdir = [${lib.concatMapStringsSep ", " (dep: "'${dep}/lib/pkgconfig'") depPcPaths}, ${lib.concatMapStringsSep ", " (dep: "'${dep}/share/pkgconfig'") depPcPaths}]

[host_machine]
system = 'darwin'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
EOF

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --wrap-mode=nofallback \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddocs=false \
      -Dmans=false

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
    for config in "$out/etc/i3/config" "$out/etc/i3/config.keycodes"; do
      if [ -f "$config" ]; then
        sed -i \
          -e 's#exec i3-sensible-terminal#exec /bin/xterm#g' \
          -e 's#exec --no-startup-id i3-sensible-terminal#exec --no-startup-id /bin/xterm#g' \
          -e 's#^set \$mod Mod1#set $mod Mod4#' \
          -e 's#bindsym Mod1#bindsym Mod4#g' \
          -e '/exec --no-startup-id dex /d' \
          -e '/exec --no-startup-id xss-lock /d' \
          -e '/exec --no-startup-id nm-applet/d' \
          "$config"
      fi
    done
    for script in i3-sensible-editor i3-sensible-pager i3-sensible-terminal; do
      if [ -f "$out/bin/$script" ]; then
        sed -i '1s|^#!.*|#!/bin/sh|' "$out/bin/$script"
      fi
    done

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

    patch_string "$out/bin/i3" "$out/etc/i3/config" "/etc/i3/config"
    patch_string "$out/bin/i3" "$out/etc/xdg" "/etc/xdg"
    patch_string "$out/bin/i3-config-wizard" "$out/etc/i3/config.keycodes" "/etc/i3/config.keycodes"
    patch_string "$out/bin/i3-config-wizard" "$out/etc/i3/config" "/etc/i3/config"
    patch_string "$out/bin/i3-config-wizard" "$out/etc/xdg" "/etc/xdg"

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
