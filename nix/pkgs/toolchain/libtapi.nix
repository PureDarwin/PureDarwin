{ stdenv, lib, fetchFromGitHub, cmake, ninja, python3 }:

stdenv.mkDerivation {
  pname = "apple-libtapi";
  version = "1300.6.5";

  src = fetchFromGitHub {
    owner = "tpoechtrager";
    repo = "apple-libtapi";
    rev = "640b4623929c923c0468143ff2a363a48665fa54";
    hash = "sha256-eXzQRB07AcH9nBgPoGpIQZbC4O/bIzkdbjJ442qM9VA=";
  };

  # riscv32/riscv64 in the TextAPI arch table so ld64 can link riscv .tbd stubs
  patches = [ ./libtapi-riscv.patch ];

  nativeBuildInputs = [ cmake ninja python3 ];

  # build.sh drives its own cmake invocation (it builds tapi out of a bundled
  # LLVM tree with a special target list), so keep Nix's cmake hook out of it.
  dontUseCmakeConfigure = true;

  postPatch = ''
    patchShebangs build.sh install.sh tools
  '';

  buildPhase = ''
    runHook preBuild
    export INSTALLPREFIX=$out
    export JOBS=$NIX_BUILD_CORES
    ./build.sh
    ./install.sh
    runHook postBuild
  '';

  dontInstall = true;

  meta = with lib; {
    description = "Apple TAPI library (Linux port used by osxcross), for linking against .tbd text stubs";
    homepage = "https://github.com/tpoechtrager/apple-libtapi";
    platforms = platforms.unix;
  };
}
