{ stdenv
, lib
, llvmRiscvMachO
, nativeLd
, libSystem
}:

stdenv.mkDerivation {
  pname = "riscv64-macho-libcalls-test";
  version = "0.1";
  src = lib.fileset.toSource {
    root = ../../../tools/riscv64-macho/libcalls;
    fileset = ../../../tools/riscv64-macho/libcalls;
  };
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    mkdir work && cd work
    bash ../check.sh ${llvmRiscvMachO.clang-unwrapped}/bin/clang ${nativeLd}/bin/ld \
      ${llvmRiscvMachO.llvm}/bin/llvm-objdump .. ${libSystem} | tee ../check.log
    cd ..
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp check.log work/liblibcalls.dylib $out/
    runHook postInstall
  '';
}
