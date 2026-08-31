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
, libcxxDylib
, libcxxabiDylib
, mesa
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# Mesa dropped GLU from the main tree long ago, but real OpenGL.framework still
# exports it and ports keep including <OpenGL/glu.h>. libnurbs is C++, hence the
# libc++ link.

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
in
stdenv.mkDerivation {
  pname = "puredarwin-glu";
  version = "9.0.3";

  src = fetchurl {
    url = "https://archive.mesa3d.org/glu/glu-9.0.3.tar.xz";
    hash = "sha256-vUP+EvN0sRkusV/iDkX/RWubwmq1fw7ukZ+Wyg+KMw8=";
  };

  nativeBuildInputs = [ meson ninja pkg-config ];

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${mesa}/usr/lib/pkgconfig:${mesa}/usr/share/pkgconfig"
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
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', '-I${mesa}/usr/include']
cpp_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-nostdinc++', '-I${libcxxDylib}/usr/include/c++/v1', '-I${libSystem}/usr/include', '-I${mesa}/usr/include']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${mesa}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-fixup_chains', '-lSystem']
cpp_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${libcxxDylib}/usr/lib', '-L${libcxxabiDylib}/usr/lib', '-L${mesa}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-fixup_chains', '-lc++', '-lc++abi', '-lSystem']

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
      --prefix=/usr \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dgl_provider=gl

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    DESTDIR="$out" ninja -C build install
    # meson names it libGLU.1.dylib but records the build-tree path; the
    # framework re-exports it, so it has to answer to its runtime name.
    ${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool \
      -id /usr/lib/libGLU.1.dylib "$out/usr/lib/libGLU.1.dylib"
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Mesa GLU, cross-built for PureDarwin's OpenGL.framework";
    homepage = "https://gitlab.freedesktop.org/mesa/glu";
    platforms = platforms.unix;
  };
}
