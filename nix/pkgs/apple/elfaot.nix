{ stdenv, lib, capstone }:

stdenv.mkDerivation {
  pname = "puredarwin-elfaot";
  version = "0.1";
  dontUnpack = true;
  buildInputs = [ capstone ];

  buildPhase = ''
    runHook preBuild
    cc -O2 -Wall -Wextra -I${../../../src/Tools/elftrans} \
      ${../../../src/Tools/elftrans/elfaot.c} \
      -lcapstone -o elfaot
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 elfaot $out/bin/elfaot
    runHook postInstall
  '';

  meta = with lib; {
    description = "Prepare static Linux x86-64 ELF for relocated PureDarwin execution";
    platforms = platforms.linux;
  };
}
