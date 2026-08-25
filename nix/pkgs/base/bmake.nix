{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, bmake
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-bmake";
  inherit (bmake) version;
  src = bmake.src;

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
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -lSystem"

    export ac_cv_func_fork_works=yes
    export ac_cv_func_wait3=yes
    export ac_cv_c_bigendian=no

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    chmod 755 make-bootstrap.sh
    ./make-bootstrap.sh
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin $out/share/man/man1
    cp bmake $out/bin/bmake
    cp make.1 $out/share/man/man1/make.1
    mkdir -p $out/share/mk
    cp -r mk/. $out/share/mk/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
