{ stdenv
, lib
, kcTools
, kernel
, kernelSource
, kexts
, kcBase ? "fffffe0006000000"
# kext bundles from other builds, e.g. drm drivers built apart from the boot kexts
, extraKexts ? [ ]
# boot kexts a board leaves out, when its extra kexts would not fit below the kernel
, omitKexts ? [ ]
}:

let
  baseKexts = [
    "IOPCIFamily"
    "corecrypto"
    "pthread"
    "amfi"
    "Sandbox"
    "PDArmPlatformExpert"
    "PDArmPCI"
    "PDEcamPCI"
    "IOStorageFamily"
    "PDTegraSDMMC"
    "PDGadgetFamily"
    "PDTegraUSB"
    "PDPowerVR"
    "PDSun60iDisplay"
    "PDSg2002SD"
    "RavynAHCIPort"
    "ext4"
    "AppleFileSystemDriver"
    "Ext4FileSystemDriver"
    "msdosfs"
    "apfs"
    "ApfsFileSystemDriver"
    "HFSEncodings"
    "hfs"
    "IONVMEFamily"
    "IOHIDFamily"
    "IOUSBFamily"
    "IOUSBCompositeDriver"
    "AppleUSBMergeNub"
    "IOUSBHIDDriver"
    "PDI2CFamily"
    "PDQcomTLMM"
    "PDQcomGeniI2C"
    "PDI2CHID"
    "AppleUSBEHCI"
    "AppleUSBOHCI"
    "PDTegraXUSB"
    "RavynXHCIPort"
    "IOGraphicsFamily"
    "IOVirtIOFamily"
    "IOVirtIOGPU"
    "IOGOPFramebuffer"
    "PDTegraDisplay"
    "IONetworkingFamily"
    "PDSg2002Eth"
    "IOVirtIOBlock"
    "IOVirtIONet"
    "PDFBSDem"
    "PDWatchdog"
    "PDIOSurface"
    "PDDiskImages2"
    "PDLifs"
    "PDSerialHID"
    "PDKeyStore"
  ];
  darwinKernelVersion = "25.5.0";
  darwinKernelVersionShort = "25.5";
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
      -kc-base ${kcBase} \
      -kernel "$KERNEL_BIN" \
      ${lib.concatMapStrings (k: "-kext \"$KEXTS/${k}.kext\" \\\n      ") (lib.subtractLists omitKexts baseKexts)}\
      ${lib.concatMapStrings (k: "-kext \"${k}\" \\\n      ") extraKexts}\
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
    platforms = platforms.unix;
  };
}
