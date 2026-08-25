# SPDX-License-Identifier: MIT

{ stdenv
, lib
, meson
, ninja
, pkg-config
, darwinCrossToolchain
, nativeLd
, nativeMesonTools
, libSystem
, iokit
, iokitHeaders
, pdgopSource
, pdVirglShim
, libdrm
, pixman
, wayland
, waylandProtocols
, waylandScanner
, xkbcommon
, xcb ? null
, xcbWm ? null
, xwayland ? null
, pdsurface
, src
  # Wayland-only images drop X11 entirely, so xwayland (and the xcb libraries
  # it needs) must come out of both the build and the runtime closure.
, withXwayland ? true
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

assert withXwayland -> (xcb != null && xcbWm != null && xwayland != null);

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
  deps = [ libdrm pixman wayland waylandProtocols xkbcommon pdsurface ]
    ++ lib.optionals withXwayland [ xcb xcbWm xwayland ];
in
stdenv.mkDerivation {
  pname = "puredarwin-wlroots${lib.optionalString (!withXwayland) "-nox"}";
  version = "0.20.1";
  inherit src;

  nativeBuildInputs = [ meson ninja pkg-config waylandScanner ];
  buildInputs = deps;

  postPatch = ''
    # PureDarwin exposes realtime APIs through libSystem rather than librt.
    substituteInPlace meson.build \
      --replace "rt = cc.find_library('rt')" "rt = cc.find_library('rt', required: false)"

  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cp ${pdgopSource}/PDGOP.c backend/puredarwin/puredarwin-pdgop.c
    cp ${pdgopSource}/include/PDGOP.h backend/puredarwin/PDGOP.h

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
    c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-D_DARWIN_C_SOURCE', '-DPUREDARWIN', '-fno-stack-protector', '-I${waylandProtocols}/include', '-I${iokitHeaders}/include', '-I${pdVirglShim}/include', '-I${pdsurface}/usr/include']
    c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${iokit}/usr/lib', '-L${pdVirglShim}/usr/lib', '-L${pdsurface}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylib_file,/usr/lib/libpd_virgl_shim.dylib:${pdVirglShim}/usr/lib/libpd_virgl_shim.dylib', '-Wl,-dylib_file,/usr/lib/libPDSurface.dylib:${pdsurface}/usr/lib/libPDSurface.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-lIOKitCF', '-lpd_virgl_shim', '-lPDSurface', '-lSystem']

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
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" deps}:${lib.makeSearchPath "share/pkgconfig" deps}:${lib.makeSearchPath "usr/lib/pkgconfig" deps}:${lib.makeSearchPath "usr/share/pkgconfig" deps}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=both \
      -Dauto_features=disabled \
      -Dexamples=false \
      -Dxwayland=${if withXwayland then "enabled" else "disabled"} \
      -Dsession=disabled \
      -Dcolor-management=disabled \
      -Dlibliftoff=disabled \
      -Dxcb-errors=disabled \
      -Dbackends=puredarwin \
      -Drenderers=[] \
      -Dallocators=[]

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
    description = "Wayland compositor library for PureDarwin";
    homepage = "https://gitlab.freedesktop.org/wlroots/wlroots";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
