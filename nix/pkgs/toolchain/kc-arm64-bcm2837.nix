{ stdenv
, lib
, kcTools
, kernel
, kexts
}:

stdenv.mkDerivation {
  pname = "puredarwin-kc-arm64-bcm2837";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    KERNEL_EXTS=${kernel}/System/Library/Extensions
    KEXTS=${kexts}/System/Library/Extensions

    KERNEL_BIN=$(ls "${kernel}"/System/Library/Kernels/kernel* | head -n1)

    codeless=()
    for p in "$KERNEL_EXTS"/System.kext/PlugIns/*.kext; do
      codeless+=( -codeless "$p" )
    done

    ${kcTools}/bin/kc-builder \
      -kernel "$KERNEL_BIN" \
      -kext "$KEXTS/corecrypto.kext" \
      -kext "$KEXTS/PDArmPlatformExpert.kext" \
      -kext "$KEXTS/IOStorageFamily.kext" \
      -kext "$KEXTS/PDBcm2835SD.kext" \
      -kext "$KEXTS/ext4.kext" \
      -kext "$KEXTS/Ext4FileSystemDriver.kext" \
      -kext "$KEXTS/IOPCIFamily.kext" \
      -kext "$KEXTS/IOGraphicsFamily.kext" \
      -kext "$KEXTS/IOGOPFramebuffer.kext" \
      "''${codeless[@]}" \
      -o kernel
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    mkdir -p $out/System/Library/Kernels
    cp kernel $out/System/Library/Kernels/kernel.release.bcm2837
    cp kernel $out/kernel
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin arm64 BCM2837 (Raspberry Pi 3) boot kernel collection, assembled by kc-tools' kc-builder";
    platforms = platforms.linux;
  };
}
