{ stdenv
, lib
, kcTools
, kernel
, kernelSource
, kexts
# more kext bundle names from the same set, for a board that needs them
, extraKexts ? [ ]
# a separate collection header page at the kernel's ARM64_KC_BASE, kexts above it
, kcBase ? null
}:

stdenv.mkDerivation {
  pname = "puredarwin-kc-arm64-t8010";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    # the codeless KPI bundles (com.apple.kpi.*) live in the xnu tree, as in kc-arm64.nix
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
        -e 's/###KERNEL_VERSION_LONG###/25.5.0/g' \
        -e 's/###KERNEL_VERSION_SHORT###/25.5/g' \
        "$p/Info.plist" > "$CODELESS_DIR/$name/Info.plist"
      codeless+=( -codeless "$CODELESS_DIR/$name" )
    done

    ${kcTools}/bin/kc-builder \
      -kernel "$KERNEL_BIN" \
      -kext "$KEXTS/corecrypto.kext" \
      -kext "$KEXTS/PDArmPlatformExpert.kext" \
      -kext "$KEXTS/ext4.kext" \
      -kext "$KEXTS/Ext4FileSystemDriver.kext" \
      ${lib.concatMapStrings (k: "-kext \"$KEXTS/" + k + ".kext\" ") extraKexts}\
      "''${codeless[@]}" \
      ${lib.optionalString (kcBase != null) "-kc-base ${kcBase}"} \
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
    description = "PureDarwin arm64 T8010 (A10) boot kernel collection, assembled by kc-tools' kc-builder";
    platforms = platforms.unix;
  };
}
