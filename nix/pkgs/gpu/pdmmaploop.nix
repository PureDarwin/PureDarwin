{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# drm dumb buffers mapped and unmapped in a loop, free pages before and after
stdenv.mkDerivation {
  pname = "puredarwin-pdmmaploop";
  version = "0.1";

  src = ../../../src/Userspace/pdmmaploop;
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -O2 -Wall \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -fno-stack-protector -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      pdmmaploop.c -lSystem -o pdmmaploop
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/sbin
    cp pdmmaploop $out/usr/sbin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "mmap pager loop test for /dev/dri";
    license = lib.licenses.mit;
  };
}
