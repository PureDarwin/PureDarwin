{ stdenv
, lib
, kcTools
, kernel
, kexts
}:

stdenv.mkDerivation {
  pname = "puredarwin-prelinked-arm32-bcm2835";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    KERNEL_BIN=$(ls ${kernel}/System/Library/Kernels/kernel* | head -n1)
    KERNEL_EXTS=${kernel}/System/Library/Extensions
    KEXTS=${kexts}/System/Library/Extensions

    codeless=()
    for p in "$KERNEL_EXTS"/System.kext/PlugIns/*.kext; do
      codeless+=( -codeless "$p" )
    done

    ${kcTools}/bin/prelink-builder \
      -kernel "$KERNEL_BIN" \
      -kext "$KEXTS/corecrypto.kext" \
      -kext "$KEXTS/pthread.kext" \
      -kext "$KEXTS/ext4.kext" \
      -kext "$KEXTS/IOStorageFamily.kext" \
      -kext "$KEXTS/Ext4FileSystemDriver.kext" \
      -kext "$KEXTS/PDBcm2835SD.kext" \
      -kext "$KEXTS/PDArmPlatformExpert.kext" \
      "''${codeless[@]}" \
      -o kernel.release.bcm2835
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/System/Library/Kernels
    cp kernel.release.bcm2835 $out/System/Library/Kernels/kernel.release.bcm2835
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin ARMv6 (BCM2835) prelinked kernel";
    platforms = platforms.linux;
  };
}
