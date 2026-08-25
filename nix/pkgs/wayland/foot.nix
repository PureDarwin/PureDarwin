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
, wayland
, waylandProtocols
, waylandScanner
, xkbcommon
, fontconfig
, freetype
, pixman
, harfbuzz
, libutf8proc
, tllist
, fcft
, epollShim
, expat
, zlib
, libpng
, libiconv
, glib
, pcre2
, libffi
, ncurses
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# foot is a Wayland-native terminal emulator - no X11 anywhere in it, which is
# why it is the terminal for the Wayland-only image. It is written against
# Linux's epoll, so it builds against the kqueue shim rather than the system
# headers; foot's own meson already looks for that under the name epoll-shim,
# as the BSD ports provide it.

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
  deps = [
    wayland waylandProtocols xkbcommon fontconfig freetype pixman harfbuzz
    libutf8proc tllist fcft epollShim expat zlib libpng libiconv glib pcre2
    libffi
  ];
in
stdenv.mkDerivation rec {
  pname = "puredarwin-foot";
  version = "1.27.0";

  src = fetchurl {
    url = "https://codeberg.org/dnkl/foot/archive/${version}.tar.gz";
    sha256 = "1qxpf0xwa7srhr8fv6hq7n9058hfs67xflryhycknwkv5nnpr4gm";
  };

  nativeBuildInputs = [ meson ninja pkg-config waylandScanner ];
  buildInputs = deps;

  postPatch = ''
    # foot guards on __STDC_ISO_10646__, which glibc defines and Darwin does
    # not - even though Darwin's wchar_t is UCS-4 just like the BSDs already
    # exempted here. Same reasoning, same exemption.
    substituteInPlace char32.c \
      --replace-fail '&& !defined(__FreeBSD__) && !defined(__OpenBSD__)' \
                '&& !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__APPLE__)'

    # foot already switches pthread_setname_np per OS; Darwin's takes only the
    # name and renames the calling thread, which is exactly how foot calls it.
    # The inner name is parenthesised so the macro does not expand into itself.
    substituteInPlace render.c \
      --replace-fail '#elif defined(__NetBSD__)' \
                '#elif defined(__APPLE__)
 #define pthread_setname_np(thread, name) (pthread_setname_np)(name)
#elif defined(__NetBSD__)'

    # XNU refuses "extended" ioctls on a pty master until the slave side has
    # been opened (tty_dev.c's fix_7070978 / TS_IOCTL_NOT_OK). foot configures
    # the master immediately after posix_openpt, long before the child opens
    # the slave in slave.c, so both of these return ENOTTY on Darwin.
    #
    # Neither is fatal here:
    #  - the TIOCSWINSZ is a 24x80 placeholder; render.c sets the real size
    #    once the Wayland surface is configured.
    #  - F_SETFL still applies O_NONBLOCK. kern_descrip.c updates f_flag before
    #    calling fo_ioctl(FIONBIO) and does not roll it back when that fails,
    #    and vn_read turns FNONBLOCK into IO_NDELAY, which ptcread honours.
    #
    # Only ENOTTY is tolerated, so a genuine failure is still fatal.
    substituteInPlace terminal.c \
      --replace-fail '&(struct winsize){.ws_row = 24, .ws_col = 80}) < 0)' \
                '&(struct winsize){.ws_row = 24, .ws_col = 80}) < 0 && errno != ENOTTY)' \
      --replace-fail 'fcntl(ptmx, F_SETFL, ptmx_flags | O_NONBLOCK) < 0)' \
                '(fcntl(ptmx, F_SETFL, ptmx_flags | O_NONBLOCK) < 0 && errno != ENOTTY))'

    # foot names a data-source callback send(), which shadows POSIX send(2)
    # once <sys/socket.h> is in scope - it is not on Linux, where selection.c
    # never pulls that header in.
    substituteInPlace selection.c \
      --replace-fail 'send(void *data, struct wl_data_source *wl_data_source' \
                'data_source_send(void *data, struct wl_data_source *wl_data_source' \
      --replace-fail '.send = &send,' '.send = &data_source_send,'

    # Darwin cannot report a socket's address family through getsockopt: there
    # is no SO_DOMAIN. Drop that one check from the passed-fd validation and
    # keep the SO_ACCEPTCONN/SO_TYPE ones, which do work.
    substituteInPlace server.c \
      --replace-fail 'int const socket_options[] = { SO_DOMAIN, SO_ACCEPTCONN, SO_TYPE };' \
                'int const socket_options[] = { SO_ACCEPTCONN, SO_TYPE };' \
      --replace-fail 'int const socket_options_values[] = { AF_UNIX, NON_ZERO_OPT, SOCK_STREAM};' \
                'int const socket_options_values[] = { NON_ZERO_OPT, SOCK_STREAM};' \
      --replace-fail 'char const * const socket_options_names[] = { "SO_DOMAIN", "SO_ACCEPTCONN", "SO_TYPE" };' \
                'char const * const socket_options_names[] = { "SO_ACCEPTCONN", "SO_TYPE" };'
  '';

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
wayland-scanner = '${waylandScanner}/bin/wayland-scanner'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-D_DARWIN_C_SOURCE', '-D_USE_EXTENDED_LOCALES_', '-include', 'xlocale.h', '-fno-stack-protector', '-I${./pd-libc-compat}', '-I${./pd-compat-include}', '-I${epollShim}/include', ${lib.concatMapStringsSep ", " (d: "'-I${lib.getDev d}/include'") deps}, '-I${lib.getDev freetype}/include/freetype2', '-I${fcft}/usr/include']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${epollShim}/usr/lib', '-L${fcft}/usr/lib', ${lib.concatMapStringsSep ", " (d: "'-L${d}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylib_file,/usr/lib/libepoll-shim.dylib:${epollShim}/usr/lib/libepoll-shim.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-fixup_chains', '-lepoll-shim', '-lSystem']

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
    export PKG_CONFIG_PATH="${fcft}/usr/lib/pkgconfig:${lib.makeSearchPath "lib/pkgconfig" deps}:${lib.makeSearchPath "share/pkgconfig" deps}:${lib.makeSearchPath "usr/lib/pkgconfig" deps}:${waylandScanner}/lib/pkgconfig:${waylandScanner}/share/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddocs=disabled \
      -Dtests=false \
      -Dime=false \
      -Dgrapheme-clustering=enabled \
      -Dterminfo=disabled \
      -Dutmp-backend=none \
      -Dsystemd-units-dir=

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
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Wayland-native terminal emulator";
    homepage = "https://codeberg.org/dnkl/foot";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
