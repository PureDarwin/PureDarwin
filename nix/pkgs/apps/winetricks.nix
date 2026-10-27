{ stdenv
, lib
, winetricks
}:

# winetricks for PureDarwin: the upstream shell script, which needs only the image's sh, curl, unzip and cabextract
stdenv.mkDerivation {
  pname = "puredarwin-winetricks";
  inherit (winetricks) version src;

  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall
    install -Dm755 src/winetricks $out/usr/bin/winetricks
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Installs Windows runtimes and fonts into a Wine prefix";
    platforms = platforms.unix;
  };
}
