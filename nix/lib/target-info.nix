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

  # cdefs.h selects the __DARWIN_ONLY_* set from XNU_PLATFORM_*; with none
  # defined it assumes the x86_64 convention, where $INODE64/$UNIX2003 symbol
  # suffixes exist. arm64 never had them, so an arm64 object built without this
  # asks for e.g. _fstat$INODE64 and fails to bind at launch. The toolchain
  # wrapper passes it for compilers it wraps; meson cross files and hand-rolled
  # cc lines that invoke clang directly have to pass it themselves.
  mesonPlatformCArgs =
    if arch == "arm64" then "'-DXNU_PLATFORM_MacOSX=1', " else "";

  # The -target clang expects, which is spelled differently from the triple
  # (macosx + a version, no vendor-style darwinNN). Passing an explicit -target
  # overrides whatever the toolchain wrapper set, so a hardcoded one silently
  # produces objects for the wrong architecture rather than failing.
  clangTarget = "${arch}-apple-macosx26.5";
}
