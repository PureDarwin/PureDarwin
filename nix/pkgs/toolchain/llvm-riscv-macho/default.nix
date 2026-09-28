# nixpkgs llvm 21 with mach-o output for riscv32 and riscv64, the relocation model
# ld64 implements, next to the tree's own clang so nothing else rebuilds
{ llvmPackages_21 }:

llvmPackages_21.overrideScope (final: prev: {
  libllvm = prev.libllvm.overrideAttrs (old: {
    patches = (old.patches or [ ]) ++ [ ./llvm-riscv-macho.patch ];
    # upstream's triple tests expect riscv to always be elf
    doCheck = false;
  });
  # just the compiler, clang-tools-extra and split debug info cost gigabytes nobody uses here
  libclang = (prev.libclang.override { enableClangToolsExtra = false; }).overrideAttrs (old: {
    patches = (old.patches or [ ]) ++ [ ./clang-riscv-macho.patch ];
    separateDebugInfo = false;
  });
})
