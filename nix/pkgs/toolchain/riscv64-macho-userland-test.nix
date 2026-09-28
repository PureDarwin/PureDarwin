{ stdenv
, lib
, python3
, llvmRiscvMachO
, nativeLd
}:

stdenv.mkDerivation {
  pname = "riscv64-macho-userland-test";
  version = "0.1";
  src = lib.fileset.toSource {
    root = ../../../tools/riscv64-macho/userland;
    fileset = ../../../tools/riscv64-macho/userland;
  };
  nativeBuildInputs = [ python3 ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    mkdir work && cd work
    bash ../check.sh ${llvmRiscvMachO.clang-unwrapped}/bin/clang ${nativeLd}/bin/ld \
      ${llvmRiscvMachO.llvm}/bin/llvm-objdump .. | tee ../check.log
    cd ..
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp check.log work/main work/libfoo.dylib work/dyld $out/
    runHook postInstall
  '';
}
