targetTriple:

let
  match = builtins.match "([^-]+)-.*" targetTriple;
  arch =
    if match == null
    then throw "target-info: cannot parse target triple ${targetTriple}"
    else builtins.head match;
in {
  inherit arch;
  mesonCpuFamily = if arch == "arm64" then "aarch64" else arch;
  mesonCpu = arch;
  mesonEndian = "little";
  mesonPlatformCArgs = if arch == "arm64" then "'-DXNU_PLATFORM_MacOSX=1', " else "";
  clangTarget = "${arch}-apple-macosx26.5";
}
