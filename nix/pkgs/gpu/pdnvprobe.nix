{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# the NVIDIA resource manager through /dev/nvidiactl: version, cards, client, device, subdevice, name, uuid
stdenv.mkDerivation {
  pname = "puredarwin-pdnvprobe";
  version = "0.1";

  src = ../../../src/Userspace/pdnvprobe;
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -O2 -Wall \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -fno-stack-protector -I${libSystem}/usr/include \
      -Invidia -I${../../../src/Kernel/Extensions/PDNVLoader/nvidia/inc} \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      pdnvprobe.c -lSystem -o pdnvprobe
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 pdnvprobe $out/usr/sbin/pdnvprobe
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "NVIDIA resource manager probe over /dev/nvidiactl";
    license = lib.licenses.mpl20;
  };
}
