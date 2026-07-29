{ stdenv
, lib
, requireFile
, pkg-config
, gnumake
, darwinCrossToolchain
, nativeLd
, libSystem
, xcalc
, libX11
, libxcb
, libXau
, libXdmcp
, libXext
, libXmu
, libXt
, libXaw
, libICE
, libSM
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
    libXmu
    libXt
    libXaw
    libICE
    libSM
  ];
  xForceLoad = lib.concatStringsSep " " [
    "-Wl,-force_load,${libXaw}/lib/libXaw.a"
    "-Wl,-force_load,${libXmu}/lib/libXmu.a"
    "-Wl,-force_load,${libXt}/lib/libXt.a"
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
  pname = "puredarwin-xcalc";
  inherit (xcalc) version;
  src = xcalc.src;

  nativeBuildInputs = [
    pkg-config
    gnumake
  ];

  buildInputs = xDeps;

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" (map lib.getDev xDeps)}:${lib.makeSearchPath "share/pkgconfig" (map lib.getDev xDeps)}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") xDeps} -include limits.h"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -DNO_XPOLL_H"
    cp ${libXext}/lib/libXext.a libXext-trimmed.a
    chmod +w libXext-trimmed.a
    $AR d libXext-trimmed.a reallocarray.o
    $RANLIB libXext-trimmed.a
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") xDeps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"
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
