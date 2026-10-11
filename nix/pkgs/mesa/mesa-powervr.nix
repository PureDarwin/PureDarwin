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
, libdrmPdpvr
, mesaPvrNativeTools
  # x11 wsi over the shared guest x libraries, presents through xcb_put_image (MESA_VK_WSI_DEBUG=sw)
, withX11 ? false
, libX11 ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, libxshmfence ? null
, xorgproto ? null
, targetTriple ? "arm64-apple-darwin20.4"
, appleSdk
}:

# mesa's powervr vulkan driver alone, no gallium and no llvm, it reaches PDPowerVR through
# libdrm-pdpvr and takes its build-time shader tools from a native build of the same source
let
  targetInfo = import ../../lib/target-info.nix targetTriple;
  pythonEnv = python3.withPackages (ps: [ ps.mako ps.pyyaml ps.packaging ps.setuptools ]);
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  q = s: "'${s}'";
  commonArgs = [
    "-isysroot" sdk "-mmacosx-version-min=26.5" "-Qunused-arguments"
    "-U_FORTIFY_SOURCE" "-D_FORTIFY_SOURCE=0" "-fno-stack-protector"
    "-I${libSystem}/usr/include" "-I${lib.getDev zlib}/include" "-I@COMPAT@"
    # the per-arch driver library includes xf86drm.h without a libdrm dependency
    "-I${libdrmPdpvr}/include" "-I${libdrmPdpvr}/include/libdrm"
  ] ++ map (d: "-I${lib.getDev d}/include") xDeps;
  linkArgs = [
    "-isysroot" sdk "-mmacosx-version-min=26.5" "-fuse-ld=${nativeLd}/bin/ld" "-nostdlib"
    "-L${libSystem}/usr/lib" "-L${zlib}/lib"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-platform_version,macos,26.5,26.5" "-Wl,-fixup_chains"
  ] ++ map (d: "-L${d}/lib") xDeps;
  list = l: lib.concatMapStringsSep ", " q l;
  xDeps = lib.optionals withX11 [ libX11 libxcb libXau libXdmcp libxshmfence xorgproto ];
  xPkgConfigPath = lib.concatMapStrings (d: ":${lib.getDev d}/lib/pkgconfig:${lib.getDev d}/share/pkgconfig") xDeps;
in
stdenv.mkDerivation {
  pname = "puredarwin-mesa-powervr${lib.optionalString withX11 "-x11"}";
  inherit version src;

  nativeBuildInputs = [ meson ninja pkg-config pythonEnv bison flex glslang ];

  postPatch = ''
    patchShebangs .
    patch -p1 < ${./mesa-powervr-darwin.patch}

    # linux headers the driver includes: major() and the uapi's _BITULL, the __u types come from drm.h
    mkdir -p compat/sys compat/linux
    printf '#include <sys/types.h>\n' > compat/sys/sysmacros.h
    printf '#ifndef _BITULL\n#define _BITULL(x) (1ULL << (x))\n#endif\n' > compat/linux/const.h
    printf '#include <stdint.h>\n' > compat/linux/types.h

    # the generated entrypoint tables take weak references to every entrypoint, which mach-o can
    # only leave unresolved through dynamic lookup, installPhase checks nothing else is left
    substituteInPlace src/vulkan/meson.build \
      --replace-fail "vulkan_icd_link_args = []" \
                     "vulkan_icd_link_args = host_machine.system() == 'darwin' ? ['-Wl,-undefined,dynamic_lookup'] : []"

    # the display wsi is built for VK_KHR_display: libdrm-pdpvr answers the kms calls for PDSun60iDisplay
    for f in src/vulkan/meson.build src/vulkan/wsi/meson.build; do
      substituteInPlace $f --replace-fail "if system_has_kms_drm and not with_platform_android" \
        "if (system_has_kms_drm or host_machine.system() == 'darwin') and not with_platform_android"
    done
  '' + lib.optionalString withX11 ''
    # a server that answers QueryExtension for XFIXES but not its version request would crash every client
    substituteInPlace src/vulkan/wsi/wsi_common_x11.c \
      --replace-fail 'wsi_conn->has_xfixes = (ver_reply->major_version >= 2);' \
                     'wsi_conn->has_xfixes = ver_reply != NULL && (ver_reply->major_version >= 2);'
    # the driver only turns on surface and swapchain for display and wayland, the platforms linux always has
    substituteInPlace src/imagination/vulkan/pvr_physical_device.h \
      --replace-fail '#if defined(VK_USE_PLATFORM_DISPLAY_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR)' \
                     '#if defined(VK_USE_PLATFORM_DISPLAY_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)'
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${mesaPvrNativeTools}/bin:${nativeMesonTools}/bin:$PATH"
    export PKG_CONFIG_PATH="${libdrmPdpvr}/lib/pkgconfig:${lib.getDev zlib}/lib/pkgconfig${xPkgConfigPath}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

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
      -Dvulkan-drivers=imagination \
      -Dgallium-drivers= \
      -Dplatforms=${lib.optionalString withX11 "x11 -Dxlib-lease=disabled"} \
      -Dopengl=false \
      -Dgles1=disabled \
      -Dgles2=disabled \
      -Degl=disabled \
      -Dglx=disabled \
      -Dgbm=disabled \
      -Dglvnd=disabled \
      -Dllvm=disabled \
      -Dmesa-clc=system \
      -Dprecomp-compiler=system \
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
    lookups=$(${darwinCrossToolchain}/bin/${targetTriple}-nm -m "$out"/usr/lib/libvulkan_powervr_mesa.dylib \
      | grep "dynamically looked up" | grep -v "weak external" || true)
    if [ -n "$lookups" ]; then
      echo "non-weak symbols left to dynamic lookup:"
      echo "$lookups"
      exit 1
    fi
    for icd in "$out"/usr/share/vulkan/icd.d/*.json; do
      sed -i "s|\"library_path\": \".*/\([^/]*\.dylib\)\"|\"library_path\": \"/usr/lib/\1\"|" "$icd"
    done
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Mesa's PowerVR Vulkan driver for PDPowerVR, cross-built for PureDarwin";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
