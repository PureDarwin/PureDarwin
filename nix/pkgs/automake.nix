{ stdenv
, lib
, automake
, perl
, autoconf
}:

stdenv.mkDerivation {
  pname = "puredarwin-automake";
  inherit (automake) version src;

  nativeBuildInputs = [ perl autoconf ];

  configurePhase = ''
    runHook preConfigure
    ./configure --prefix=/usr --datarootdir=/usr/share
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install DESTDIR="$out"
    for f in "$out"/usr/bin/*; do
      if [ -f "$f" ] && head -c2 "$f" | grep -q '^#!'; then
        sed -i '1s|^#!.*perl.*$|#!/usr/bin/perl|' "$f"
      fi
    done
    runHook postInstall
  '';

  doCheck = false;
  dontFixup = true;

  meta = with lib; {
    description = "GNU Automake, installed for PureDarwin (pure Perl - no cross-compiling needed)";
    platforms = platforms.linux;
  };
}
