# boards.nix - the arm64 boards PureDarwin builds for, one entry each, and the kernels generated from them
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
{ lib, mkPureDarwinBuild, kernelSource, arm64CrossToolchain }:

let
  # machine: xnu's PUREDARWIN_ARM64_MACHINE_CONFIG; configs: the kernel configurations the board is built in
  boards = {
    virt = { machine = "VIRT"; configs = [ "RELEASE" "DEBUG" ]; };
    virtamp = { machine = "VIRTAMP"; configs = [ "RELEASE" ]; };
    virt4k = { machine = "VIRT4K"; configs = [ "DEBUG" ]; };
    bcm2837 = { machine = "BCM2837"; configs = [ "RELEASE" "DEBUG" ]; };
    sun50i = { machine = "SUN50I"; configs = [ "RELEASE" "DEBUG" ]; };
    t8010 = { machine = "T8010"; configs = [ "RELEASE" "DEBUG" ]; };
    s8000 = { machine = "S8000"; configs = [ "RELEASE" "DEBUG" ]; };
    t8103 = { machine = "T8103"; configs = [ "RELEASE" "DEBUG" ]; };
    sc8280xp = { machine = "SC8280XP"; configs = [ "RELEASE" "DEBUG" ]; };
  };

  kernel = board: machine: config: mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm64-${board}${lib.optionalString (config != "RELEASE") "-${lib.toLower config}"}";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = config;
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=${machine}" ];
  };
in
{
  inherit boards;
  # kernels.<board>.release / .debug
  kernels = lib.mapAttrs (board: b: lib.listToAttrs (map (config:
    lib.nameValuePair (lib.toLower config) (kernel board b.machine config)) b.configs)) boards;
}
