{ lib
, runCommand
, mkfontscale
, dejavu_fonts
}:

runCommand "puredarwin-fonts" { nativeBuildInputs = [ mkfontscale ]; } ''
  mkdir -p "$out/usr/share/fonts"
  cp ${dejavu_fonts}/share/fonts/truetype/*.ttf "$out/usr/share/fonts/"
  chmod u+w "$out/usr/share/fonts"/*.ttf

  cd "$out/usr/share/fonts"
  mkfontscale .
  mkfontdir .
''
