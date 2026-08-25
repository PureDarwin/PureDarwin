{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, ncurses
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-ncurses";
  inherit (ncurses) version;
  src = ncurses.src;

  nativeBuildInputs = [
    ncurses
  ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    # PureDarwin ships no /usr/share/terminfo database. --with-fallbacks bakes
    # a fixed set of terminal descriptions directly into the library so
    # setupterm()/curses can work without ever touching the filesystem; ncurses
    # falls back to these automatically when TERMINFO lookups fail. xterm and
    # xterm-256color cover the terminal this system actually has (PD's xterm
    # build); vt100/vt220/linux/ansi are cheap, common fallbacks for whatever
    # TERM the serial/kernel console sets (observed: vt220).
    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --without-cxx \
      --without-cxx-binding \
      --without-ada \
      --without-manpages \
      --without-tests \
      --without-progs \
      --without-debug \
      --without-shared \
      --with-normal \
      --disable-db-install \
      --disable-stripping \
      --enable-widec \
      --with-fallbacks=xterm,xterm-256color,linux,vt100,vt220,ansi \
      --with-terminfo-dirs=/usr/share/terminfo \
      --with-default-terminfo-dir=/usr/share/terminfo

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
