{ stdenv
, lib
, requireFile
, gnumake
, perl
, m4
, darwinCrossToolchain
, nativeLd
, libSystem
, bison
, targetTriple ? "x86_64-apple-darwin20.4"
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
  pname = "puredarwin-bison";
  inherit (bison) version src;

  nativeBuildInputs = [ gnumake perl m4 ];

  postPatch = ''
    substituteInPlace lib/spawn.in.h \
      --replace-fail '#  undef POSIX_SPAWN_FORK_HANDLERS' '#  undef POSIX_SPAWN_FORK_HANDLERS
#  ifndef POSIX_SPAWN_SETSCHEDPARAM
#   define POSIX_SPAWN_SETSCHEDPARAM 0
#  endif
#  ifndef POSIX_SPAWN_SETSCHEDULER
#   define POSIX_SPAWN_SETSCHEDULER 0
#  endif'
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export CXX="${darwinCrossToolchain}/bin/${targetTriple}-clang++"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export CXXFLAGS="$CFLAGS"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -lSystem"

    export gl_cv_func_gettimeofday_clobber=no
    export ac_cv_func_strerror_r_char_p=no
    export ac_cv_func_opendir=yes
    export ac_cv_func_readdir=yes
    export ac_cv_func_rewinddir=yes

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --disable-nls

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

  meta = with lib; {
    description = "GNU Bison, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
