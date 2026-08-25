{ stdenv
, lib
, meson
, ninja
, pkg-config
, bison
, darwinCrossToolchain
, nativeLd
, libSystem
, xkbcomp
, xorgproto
, libX11
, libxkbfile
, libXau
, libXdmcp
, libxcb
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  xDeps = [ xorgproto libX11 libxkbfile libXau libXdmcp libxcb ];
  xPkgConfigDeps = map lib.getDev xDeps;
in
stdenv.mkDerivation {
  pname = "puredarwin-xkbcomp";
  version = xkbcomp.version or "0";

  src = xkbcomp.src;

  nativeBuildInputs = [ meson ninja pkg-config bison ];
  buildInputs = xDeps;

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkgconfig = '${pkg-config}/bin/pkg-config'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include']
# libX11.pc's real transitive dep (libxcb) sits under "Libs.private:", which
# meson's default (non-static) pkgconfig lookup skips -- same as libXfont2's
# freetype/fontenc/zlib issue in xvfb.nix. Append the archive directly.
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylinker_install_name,/usr/lib/dyld', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem', '${libxcb}/lib/libxcb.a', '${libXau}/lib/libXau.a', '${libXdmcp}/lib/libXdmcp.a']

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
      --buildtype=release

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/bin
    cp build/xkbcomp $out/usr/bin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
