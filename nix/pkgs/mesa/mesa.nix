{ stdenv
, glslang
, spirv-tools
, lib
, fetchurl
, meson
, ninja
, pkg-config
, python3
, bison
, flex
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, llvm
, libxshmfence
, zlib
, expat
, libX11 ? null
, libXext ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, libXxf86vm ? null
, xorgproto ? null
, xtrans ? null
, wayland
, waylandProtocols
, waylandScanner
, pdVirglShim
, virglWinsysSrc
, virglAbiHeader
  # Wayland-only build: no x11 platform, no GLX, and no X11 library anywhere in
  # the closure. See the with_dri patch in postPatch for why that needs help.
, withX11 ? true
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  pythonEnv = python3.withPackages (ps: [ ps.mako ps.pyyaml ps.setuptools ]);

  depIncludes = [
    "-I${lib.getDev zlib}/include"
    "-I${lib.getDev expat}/include"
    "-I${llvm}/usr/include"
  ] ++ map (p: "-I${lib.getDev p}/include") xDeps;
  depLibs = [
    "-L${zlib}/lib"
    "-L${expat}/lib"
    "-L${llvm}/usr/lib"
    "-L${wayland}/lib"
  ];

  xDeps = [ wayland waylandProtocols ]
    ++ lib.optionals withX11 [ libX11 libXext libxcb libXau libXdmcp libXxf86vm
                               xorgproto xtrans libxshmfence ];
  xPkgConfigPath = lib.concatMapStringsSep ":"
    (p: "${p}/lib/pkgconfig:${p}/share/pkgconfig") xDeps;
in
stdenv.mkDerivation rec {
  pname = "puredarwin-mesa${lib.optionalString (!withX11) "-nox"}";
  version = "26.1.6";

  # archive.mesa3d.org and the rest of freedesktop have been unreachable, so
  # Debian's pool is listed first - its .orig tarball is upstream's, unpacking
  # to the same mesa-<version>/ layout. Upstream stays as a fallback for when
  # it comes back.
  src = fetchurl {
    urls = [
      "http://deb.debian.org/debian/pool/main/m/mesa/mesa_${version}.orig.tar.xz"
      "https://archive.mesa3d.org/mesa-${version}.tar.xz"
    ];
    sha256 = "01l62p9a90rwnhmgp975h752ixxdpfi518ddp4n2w08y1y5bi5jj";
  };

  nativeBuildInputs = [ meson ninja pkg-config pythonEnv bison flex glslang spirv-tools waylandScanner ];
  buildInputs = [ zlib expat ];

  postPatch = ''
    patchShebangs .

    patch -p1 < ${./mesa-virgl-darwin.patch}

    # The XFIXES QueryVersion reply is dereferenced without a NULL check, unlike
    # the DRI3/Present queries right above it, so a server that advertises
    # XFIXES but does not answer the version request takes every Vulkan X11
    # client down. Guard it the way its neighbours already are.
    substituteInPlace src/vulkan/wsi/wsi_common_x11.c \
      --replace 'wsi_conn->has_xfixes = (ver_reply->major_version >= 2);' \
                'wsi_conn->has_xfixes = ver_reply != NULL && (ver_reply->major_version >= 2);'

    # Mesa equates "darwin" with "macOS running XQuartz" and picks the applegl
    # DRI platform, whose GLX provider is a client of libXplugin - an XQuartz
    # private library that cannot exist here, so libGL comes out with an
    # unsatisfiable /usr/lib/libXplugin.1.dylib dependency and nothing that
    # links it can be loaded at all. pseudo-drm is the platform for exactly
    # this shape: real X11, no KMS/DRM device underneath.
    substituteInPlace meson.build \
      --replace "  with_dri_platform = 'apple'" \
                "  with_dri_platform = 'pseudo-drm'"

  '' + lib.optionalString (!withX11) ''
    # with_dri is what EGL requires, and Mesa only sets it from
    # system_has_kms_drm (false on Darwin) or glx=='dri' - and glx=dri in turn
    # demands the x11 platform. That circle forces X11 into an otherwise
    # Wayland-only build. Darwin already gets the pseudo-drm DRI platform
    # above, so let it reach the same with_dri via the egl branch instead.
    substituteInPlace meson.build \
      --replace "if with_gallium and (system_has_kms_drm or (freedreno_kmds.contains('kgsl') and with_gallium_zink))" \
                "if with_gallium and (system_has_kms_drm or host_machine.system() == 'darwin' or (freedreno_kmds.contains('kgsl') and with_gallium_zink))"

    # Mesa only builds a desktop GL library as part of the GLX target, so
    # disabling GLX leaves libEGL/libGLESv2 and no libGL at all. The dispatch
    # generator already has an "opengl" target (libglvnd's libOpenGL set: every
    # desktop entry point, no GLX), so build that as libGL alongside es2api.
    mkdir -p src/mesa/glapi/openglapi
    cp ${./openglapi/meson.build} src/mesa/glapi/openglapi/meson.build
    cp ${./openglapi/libopengl_public.c} src/mesa/glapi/openglapi/libopengl_public.c
    # Appended rather than spliced into the gles2 block: by the end of
    # src/meson.build every variable the target needs (shared_glapi_lib,
    # glapi_xml_py_deps, inc_mesa, gl_priv_libs) is already in scope.
    echo "subdir('mesa/glapi/openglapi')" >> src/meson.build
  '' + ''
    mkdir -p src/gallium/winsys/virgl/puredarwin
    cp ${virglWinsysSrc}/virgl_puredarwin_winsys.c src/gallium/winsys/virgl/puredarwin/
    cp ${virglWinsysSrc}/virgl_puredarwin_public.h src/gallium/winsys/virgl/puredarwin/
    cp ${pdVirglShim}/include/pd_virgl_shim.h src/gallium/winsys/virgl/puredarwin/
    cp ${virglAbiHeader} src/gallium/winsys/virgl/puredarwin/IOVirtIOGPU3DShared.h

    # Two changes beyond swapping vtest for our winsys. The wrap returns NULL
    # when IOVirtIOGPU has no 3D user client (real hardware, or virtio-gpu
    # without virgl), and virgl_create_screen() is not prepared for that - so
    # guard it. And virgl only ever got picked when GALLIUM_DRIVER named it,
    # which left every default launch on llvmpipe; adding it to the implicit
    # list ahead of llvmpipe makes it the preference while still falling
    # through when the device is not there. LIBGL_ALWAYS_SOFTWARE still forces
    # software, and an explicit GALLIUM_DRIVER still wins outright.
    helper=src/gallium/auxiliary/target-helpers/inline_sw_helper.h
    for helper in src/gallium/auxiliary/target-helpers/inline_sw_helper.h \
                  src/gallium/auxiliary/target-helpers/sw_helper.h; do
      substituteInPlace "$helper" \
      --replace '#include "virgl/vtest/virgl_vtest_public.h"' \
                '#include "virgl/puredarwin/virgl_puredarwin_public.h"' \
      --replace 'vws = virgl_vtest_winsys_wrap(winsys);' \
                'vws = virgl_puredarwin_winsys_wrap(winsys);' \
      --replace 'screen = virgl_create_screen(vws, NULL);' \
                'screen = vws ? virgl_create_screen(vws, NULL) : NULL;' \
      --replace '      "llvmpipe",' \
                '#if defined(GALLIUM_VIRGL)
      (sw_vk || only_sw) ? "" : "virpipe",
#endif
      "llvmpipe",'
    done

    # llvmpipe's screen-creation branch also accepts the EMPTY driver name, and
    # the candidate list starts with $GALLIUM_DRIVER - empty whenever the user
    # has not asked for anything - so llvmpipe was created on the very first
    # iteration and the virpipe entry added below it was never reached. That is
    # why GALLIUM_DRIVER=virpipe gave the host GPU but the default did not.
    # Match its own name only: an unset GALLIUM_DRIVER now falls through to
    # virpipe, and back to llvmpipe when virgl is unavailable (the wrap returns
    # NULL without an IOVirtIOGPU 3D device, e.g. on real hardware).
    # LIBGL_ALWAYS_SOFTWARE still forces software via the only_sw guard.
    substituteInPlace src/gallium/auxiliary/target-helpers/sw_helper.h \
      --replace 'if (screen == NULL && (strcmp(driver, "llvmpipe") == 0 || !driver[0]))' \
                'if (screen == NULL && strcmp(driver, "llvmpipe") == 0)'

    # The DRI helper still contains the Linux virtio-gpu entry point even
    # though the Darwin virgl build intentionally omits the DRM winsys.
    # Leave the descriptor as a stub on Darwin; the PureDarwin winsys is
    # selected through the software frontend below.
    drmhelper=src/gallium/auxiliary/target-helpers/drm_helper.h
    substituteInPlace "$drmhelper" \
      --replace '#if defined(GALLIUM_VIRGL)' \
                '#if defined(GALLIUM_VIRGL) && !defined(__APPLE__)'

    dritarget=src/gallium/targets/dri/meson.build
    substituteInPlace "$dritarget" \
      --replace "files('dri_target.c')," \
                "files('dri_target.c', '../../winsys/virgl/puredarwin/virgl_puredarwin_winsys.c')," \
      --replace "include_directories('../../frontends/dri')," \
                "include_directories('../../frontends/dri'), include_directories('../../winsys/virgl/puredarwin'), inc_virtio," \
      --replace 'gallium_dri_ld_args = [cc.get_supported_link_arguments' \
                "gallium_dri_ld_args = ['-L${pdVirglShim}/usr/lib', '-lpd_virgl_shim'] + [cc.get_supported_link_arguments"

    xmeson=src/gallium/targets/libgl-xlib/meson.build
    substituteInPlace $xmeson \
      --replace "files('xlib.c')," \
                "files('xlib.c', '../../winsys/virgl/puredarwin/virgl_puredarwin_winsys.c')," \
      --replace 'gallium_xlib_ld_args = []' \
                "gallium_xlib_ld_args = ['-L${pdVirglShim}/usr/lib', '-lpd_virgl_shim']" \
      --replace "include_directories('../../frontends/glx/xlib')," \
                "include_directories('../../frontends/glx/xlib'), include_directories('../../winsys/virgl/puredarwin'), inc_virtio,"
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${llvm}/usr/bin:${nativeMesonTools}/bin:${waylandScanner}/bin:$PATH"

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${lib.getDev zlib}/lib/pkgconfig:${lib.getDev expat}/lib/pkgconfig:${xPkgConfigPath}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    # meson asks for wayland-scanner as a native dependency, so it resolves
    # through the build machine's pkg-config rather than the cross one above.
    export PKG_CONFIG_PATH_FOR_BUILD="${waylandScanner}/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR_FOR_BUILD="${waylandScanner}/lib/pkgconfig"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
cpp = '${darwinCrossToolchain}/bin/${targetTriple}-clang++'
objc = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
objcpp = '${darwinCrossToolchain}/bin/${targetTriple}-clang++'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool'
llvm-config = '${llvm}/usr/bin/llvm-config'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (s: "'${s}'") depIncludes}]
objc_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (s: "'${s}'") depIncludes}]
cpp_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-nostdinc++', '-I${libcxxDylib}/usr/include/c++/v1', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (s: "'${s}'") depIncludes}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (s: "'${s}'") depLibs}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,26.5,26.5', '-Wl,-fixup_chains', '-lSystem']
objc_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (s: "'${s}'") depLibs}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,26.5,26.5', '-Wl,-fixup_chains', '-lSystem']
cpp_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=26.5', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${libcxxDylib}/usr/lib', '-L${libcxxabiDylib}/usr/lib', ${lib.concatMapStringsSep ", " (s: "'${s}'") depLibs}, ${lib.optionalString withX11 "'-L${libXau}/lib', '-L${libXdmcp}/lib', "} '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,26.5,26.5', '-Wl,-fixup_chains', ${lib.optionalString withX11 "'-lXau', '-lXdmcp', "}'-lc++', '-lc++abi', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    # glx-direct: direct contexts are the only ones that can work. The module
    # the X server loads for AIGLX is Mesa's "dril" stub, whose
    # createNewContext returns NULL by design, so an indirect context cannot be
    # created at all (and the server then crashes destroying it)
    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out/usr \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dgallium-drivers=llvmpipe,softpipe,virgl \
      -Dvulkan-drivers=swrast \
      -Dplatforms=${if withX11 then "x11,wayland" else "wayland"} \
      -Dopengl=true \
      -Dgles1=disabled \
      -Dgles2=enabled \
      -Dglx=${if withX11 then "dri" else "disabled"} \
      -Dglx-direct=true \
      -Degl=enabled \
      -Dgbm=disabled \
      -Dllvm=enabled \
      -Dshared-llvm=enabled \
      -Dgallium-va=disabled \
      -Dgallium-rusticl=false \
      -Dglvnd=false \
      -Dlmsensors=disabled \
      -Dzstd=disabled \
      -Dvalgrind=disabled \
      -Dlibunwind=disabled \
      -Dxlib-lease=disabled \
      -Dbuild-tests=false

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

    # Re-root the install_names at /usr/lib (image layout) and rewrite any
    # @rpath refs, same postInstall dance as libepoxy.nix.
    INSTALL_NAME_TOOL="${nativeMesonTools}/bin/install_name_tool"
    dylibs=$(find "$out/usr/lib" -maxdepth 1 -name "*.dylib" -not -type l)
    for dylib in $dylibs; do
      base=$(basename "$dylib")
      "$INSTALL_NAME_TOOL" -id "/usr/lib/$base" "$dylib" || true
    done
    allfiles=$(
      [ ! -d "$out/usr/bin" ] || find "$out/usr/bin" -type f
      [ ! -d "$out/usr/lib" ] || find "$out/usr/lib" -type f
    )
    for f in $allfiles; do
      for dylib in $dylibs; do
        base=$(basename "$dylib")
        # Rewrite both @rpath and absolute-store-path refs (meson records inter-
        # library deps like libGL -> libglapi by their $out build path) to the
        # image layout so they resolve from /usr/lib at runtime.
        "$INSTALL_NAME_TOOL" -change "@rpath/$base" "/usr/lib/$base" "$f" 2>/dev/null || true
        "$INSTALL_NAME_TOOL" -change "$out/usr/lib/$base" "/usr/lib/$base" "$f" 2>/dev/null || true
      done
    done

    # The EGL Wayland platform links libwayland-client directly, and that one
    # comes from another package, so the loop above never sees it. It installs
    # at /lib on the image, not /usr/lib.
    for f in $allfiles; do
      for dylib in ${wayland}/lib/*.dylib; do
        [ -e "$dylib" ] || continue
        base=$(basename "$dylib")
        "$INSTALL_NAME_TOOL" -change "@rpath/$base" "/lib/$base" "$f" 2>/dev/null || true
      done
    done

    # The generated ICD manifest points library_path at the store path, which
    # does not exist on the guest. The loader accepts a bare filename and then
    # resolves it the way dlopen would, i.e. from /usr/lib.
    for icd in "$out"/usr/share/vulkan/icd.d/*.json; do
      [ -e "$icd" ] || continue
      sed -i "s|\"library_path\": \".*/\([^/]*\.dylib\)\"|\"library_path\": \"/usr/lib/\1\"|" "$icd"
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Mesa softpipe (Gallium swrast) + OSMesa, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
