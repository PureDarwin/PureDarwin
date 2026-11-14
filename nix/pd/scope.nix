# scope.nix - one PureDarwin package set per target: each package is written once in packages.nix, its PureDarwin
# dependencies are found by name in the same target's set, nixpkgs inputs are passed by name at the call
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
# shared: the inputs that are the same for every target (source trees, the SDK, host tools)
{ lib, pkgs, shared ? { } }:

# target: the cross toolchain and triple the set builds with, nativeLd, and (while the move is under way) the
# packages not yet in packages.nix, by the names the recipes take them
{ darwinCrossToolchain, targetTriple ? null, nativeLd, bridge ? { } }:

lib.makeScope pkgs.newScope (self:
  shared // bridge // {
    inherit darwinCrossToolchain nativeLd;
  }
  # x86_64 recipes take their default triple, an explicit one here would still be the same value
  // lib.optionalAttrs (targetTriple != null) { inherit targetTriple; }
  // import ./packages.nix { inherit pkgs lib self; inherit (self) callPackage; })
