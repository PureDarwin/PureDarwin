{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, cabextract
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# cabextract for PureDarwin: winetricks unpacks Microsoft's redistributables (corefonts and the like) with it
stdenv.mkDerivation {
  pname = "puredarwin-cabextract";
  inherit (cabextract) version src;

  configurePhase = ''
    runHook preConfigure

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export CPPFLAGS="-I${libSystem}/usr/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5"
    export LIBS="-lSystem"

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out/usr

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
    description = "Microsoft cabinet extractor, for winetricks";
    platforms = platforms.unix;
  };
}
