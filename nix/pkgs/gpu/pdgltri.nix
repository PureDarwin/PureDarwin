{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, mesa
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# a gles2 triangle on egl's surfaceless platform, read back and checked
stdenv.mkDerivation {
  pname = "puredarwin-pdgltri";
  version = "0.1";

  src = ../../../src/Userspace/pdgltri;
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -O2 -Wall \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -fno-stack-protector -I${libSystem}/usr/include -I${mesa}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${mesa}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      pdgltri.c -lEGL -lGLESv2 -lSystem -o pdgltri
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/sbin
    cp pdgltri $out/usr/sbin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "GLES2 triangle check for PureDarwin's gpu drivers";
    license = lib.licenses.mit;
  };
}
