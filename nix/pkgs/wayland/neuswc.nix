# SPDX-License-Identifier: MIT

{ stdenv
, lib
, meson
, ninja
, pkg-config
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, fontconfig
, freetype
, iokit
, iokitHeaders
, neuwld
, pixman
, wayland
, waylandProtocols
, waylandScanner
, xkbcommon
, src
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
  deps = [ fontconfig freetype neuwld pixman wayland waylandProtocols xkbcommon ];
in
stdenv.mkDerivation {
  pname = "puredarwin-neuswc";
  version = "0-unstable-2026-09-04";
  inherit src;

  nativeBuildInputs = [ meson ninja pkg-config waylandScanner ];
  buildInputs = deps;

  configurePhase = ''
    runHook preConfigure
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${waylandScanner}/bin:$PATH"
    export PKG_CONFIG_PATH="${fontconfig}/lib/pkgconfig:${freetype}/lib/pkgconfig:${neuwld}/lib/pkgconfig:${pixman}/lib/pkgconfig:${wayland}/lib/pkgconfig:${waylandProtocols}/share/pkgconfig:${xkbcommon}/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-D_DARWIN_C_SOURCE', '-fno-stack-protector', '-I${libSystem}/usr/include', '-I${iokitHeaders}/include']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${corefoundation}/usr/lib', '-L${iokit}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylib_file,/usr/lib/libCoreFoundation.dylib:${corefoundation}/usr/lib/libCoreFoundation.dylib', '-Wl,-platform_version,macos,26.5,26.5', '-lIOKitCF', '-lCoreFoundation', '-lSystem']

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
      -Ddefault_library=static \
      -Dauto_features=disabled \
      -Dvideo=fb \
      -Dfb=darwin \
      -Dinput=darwin \
      -Dxwayland=disabled \
      -Dudev=disabled \
      -Dextra=true \
      -Dexample=true
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
    install -m755 build/example/wm $out/bin/neuswc
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Simple Wayland compositor library for Darwin";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
