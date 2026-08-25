{ stdenv
, lib
, meson
, ninja
, pkg-config
, python3
, darwinCrossToolchain
, nativeLd
, xwayland
, libSystem
, pixman
, xorgproto
, xtrans
, xlib
, xcb
, libXfont2
, libxkbfile
, libXau
, libXdmcp
, libXext
, libXfixes
, libXrender
, libXrandr
, libXres
, libXcomposite
, libXdamage
, libxshmfence
, zlib
, freetype2
, libfontenc
, xvfbZlib
, libxcvt
, wayland
, waylandProtocols
, waylandScanner
, xkbcommon
, xkbcomp
, xkeyboardConfig
, openssl
, mesaGlHeaders
, mesa
, libepoxy
, libdrm
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
  deps = [
    pixman xorgproto xtrans xlib xcb libXfont2 libxkbfile libXau libXdmcp
    libXext libXfixes libXrender libXrandr libXres libXcomposite libXdamage libxshmfence
    zlib freetype2 libfontenc xvfbZlib libxcvt wayland waylandProtocols openssl mesaGlHeaders mesa libepoxy
    xkbcommon libdrm
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-xwayland";
  version = xwayland.version;
  src = xwayland.src;

  nativeBuildInputs = [ meson ninja pkg-config python3 waylandScanner ];
  buildInputs = deps;

  postPatch = ''
    # PureDarwin's Wayland path does not expose Linux DRM/GBM yet. Keep the
    # generic glamor/EGL sources buildable while leaving the GBM compositor
    # integration optional until the native buffer allocator is available.
    substituteInPlace meson.build \
      --replace "dependency('libdrm', version: libdrm_req, required: true)" \
                "dependency('libdrm', version: libdrm_req, required: false)"

    mkdir -p compat/linux
    cat > compat/linux/input.h <<EOF
#ifndef PUREDARWIN_LINUX_INPUT_H
#define PUREDARWIN_LINUX_INPUT_H
#define KEY_LEFTCTRL 29
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTCTRL 97
#define KEY_RIGHTSHIFT 54
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE 0x113
#endif
EOF
    cat > compat/xf86drm.h <<EOF
#ifndef PUREDARWIN_XF86DRM_H
#define PUREDARWIN_XF86DRM_H
typedef struct _drmDevice drmDevice;
#endif
EOF

    # Every window-sized X pixmap goes through os_create_anonymous_file(). With
    # no memfd_create() that falls back to a real file in XDG_RUNTIME_DIR, so
    # each drawable becomes a MAP_SHARED mapping of a file on a journaling
    # filesystem and every pixel drawn heads for writeback. POSIX shared memory
    # is Darwin's spelling of the same anonymous, kernel-backed object.
    substituteInPlace hw/xwayland/xwayland-shm.c \
      --replace '#ifdef HAVE_MEMFD_CREATE' \
'#if defined(__APPLE__)
    {
        static unsigned pd_shm_serial;
        char pd_shm_name[64];

        snprintf(pd_shm_name, sizeof(pd_shm_name), "/xwayland-%u-%u",
                 (unsigned) getpid(), pd_shm_serial++);
        fd = shm_open(pd_shm_name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0)
            shm_unlink(pd_shm_name);
    }
    if (fd >= 0) { } else
#elif defined(HAVE_MEMFD_CREATE)'
  '';

  configurePhase = ''
    runHook preConfigure
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = ['${darwinCrossToolchain}/bin/${targetTriple}-clang', '-isysroot', '$DARWIN_SDK_ROOT']
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-D_XSERVER64=1', '-D_DARWIN_C_SOURCE', '-fno-stack-protector', '-I${libSystem}/usr/include', '-I${mesa}/usr/include', '-I$PWD/compat']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${libXau}/lib', '-L${libXdmcp}/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylinker_install_name,/usr/lib/dyld', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem', '-lXau', '-lXdmcp', '${freetype2}/lib/libfreetype.dylib', '${libfontenc}/lib/libfontenc.a', '${xvfbZlib}/lib/libz.a']

[host_machine]
system = 'darwin'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    # XWayland only needs the DRI headers when glamor is disabled. The
    # mesa-gl-headers output intentionally has no pkg-config file, so provide
    # the small metadata file the server's include checks expect.
    mkdir -p dri-pkgconfig/lib/pkgconfig
    cat > dri-pkgconfig/lib/pkgconfig/dri.pc <<EOF
prefix=${mesaGlHeaders}
includedir=''${prefix}/include
Name: dri
Description: Direct Rendering Infrastructure headers
Version: 1.0.0
Cflags: -I''${includedir}
EOF
    export PKG_CONFIG_PATH="$PWD/dri-pkgconfig/lib/pkgconfig:${lib.makeSearchPath "lib/pkgconfig" (deps ++ [ waylandScanner ])}:${lib.makeSearchPath "share/pkgconfig" (deps ++ [ waylandScanner ])}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    # These paths are consumed by XWayland at runtime. The corresponding
    # packages are flattened into the target image, so store paths would be
    # invalid there.
    # Glamor renders every X operation through GL and reads the result back for
    # the Wayland commit.  With no GPU driver behind it that is a full software
    # GL pipeline plus a per-frame readback, far slower than the plain software
    # path - and pointless anyway with glx, dri3 and drm all off, which leave it
    # nothing to accelerate.
    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=/usr \
      --bindir=bin \
      --libdir=lib \
      --buildtype=release \
      -Dxcsecurity=true \
      -Dlibunwind=false \
      -Dglamor=false \
      -Dglx=false \
      -Ddri3=false \
      -Ddrm=false \
      -Dxdmcp=false \
      -Dsecure-rpc=false \
      -Ddefault_font_path=/usr/share/fonts \
      -Dxkb_bin_dir=/usr/bin \
      -Dxkb_dir=/usr/share/X11/xkb \
      -Dxkb_output_dir=/tmp/xkb/compiled
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    DESTDIR=$out ninja -C build install
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "X11 server running as a Wayland client on PureDarwin";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
