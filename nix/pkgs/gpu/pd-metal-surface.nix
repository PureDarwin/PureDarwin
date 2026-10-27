{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, vulkanHeaders
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# VK_EXT_metal_surface as an implicit Vulkan layer (src/Tools/pdmetalsurface): a CAMetalLayer's Wayland subsurface
# becomes a VK_KHR_wayland_surface on the driver below, for Wine's Mac driver and MoltenVK-style apps
stdenv.mkDerivation {
  pname = "puredarwin-metal-surface-layer";
  version = "0.1";
  src = ../../../src/Tools/pdmetalsurface;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -O2 -Wall \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I${vulkanHeaders}/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -dynamiclib \
      -install_name /usr/lib/libVkLayer_pd_metal_surface.dylib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      pdmetalsurface.c -lSystem -o libVkLayer_pd_metal_surface.dylib
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 libVkLayer_pd_metal_surface.dylib $out/usr/lib/libVkLayer_pd_metal_surface.dylib
    install -Dm644 VkLayer_pd_metal_surface.json $out/usr/share/vulkan/implicit_layer.d/VkLayer_pd_metal_surface.json
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "VK_EXT_metal_surface over CAMetalLayer's Wayland subsurface";
    platforms = platforms.unix;
  };
}
