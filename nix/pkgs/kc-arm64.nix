{ stdenv
, lib
, kcTools
, kernel
, kexts
}:

stdenv.mkDerivation {
  pname = "puredarwin-kc-arm64";
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
      -kext "$KEXTS/IOPCIFamily.kext" \
      -kext "$KEXTS/corecrypto.kext" \
      -kext "$KEXTS/pthread.kext" \
      -kext "$KEXTS/PDArmPlatformExpert.kext" \
      -kext "$KEXTS/PDArmPCI.kext" \
      -kext "$KEXTS/IOStorageFamily.kext" \
      -kext "$KEXTS/RavynAHCIPort.kext" \
      -kext "$KEXTS/ext4.kext" \
      -kext "$KEXTS/AppleFileSystemDriver.kext" \
      -kext "$KEXTS/Ext4FileSystemDriver.kext" \
      -kext "$KEXTS/msdosfs.kext" \
      -kext "$KEXTS/apfs.kext" \
      -kext "$KEXTS/HFSEncodings.kext" \
      -kext "$KEXTS/hfs.kext" \
      -kext "$KEXTS/IONVMEFamily.kext" \
      -kext "$KEXTS/IOHIDFamily.kext" \
      -kext "$KEXTS/IOUSBFamily.kext" \
      -kext "$KEXTS/IOUSBCompositeDriver.kext" \
      -kext "$KEXTS/AppleUSBMergeNub.kext" \
      -kext "$KEXTS/IOUSBHIDDriver.kext" \
      -kext "$KEXTS/AppleUSBEHCI.kext" \
      -kext "$KEXTS/AppleUSBOHCI.kext" \
      -kext "$KEXTS/RavynXHCIPort.kext" \
      -kext "$KEXTS/IONetworkingFamily.kext" \
      -kext "$KEXTS/IOVirtIOFamily.kext" \
      -kext "$KEXTS/IOVirtIOGPU.kext" \
      -kext "$KEXTS/IOVirtIONet.kext" \
      "''${codeless[@]}" \
      -o kernel
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp kernel $out/kernel
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin arm64-virt boot kernel collection (kernel + kexts fileset), assembled by kc-tools' kc-builder";
    platforms = platforms.linux;
  };
}
