{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
in
stdenv.mkDerivation {
  pname = "puredarwin-vmprobe";
  version = "1";
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 -O0 -g0 \
      -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -lSystem \
      -o vmprobe ${./vmprobe.c}
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/bin
    cp vmprobe $out/usr/bin/
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "/usr/bin/vmprobe: dumps hw.memsize, hw.pagesize and every vm_statistics64 counter";
    platforms = platforms.unix;
  };
}
