{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, glslang
, vulkanHeaders
, vulkanLoader
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# the offscreen triangle check (src/Tools/pdvktri) built for the venus image, run per icd to compare
# venus against lavapipe and the host's linux render
stdenv.mkDerivation {
  pname = "puredarwin-venus-tests";
  version = "0.1";
  src = ../../../src/Tools/pdvktri;

  nativeBuildInputs = [ glslang ];

  # venus offers every host device: take the one PDVKTRI_DEVICE names (default 0, the first), so the same binary
  # compares the host gpu with the host's llvmpipe
  postPatch = ''
    substituteInPlace pdvktri.c --replace-fail \
      'CK(vkEnumeratePhysicalDevices(inst, &n, &pd));' \
      '{ VkPhysicalDevice all[16]; uint32_t c = 16; const char *e = getenv("PDVKTRI_DEVICE"); uint32_t i = e ? (uint32_t)atoi(e) : 0; VkResult r_ = vkEnumeratePhysicalDevices(inst, &c, all); if (r_ < 0 || i >= c) { printf("pdvktri: no device %u of %u\nFAIL\n", i, c); return 1; } pd = all[i]; n = c; }'
  '';

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    glslangValidator -V --vn tri_vert -o tri_vert.h tri.vert
    glslangValidator -V --vn tri_frag -o tri_frag.h tri.frag
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -O2 -Wall \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I${vulkanHeaders}/include -I. \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${vulkanLoader}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      pdvktri.c -lvulkan -lSystem -o pdvktri
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 pdvktri $out/usr/local/bin/pdvktri
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Offscreen Vulkan triangle for comparing venus with lavapipe";
    platforms = platforms.unix;
  };
}
