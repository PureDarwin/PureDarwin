{ stdenv
, lib
, requireFile
, fetchurl
, meson
, ninja
, pkg-config
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, mesa
, libX11
, libXext
, libxcb
, libXau
, libXdmcp
, xorgproto
, xtrans
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  targetInfo = import ../lib/target-info.nix targetTriple;

  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary) not in your Nix store.
      Register it with: nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };

  incs = [
    "-I${mesa}/usr/include"
    "-I${lib.getDev libX11}/include"
    "-I${lib.getDev xorgproto}/include"
  ];
  xPkgConfigPath = lib.concatMapStringsSep ":"
    (p: "${p}/lib/pkgconfig:${p}/usr/lib/pkgconfig:${p}/share/pkgconfig:${p}/usr/share/pkgconfig")
    [ mesa libX11 libXext libxcb libXau libXdmcp xorgproto xtrans ];
in
stdenv.mkDerivation {
  pname = "puredarwin-mesa-demos";
  version = "9.0.0";

  src = fetchurl {
    url = "https://archive.mesa3d.org/demos/mesa-demos-9.0.0.tar.xz";
    hash = "sha256-MEaj0mp7BRr3690lel8jv+sWDK1u2VIynN/x6fHtSWs=";
  };

  nativeBuildInputs = [ meson ninja pkg-config ];

  postPatch = ''
    patchShebangs .
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PKG_CONFIG_PATH="${xPkgConfigPath}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
cpp = '${darwinCrossToolchain}/bin/${targetTriple}-clang++'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (s: "'${s}'") incs}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${mesa}/usr/lib', '-L${libX11}/lib', '-L${libXext}/lib', '-L${libxcb}/lib', '-L${libXau}/lib', '-L${libXdmcp}/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-fixup_chains', '-lGL', '-lX11', '-lXext', '-lxcb', '-lXau', '-lXdmcp', '-lSystem']

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
      --prefix=$out/usr \
      --buildtype=release \
      -Dgles1=disabled \
      -Dgles2=disabled \
      -Degl=disabled \
      -Dvulkan=disabled \
      -Dosmesa=disabled \
      -Dwayland=disabled \
      -Dlibdrm=disabled \
      -Dglut=disabled \
      -Dx11=enabled

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
    # Re-root any @rpath GL reference at /usr/lib for the image.
    INT="${nativeMesonTools}/bin/install_name_tool"
    for f in $(find "$out" -type f -perm -u+x 2>/dev/null); do
      "$INT" -change "@rpath/libGL.1.dylib" "/usr/lib/libGL.1.dylib" "$f" 2>/dev/null || true
    done
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "mesa-demos cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
