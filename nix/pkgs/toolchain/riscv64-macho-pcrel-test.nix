{ stdenv
, lib
, python3
, qemu
, llvmRiscvMachO
, nativeLd
}:

stdenv.mkDerivation {
  pname = "riscv64-macho-pcrel-test";
  version = "0.1";
  src = lib.fileset.toSource {
    root = ../../../tools/riscv64-macho;
    fileset = ../../../tools/riscv64-macho;
  };
  nativeBuildInputs = [ python3 qemu ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    mkdir work && cd work
    bash ../pcrel/check.sh ${llvmRiscvMachO.clang-unwrapped}/bin/clang ${nativeLd}/bin/ld \
      ../pcrel ../hello ../macho2bin.py qemu-system-riscv64 ${llvmRiscvMachO.llvm}/bin/llc | tee ../check.log
    cd ..
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp check.log work/qemu.log work/pcrel.macho $out/
    runHook postInstall
  '';
}
