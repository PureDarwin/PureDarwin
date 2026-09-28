{ stdenv
, lib
, python3
, llvmRiscvMachO
, nativeLd
}:

stdenv.mkDerivation {
  pname = "riscv64-macho-hello";
  version = "0.1";
  src = lib.fileset.toSource {
    root = ../../../tools/riscv64-macho;
    fileset = ../../../tools/riscv64-macho;
  };
  nativeBuildInputs = [ python3 ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    clang=${llvmRiscvMachO.clang-unwrapped}/bin/clang
    flags="-target riscv64-apple-macos -O2 -ffreestanding -fasynchronous-unwind-tables -mcmodel=medany"
    $clang $flags -c hello/start.s -o start.o
    $clang $flags -c hello/kmain.c -o kmain.o
    ${nativeLd}/bin/ld -arch riscv64 -preload -e _start -pagezero_size 0 \
      -segaddr __TEXT 0x80200000 -o hello.macho start.o kmain.o
    python3 macho2bin.py hello.macho hello.bin
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp hello.macho hello.bin $out/
    runHook postInstall
  '';
}
