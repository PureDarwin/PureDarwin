# nvidia-drm-src.nix - nvidia-drm and the common headers from NVIDIA's 580 package, unmodified, for PDDRMnvidia
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
{ stdenvNoCC, nvidia580 }:

stdenvNoCC.mkDerivation {
  pname = "nvidia-drm-src";
  inherit (nvidia580) version src nativeBuildInputs unpackPhase;

  # MIT sources, each under its own notice, with the package's LICENSE beside them
  installPhase = ''
    runHook preInstall
    cd x
    mkdir -p $out
    cp -r kernel/nvidia-drm kernel/common $out/
    install -Dm644 kernel/nvidia-modeset/nvkms.h kernel/nvidia-modeset/nvidia-modeset-os-interface.h -t $out/nvidia-modeset
    install -Dm644 LICENSE $out/LICENSE
    runHook postInstall
  '';

  dontFixup = true;
}
