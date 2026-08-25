{ stdenv
, lib
, meson
, ninja
, pkg-config
, darwinCrossToolchain
, nativeLd
, libSystem
, libffi
, waylandScanner
, src
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
in
stdenv.mkDerivation {
  pname = "puredarwin-wayland";
  version = "1.25.0";
  inherit src;

  nativeBuildInputs = [ meson ninja pkg-config waylandScanner ];
  buildInputs = [ libffi ];

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

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-D_DARWIN_C_SOURCE', '-fno-stack-protector', '-I${libSystem}/usr/include']
    c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    export PKG_CONFIG_PATH="${waylandScanner}/lib/pkgconfig:${libffi}/lib/pkgconfig"
    export PKG_CONFIG_PATH_FOR_BUILD="${waylandScanner}/lib/pkgconfig"
    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=both \
      -Dlibraries=true \
      -Dscanner=false \
      -Dtests=false \
      -Ddocumentation=false \
      -Ddtd_validation=false

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
    description = "Wayland client and server libraries cross-built for PureDarwin";
    homepage = "https://wayland.freedesktop.org/";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
