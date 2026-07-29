{ stdenv
, lib
, requireFile
, pkg-config
, gnumake
, darwinCrossToolchain
, nativeLd
, libSystem
, xterm
, libX11
, libxcb
, libXau
, libXdmcp
, libICE
, libSM
, libXt
, libXext
, libXmu
, libXpm
, libXaw
, xorgproto
, ncurses
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  xDeps = [
    xorgproto
    libX11
    libxcb
    libXau
    libXdmcp
    libICE
    libSM
    libXt
    libXext
    libXmu
    libXpm
    libXaw
  ];
  xForceLoad = lib.concatStringsSep " " [
    "-Wl,-force_load,${libXaw}/lib/libXaw.a"
    "-Wl,-force_load,${libXmu}/lib/libXmu.a"
    "-Wl,-force_load,${libXt}/lib/libXt.a"
    "-Wl,-force_load,${libXpm}/lib/libXpm.a"
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
  pname = "puredarwin-xterm";
  inherit (xterm) version;
  src = xterm.src;

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
    export CPPFLAGS="-I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") xDeps}"
    # -DNO_XPOLL_H: xorgproto's <X11/Xpoll.h> hardcodes the glibc fd_set member
    # name (__fds_bits) in its XFD_COPYSET; Darwin's fd_set uses fds_bits. Skip
    # Xpoll.h so xterm falls back to its own XFD_COPYSET, which uses fds_bits.
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -DNO_XPOLL_H"
    # Darwin's struct utmpx has ut_tv, not the classic BSD ut_time xterm's utmp
    # path writes. PureDarwin has no utmp login database anyway - disable it so
    # the incompatible block is never compiled.
    export cf_cv_have_utmp=no
    # Trim reallocarray.o out of a writable copy of libXext.a so force_load'ing
    # both it and libX11.a (which also ships reallocarray.o) isn't a duplicate.
    cp ${libXext}/lib/libXext.a libXext-trimmed.a
    chmod +w libXext-trimmed.a
    $AR d libXext-trimmed.a reallocarray.o
    $RANLIB libXext-trimmed.a
    trimmedXext="-Wl,-force_load,$PWD/libXext-trimmed.a"
    export CPPFLAGS="''${CPPFLAGS:-} -I${ncurses}/include/ncursesw -I${ncurses}/include"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${ncurses}/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") xDeps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"
    # Link the real static ncurses rather than leaning on the terminfo shims
    # that used to live in libSystem: those made setupterm() succeed while every
    # tigetstr() reported the capability absent, so xterm's TcapInit passed but
    # its termcap-query feature (xtermcap.c, OPT_TCAP_*) saw an empty terminal.
    export LIBS="${xForceLoad} $trimmedXext -Wl,-force_load,${ncurses}/lib/libncursesw.a -lSystem"

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --disable-desktop \
      --disable-freetype \
      --disable-luit \
      --disable-setgid \
      --disable-setuid \
      --disable-trace \
      --without-Xaw3d \
      --without-Xft \
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
    make install
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
