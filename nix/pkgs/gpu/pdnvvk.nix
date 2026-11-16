{ stdenv
, lib
, glslang
, vulkan-headers
, vulkan-loader
}:

# Linux Vulkan checks run under elftrans on NVIDIA's driver: pdvkhead (devices, memory, a buffer) and
# pdvktri (a triangle on the GPU's own display through VK_KHR_display), and pdcuda (the CUDA driver API: a
# context, PTX JIT and a vector add)
stdenv.mkDerivation {
  pname = "puredarwin-pdnvvk";
  version = "0.1";
  src = ../../../src/Tools/pdnvvk;

  nativeBuildInputs = [ glslang ];
  buildInputs = [ vulkan-headers vulkan-loader ];

  buildPhase = ''
    runHook preBuild
    glslangValidator -V --vn tri_vert -o tri_vert.h tri.vert
    glslangValidator -V --vn tri_frag -o tri_frag.h tri.frag
    $CC -O2 -Wall pdvkhead.c -lvulkan -o pdvkhead
    $CC -O2 -Wall -I. pdvktri.c -lvulkan -o pdvktri
    $CC -O2 -Wall pdcuda.c -ldl -o pdcuda
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 pdvkhead $out/bin/pdvkhead
    install -Dm755 pdvktri $out/bin/pdvktri
    install -Dm755 pdcuda $out/bin/pdcuda
    runHook postInstall
  '';

  meta = with lib; {
    description = "Vulkan checks for NVIDIA's driver under elftrans";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.linux;
  };
}
