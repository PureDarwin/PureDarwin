{ stdenv
, lib
, requireFile
, gnumake
, pkg-config
, darwinCrossToolchain
, nativeLd
, libSystem
, python3
, zlib
, openssl
, libffi
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
  pname = "puredarwin-python";
  inherit (python3) version src;

  nativeBuildInputs = [ gnumake pkg-config python3 ];

  postPatch = ''
    substituteInPlace configure \
      --replace-fail '*-*-cygwin*)
		ac_sys_system=Cygwin
		;;' '*-*-cygwin*)
		ac_sys_system=Cygwin
		;;
	*-*-darwin*)
		ac_sys_system=Darwin
		;;' \
      --replace-fail '*-*-cygwin*)
		_host_ident=
		;;' '*-*-cygwin*)
		_host_ident=
		;;
	*-*-darwin*)
		_host_ident=$host_cpu
		;;'
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang"
    export CXX="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang++"
    export AR="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar"
    export RANLIB="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip"
    export PKG_CONFIG_LIBDIR="${zlib}/lib/pkgconfig:${openssl}/lib/pkgconfig:${libffi}/lib/pkgconfig"
    export CPPFLAGS="-I${libSystem}/usr/include -I${zlib}/include -I${openssl}/include -I${libffi}/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export CXXFLAGS="$CFLAGS"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${zlib}/lib -L${openssl}/lib -L${libffi}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    export ac_cv_file__dev_ptmx=yes
    export ac_cv_file__dev_ptc=no
    export ac_cv_buggy_getaddrinfo=no
    export ac_cv_little_endian_double=yes
    export ac_cv_working_tzset=yes
    export ac_cv_func_clock_nanosleep=no
    export ac_cv_func_close_range=no
    export ac_cv_func_closefrom=no
    export ac_cv_func_copy_file_range=no
    export ac_cv_func_fdwalk=no
    export ac_cv_func_dup3=no
    export ac_cv_func_fexecve=no
    export ac_cv_func_fork1=no
    export ac_cv_func_futimesat=no
    export ac_cv_func_getresgid=no
    export ac_cv_func_getresuid=no
    export ac_cv_func_memrchr=no
    export ac_cv_func_mremap=no
    export ac_cv_func_mkfifoat=no
    export ac_cv_func_mknodat=no
    export ac_cv_func_pipe2=no
    export ac_cv_func_plock=no
    export ac_cv_func_posix_spawn_file_actions_addclosefrom_np=no
    export ac_cv_func_posix_fadvise=no
    export ac_cv_func_posix_fallocate=no
    export ac_cv_func_preadv2=no
    export ac_cv_func_process_vm_readv=no
    export ac_cv_func_pthread_condattr_setclock=no
    export ac_cv_func_pthread_getcpuclockid=no
    export ac_cv_func_pwritev2=no
    export ac_cv_func_rtpSpawn=no
    export ac_cv_func_sched_rr_get_interval=no
    export ac_cv_func_sched_setaffinity=no
    export ac_cv_func_sched_setparam=no
    export ac_cv_func_sched_setscheduler=no
    export ac_cv_func_sem_clockwait=no
    export ac_cv_func_sendfile=no
    export ac_cv_func_setns=no
    export ac_cv_func_setresgid=no
    export ac_cv_func_setresuid=no
    export ac_cv_func_sigtimedwait=no
    export ac_cv_func_sigwaitinfo=no
    export ac_cv_func_splice=no
    export ac_cv_func_unshare=no
    export ac_cv_func_explicit_bzero=no
    export ac_cv_func_explicit_memset=no
    export ac_cv_func_sem_timedwait=no

    ./configure \
      --host=x86_64-apple-darwin20.4 \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --with-build-python=${python3}/bin/python3 \
      --without-ensurepip \
      --disable-test-modules \
      --disable-framework \
      --disable-shared \
      --with-system-ffi \
      --with-openssl=${openssl} \
      --with-openssl-rpath=no

    substituteInPlace Makefile \
      --replace-fail ' -framework CoreFoundation' "" \
      --replace-fail '-framework SystemConfiguration' "" \
      --replace-fail 'LIBS=		 -latomic' 'LIBS=' \
      --replace-fail 'none required' "" \
      --replace-fail \
        'MODULE_PYEXPAT_LDFLAGS=-lm $(LIBEXPAT_A)' \
        'MODULE_PYEXPAT_LDFLAGS=$(LIBEXPAT_A)' \
      --replace-fail \
        'MODULE__DECIMAL_LDFLAGS=-lm $(LIBMPDEC_A)' \
        'MODULE__DECIMAL_LDFLAGS=$(LIBMPDEC_A)'

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install DESTDIR="$out"
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "CPython, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
