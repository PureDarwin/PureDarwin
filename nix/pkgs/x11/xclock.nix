{ stdenv
, lib
, pkg-config
, gnumake
, autoconf
, automake
, libtool
, util-macros
, gettext
, darwinCrossToolchain
, nativeLd
, libSystem
, xclock
, libX11
, libxcb
, libXau
, libXdmcp
, libXext
, libXrender
, libXmu
, libXt
, libXaw
, libXft
, libxkbfile
, freetype2
, fontconfig
, expat
, libICE
, libSM
, xorgproto
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
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
    libXmu
    libXt
    libXaw
    libXft
    libxkbfile
    freetype2
    fontconfig
    expat
    libICE
    libSM
  ];
  xForceLoad = lib.concatStringsSep " " [
    "-Wl,-force_load,${libXaw}/lib/libXaw.a"
    "-Wl,-force_load,${libXmu}/lib/libXmu.a"
    "-Wl,-force_load,${libXt}/lib/libXt.a"
    "-Wl,-force_load,${libXft}/lib/libXft.a"
    # fontconfig is a dylib now, so it links normally rather than by force_load.
    "-lfontconfig"
    "-Wl,-force_load,${expat}/lib/libexpat.a"
    # freetype is a dylib now, so it links normally rather than by force_load.
    "-lfreetype"
    "-Wl,-force_load,${libxkbfile}/lib/libxkbfile.a"
    "-Wl,-force_load,${libXrender}/lib/libXrender.a"
    # libXext's reallocarray.o duplicates the copy in libX11.a; trim it from a
    # writable copy before force_load'ing both (see xterm.nix for the same fix).
    "-Wl,-force_load,$PWD/libXext-trimmed.a"
    "-Wl,-force_load,${libX11}/lib/libX11.a"
    "-Wl,-force_load,${libxcb}/lib/libxcb.a"
    "-Wl,-force_load,${libXau}/lib/libXau.a"
    "-Wl,-force_load,${libXdmcp}/lib/libXdmcp.a"
    "-Wl,-force_load,${libSM}/lib/libSM.a"
    "-Wl,-force_load,${libICE}/lib/libICE.a"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-xclock";
  inherit (xclock) version;
  src = xclock.src;

  nativeBuildInputs = [
    pkg-config
    gnumake
    autoconf
    automake
    libtool
    gettext
  ];

  buildInputs = xDeps;

  configurePhase = ''
    runHook preConfigure

    # Just ignore what nixpkgs does
    export ACLOCAL_PATH="${util-macros}/share/aclocal:${xorgproto}/share/aclocal:${gettext}/share/aclocal:${pkg-config.pkg-config}/share/aclocal"
    autoreconf -fiv

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" (map lib.getDev xDeps)}:${lib.makeSearchPath "share/pkgconfig" (map lib.getDev xDeps)}:${util-macros}/share/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") xDeps} -I${lib.getDev freetype2}/include/freetype2 -include limits.h"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -DNO_XPOLL_H -Wno-implicit-function-declaration"
    cp ${libXext}/lib/libXext.a libXext-trimmed.a
    chmod +w libXext-trimmed.a
    $AR d libXext-trimmed.a reallocarray.o
    $RANLIB libXext-trimmed.a
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") xDeps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -Wl,-undefined,dynamic_lookup -lSystem"
    export LIBS="${xForceLoad} -lSystem"

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --without-app-defaults

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install appdefaultdir=$out/share/X11/app-defaults
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
