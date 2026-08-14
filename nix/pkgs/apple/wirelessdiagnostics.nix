{ stdenv
, lib
, src
}:

stdenv.mkDerivation {
  pname = "puredarwin-wirelessdiagnostics";
  version = "1";

  inherit src;

  dontConfigure = true;
  dontBuild = true;
  dontFixup = true;

  installPhase = ''
    runHook preInstall
    fw="$out/System/Library/PrivateFrameworks/WirelessDiagnostics.framework"
    mkdir -p "$fw/Versions/A/Headers"
    cp -a include/WirelessDiagnostics/. "$fw/Versions/A/Headers/"
    ln -s A "$fw/Versions/Current"
    ln -s Versions/Current/Headers "$fw/Headers"
    mkdir -p "$out/include"
    cp -a include/WirelessDiagnostics "$out/include/"
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin WirelessDiagnostics interface headers (weak-linked; no implementation by design)";
    platforms = platforms.unix;
  };
}
