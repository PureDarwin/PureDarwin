# nvidia-uvm-src.nix - nvidia-uvm and the common headers from NVIDIA's 580 package, unmodified, for PDNVUVM
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
{ stdenvNoCC, nvidia580 }:

stdenvNoCC.mkDerivation {
  pname = "nvidia-uvm-src";
  inherit (nvidia580) version src nativeBuildInputs unpackPhase;

  # the MIT sources only, each under its own notice, with the package's LICENSE beside them. uvm_common.c is the
  # one GPL file in nvidia-uvm: it is left out unread and PDNVUVM carries its own in its place
  installPhase = ''
    runHook preInstall
    cd x
    mkdir -p $out
    cp -r kernel/nvidia-uvm kernel/common $out/
    rm $out/nvidia-uvm/uvm_common.c
    # the interface layer's UVM side, which nvidia.ko carries on Linux: nvUvmInterface* over the RM's rm_gpu_ops_*
    mkdir -p $out/nvidia
    cp kernel/nvidia/nv_uvm_interface.c kernel/nvidia/nv_gpu_ops.h $out/nvidia/
    # every C file kept carries NVIDIA's MIT notice and none mentions the GPL
    for f in $out/nvidia-uvm/*.c $out/nvidia-uvm/*.h $out/nvidia/*; do
      grep -q "Permission is hereby granted, free of charge" "$f" || { echo "no MIT notice: $f"; exit 1; }
      ! grep -q "GNU General Public" "$f" || { echo "GPL text: $f"; exit 1; }
    done
    install -Dm644 LICENSE $out/LICENSE
    runHook postInstall
  '';

  dontFixup = true;
}
