{ stdenv
, lib
, kcTools
, kernel
, kexts
}:

stdenv.mkDerivation {
  pname = "puredarwin-kc-arm64-t8010";
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
      -kext "$KEXTS/ext4.kext" \
      -kext "$KEXTS/Ext4FileSystemDriver.kext" \
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
    description = "PureDarwin arm64 T8010 (A10) boot kernel collection, assembled by kc-tools' kc-builder";
    platforms = platforms.linux;
  };
}
