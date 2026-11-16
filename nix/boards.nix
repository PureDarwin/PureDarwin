# boards.nix - the arm64 boards PureDarwin builds for, one entry each, and the kernels generated from them
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
let
  # machine: xnu's PUREDARWIN_ARM64_MACHINE_CONFIG, configs: the kernel configurations the board is built in,
  # kc: the collection recipe under nix/pkgs/toolchain, kcBase: where the collection header goes when not the default
  boards = {
    virt = { machine = "VIRT"; configs = [ "RELEASE" "DEBUG" ]; kc = "kc-arm64"; };
    virtamp = { machine = "VIRTAMP"; configs = [ "RELEASE" ]; kc = "kc-arm64"; };
    virt4k = { machine = "VIRT4K"; configs = [ "DEBUG" ]; kc = "kc-arm64"; kcBase = "fffffff006000000"; };
    bcm2837 = { machine = "BCM2837"; configs = [ "RELEASE" "DEBUG" ]; kc = "kc-arm64-bcm2837"; };
    sun50i = { machine = "SUN50I"; configs = [ "RELEASE" "DEBUG" ]; kc = "kc-arm64"; kcBase = "fffffff006000000"; };
    t8010 = { machine = "T8010"; configs = [ "RELEASE" "DEBUG" ]; kc = "kc-arm64-t8010"; };
    s8000 = { machine = "S8000"; configs = [ "RELEASE" "DEBUG" ]; kc = "kc-arm64-t8010"; kcBase = "fffffff006000000"; };
    t8103 = { machine = "T8103"; configs = [ "RELEASE" "DEBUG" ]; kc = "kc-arm64"; };
    sc8280xp = { machine = "SC8280XP"; configs = [ "RELEASE" "DEBUG" ]; kc = "kc-arm64"; };
    sdm670 = { machine = "SDM670"; configs = [ "RELEASE" ]; kc = "kc-arm64"; };
  };

  kernel = { lib, mkPureDarwinBuild, kernelSource, arm64CrossToolchain }: board: machine: config: mkPureDarwinBuild {
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
  # kernels args gives <board>.release / .debug
  kernels = { lib, ... }@args: lib.mapAttrs (board: b: lib.listToAttrs (map (config:
    lib.nameValuePair (lib.toLower config) (kernel args board b.machine config)) b.configs)) boards;
}
