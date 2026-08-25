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
, libX11 ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, libXext ? null
, libXi ? null
, libXrender ? null
, libXrandr ? null
, libXfixes ? null
, libXcursor ? null
, xorgproto ? null
, libpng
, wayland ? null
, waylandProtocols ? null
, waylandScanner ? null
, xkbcommon ? null
, mesa ? null
  # Wayland-only images have no X11 libraries at all, so GDK's X11 backend has
  # to be compiled out rather than merely unused.
, withX11 ? true
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

assert withX11 -> lib.all (d: d != null) [
  libX11 libxcb libXau libXdmcp libXext libXi libXrender libXrandr libXfixes
  libXcursor xorgproto
];

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  waylandEnabled = wayland != null && waylandProtocols != null
    && waylandScanner != null && xkbcommon != null && mesa != null;

  # WL_EGL_PLATFORM picks the wl_display-based EGLNativeDisplayType in Mesa's
  # eglplatform.h. Without it the __APPLE__ branch wins and the type is an int,
  # so GDK's cast of a wl_display to it is a pointer-to-int truncation. Safe to
  # set for the whole build: GDK's X11 backend uses GLX, never EGL.
  waylandCArgs = lib.optionalString waylandEnabled
    ", '-I${mesa}/usr/include', '-I${./../wayland/pd-compat-include}', '-DWL_EGL_PLATFORM=1'";

  deps = [
    glib pcre2 libffi zlib libiconv
    cairo cairoGobject pixman
    pango fribidi harfbuzz freetype2 fontconfig expat
    gdkPixbuf
    libepoxy
    atspi2Core
    dbus
    libpng
  ] ++ lib.optionals withX11 [
    libX11 libxcb libXau libXdmcp libXext libXi libXrender libXrandr libXfixes
    libXcursor xorgproto
  ] ++ lib.optionals waylandEnabled [ wayland waylandProtocols xkbcommon ];
  depPcPaths = map lib.getDev deps;
in
stdenv.mkDerivation {
  pname = "puredarwin-gtk3${lib.optionalString (!withX11) "-nox"}";
  inherit (gtk3) version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 glibNative ]
    ++ lib.optional waylandEnabled waylandScanner;
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
# Upstream forces X11 and Wayland off on Darwin because it assumes Quartz is
# the only display server there. PureDarwin has neither, so drop the special
# case entirely and let the backend options decide; Quartz is never wanted.
new = """quartz_enabled = false"""
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
${lib.optionalString waylandEnabled ''    export PATH="${waylandScanner}/bin:$PATH"
''}
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
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
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-DCAIRO_HAS_GOBJECT_FUNCTIONS=1', '-Ddngettext(Domain,Singular,Plural,N)=((N)==1?(Singular):(Plural))', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}${waylandCArgs}]
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
      -Dx11_backend=${lib.boolToString withX11} \
      -Dwayland_backend=${lib.boolToString waylandEnabled} \
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
          # a sibling inside the same project can be recorded by absolute install
          # path rather than @rpath (libxfce4windowingui -> libxfce4windowing), which
          # the @rpath rewrite above never matches
          "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
          # siblings inside one project can be recorded by absolute install path
          # rather than @rpath (libxfce4windowingui -> libxfce4windowing), which the
          # @rpath rewrite above never matches
          "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
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
