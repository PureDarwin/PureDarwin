{ stdenv
, lib
, kcTools
, kernel
, kernelSource
, kexts
, kextNames
, variant ? "virt-debug"
# RISCV64_KC_BASE in xnu's MakeInc.def.in, xnu finds the collection header there
, kcBase ? "ffffffff82000000"
}:

let
  darwinKernelVersion = "25.5.0";
  darwinKernelVersionShort = "25.5";
in
stdenv.mkDerivation {
  pname = "puredarwin-kc-riscv64-${variant}";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    # the codeless kpi bundles come from the xnu tree, the kernel output does not carry them
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

    kextArgs=()
    for k in ${lib.escapeShellArgs kextNames}; do
      kextArgs+=( -kext "$KEXTS/$k" )
    done

    # 4K pages and the arm64 collection layout, prelink text lands at 0xffffffff80004000
    ${kcTools}/bin/kc-builder \
      -kc-base ${kcBase} \
      -kernel "$KERNEL_BIN" \
      "''${kextArgs[@]}" \
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
    description = "PureDarwin riscv64 ${variant} boot kernel collection (kernel + kexts fileset), assembled by kc-tools' kc-builder";
    platforms = platforms.unix;
  };
}
