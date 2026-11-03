{ stdenv
, lib
, kcTools
, kernel
, kexts
, extraKexts ? [ ]
}:

# Generic Cortex-A7 (QEMU virt) prelinked kernel: the Pi Zero recipe without its
# SD host driver, since the root filesystem comes from a ramdisk.
stdenv.mkDerivation {
  pname = "puredarwin-prelinked-arm32-virta7";
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
      -kext "$KEXTS/PDArmPlatformExpert.kext" \
      ${lib.concatMapStrings (k: "-kext \"$KEXTS/${k}\" ") extraKexts}\
      "''${codeless[@]}" \
      -o kernel.virta7
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/System/Library/Kernels
    cp kernel.virta7 $out/System/Library/Kernels/kernel.virta7
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin generic Cortex-A7 prelinked kernel";
    platforms = platforms.linux;
  };
}
