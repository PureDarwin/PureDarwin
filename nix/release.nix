# release.nix - an image as a release file, named the way NixOS names its ISOs, so a download says what it is
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#
#   puredarwin-<target>-<version>.<YYYYMMDD>.<HHMM>.<commit>-<arch>.img.zst
#   puredarwin-wayland-26.5.20261118.0629.591d1f137-x86_64.img.zst
#
# The date and time are the commit's, so the name is the same wherever and whenever it is built. A tree with
# uncommitted changes gets "-dirty" after the commit, and is never a release.
{ lib, runCommand, zstd, self }:

let
  pdVersion = (import ./version.nix).version;
  stamp = self.lastModifiedDate or "19700101000000";
  date = builtins.substring 0 8 stamp;
  time = builtins.substring 8 4 stamp;
  # a tree with uncommitted changes keeps its commit and is marked: 9ce76d19b-dirty
  commit = self.shortRev or self.dirtyShortRev or "unknown";
in
{
  inherit pdVersion commit;

  # target: the image's name without "image-" (wayland, minimal, arm64-virt-minimal), arch: x86_64, arm64, ...
  mkRelease = { target, arch, image }:
    let
      base = "puredarwin-${target}-${pdVersion}.${date}.${time}.${commit}-${arch}";
    in
    runCommand base { nativeBuildInputs = [ zstd ]; } ''
      mkdir -p $out/nix-support
      # the image derivation holds one disk image, whatever its own file is called
      img=$(find ${image} -maxdepth 1 -type f \( -name '*.img' -o -name '*.iso' -o -name '*.bin' \) | head -n 1)
      if [ -z "$img" ]; then
        echo "release: no image file in ${image}" >&2
        exit 1
      fi
      ext=''${img##*.}
      zstd -T0 -19 --long -q "$img" -o "$out/${base}.$ext.zst"
      (cd $out && sha256sum "${base}.$ext.zst" > "${base}.$ext.zst.sha256")
      echo "${base}" > $out/name
      echo "file disk-image $out/${base}.$ext.zst" >> $out/nix-support/hydra-build-products
      echo "file sha256 $out/${base}.$ext.zst.sha256" >> $out/nix-support/hydra-build-products
    '';
}
