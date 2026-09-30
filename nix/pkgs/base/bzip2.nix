# bzip2 1.0.8: libbz2.a plus bzip2/bunzip2/bzcat, from the upstream Makefile rather
# than nixpkgs' autoconfiscated variant, which only exists for the shared library
{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, bzip2
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

stdenv.mkDerivation {
  pname = "puredarwin-bzip2";
  inherit (bzip2) version src;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"

    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    cflags="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I${libSystem}/usr/include"
    ldflags="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -lSystem"

    # Not "all": that target runs the test suite, which means running the
    # cross-built binaries.
    make -j$NIX_BUILD_CORES libbz2.a bzip2 bzip2recover \
      CC="$cc" \
      AR="${darwinCrossToolchain}/bin/${targetTriple}-ar" \
      RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib" \
      CFLAGS="-Wall -Winline -O2 -D_FILE_OFFSET_BITS=64 $cflags" \
      LDFLAGS="$ldflags"

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    make install PREFIX=$out
    # The other ports keep man pages under share/man.
    mkdir -p $out/share
    mv $out/man $out/share/man

    # Its install symlinks through $PREFIX, which would bake store paths into
    # the image and dangle there.
    ln -sf bzdiff $out/bin/bzcmp
    ln -sf bzgrep $out/bin/bzegrep
    ln -sf bzgrep $out/bin/bzfgrep
    ln -sf bzmore $out/bin/bzless

    mkdir -p $out/lib/pkgconfig
    cat > $out/lib/pkgconfig/bzip2.pc <<EOF
    Name: bzip2
    Description: bzip2 compression library
    Version: ${bzip2.version}
    Libs: -L$out/lib -lbz2
    Cflags: -I$out/include
    EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "bzip2 compression tools and libbz2, cross-built for PureDarwin";
    platforms = platforms.unix;
  };
}
