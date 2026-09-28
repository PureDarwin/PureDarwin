{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, unzip
, bzip2
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

stdenv.mkDerivation {
  pname = "puredarwin-unzip";
  inherit (unzip) version src patches;

  # unxcfg.h redeclares localtime() and clashes with the SDK header.
  postPatch = ''
    sed -i '/localtime()/ d' unix/unxcfg.h
  '';

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"

    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    cflags="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I${libSystem}/usr/include"
    ldflags="-L${bzip2}/lib -isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -lSystem"

    make -f unix/Makefile unix_make

    # LF2 is -s by default, which strips at link time; leave that to the image.
    make -f unix/Makefile unzips -j$NIX_BUILD_CORES \
      CC="$cc" LD="$cc" \
      AR="${darwinCrossToolchain}/bin/${targetTriple}-ar" \
      RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib" \
      CFLAGS="-O3 -Wall -DBSD $cflags" \
      LFLAGS1="$ldflags" \
      LF2="" \
      LOCAL_UNZIP="-DNO_LCHMOD" \
      IZ_BZIP2="${bzip2}/include" \
      D_USE_BZ2="-DUSE_BZIP2" \
      L_BZ2="-lbz2"

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin $out/share/man/man1
    cp unzip funzip unzipsfx $out/bin/
    cp unix/zipgrep $out/bin/zipgrep
    # zipinfo is unzip under another name, the way its own install does it.
    ln -s unzip $out/bin/zipinfo
    for page in man/unzip.1 man/funzip.1 man/unzipsfx.1 man/zipgrep.1 man/zipinfo.1; do
      cp $page $out/share/man/man1/
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Info-ZIP unzip, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
