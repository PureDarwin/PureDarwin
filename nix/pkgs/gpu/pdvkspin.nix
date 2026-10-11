{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, glslang
, vulkanHeaders
, vulkanLoader
, iokit
, corefoundation
, targetTriple ? "arm64-apple-darwin20.4"
}:

# a spinning cube on the powervr vulkan driver, copied into the console framebuffer each frame
stdenv.mkDerivation {
  pname = "puredarwin-pdvkspin";
  version = "0.1";
  src = ../../../src/Tools/pdvkspin;

  nativeBuildInputs = [ glslang ];

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    glslangValidator -V --vn cube_vert -o cube_vert.h cube.vert
    glslangValidator -V --vn cube_frag -o cube_frag.h cube.frag
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -O2 -Wall \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I${corefoundation}/include -I${vulkanHeaders}/include -I. \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${vulkanLoader}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      pdvkspin.c pdfb.c -lvulkan -lIOKitCF -lCoreFoundation -lSystem -o pdvkspin
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -O2 -Wall \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I${vulkanHeaders}/include -I. \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${vulkanLoader}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      pdvkdisplay.c -lvulkan -lSystem -o pdvkdisplay
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 pdvkspin $out/usr/local/bin/pdvkspin
    install -Dm755 pdvkdisplay $out/usr/local/bin/pdvkdisplay
    install -Dm644 org.puredarwin.pdvkspin.plist $out/System/Library/LaunchDaemons/org.puredarwin.pdvkspin.plist
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Spinning Vulkan cube on PDPowerVR, presented by copying into the console framebuffer";
    platforms = platforms.unix;
  };
}
