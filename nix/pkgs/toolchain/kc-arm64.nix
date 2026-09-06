{ stdenv
, lib
, kcTools
, kernel
, kernelSource
, kexts
}:

let
  darwinKernelVersion = "26.5.0";
  darwinKernelVersionShort = "26.5";
in
stdenv.mkDerivation {
  pname = "puredarwin-kc-arm64";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    # The codeless KPI bundles live in the XNU source tree, not the kernel
    # derivation's output; without them com.apple.kpi.bsd and friends are
    # missing and any kext depending on them fails dependency resolution.
    KERNEL_EXTS=${kernelSource}/src/Kernel/xnu/config/System.kext/PlugIns
    KEXTS=${kexts}/System/Library/Extensions

    KERNEL_BIN=$(ls "${kernel}"/System/Library/Kernels/kernel* | head -n1)

    codeless=()
    CODELESS_DIR="$TMPDIR/puredarwin-codeless"
    mkdir -p "$CODELESS_DIR"
    for p in "$KERNEL_EXTS"/*.kext; do
      name=$(basename "$p")
      mkdir -p "$CODELESS_DIR/$name"
      sed \
        -e 's/###KERNEL_VERSION_LONG###/${darwinKernelVersion}/g' \
        -e 's/###KERNEL_VERSION_SHORT###/${darwinKernelVersionShort}/g' \
        "$p/Info.plist" > "$CODELESS_DIR/$name/Info.plist"
      codeless+=( -codeless "$CODELESS_DIR/$name" )
    done

    # Must match ARM64_KC_BASE in xnu's MakeInc.def.in: XNU locates the
    # collection header at VM_KERNEL_LINK_ADDRESS, which is compiled in.
    ${kcTools}/bin/kc-builder \
      -kc-base fffffe0006000000 \
      -kernel "$KERNEL_BIN" \
      -kext "$KEXTS/IOPCIFamily.kext" \
      -kext "$KEXTS/corecrypto.kext" \
      -kext "$KEXTS/pthread.kext" \
      -kext "$KEXTS/amfi.kext" \
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
      -kext "$KEXTS/IOGraphicsFamily.kext" \
      -kext "$KEXTS/IOVirtIOFamily.kext" \
      -kext "$KEXTS/IOVirtIOGPU.kext" \
      -kext "$KEXTS/IOGOPFramebuffer.kext" \
      -kext "$KEXTS/IONetworkingFamily.kext" \
      -kext "$KEXTS/IOVirtIOBlock.kext" \
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
