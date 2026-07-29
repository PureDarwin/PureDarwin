{ stdenv
, lib
, requireFile
, pkg-config
, gnumake
, autoconf
, automake
, libtool
, gettext
, util-macros
, darwinCrossToolchain
, nativeLd
, libSystem
, fltk_1_3
, libX11
, libxcb
, libXau
, libXdmcp
, libXext
, libXrender
, libXfixes
, libXft
, libxcbcursor
, libICE
, libSM
, fontconfig
, freetype2
, expat
, xorgproto
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  xDeps = [
    xorgproto
    libX11
    libxcb
    libXau
    libXdmcp
    libXext
    libXrender
    libXfixes
    libXft
    libxcbcursor
    libICE
    libSM
    fontconfig
    freetype2
    expat
  ];
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-fltk";
  inherit (fltk_1_3) version;
  src = fltk_1_3.src;

  nativeBuildInputs = [
    pkg-config
    gnumake
    autoconf
    automake
    libtool
    gettext
  ];

  buildInputs = xDeps;

  postPatch = ''
    sed -i 's/__APPLE__/FLTK_FAKE_APPLE_DISABLED/g' src/*.cxx FL/*.H $(find src/xutf8 -name '*.c' -o -name '*.h')
    # host_os_gui otherwise defaults to $host_os ("darwin20.4"), which
    # configure.ac's own graphics-backend case statement matches against
    # "darwin*" to pick Quartz - force it to fall through to the generic
    # (X11 [+ Xft/Xfixes/Xrender]) branch instead.
    sed -i 's/^host_os_gui=\$host_os$/host_os_gui=puredarwin-x11/' configure.ac
    # Fl_PostScript.cxx uses LC_NUMERIC without including <locale.h> -
    # relies on it arriving transitively on real macOS.
    sed -i '/^#include <stdio.h>$/a #include <locale.h>' src/Fl_PostScript.cxx
  '';

  configurePhase = ''
    runHook preConfigure

    export PATH=${autoconf}/bin:${automake}/bin:${libtool}/bin:${gettext}/bin:$PATH
    autoreconf -fiv

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" (map lib.getDev xDeps)}:${lib.makeSearchPath "share/pkgconfig" (map lib.getDev xDeps)}:${util-macros}/share/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export CXX="${darwinCrossToolchain}/bin/${targetTriple}-clang++"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") xDeps} -I${lib.getDev freetype2}/include/freetype2"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -Wno-implicit-function-declaration"
    export CXXFLAGS="$CFLAGS"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") xDeps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --disable-gl --disable-xinerama --disable-xdbe --disable-shared \
      --enable-localjpeg --enable-localzlib --enable-localpng \
      --enable-xft --enable-xrender --enable-xfixes --enable-xcursor

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES -k || true
    test -f lib/libfltk.a
    test -x fluid/fluid
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/lib $out/include $out/bin
    cp lib/*.a $out/lib/
    cp -r FL $out/include/
    cp fluid/fluid $out/bin/
    sed "s|^prefix=.*|prefix=$out|" fltk-config > $out/bin/fltk-config
    chmod +x $out/bin/fltk-config
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "FLTK 1.3, cross-built for PureDarwin (X11 backend, no OpenGL)";
    platforms = platforms.linux;
  };
}
