targetTriple:

let
  match = builtins.match "([^-]+)-.*" targetTriple;
  arch =
    if match == null
    then throw "target-info: cannot parse target triple ${targetTriple}"
    else builtins.head match;
in
{
  inherit arch;

  mesonCpuFamily = if arch == "arm64" then "aarch64" else arch;

  # cpu is the finer-grained name and meson places no constraints on it.
  mesonCpu = arch;

  # Both architectures PureDarwin targets are little-endian.
  mesonEndian = "little";

  # The -target clang expects, which is spelled differently from the triple
  # (macosx + a version, no vendor-style darwinNN). Passing an explicit -target
  # overrides whatever the toolchain wrapper set, so a hardcoded one silently
  # produces objects for the wrong architecture rather than failing.
  clangTarget = "${arch}-apple-macosx11.0";
}
