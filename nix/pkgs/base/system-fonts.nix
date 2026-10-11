# the system font set in /System/Library/Fonts, each family fetched from its upstream with its licence
{ lib
, stdenvNoCC
, fetchzip
, fetchFromGitHub
, source-code-pro
}:

let
  inter = fetchzip {
    url = "https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip";
    sha256 = "115avnh5g43hjvrlpygrdxgl76c5knrsbpqaiax9hyf0y4m4mxz6";
    stripRoot = false;
  };
  urw = fetchFromGitHub {
    owner = "ArtifexSoftware";
    repo = "urw-base35-fonts";
    rev = "c15105598aa7eb256b1ebfcecd3d078801521e73";
    sha256 = "119zka2050ma3cgrz5y7s5b32b7ccb9bxykhaz9wcxb87ch7j2b1";
  };
in
stdenvNoCC.mkDerivation {
  pname = "puredarwin-system-fonts";
  version = "2026-10-09";

  dontUnpack = true;

  installPhase = ''
    runHook preInstall
    fonts="$out/System/Library/Fonts"

    # static faces, the variable font and the collection are not used by AppKit's FreeType path
    mkdir -p "$fonts/Inter"
    for face in Regular Italic Medium MediumItalic SemiBold SemiBoldItalic Bold BoldItalic; do
      install -m644 "${inter}/extras/otf/Inter-$face.otf" "$fonts/Inter/"
    done
    install -m644 "${inter}/LICENSE.txt" "$fonts/Inter/LICENSE.txt"

    # Nimbus Sans, Nimbus Mono PS and the rest of the PostScript base 35
    mkdir -p "$fonts/URW"
    install -m644 ${urw}/fonts/*.otf "$fonts/URW/"
    install -m644 "${urw}/LICENSE" "${urw}/COPYING" "$fonts/URW/"

    mkdir -p "$fonts/SourceCodePro"
    install -m644 ${source-code-pro}/share/fonts/opentype/*.otf "$fonts/SourceCodePro/"
    runHook postInstall
  '';

  meta = {
    description = "PureDarwin system fonts: Inter, URW base35 and Source Code Pro";
    license = with lib.licenses; [ ofl agpl3Only ];
    platforms = lib.platforms.all;
  };
}
