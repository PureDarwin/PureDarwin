{ stdenv
, lib
, meson
, ninja
, pkg-config
, python3
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, libepoxy
, libX11 ? null
, xorgproto ? null
, mesa
  # Wayland-only image: no X11 platform and no GLX in mesa-nox, so epoxy must
  # not build its GLX resolver either - EGL is what GTK's Wayland backend uses.
, withX11 ? true
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

  deps = [ mesa ] ++ lib.optionals withX11 [ libX11 xorgproto ];
  depPcPaths = map lib.getDev deps;
in
stdenv.mkDerivation {
  pname = "puredarwin-libepoxy${lib.optionalString (!withX11) "-nox"}";
  inherit (libepoxy) version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .

    # epoxy's Darwin GLX_LIB is XQuartz's /opt/X11 prefix, which PD does
    # not have: Mesa installs libGL with an install_name of /usr/lib/libGL.1.dylib.
    substituteInPlace src/dispatch_common.c \
      --replace '"/opt/X11/lib/libGL.1.dylib"' '"/usr/lib/libGL.1.dylib"'
    substituteInPlace src/dispatch_common.h \
      --replace '#elif defined(__APPLE__)' '#elif defined(__APPLE__) && !defined(PUREDARWIN)'
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:$PATH"

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${mesa}/usr/lib/pkgconfig:${mesa}/usr/share/pkgconfig:${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}"
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
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-DPUREDARWIN=1', '-DEGL_LIB="/usr/lib/libEGL.dylib"', '-fno-stack-protector', '-I${libSystem}/usr/include', '-I${mesa}/usr/include', ${lib.concatMapStringsSep ", " (dep: "'-I${lib.getDev dep}/include'") deps}]
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${mesa}/usr/lib', ${lib.concatMapStringsSep ", " (dep: "'-L${dep}/lib'") deps}, '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-undefined,dynamic_lookup', '-lSystem']

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
      -Ddefault_library=shared \
      -Dtests=false \
      -Ddocs=false \
      -Dglx=${if withX11 then "yes" else "no"} \
      -Degl=yes \
      -Dx11=${lib.boolToString withX11}

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    ninja -C build install || (find . -name install_name_tool_debug.log -exec cat {} \; 2>/dev/null; exit 1)
    find . -name install_name_tool_debug.log -exec cat {} \; 2>/dev/null || true

    INSTALL_NAME_TOOL="${nativeMesonTools}/bin/install_name_tool"
    dylibs=$(find "$out/lib" -maxdepth 1 -name "*.dylib" -not -type l)
    for dylib in $dylibs; do
      base=$(basename "$dylib")
      "$INSTALL_NAME_TOOL" -id "/lib/$base" "$dylib"
    done
    allfiles=$(
      [ ! -d "$out/bin" ] || find "$out/bin" -type f
      [ ! -d "$out/lib" ] || find "$out/lib" -type f
    )
    for f in $allfiles; do
      for dylib in $dylibs; do
        base=$(basename "$dylib")
        "$INSTALL_NAME_TOOL" -change "@rpath/$base" "/lib/$base" "$f" 2>/dev/null || true
        # a sibling inside the same project can be recorded by absolute install path
        # rather than @rpath (libxfce4windowingui -> libxfce4windowing), which the
        # @rpath rewrite above never matches
        "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/lib/$base" "$f" 2>/dev/null || true
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libepoxy, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
