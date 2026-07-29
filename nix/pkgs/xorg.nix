{ stdenv
, lib
, requireFile
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
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  targetInfo = import ../lib/target-info.nix targetTriple;

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
  pname = "puredarwin-xorg";
  version = "21.1.24";

  src = xorg-server.src;

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    python3
  ];

  buildInputs = xDeps;

  postPatch = ''
    patchShebangs .
    patch -p2 < ${../../patches/xorg-xvfb-local-sha1.patch}
    patch -p1 < ${../../patches/xorg-xvfb-present-card32.patch}
    sed -i "s/    subdir('test')/    # subdir('test') - removed for PureDarwin cross build/" meson.build

    # meson's cross cc.sizeof('unsigned long') returns -1 with this cross
    # file, so _XSERVER64 never lands in dix-config.h and XID/Mask/Atom stay
    # 8 bytes (the server then misparses 32-bit protocol value lists, and
    # drivers built against the installed SDK headers would disagree with
    # the server's struct layouts). Target is x86_64-only: force it.
    sed -i "s/if cc.sizeof('unsigned long') == 8/if true/" include/meson.build

    sed -i '/static const ExtensionModule extensionModules\[\] = {/,+1 s|#ifdef XF86VIDMODE|#if 0 /* PureDarwin: VidMode disabled - crashes in init, unused */|' \
      hw/xfree86/common/xf86Extensions.c
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
pkgconfig = '${pkg-config}/bin/pkg-config'

[built-in options]
# _XSERVER64: see xvfb.nix -- meson's cross sizeof check fails, so define
# it by hand or XID/Mask/Atom are 8 bytes and value-list parsing breaks.
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-D_XSERVER64=1', '-I${libSystem}/usr/include']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylinker_install_name,/usr/lib/dyld', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem', '${freetype2}/lib/libfreetype.a', '${libfontenc}/lib/libfontenc.a', '${xvfbZlib}/lib/libz.a']

[host_machine]
system = 'darwin'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'
EOF

    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" xPkgConfigDeps}:${lib.makeSearchPath "share/pkgconfig" xPkgConfigDeps}"
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
      -Dglx=false \
      -Dxdmcp=false \
      -Dxdm-auth-1=false \
      -Dsecure-rpc=false \
      -Dipv6=false \
      -Dlisten_tcp=true \
      -Dlisten_unix=false \
      -Dlisten_local=false \
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
      -Dmitshm=false \
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

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
