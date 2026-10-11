{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, xorg
, xorgproto
, pixman
, iokit
, corefoundation
, targetTriple ? "arm64-apple-darwin20.4"
}:

# the puredarwingop xorg driver on its own, with PDGOP over the system IOKit (the userland tree's IOKitLib is a minimal string-matcher)
stdenv.mkDerivation {
  pname = "puredarwin-xf86-video-puredarwingop";
  version = "1.0";

  srcs = [ ../../../src/Userspace/xf86-video-puredarwingop ../../../src/Libraries/PDGOP ];
  sourceRoot = ".";

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    drv=$(ls -d *xf86-video-puredarwingop)
    gop=$(ls -d *PDGOP)
    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang -isysroot $SDK -mmacosx-version-min=26.5 -O2 \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -fno-builtin \
      -I${libSystem}/usr/include -I${corefoundation}/include -I$gop/include"
    $cc -c $gop/PDGOP.c -o PDGOP.o
    $cc -I${../../../src/Kernel/Extensions/PDPowerVR/include} -c $drv/pdscanout.c -o pdscanout.o
    $cc -DHAVE_XORG_CONFIG_H=1 -DIN_MODULE=1 \
      -I${xorg}/usr/include/xorg -I${xorg}/usr/include -I${lib.getDev xorgproto}/include -I${pixman}/include/pixman-1 \
      -c $drv/puredarwingop_drv.c -o puredarwingop_drv.o
    # server symbols are left to dynamic lookup against the Xorg image at dlopen, one -U each
    ${darwinCrossToolchain}/bin/${targetTriple}-nm -u puredarwingop_drv.o | sed 's/^/-Wl,-U,/' > undef.args
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -isysroot $SDK -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -dynamiclib \
      -install_name /usr/lib/xorg/modules/drivers/puredarwingop_drv.so \
      @undef.args \
      -Wl,-exported_symbols_list,$drv/puredarwingop_drv.exports \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      puredarwingop_drv.o PDGOP.o pdscanout.o -lSystem -lIOKitCF -o puredarwingop_drv.so
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 puredarwingop_drv.so $out/usr/lib/xorg/modules/drivers/puredarwingop_drv.so
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Xorg video driver for PureDarwin's IOGOPFramebuffer";
    platforms = platforms.unix;
  };
}
