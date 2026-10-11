{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, appleSdk
, mesa
, libdrmSrc
, vulkanHeaders
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# cross-built probes for the native PDRadeon render user client.
stdenv.mkDerivation {
  pname = "puredarwin-radeon-gl-tests";
  version = "0.1";

  src = libdrmSrc;
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    shim=${../../../src/Tools/pdradeon}
    mkdir -p inc
    cp ${../../../src/Kernel/Extensions/PDRadeonFramebuffer/PDRadeonRenderShared.h} inc/PDRadeonRenderShared.h

    cc() {
      ${darwinCrossToolchain}/bin/${targetTriple}-clang \
        -isysroot "$SDK" -mmacosx-version-min=26.5 -O2 -Wall \
        -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
        -I${libSystem}/usr/include -I${corefoundation}/include -Iinc -Iinc/libdrm \
        -fuse-ld=${nativeLd}/bin/ld -nostdlib \
        -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
        -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
        -Wl,-platform_version,macos,26.5,26.5 "$@"
    }
    cc -I${mesa}/usr/include -L${mesa}/usr/lib $shim/radeon-gl-smoke.c -lEGL -lGLESv2 -lSystem -o radeon-gl-smoke
    cc -I${mesa}/usr/include -L${mesa}/usr/lib $shim/radeon-demo.c -lEGL -lGLESv2 -lIOKitCF -lCoreFoundation -lSystem -o radeon-demo
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 radeon-gl-smoke $out/usr/local/bin/radeon-gl-smoke
    install -Dm755 radeon-demo $out/usr/local/bin/radeon-demo
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Native Radeon submission tests";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
