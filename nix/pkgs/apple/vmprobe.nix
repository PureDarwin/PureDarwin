{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
in
stdenv.mkDerivation {
  pname = "puredarwin-vmprobe";
  version = "1";
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=11.0 -O0 -g0 \
      -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains \
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
