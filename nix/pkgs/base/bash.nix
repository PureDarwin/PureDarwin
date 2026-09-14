{ stdenv
, lib
, gnumake
, bison
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, ncurses
, bash
, appleSdk
}:

stdenv.mkDerivation {
  pname = "puredarwin-bash";
  inherit (bash) version src patches;
  # GNU's bash53-NNN patches are relative to the source root.
  patchFlags = [ "-p0" ];

  # The upstream patch set touches parse.y.
  nativeBuildInputs = [ gnumake bison ];

  configurePhase = ''
    runHook preConfigure

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    # Bash builds helper programs (mkbuiltins, bashversion) with the host
    # compiler; pin it to C17 like the cross compiler, or C23's bool keyword
    # collides with the typedef configure chose.
    export CC_FOR_BUILD=cc
    export CFLAGS_FOR_BUILD="-std=gnu17"
    export CPPFLAGS="-I${libSystem}/usr/include -I${ncurses}/include/ncursesw -I${ncurses}/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -O2"
    # The libraries go in LDFLAGS, not LIBS: bash links its host-side helpers
    # (man2html) with LIBS too, and the host linker rejects -force_load.
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${ncurses}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -Wl,-force_load,${ncurses}/lib/libncursesw.a -lSystem"

    # Answers configure cannot probe when cross-compiling (from nixpkgs' bash).
    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --without-bash-malloc \
      --with-installed-readline=no \
      --with-curses \
      --disable-nls \
      bash_cv_job_control_missing=nomissing \
      bash_cv_sys_named_pipes=nomissing \
      bash_cv_getcwd_malloc=yes \
      bash_cv_getenv_redef=no \
      bash_cv_dev_stdin=present \
      bash_cv_dev_fd=standard \
      bash_cv_termcap_lib=libncursesw \
      ac_cv_func_getresuid=no ac_cv_func_getresgid=no \
      ac_cv_func_setresuid=no ac_cv_func_setresgid=no

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
    # /bin/sh stays zsh; this is /bin/bash, and the docs and headers are not
    # runtime content.
    rm -rf $out/share $out/include $out/lib/pkgconfig
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "GNU Bash for PureDarwin (/bin/bash)";
    license = licenses.gpl3Plus;
    platforms = platforms.linux;
  };
}
