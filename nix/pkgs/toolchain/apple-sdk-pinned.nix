# The MacOSX11.3 SDK, packaged in the layout nixpkgs' cc-wrapper expects from
# an apple-sdk so a Darwin host can be pointed at it via DEVELOPER_DIR.
{ stdenvNoCC
, lib
, requireFile
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
in
stdenvNoCC.mkDerivation {
  pname = "puredarwin-apple-sdk";
  version = "11.3";

  src = sdkTarball;

  sourceRoot = "MacOSX11.3.sdk";

  dontConfigure = true;
  dontBuild = true;
  dontFixup = true;

  installPhase = ''
    runHook preInstall
    sdks="$out/Platforms/MacOSX.platform/Developer/SDKs"
    mkdir -p "$sdks"
    cp -R . "$sdks/MacOSX.sdk"
    # The SDK's bundled libc++ under usr/include/c++/v1 is kept: targets that
    # pass -nostdinc reference it directly, and it is the libc++ that matches
    # these C headers. build.nix makes it the only libc++ in scope on a Darwin
    # host by adding -nostdinc++ globally, so it cannot collide with the
    # toolchain's own.
    # xcrun and some of xnu's makefiles look for the versioned name too.
    ln -s MacOSX.sdk "$sdks/MacOSX11.3.sdk"
    runHook postInstall
  '';

  meta = with lib; {
    description = "Pinned MacOSX11.3 SDK in apple-sdk layout, for Darwin-host PureDarwin builds";
    platforms = platforms.darwin;
  };
}
