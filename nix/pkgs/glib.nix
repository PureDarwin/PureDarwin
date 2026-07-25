{ stdenv
, lib
, requireFile
, meson
, ninja
, pkg-config
, python3
, perl
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, glib
, pcre2
, libffi
, zlib
, libiconv
}:

let
  deps = [ pcre2 libffi zlib libiconv ];
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
  pname = "puredarwin-glib";
  version = glib.version;

  src = glib.src;

  nativeBuildInputs = [ meson ninja pkg-config python3 perl ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .
    sed -i "/subdir('girepository')/d" meson.build
    sed -i "/subdir('fuzzing')/d" meson.build
    sed -i "s/if host_system == 'darwin' and subsystem == 'macos'/if false/" meson.build
    sed -i "s/glib_conf.set('ENABLE_NLS', 1)/if get_option('nls').enabled()\\n  glib_conf.set('ENABLE_NLS', 1)\\nendif/" meson.build
    sed -i "/# First check in libc, fallback to libintl/,/glib_conf.set('HAVE_BIND_TEXTDOMAIN_CODESET', have_bind_textdomain_codeset)/c\\libintl_deps = []\\nhave_bind_textdomain_codeset = false\\nglib_conf.set('HAVE_BIND_TEXTDOMAIN_CODESET', have_bind_textdomain_codeset)" meson.build
    sed -i "s/#include <libintl.h>/#ifdef ENABLE_NLS\\n#include <libintl.h>\\n#endif/" glib/ggettext.c
    perl -0pi -e 's/#include <libintl\.h>\n#include <string\.h>.*?#define NC_\(Context, String\) \(String\)/#include <string.h>\n\n#ifdef ENABLE_NLS\n#include <libintl.h>\n#define  _(String) gettext (String)\n#define Q_(String) g_dpgettext (NULL, String, 0)\n#define N_(String) (String)\n#define C_(Context,String) g_dpgettext (NULL, Context "\\004" String, strlen (Context) + 1)\n#define NC_(Context, String) (String)\n#else\n#define  _(String) (String)\n#define Q_(String) (String)\n#define N_(String) (String)\n#define C_(Context,String) (String)\n#define NC_(Context, String) (String)\n#endif/s' glib/gi18n.h
    perl -0pi -e 's/#include <libintl\.h>\n#include <string\.h>\n\n#ifndef GETTEXT_PACKAGE\n#error You must define GETTEXT_PACKAGE before including gi18n-lib\.h\.  Did you forget to include config\.h\?\n#endif\n\n#define  _\(String\) \(\(char \*\) g_dgettext \(GETTEXT_PACKAGE, String\)\)\n#define Q_\(String\) g_dpgettext \(GETTEXT_PACKAGE, String, 0\)\n#define N_\(String\) \(String\)\n#define C_\(Context,String\) g_dpgettext \(GETTEXT_PACKAGE, Context "\\004" String, strlen \(Context\) \+ 1\)\n#define NC_\(Context, String\) \(String\)/#include <string.h>\n\n#ifndef GETTEXT_PACKAGE\n#error You must define GETTEXT_PACKAGE before including gi18n-lib.h.  Did you forget to include config.h?\n#endif\n\n#ifdef ENABLE_NLS\n#include <libintl.h>\n#define  _(String) ((char *) g_dgettext (GETTEXT_PACKAGE, String))\n#define Q_(String) g_dpgettext (GETTEXT_PACKAGE, String, 0)\n#define N_(String) (String)\n#define C_(Context,String) g_dpgettext (GETTEXT_PACKAGE, Context "\\004" String, strlen (Context) + 1)\n#define NC_(Context, String) (String)\n#else\n#define  _(String) (String)\n#define Q_(String) (String)\n#define N_(String) (String)\n#define C_(Context,String) (String)\n#define NC_(Context, String) (String)\n#endif/s' glib/gi18n-lib.h
    perl -0pi -e 's/#define NC_\(Context, String\) \(String\)\n#endif/#define NC_(Context, String) (String)\n#define textdomain(String) ((String) ? (String) : "messages")\n#define gettext(String) (String)\n#define dgettext(Domain,String) (String)\n#define dcgettext(Domain,String,Category) (String)\n#define bindtextdomain(Domain,Directory) (Domain)\n#define bind_textdomain_codeset(Domain,Codeset) ((void) 0)\n#endif/g' glib/gi18n.h glib/gi18n-lib.h
    perl -0pi -e 's@/\* Common code \{\{\{2 \*/\n#else\n#error No _g_get_unix_mounts\(\) implementation for system\n#endif@/* PureDarwin {{{2 */\n#elif defined (__APPLE__)\n\nstatic char *\nget_mtab_monitor_file (void)\n{\n  return NULL;\n}\n\nstatic GUnixMountEntry **\n_g_unix_mounts_get_from_file (const char *table_path,\n                              uint64_t   *time_read_out,\n                              size_t     *n_entries_out)\n{\n  if (time_read_out != NULL)\n    *time_read_out = 0;\n  if (n_entries_out != NULL)\n    *n_entries_out = 0;\n\n  return NULL;\n}\n\nstatic GList *\n_g_get_unix_mounts (void)\n{\n  return NULL;\n}\n\n/* Common code {{{2 */\n#else\n#error No _g_get_unix_mounts() implementation for system\n#endif@' gio/gunixmounts.c
    perl -0pi -e 's@/\* Common code \{\{\{2 \*/\n#else\n#error No g_get_mount_table\(\) implementation for system\n#endif@/* PureDarwin {{{2 */\n#elif defined (__APPLE__)\n\nstatic GList *\n_g_get_unix_mount_points (void)\n{\n  return NULL;\n}\n\nstatic GUnixMountPoint **\n_g_unix_mount_points_get_from_file (const char *table_path,\n                                    uint64_t   *time_read_out,\n                                    size_t     *n_points_out)\n{\n  if (time_read_out != NULL)\n    *time_read_out = 0;\n  if (n_points_out != NULL)\n    *n_points_out = 0;\n\n  return NULL;\n}\n\n/* Common code {{{2 */\n#else\n#error No g_get_mount_table() implementation for system\n#endif@' gio/gunixmounts.c
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
objc = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang'
ar = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar'
strip = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-install_name_tool'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
cpp_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
objc_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem']
cpp_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem']
objc_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem']

[properties]
needs_exe_wrapper = true
have_c99_vsnprintf = true
have_c99_snprintf = true
have_unix98_printf = true
va_val_copy = true
growing_stack = false
have_strlcpy = true
have_proc_self_cmdline = false

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
EOF

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dtests=false \
      -Dinstalled_tests=false \
      -Ddocumentation=false \
      -Dman-pages=disabled \
      -Ddtrace=disabled \
      -Dsystemtap=disabled \
      -Dsysprof=disabled \
      -Dintrospection=disabled \
      -Dselinux=disabled \
      -Dlibmount=disabled \
      -Dxattr=false \
      -Dnls=disabled \
      -Dlibelf=disabled \
      -Dglib_debug=disabled \
      -Dglib_assert=false \
      -Dglib_checks=false \
      -Dforce_posix_threads=true

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
    patchShebangs $out/bin

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
    platforms = platforms.linux;
  };
}
