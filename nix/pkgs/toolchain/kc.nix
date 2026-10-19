{ stdenv
, lib
, kcTools
, kernel
, kernelSource
, kexts
# out-of-tree kexts, by bundle name, linked into an auxiliary KC against this boot KC
, auxKexts ? [ ]
# boot KC kexts left out, for images where an aux kext drives the same device
, excludeKexts ? [ ]
}:

let
  kextList = lib.subtractLists excludeKexts (import ../../lib/kc-kexts.nix);
  darwinKernelVersion = "25.5.0";
  darwinKernelVersionShort = "25.5";
in
stdenv.mkDerivation {
  pname = "puredarwin-kc";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    # The kernel derivation contains the linked kernel only. The codeless
    # KPI bundles live in the XNU source tree and are required for dependency
    # resolution (for example com.apple.kpi.bsd -> BSDKernel.kext).
    KERNEL_EXTS=${kernelSource}/src/Kernel/xnu/config/System.kext/PlugIns
    KEXTS=${kexts}/System/Library/Extensions

    # The kernel binary's filename depends on which build variant xnu was
    # configured for (RELEASE -> kernel, DEBUG -> kernel.debug, etc.) -
    # pick whichever one is actually present rather than hardcoding it.
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

    # out-of-tree kexts go into an auxiliary KC linked against this one
    aux=(${lib.concatMapStringsSep " " (k: "-aux-kext \"$KEXTS/" + k + "\"") auxKexts}${lib.optionalString (auxKexts != [ ]) " -aux-o aux.kc"})

    ${kcTools}/bin/kc-builder \
      -kernel "$KERNEL_BIN" \
${lib.concatMapStringsSep "\n" (k: "      -kext \"$KEXTS/" + k + "\" \\") kextList}
      "''${codeless[@]}" \
      "''${aux[@]}" \
      -o kernel
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp kernel $out/kernel
    ${lib.optionalString (auxKexts != [ ]) "cp aux.kc $out/aux.kc"}
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin boot kernel collection (kernel + kexts fileset), assembled by kc-tools' kc-builder";
    platforms = platforms.unix;
  };
}
