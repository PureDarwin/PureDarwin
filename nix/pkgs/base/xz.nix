{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, xz
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-xz";
  inherit (xz) version;
  src = xz.src;

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
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    export gl_cv_func_gettimeofday_clobber=no
    export ac_cv_func_pthread_condattr_setclock=no
    export ac_cv_func_posix_fadvise=no

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --disable-shared \
      --enable-static \
      --disable-nls \
      --disable-doc \
      --disable-sandbox \
      --disable-scripts

    find . -name Makefile -exec sed -i -E 's/-lm\b//g' {} +

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
    description = "XZ compression utilities, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
