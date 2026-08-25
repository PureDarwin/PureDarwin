{ stdenv
, lib
, meson
, ninja
, pkg-config
, python3
, darwinCrossToolchain
, nativeLd
, libSystem
, xorg-server
, pixman
, xorgproto
, xtrans
, libXfont2
, libxkbfile
, libXau
, libXdmcp
, zlib
, freetype2
, libfontenc
, xvfbZlib
, libxcvt
, mesa
, libX11
, libxcb
, libXext
, libXfixes
, mesaGlHeaders
, glHeaders
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  xDeps = [
    pixman
    xorgproto
    xtrans
    libXfont2
    libxkbfile
    libXau
    libXdmcp
    zlib
    libxcvt
  ];
  xPkgConfigDeps = map lib.getDev xDeps;

  # GLX pulls in Mesa (gl.pc, dri.pc) plus everything gl.pc/glx.pc name in
  # Requires.private - pkg-config walks those even for a shared link, so all
  # of x11, xext, xfixes, x11-xcb, xcb, xcb-glx and xcb-dri2 have to resolve.
  glxDeps = [ mesa libX11 libxcb libXext libXfixes glHeaders ];
  glxPkgConfigDeps = map lib.getDev glxDeps;
in
stdenv.mkDerivation {
  pname = "puredarwin-xorg";
  version = "21.1.24";

  src = xorg-server.src;

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    python3
  ];

  buildInputs = xDeps ++ [ glHeaders ];

  postPatch = ''
    patchShebangs .
    patch -p2 < ${../../../patches/xorg-xvfb-local-sha1.patch}
    patch -p1 < ${../../../patches/xorg-xvfb-present-card32.patch}
    sed -i "s/    subdir('test')/    # subdir('test') - removed for PureDarwin cross build/" meson.build

    # meson's cross cc.sizeof('unsigned long') returns -1 with this cross
    # file, so _XSERVER64 never lands in dix-config.h and XID/Mask/Atom stay
    # 8 bytes (the server then misparses 32-bit protocol value lists, and
    # drivers built against the installed SDK headers would disagree with
    # the server's struct layouts). Target is x86_64-only: force it.
    sed -i "s/if cc.sizeof('unsigned long') == 8/if true/" include/meson.build

    sed -i '/static const ExtensionModule extensionModules\[\] = {/,+1 s|#ifdef XF86VIDMODE|#if 0 /* PureDarwin: VidMode disabled - crashes in init, unused */|' \
      hw/xfree86/common/xf86Extensions.c

    patch -p1 < ${../../../patches/xorg-mitshm-puredarwin.patch}

    # DRI_DRIVER_PATH is taken from dri.pc's dridriverdir, which points into
    # the Mesa store path. The server only ever runs on the guest, where the
    # drivers land at /usr/lib/dri.
    sed -i "s|dri_dep.get_pkgconfig_variable('dridriverdir')|'/usr/lib/dri'|" \
      include/meson.build

    # glxProbeDriver() builds "<dir>/<name>_dri.so"; Mesa names its loadable
    # modules .dylib here, so the dlopen never found swrast.
    sed -i 's|%.\*s/%s_dri\.so|%.*s/%s_dri.dylib|' glx/glxdricommon.c

    # The module loaded for AIGLX is Mesa's "dril" compatibility stub, whose
    # createNewContext returns NULL by design (indirect GLX has been dead
    # upstream since Mesa 24.1). Xorg does not check for that and tears the
    # server down through a null driContext
    sed -i 's|^    return \&context->base;$|    if (context->driContext == NULL) { free(context); return NULL; }\n    return \&context->base;|' \
      glx/glxdriswrast.c
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cat > puredarwin-cross.ini <<EOF
[binaries]
# -isysroot travels with the compiler, not in c_args: meson runs its
# cc.has_header_symbol()/has_header() probes without the [built-in options]
# c_args, so with the sysroot only there every probe failed to find even
# <sys/socket.h>. That silently mis-detected SCM_RIGHTS (losing xtrans fd
# passing, which caps MIT-SHM at 1.1 and blocks picom), PTHREAD_MUTEX_RECURSIVE
# and strndup.
c = ['${darwinCrossToolchain}/bin/${targetTriple}-clang', '-isysroot', '$DARWIN_SDK_ROOT']
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkgconfig = '${pkg-config}/bin/pkg-config'

[built-in options]
# _XSERVER64: see xvfb.nix -- meson's cross sizeof check fails, so define
# it by hand or XID/Mask/Atom are 8 bytes and value-list parsing breaks.
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-D_XSERVER64=1', '-I${libSystem}/usr/include', '-I${glHeaders}/include']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylinker_install_name,/usr/lib/dyld', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem', '${freetype2}/lib/libfreetype.dylib', '${libfontenc}/lib/libfontenc.a', '${xvfbZlib}/lib/libz.a']

[host_machine]
system = 'darwin'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'
EOF

    # Mesa's DRI headers have no pkg-config file, but Xorg's include checks
    # require the small metadata package even when DRI1/2/3 are disabled.
    mkdir -p dri-pkgconfig/lib/pkgconfig
    cat > dri-pkgconfig/lib/pkgconfig/dri.pc <<EOF
prefix=${mesaGlHeaders}
includedir=${mesaGlHeaders}/include
Name: dri
Description: Direct Rendering Infrastructure headers
Version: 1.0.0
    Cflags: -I${mesaGlHeaders}/include
EOF
    cat > dri-pkgconfig/lib/pkgconfig/gl.pc <<EOF
prefix=${mesa}
exec_prefix=${mesa}
libdir=${mesa}/usr/lib
includedir=${glHeaders}/include
Name: gl
Description: PureDarwin Mesa OpenGL library
Version: 1.2
Libs: -L${mesa}/usr/lib -lGL
Cflags: -I${glHeaders}/include
EOF
    # Mesa keeps its .pc files under usr/lib/pkgconfig, not lib/pkgconfig.
    export PKG_CONFIG_PATH="$PWD/dri-pkgconfig/lib/pkgconfig:${lib.makeSearchPath "lib/pkgconfig" xPkgConfigDeps}:${lib.makeSearchPath "share/pkgconfig" xPkgConfigDeps}:${lib.makeSearchPath "lib/pkgconfig" glxPkgConfigDeps}:${lib.makeSearchPath "usr/lib/pkgconfig" glxPkgConfigDeps}:${lib.makeSearchPath "share/pkgconfig" glxPkgConfigDeps}"
    # input_thread=false: the threaded input path spins on Darwin - ospoll's
    # poll() backend returns immediately and InputThreadDoWork busy-loops. It was
    # off by accident until now (its PTHREAD_MUTEX_RECURSIVE probe was failing
    # along with every other meson check, see toolchain.nix), so this keeps the
    # behaviour the server has always actually had. Input still works; it is
    # handled on the main thread as before.
    # Keep Unix/local transports enabled; MIT-SHM 1.2 passes descriptors over
    # the local socket with SCM_RIGHTS.
    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=/usr \
      --bindir=bin \
      --libdir=lib \
      --buildtype=debug \
      -Dxorg=true \
      -Dxvfb=false \
      -Dfallback_input_driver= \
      -Dxephyr=false \
      -Dxnest=false \
      -Dxwin=false \
      -Dxquartz=false \
      -Dglamor=false \
      -Dglx=true \
      -Dxdmcp=false \
      -Dxdm-auth-1=false \
      -Dsecure-rpc=false \
      -Dipv6=false \
      -Dlisten_tcp=true \
      -Dlisten_unix=true \
      -Dlisten_local=true \
      -Dpciaccess=false \
      -Dint10=false \
      -Dvgahw=false \
      -Ddga=false \
      -Dudev=false \
      -Dudev_kms=false \
      -Dsystemd_logind=false \
      -Dhal=false \
      -Ddrm=false \
      -Ddri1=false \
      -Ddri2=false \
      -Ddri3=false \
      -Dmitshm=true \
      -Dinput_thread=false \
      -Dxselinux=false \
      -Ddocs=false \
      -Ddevel-docs=false \
      -Ddocs-pdf=false \
      -Dsha1=libc \
      -Ddefault_font_path=/usr/share/fonts

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    DESTDIR=$out meson install -C build --no-rebuild
    find build -maxdepth 2 -name '*-config.h' -exec cp {} $out/usr/include/xorg/ \;
    mkdir -p $out/bin
    ln -sf ../usr/bin/Xorg $out/bin/Xorg

    # Xorg's loader builds candidate module filenames as "lib%s.so"/"%s.so"
    # (see LoaderOpen callers), but meson names a shared_module ".dylib" on
    # darwin.
    for mod in "$out"/usr/lib/xorg/modules/*.dylib \
               "$out"/usr/lib/xorg/modules/*/*.dylib; do
      [ -e "$mod" ] || continue
      ln -sf "$(basename "$mod")" "''${mod%.dylib}.so"
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
