{ stdenv
, lib
, src
, version
, meson
, ninja
, pkg-config
, python3
, bison
, flex
, glslang
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, zlib
, libdrmVirtgpu
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
  # x11 wsi over the shared guest x libraries, no dri3 on PD, so frames go through the software path
, withX11 ? false
, libX11 ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, libxshmfence ? null
, xorgproto ? null
  # wayland wsi (wl_shm through the same software path), for CAMetalLayer on PD's Wayland windows
, withWayland ? false
, wayland ? null
, waylandProtocols ? null
, waylandScanner ? null
}:

# mesa's venus vulkan driver alone: vulkan commands are serialized to the host's virglrenderer and run on
# the host gpu. it reaches IOVirtIOGPU's venus client through libdrm-virtgpu
let
  targetInfo = import ../../lib/target-info.nix targetTriple;
  pythonEnv = python3.withPackages (ps: [ ps.mako ps.pyyaml ps.packaging ps.setuptools ]);
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  q = s: "'${s}'";
  commonArgs = [
    "-isysroot" sdk "-mmacosx-version-min=26.5" "-Qunused-arguments"
    "-U_FORTIFY_SOURCE" "-D_FORTIFY_SOURCE=0" "-fno-stack-protector"
    "-I${libSystem}/usr/include" "-I${lib.getDev zlib}/include" "-I@COMPAT@"
  ] ++ map (d: "-I${lib.getDev d}/include") xDeps;
  linkArgs = [
    "-isysroot" sdk "-mmacosx-version-min=26.5" "-fuse-ld=${nativeLd}/bin/ld" "-nostdlib"
    "-L${libSystem}/usr/lib" "-L${zlib}/lib"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-platform_version,macos,26.5,26.5" "-Wl,-fixup_chains"
  ] ++ map (d: "-L${d}/lib") xDeps;
  xDeps = lib.optionals withX11 [ libX11 libxcb libXau libXdmcp libxshmfence xorgproto ]
    ++ lib.optionals withWayland [ wayland waylandProtocols ];
  wsiPlatforms = lib.concatStringsSep "," (lib.optional withX11 "x11" ++ lib.optional withWayland "wayland");
  xPkgConfigPath = lib.concatMapStrings (d: ":${lib.getDev d}/lib/pkgconfig:${lib.getDev d}/share/pkgconfig") xDeps;
  list = l: lib.concatMapStringsSep ", " q l;
