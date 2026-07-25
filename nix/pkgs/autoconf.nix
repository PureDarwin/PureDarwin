{ stdenv
, lib
, autoconf
, perl
, m4
}:

stdenv.mkDerivation {
  pname = "puredarwin-autoconf";
  inherit (autoconf) version src;

  nativeBuildInputs = [ perl m4 ];

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
    # Retarget the installed scripts' shebangs from this build's host Nix
    # store perl to the guest's future runtime path - patchShebangs goes
    # the wrong direction (it points *into* the store), so do it by hand.
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
    description = "GNU Autoconf, installed for PureDarwin (pure Perl/m4 - no cross-compiling needed)";
    platforms = platforms.linux;
  };
}
