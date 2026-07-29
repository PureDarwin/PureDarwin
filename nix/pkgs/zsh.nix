{ stdenv
, lib
, requireFile
, gnumake
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, zsh
, ncurses
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
in
stdenv.mkDerivation {
  pname = "puredarwin-zsh";
  inherit (zsh) version;
  src = zsh.src;

  nativeBuildInputs = [
    gnumake
  ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include -I${ncurses}/include/ncursesw -I${ncurses}/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${ncurses}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup"
    export LIBS="-Wl,-force_load,${ncurses}/lib/libncursesw.a -lSystem"

    export ac_cv_func_getpwnam=yes
    export ac_cv_func_getpwuid=yes
    export ac_cv_func_getgrgid=yes
    export ac_cv_func_getgrnam=yes
    export ac_cv_func_faccessx=no
    export ac_cv_func_srand_deterministic=no
    export ac_cv_func_prctl=no
    export ac_cv_func_setproctitle=no
    export ac_cv_func_sigqueue=no
    export ac_cv_func_setresgid=no
    export ac_cv_func_setresuid=no
    export ac_cv_header_sys_prctl_h=no
    export zsh_cv_path_utmp_file=/var/run/utmp
    export zsh_cv_path_wtmp_file=/var/log/wtmp
    export zsh_cv_sys_dynamic_clash_ok=no

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --bindir=$out/bin \
      --enable-etcdir=/etc \
      --disable-dynamic \
      --disable-gdbm \
      --disable-pcre \
      --disable-multibyte \
      --with-term-lib=ncursesw

    sed -i \
      -e 's/^#define HAVE_SETPROCTITLE .*/#undef HAVE_SETPROCTITLE/' \
      -e 's/^#define HAVE_PRCTL .*/#undef HAVE_PRCTL/' \
      -e 's/^#define HAVE_SIGQUEUE .*/#undef HAVE_SIGQUEUE/' \
      -e 's/^#define HAVE_SETRESGID .*/#undef HAVE_SETRESGID/' \
      -e 's/^#define HAVE_SETRESUID .*/#undef HAVE_SETRESUID/' \
      -e 's/^#define HAVE_SYS_PRCTL_H .*/#undef HAVE_SYS_PRCTL_H/' \
      config.h

    substituteInPlace Src/init.c \
      --replace-fail 'opts[MONITOR] = 2;   /* may be unset in init_io() */' \
                     'opts[MONITOR] = 0;   /* PureDarwin: tty pgrp support is incomplete */'
    substituteInPlace Src/signals.c \
      --replace-fail '/* Array describing the state of each signal: an element contains *' \
                     '/**/
void wait_for_processes(void);

/* Array describing the state of each signal: an element contains *' \
      --replace-fail '    ret = sigsuspend(&set);' \
                     '    if (sig == SIGCHLD) {
	zsleep(10000);
	wait_for_processes();
	return -1;
    }

    ret = sigsuspend(&set);'

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install.bin install.modules
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
