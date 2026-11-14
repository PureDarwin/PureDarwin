# outputs.nix - the flake's outputs for one system, sorted from the full list of everything the flake builds
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
{ lib, all, pd, pdArm64, tests }:

let
  has = lib.hasPrefix;
  ends = lib.hasSuffix;
  inGroup = pred: lib.filterAttrs (n: _: pred n) all;
  strip = suffix: set: lib.mapAttrs' (n: v: lib.nameValuePair (lib.removeSuffix suffix n) v) set;

  isImage = n: (has "image" n && n != "image-viewer") || has "ramdisk-" n || has "netboot-" n || n == "iso-installer";
  isKernel = n: has "kernel" n || has "kexts" n || has "drm-" n || has "prelinked-" n || has "xnu" n
    || n == "iographics" || n == "iovirtiogpu-kext" || has "basesystem" n || n == "default"
    || n == "initrd-riscv64-virt";
  isKc = n: n == "kc" || has "kc-" n && n != "kc-tools-guest";
  isFirmware = n: ends "-firmware" n || has "nouveau-firmware" n || has "amdgpu-firmware" n;
  isToolchain = n: builtins.elem n [ "clang" "clang-arm64" "llvm" "llvm-arm64" "llvm-cross" "llvm-riscv-macho" "native-ld"
    "darwin-cross-toolchain" "darwin-toolchain" "arm64-cross-toolchain" "riscv64-cross-toolchain" "dsymutil" "xxd"
    "xxd-arm64" "migcomDarwin" "migcomDarwin-arm64" "swift-toolchain" "apple-sdk" "mesa-pvr-native-tools"
    "kc-tools-guest" "debugserver" "mingw-aarch64-cc" "mingw-aarch64-crt" "mingw-arm64ec-cc" "xnu-loader-ia32" ]
    || has "compiler-rt" n;
  isRunner = n: ends "-runner" n;
  isOtherTarget = n: lib.any (s: ends s n || lib.hasInfix s n) [ "-armv6" "-riscv64" "-arm32-" ] || has "riscv64-macho" n;
  isArm64 = n: ends "-arm64" n || lib.hasInfix "-arm64-" n;

  claimed = n: isImage n || isKernel n || isKc n || isFirmware n || isToolchain n || isRunner n || isOtherTarget n;
  # everything else is a package of its target: added to the set, whose own members win on a shared name
  extra = pred: lib.filterAttrs (n: _: !(claimed n) && pred n) all;
in
{
  # what people build: the images
  packages = inGroup isImage;
  legacyPackages = {
    pd = {
      x86_64 = extra (n: !(isArm64 n)) // pd;
      arm64 = strip "-arm64" (extra isArm64) // pdArm64;
      armv6 = strip "-armv6" (inGroup (n: lib.hasInfix "-armv6" n));
      riscv64 = strip "-riscv64" (inGroup (n: lib.hasInfix "riscv64" n && !(isImage n) && !(isKc n) && !(isRunner n)));
      arm32 = inGroup (n: lib.hasInfix "-arm32-" n && !(isKernel n));
    };
    kernel = inGroup (n: isKernel n && !(isImage n));
    kc = lib.mapAttrs' (n: v: lib.nameValuePair (if n == "kc" then "default" else lib.removePrefix "kc-" n) v) (inGroup isKc);
    firmware = inGroup isFirmware;
    toolchain = inGroup isToolchain;
    runners = inGroup isRunner;
    inherit tests;
  };
}
