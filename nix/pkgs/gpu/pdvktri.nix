{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, glslang
, vulkanHeaders
, vulkanLoader
, libdrmPdpvr
, targetTriple ? "arm64-apple-darwin20.4"
}:

# the offscreen triangle check for the powervr vulkan driver, with its spir-v built in, and a
# buffer lifetime check against the kext
stdenv.mkDerivation {
  pname = "puredarwin-pdvktri";
  version = "0.1";
  src = ../../../src/Tools/pdvktri;

  nativeBuildInputs = [ glslang ];

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
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -O2 -Wall \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I${libdrmPdpvr}/include -I${libdrmPdpvr}/include/libdrm -I. \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libdrmPdpvr}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      pdvklife.c -ldrm_pdpvr -lSystem -o pdvklife
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 pdvktri $out/usr/local/bin/pdvktri
    install -Dm755 pdvklife $out/usr/local/bin/pdvklife
    install -Dm644 org.puredarwin.pdvktri.plist $out/System/Library/LaunchDaemons/org.puredarwin.pdvktri.plist
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Offscreen Vulkan triangle on PDPowerVR, checked against the Linux render";
    platforms = platforms.unix;
  };
}
