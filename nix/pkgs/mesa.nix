{ stdenv
, lib
, requireFile
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
, zlib
, expat
, libX11
, libXext
, libxcb
, libXau
, libXdmcp
, xorgproto
, xtrans
, pdVirglShim
, virglWinsysSrc
, virglAbiHeader
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };

  pythonEnv = python3.withPackages (ps: [ ps.mako ps.pyyaml ps.setuptools ]);

  depIncludes = [
    "-I${lib.getDev zlib}/include"
    "-I${lib.getDev expat}/include"
  ];
  depLibs = [
    "-L${zlib}/lib"
    "-L${expat}/lib"
  ];

  xDeps = [ libX11 libXext libxcb libXau libXdmcp xorgproto xtrans ];
  xPkgConfigPath = lib.concatMapStringsSep ":"
    (p: "${p}/lib/pkgconfig:${p}/share/pkgconfig") xDeps;
in
stdenv.mkDerivation {
  pname = "puredarwin-mesa";
  version = "23.1.9";

  src = fetchurl {
    url = "https://archive.mesa3d.org/mesa-23.1.9.tar.xz";
    hash = "sha256-KVuifCgUbtCSFOjOea+hZZ7fnRQt7MPJH4BFUtZPdRA=";
  };

  nativeBuildInputs = [ meson ninja pkg-config pythonEnv bison flex ];
  buildInputs = [ zlib expat ];

  postPatch = ''
    patchShebangs .

    patch -p1 < ${./mesa/mesa-virgl-darwin.patch}

    mkdir -p src/gallium/winsys/virgl/puredarwin
    cp ${virglWinsysSrc}/virgl_puredarwin_winsys.c src/gallium/winsys/virgl/puredarwin/
    cp ${virglWinsysSrc}/virgl_puredarwin_public.h src/gallium/winsys/virgl/puredarwin/
    cp ${pdVirglShim}/include/pd_virgl_shim.h src/gallium/winsys/virgl/puredarwin/
    cp ${virglAbiHeader} src/gallium/winsys/virgl/puredarwin/IOVirtIOGPU3DShared.h

    helper=src/gallium/auxiliary/target-helpers/inline_sw_helper.h
    substituteInPlace $helper \
      --replace '#include "virgl/vtest/virgl_vtest_public.h"' \
                '#include "virgl/puredarwin/virgl_puredarwin_public.h"' \
      --replace 'vws = virgl_vtest_winsys_wrap(winsys);' \
                'vws = virgl_puredarwin_winsys_wrap(winsys);'

    xmeson=src/gallium/targets/libgl-xlib/meson.build
    substituteInPlace $xmeson \
      --replace "files('xlib.c')," \
                "files('xlib.c', '../../winsys/virgl/puredarwin/virgl_puredarwin_winsys.c')," \
      --replace 'gallium_xlib_ld_args = []' \
                "gallium_xlib_ld_args = ['-L${pdVirglShim}/usr/lib', '-lpd_virgl_shim']"
    substituteInPlace $xmeson \
      --replace "include_directories('../../frontends/glx/xlib')," \
                "include_directories('../../frontends/glx/xlib'), include_directories('../../winsys/virgl/puredarwin'), inc_virtio, inc_gallium_drivers,"
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PKG_CONFIG_PATH="${lib.getDev zlib}/lib/pkgconfig:${lib.getDev expat}/lib/pkgconfig:${xPkgConfigPath}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang'
cpp = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang++'
ar = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar'
strip = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip'
pkg-config = '${pkg-config}/bin/pkg-config'
install_name_tool = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-install_name_tool'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (s: "'${s}'") depIncludes}]
cpp_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-nostdinc++', '-I${libcxxDylib}/usr/include/c++/v1', '-I${libSystem}/usr/include', ${lib.concatMapStringsSep ", " (s: "'${s}'") depIncludes}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', ${lib.concatMapStringsSep ", " (s: "'${s}'") depLibs}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-fixup_chains', '-lSystem']
cpp_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${libcxxDylib}/usr/lib', '-L${libcxxabiDylib}/usr/lib', ${lib.concatMapStringsSep ", " (s: "'${s}'") depLibs}, '-L${libXau}/lib', '-L${libXdmcp}/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-fixup_chains', '-lXau', '-lXdmcp', '-lc++', '-lc++abi', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
needs_exe_wrapper = true
EOF

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out/usr \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dgallium-drivers=swrast,virgl \
      -Dvulkan-drivers= \
      -Dplatforms=x11 \
      -Dosmesa=true \
      -Dopengl=true \
      -Dgles1=disabled \
      -Dgles2=disabled \
      -Dglx=xlib \
      -Degl=disabled \
      -Dgbm=disabled \
      -Ddri3=disabled \
      -Dllvm=disabled \
      -Dshared-glapi=enabled \
      -Dgallium-vdpau=disabled \
      -Dgallium-va=disabled \
      -Dgallium-xa=disabled \
      -Dgallium-nine=false \
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

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Mesa softpipe (Gallium swrast) + OSMesa, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
