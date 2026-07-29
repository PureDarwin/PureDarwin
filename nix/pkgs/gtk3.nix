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
, gtk3
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
, xorgproto
, libpng
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  targetInfo = import ../lib/target-info.nix targetTriple;

  deps = [
    glib pcre2 libffi zlib libiconv
    cairo cairoGobject pixman
    pango fribidi harfbuzz freetype2 fontconfig expat
    gdkPixbuf
    libepoxy
    atspi2Core
    dbus
    libX11 libxcb libXau libXdmcp libXext libXi libXrender libXrandr libXfixes libXcursor
    xorgproto libpng
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
  pname = "puredarwin-gtk3";
  inherit (gtk3) version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 glibNative ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .

    python3 - <<'PYEOF'
import re
with open('meson.build') as f:
    content = f.read()

old = """if os_darwin
  wayland_enabled = false
  x11_enabled = false
else
  quartz_enabled = false
endif"""
new = """if os_darwin
  wayland_enabled = false
  quartz_enabled = false
else
  quartz_enabled = false
endif"""
assert old in content, "os_darwin backend-forcing block not found"
content = content.replace(old, new)

old_nls = "cdata.set('ENABLE_NLS', 1)"
assert old_nls in content, "ENABLE_NLS line not found"
content = content.replace(old_nls, "# ENABLE_NLS intentionally not set (no gettext port)")

for line in ["subdir('docs/tools')", "subdir('docs/reference')"]:
    assert line in content, f"{line!r} not found"
    content = content.replace(line, f"# {line} (removed - doc tooling unused)")

with open('meson.build', 'w') as f:
    f.write(content)
PYEOF

    # PureDarwin's current dlsym/dlopen(NULL) path can report GTK2's
    # gtk_progress_get_type marker as present even though no loaded image
    # exports it.
    sed -i '/_gtk_module_has_mixed_deps (NULL)/,+1d' gtk/gtkmain.c

    # Keep GtkEntry robust if the X11 backend cannot construct the named
    # invisible cursor for any reason.
    python3 - <<'PYEOF'
from pathlib import Path

path = Path('gtk/gtkentry.c')
content = path.read_text()
old = """static void
set_invisible_cursor (GdkWindow *window)
{
  GdkCursor *cursor;

  cursor = gdk_cursor_new_from_name (gdk_window_get_display (window), "none");
  gdk_window_set_cursor (window, cursor);
  g_object_unref (cursor);
}
"""
new = """static void
set_invisible_cursor (GdkWindow *window)
{
  GdkDisplay *display;
  GdkCursor *cursor;

  display = gdk_window_get_display (window);
  cursor = gdk_cursor_new_from_name (display, "none");
  if (cursor == NULL)
    cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);

  if (cursor != NULL)
    {
      gdk_window_set_cursor (window, cursor);
      g_object_unref (cursor);
    }
}
"""
assert old in content, "GtkEntry invisible cursor helper not found"
path.write_text(content.replace(old, new))
PYEOF
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"

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
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-DCAIRO_HAS_GOBJECT_FUNCTIONS=1', '-Ddngettext(Domain,Singular,Plural,N)=((N)==1?(Singular):(Plural))', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
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
      -Dx11_backend=true \
      -Dwayland_backend=false \
      -Dbroadway_backend=false \
      -Dwin32_backend=false \
      -Dquartz_backend=false \
      -Dxinerama=no \
      -Dcloudproviders=false \
      -Dprofiler=false \
      -Dtracker3=false \
      -Dcolord=no \
      -Dprint_backends=file \
      -Dgtk_doc=false \
      -Dman=false \
      -Dintrospection=false \
      -Ddemos=false \
      -Dexamples=false \
      -Dtests=false \
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
          "$INSTALL_NAME_TOOL" -change "$dep/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
        done
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "GTK3, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
