{ stdenv
, lib
, llvmPackages_21
, spirv-headers
, vulkan-headers
, vulkan-loader
, mesa
}:

# air2spv (AIR bitcode from a metallib to Vulkan SPIR-V) and vkreplay (a captured airjitd job through
# airjitd's Vulkan executor, compared with the CPU path's output), built for the host
let
  llvm = llvmPackages_21.libllvm;
in
stdenv.mkDerivation {
  pname = "puredarwin-air2spv";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src/Tools;
    fileset = lib.fileset.unions [
      ../../../src/Tools/air2spv
      ../../../src/Tools/airjitd/bridge.h
      ../../../src/Tools/airjitd/vulkan
    ];
  };

  buildInputs = [ llvm spirv-headers vulkan-headers vulkan-loader ];

  buildPhase = ''
    runHook preBuild
    cd air2spv
    LIB="spv.c translate.c iface.c air.c cfg.c a2s.c"
    CFLAGS="-std=gnu11 -O2 -Wall -Wno-unused-parameter -I${lib.getDev llvm}/include -I${spirv-headers}/include"
    $CC $CFLAGS $LIB main.c -o air2spv -L${lib.getLib llvm}/lib -lLLVM
    # the executor opens the loader at run time: rpath it so it is found without LD_LIBRARY_PATH
    $CC $CFLAGS -I${vulkan-headers}/include $LIB vkreplay.c ../airjitd/vulkan/vk.c -o vkreplay \
      -L${lib.getLib llvm}/lib -lLLVM -Wl,-rpath,${vulkan-loader}/lib -ldl -lm
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 air2spv $out/bin/air2spv
    install -Dm755 vkreplay $out/bin/vkreplay
    # lavapipe for vkreplay without a GPU: VK_ICD_FILENAMES=$out/share/air2spv/lvp_icd.json
    install -Dm644 ${mesa}/share/vulkan/icd.d/lvp_icd.${stdenv.hostPlatform.parsed.cpu.name}.json \
      $out/share/air2spv/lvp_icd.json
    runHook postInstall
  '';

  meta = with lib; {
    description = "AIR to SPIR-V translator and Vulkan replay checker for airjitd captures";
    platforms = platforms.linux;
  };
}
