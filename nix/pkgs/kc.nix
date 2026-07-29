{ stdenv
, lib
, kcTools
, kernel
, kexts
}:

stdenv.mkDerivation {
  pname = "puredarwin-kc";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    KERNEL_EXTS=${kernel}/System/Library/Extensions
    KEXTS=${kexts}/System/Library/Extensions

    # The kernel binary's filename depends on which build variant xnu was
    # configured for (RELEASE -> kernel, DEBUG -> kernel.debug, etc.) -
    # pick whichever one is actually present rather than hardcoding it.
    KERNEL_BIN=$(ls "${kernel}"/System/Library/Kernels/kernel* | head -n1)

    codeless=()
    for p in "$KERNEL_EXTS"/System.kext/PlugIns/*.kext; do
      codeless+=( -codeless "$p" )
    done

    ${kcTools}/bin/kc-builder \
      -kernel "$KERNEL_BIN" \
      -kext "$KEXTS/corecrypto.kext" \
      -kext "$KEXTS/pthread.kext" \
      -kext "$KEXTS/IOACPIFamily.kext" \
      -kext "$KEXTS/IOPCIFamily.kext" \
      -kext "$KEXTS/AppleAPIC.kext" \
      -kext "$KEXTS/AppleI386GenericPlatform.kext" \
      -kext "$KEXTS/AppleI386PCI.kext" \
      -kext "$KEXTS/IOStorageFamily.kext" \
      -kext "$KEXTS/IONVMEFamily.kext" \
      -kext "$KEXTS/IOATAFamily.kext" \
      -kext "$KEXTS/IOATAFamily.kext/Contents/PlugIns/AppleIntelPIIXATA.kext" \
      -kext "$KEXTS/IOATABlockStorage.kext" \
      -kext "$KEXTS/ext4.kext" \
      -kext "$KEXTS/msdosfs.kext" \
      -kext "$KEXTS/apfs.kext" \
      -kext "$KEXTS/HFSEncodings.kext" \
      -kext "$KEXTS/hfs.kext" \
      -kext "$KEXTS/AppleFileSystemDriver.kext" \
      -kext "$KEXTS/Ext4FileSystemDriver.kext" \
      -kext "$KEXTS/IOHIDFamily.kext" \
      -kext "$KEXTS/IOUSBFamily.kext" \
      -kext "$KEXTS/IOUSBCompositeDriver.kext" \
      -kext "$KEXTS/AppleUSBMergeNub.kext" \
      -kext "$KEXTS/IOUSBHIDDriver.kext" \
      -kext "$KEXTS/AppleUSBEHCI.kext" \
      -kext "$KEXTS/AppleUSBOHCI.kext" \
      -kext "$KEXTS/AppleUSBUHCI.kext" \
      -kext "$KEXTS/RavynAHCIPort.kext" \
      -kext "$KEXTS/RavynXHCIPort.kext" \
      -kext "$KEXTS/IOGraphicsFamily.kext" \
      -kext "$KEXTS/IOGOPFramebuffer.kext" \
      -kext "$KEXTS/IOVirtIOFamily.kext" \
      -kext "$KEXTS/IOVirtIOGPU.kext" \
      -kext "$KEXTS/IOVirtIONet.kext" \
      -kext "$KEXTS/IONetworkingFamily.kext" \
      -kext "$KEXTS/IOIntelGen9Framebuffer.kext" \
      -kext "$KEXTS/PDE1000.kext" \
      -kext "$KEXTS/RavynHDAudio.kext" \
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
    description = "PureDarwin boot kernel collection (kernel + kexts fileset), assembled by kc-tools' kc-builder";
    platforms = platforms.linux;
  };
}
