{ stdenv
, lib
, glslang
, vulkan-headers
, vulkan-loader
}:

# Linux Vulkan checks run under elftrans on NVIDIA's driver: pdvkhead (devices, memory, a buffer) and
# pdvktri (a triangle on the GPU's own display through VK_KHR_display)
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
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 pdvkhead $out/bin/pdvkhead
    install -Dm755 pdvktri $out/bin/pdvktri
    runHook postInstall
  '';

  meta = with lib; {
    description = "Vulkan checks for NVIDIA's driver under elftrans";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.linux;
  };
}
