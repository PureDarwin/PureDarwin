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
  pname = "puredarwin-xvfb";
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

    # See xorg.nix: force _XSERVER64 into dix-config.h; meson's cross
    # sizeof('unsigned long') check fails and leaves XID/Mask/Atom 8-byte.
    sed -i "s/if cc.sizeof('unsigned long') == 8/if true/" include/meson.build
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
# _XSERVER64: meson's cross cc.sizeof('unsigned long') returns -1 here, so
# include/meson.build never sets it and XID/Mask/Atom stay 8-byte unsigned
# long -- the server then walks 32-bit protocol value lists with 8-byte
# reads (BadValue on any CWEventMask, garbage Atoms, ...). Define it by hand.
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-D_XSERVER64=1', '-I${libSystem}/usr/include']
# xfont2.pc only lists -lXfont2 under "Libs:"; its real transitive statics
# (freetype2/fontenc/zlib) sit under "Libs.private:", which meson's default
# (non-static) pkgconfig dependency() lookup doesn't pull in. Rather than
# force --static globally (breaks libxkbfile's own Requires.private: x11,
# which we don't have a .pc for), just append those archives directly here.
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
      -Dxorg=false \
      -Dxvfb=true \
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
    ninja -C build hw/vfb/Xvfb
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp build/hw/vfb/Xvfb $out/bin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
