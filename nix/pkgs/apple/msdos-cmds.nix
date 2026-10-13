{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, libcxxDylib
, libcxxabiDylib
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# Apple's newfs_msdos and fsck_msdos (msdosfs) with wipefs from libutil, for EFI system partitions
stdenv.mkDerivation {
  pname = "puredarwin-msdos-cmds";
  version = "0.1";

  src = ../../../src/Userspace/msdos_cmds;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    CXX="${darwinCrossToolchain}/bin/${targetTriple}-clang++"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -F$DARWIN_SDK_ROOT/System/Library/Frameworks -I${libSystem}/usr/include -I${corefoundation}/include -Iwipefs -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -O2"
    # PD's IOStorageFamily has no unmap options and refuses any bit, so wipefs' whole-disk hint is 0
    CXXFLAGS="$CFLAGS -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 -std=c++17 -DkIOStorageUnmapOptionWhole=0"
    # no -undefined dynamic_lookup, a missing symbol has to fail here
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5"

    $CXX $CXXFLAGS -c wipefs/wipefs.cpp -o wipefs.o
    $CXX $CXXFLAGS -c wipefs/ExtentManager.cpp -o ExtentManager.o
    $CC $CFLAGS -c newfs_msdos/newfs_msdos.c -o newfs_msdos.o
    $CC $LDFLAGS -o newfs_msdos.bin newfs_msdos.o wipefs.o ExtentManager.o -lIOKitCF -lCoreFoundation -lc++ -lc++abi -lSystem

    objs=""
    for f in fsck_msdos/*.c; do
      o=fsck_$(basename "$f" .c).o
      # fsck_msdos is K&R C with implicit int parameters, which gnu89 still accepts
      $CC $CFLAGS -std=gnu89 -c "$f" -o "$o"
      objs="$objs $o"
    done
    $CC $LDFLAGS -o fsck_msdos.bin $objs -lSystem

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/sbin $out/usr/share/man/man8
    cp newfs_msdos.bin $out/sbin/newfs_msdos
    cp fsck_msdos.bin $out/sbin/fsck_msdos
    cp newfs_msdos/newfs_msdos.8 fsck_msdos/fsck_msdos.8 $out/usr/share/man/man8/
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Apple's newfs_msdos and fsck_msdos, cross-built for PureDarwin";
    platforms = platforms.unix;
  };
}
