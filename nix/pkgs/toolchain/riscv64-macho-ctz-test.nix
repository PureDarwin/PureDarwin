{ stdenv
, lib
, python3
, qemu
, llvmRiscvMachO
, nativeLd
}:

stdenv.mkDerivation {
  pname = "riscv64-macho-ctz-test";
  version = "0.1";
  src = lib.fileset.toSource {
    root = ../../../tools/riscv64-macho;
    fileset = ../../../tools/riscv64-macho;
  };
  nativeBuildInputs = [ python3 qemu ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    clang=${llvmRiscvMachO.clang-unwrapped}/bin/clang
    flags="-target riscv64-apple-darwin20.4 -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany -mkernel -ffreestanding -fno-stack-protector"
    $clang $flags -O2 -c ctz/ctz_a.c -o ctz_a.o
    $clang $flags -O0 -c ctz/ctz_b.c -o ctz_b.o
    # temporary LCPI labels, what objects from before the linker-private names look like
    $clang $flags -O2 -S ctz/ctz_c.c -o - | sed 's/lCPI/LCPI/g' > ctz_c.s
    $clang $flags -c ctz_c.s -o ctz_c.o
    $clang $flags -O2 -c ctz/ctzmain.c -o ctzmain.o
    $clang $flags -O2 -c hello/start.s -o start.o
    $clang $flags -O2 -Dkmain=kmain_unused -c hello/kmain.c -o hk.o
    ${nativeLd}/bin/ld -arch riscv64 -preload -e _start -pagezero_size 0 \
      -segaddr __TEXT 0x80200000 -o ctz.macho start.o ctzmain.o hk.o ctz_a.o ctz_b.o ctz_c.o
    python3 ctz/check_tables.py ctz.macho
    python3 macho2bin.py ctz.macho ctz.bin
    timeout 60 qemu-system-riscv64 -machine virt -nographic -bios default -kernel ctz.bin -m 256M \
      > qemu.log 2>&1 || true
    grep -a "ctz checks" qemu.log
    grep -aq "ctz checks [0-9]* mismatches 0" qemu.log
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp ctz.macho ctz.bin qemu.log $out/
    runHook postInstall
  '';
}
