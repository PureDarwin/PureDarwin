{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, e2fsprogs
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# mke2fs, e2fsck, tune2fs, resize2fs and debugfs for PureDarwin, so the installer and disk
# repair can make and check the same ext4 the kext mounts, with their own static libuuid and libblkid
stdenv.mkDerivation {
  pname = "puredarwin-e2fsprogs";
  inherit (e2fsprogs) version;
  src = e2fsprogs.src;

  configurePhase = ''
    runHook preConfigure

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    # the build runs a few helpers it compiles first (crc tables, subst), those are for this machine
    export BUILD_CC=cc
    export CPPFLAGS="-I${libSystem}/usr/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -O2"
    # no -undefined dynamic_lookup: a missing libSystem symbol has to fail the link, not a configure probe
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -lSystem"

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --sbindir=/usr/sbin \
      --disable-nls \
      --disable-fuse2fs \
      --disable-uuidd \
      --disable-e2initrd-helper \
      --disable-defrag \
      --disable-tdb \
      --disable-debugfs-readline \
      --without-libarchive \
      --enable-libuuid \
      --enable-libblkid \
      --disable-elf-shlibs \
      --disable-bsd-shlibs

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install DESTDIR=$out
    # headers, static libraries and pkg-config files are for building against, not for the system
    rm -rf $out/usr/include $out/usr/lib/pkgconfig $out/usr/lib/*.a
    # Darwin's /sbin/fsck is diskdev_cmds', with its own options, so the Linux wrapper stays out
    rm -f $out/usr/sbin/fsck
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "ext2/3/4 tools cross-built for PureDarwin";
    platforms = platforms.unix;
  };
}