in
stdenv.mkDerivation {
  pname = "puredarwin-mesa-venus${lib.optionalString withX11 "-x11"}${lib.optionalString withWayland "-wayland"}";
  inherit version src;

  nativeBuildInputs = [ meson ninja pkg-config pythonEnv bison flex glslang ] ++ lib.optional withWayland waylandScanner;

  postPatch = ''
    patchShebangs .

    # linux headers the driver includes: the uapi's _BITULL and __u types
    mkdir -p compat/linux
    printf '#ifndef _BITULL\n#define _BITULL(x) (1ULL << (x))\n#endif\n' > compat/linux/const.h
    printf '#include <stdint.h>\n' > compat/linux/types.h

    # no kms on darwin, but the virtgpu backend talks to IOVirtIOGPU through a libdrm shim
    python3 - <<'PY'
    import re
    p = 'meson.build'
    s = open(p).read()
    old = "if system_has_kms_drm\n  dep_libdrm = dependency("
    assert s.count(old) == 1
    s = s.replace(old, "if system_has_kms_drm or (host_machine.system() == 'darwin' and with_virtio_vk)\n  dep_libdrm = dependency(")
    s = s.replace("required : with_dri2 or with_dri or with_gbm,", "required : with_dri2 or with_dri or with_gbm or not system_has_kms_drm,", 1)
    open(p, 'w').write(s)

    p = 'src/virtio/vulkan/meson.build'
    s = open(p).read()
    old = "if system_has_kms_drm\n  libvn_files += files('vn_renderer_virtgpu.c')"
    assert s.count(old) == 1
    s = s.replace(old, "if system_has_kms_drm or host_machine.system() == 'darwin'\n  libvn_files += files('vn_renderer_virtgpu.c')")
    open(p, 'w').write(s)

    # mesa maps blobs with mmap on the drm fd, on PD that goes through the user client
    p = 'src/virtio/vulkan/vn_renderer_virtgpu.c'
    s = open(p).read()
    old = '#include <xf86drm.h>\n'
    assert s.count(old) == 1
    s = s.replace(old, old + '#ifdef __APPLE__\n#include "pd_virtgpu_drm.h"\n#define mmap pd_drm_mmap\n#define munmap pd_drm_munmap\n#endif\n')
    open(p, 'w').write(s)

    # darwin has no SOCK_CLOEXEC, the vtest socket is only used with VN_DEBUG=vtest
    p = 'src/virtio/vulkan/vn_renderer_vtest.c'
    s = open(p).read()
    s = s.replace('SOCK_STREAM | SOCK_CLOEXEC', 'SOCK_STREAM')
    open(p, 'w').write(s)
    PY

    # the generated entrypoint tables take weak references to every entrypoint, which mach-o can
    # only leave unresolved through dynamic lookup, installPhase checks nothing else is left
    substituteInPlace src/vulkan/meson.build \
      --replace-fail "vulkan_icd_link_args = []" \
                     "vulkan_icd_link_args = host_machine.system() == 'darwin' ? ['-Wl,-undefined,dynamic_lookup'] : []"
  '' + lib.optionalString withX11 ''
    # a server that answers QueryExtension for XFIXES but not its version request would crash every client
    substituteInPlace src/vulkan/wsi/wsi_common_x11.c \
      --replace-fail 'wsi_conn->has_xfixes = (ver_reply->major_version >= 2);' \
                     'wsi_conn->has_xfixes = ver_reply != NULL && (ver_reply->major_version >= 2);'
    # no dma-buf export or dri3 here: the cpu image path is the only way to present
    substituteInPlace src/virtio/vulkan/vn_wsi.c \
      --replace-fail 'const bool use_sw_device =' 'const bool use_sw_device = true ||'
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"
    export PKG_CONFIG_PATH="${libdrmVirtgpu}/lib/pkgconfig:${lib.getDev zlib}/lib/pkgconfig${xPkgConfigPath}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
  '' + lib.optionalString withWayland ''
    # wayland-scanner is a native dependency, found through the build machine's pkg-config
    export PKG_CONFIG_PATH_FOR_BUILD="${waylandScanner}/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR_FOR_BUILD="${waylandScanner}/lib/pkgconfig"
  '' + ''

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
cpp = '${darwinCrossToolchain}/bin/${targetTriple}-clang++'
objc = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool'

[built-in options]
c_args = [${list commonArgs}]
cpp_args = [${list commonArgs}, '-nostdinc++', '-I${libcxxDylib}/usr/include/c++/v1']
objc_args = [${list commonArgs}]
c_link_args = [${list linkArgs}, '-lSystem']
objc_link_args = [${list linkArgs}, '-lSystem']
cpp_link_args = [${list linkArgs}, '-L${libcxxDylib}/usr/lib', '-L${libcxxabiDylib}/usr/lib', '-lc++', '-lc++abi', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF
    sed -i "s|@COMPAT@|$PWD/compat|g" puredarwin-cross.ini

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out/usr \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dvulkan-drivers=virtio \
      -Dgallium-drivers= \
      -Dplatforms=${wsiPlatforms} ${lib.optionalString withX11 "-Dxlib-lease=disabled"} \
      -Dopengl=false \
      -Dgles1=disabled \
      -Dgles2=disabled \
      -Degl=disabled \
      -Dglx=disabled \
      -Dgbm=disabled \
      -Dglvnd=disabled \
      -Dllvm=disabled \
      -Dshader-cache=disabled \
      -Dxmlconfig=disabled \
      -Dvalgrind=disabled \
      -Dlibunwind=disabled \
      -Dlmsensors=disabled \
      -Dzstd=disabled \
      -Ddisplay-info=disabled \
      -Dbuild-tests=false

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  # install names go to the image's /usr/lib, and the icd manifest names the dylib there
  installPhase = ''
    runHook preInstall
    ninja -C build install

    INSTALL_NAME_TOOL="${nativeMesonTools}/bin/install_name_tool"
    for dylib in $(find "$out/usr/lib" -maxdepth 1 -name "*.dylib" -not -type l); do
      "$INSTALL_NAME_TOOL" -id "/usr/lib/$(basename "$dylib")" "$dylib"
    done
    lookups=$(${darwinCrossToolchain}/bin/${targetTriple}-nm -m "$out"/usr/lib/libvulkan_virtio.dylib \
      | grep "dynamically looked up" | grep -v "weak external" || true)
    if [ -n "$lookups" ]; then
      echo "non-weak symbols left to dynamic lookup:"
      echo "$lookups"
      exit 1
    fi
    for icd in "$out"/usr/share/vulkan/icd.d/*.json; do
      sed -i "s|\"library_path\": \".*/\([^/]*\.dylib\)\"|\"library_path\": \"/usr/lib/\1\"|" "$icd"
    done
    # libwayland-client is @rpath-named and lives at /lib on the image
    ${lib.optionalString withWayland ''
    for dylib in ${wayland}/lib/*.dylib; do
      "$INSTALL_NAME_TOOL" -change "@rpath/$(basename $dylib)" "/lib/$(basename $dylib)" "$out"/usr/lib/libvulkan_virtio.dylib 2>/dev/null || true
    done
    ''}
    # the shim travels with the driver
    cp ${libdrmVirtgpu}/usr/lib/libdrm_virtgpu.dylib $out/usr/lib/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Mesa's venus (virtio-gpu) Vulkan driver over IOVirtIOGPU, cross-built for PureDarwin";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
