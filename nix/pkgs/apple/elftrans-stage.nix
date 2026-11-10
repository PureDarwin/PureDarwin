{ stdenvNoCC
, lib
, makeWrapper
, python3
, patchelf
, binutils
, e2fsprogs
, glibc
, elfaot
}:

stdenvNoCC.mkDerivation {
  pname = "puredarwin-elftrans-stage";
  version = "0.1";
  dontUnpack = true;
  nativeBuildInputs = [ makeWrapper ];

  installPhase = ''
    runHook preInstall
    install -Dm644 ${../../../src/Tools/elftrans/stage_glibc.py} \
      $out/libexec/elftrans/stage_glibc.py
    mkdir -p $out/bin
    makeWrapper ${python3}/bin/python3 $out/bin/elftrans-stage \
      --add-flags "$out/libexec/elftrans/stage_glibc.py" \
      --prefix PATH : ${lib.makeBinPath [ patchelf binutils e2fsprogs glibc.bin elfaot ]}
    runHook postInstall
  '';

  meta = with lib; {
    description = "Stage a Linux ELF and its glibc library closure for PureDarwin";
    platforms = platforms.linux;
  };
}
