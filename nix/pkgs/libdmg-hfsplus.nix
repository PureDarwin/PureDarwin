{ stdenv
, lib
, fetchFromGitHub
, cmake
, zlib
}:

stdenv.mkDerivation {
  pname = "libdmg-hfsplus";
  version = "unstable-2022";

  src = fetchFromGitHub {
    owner = "planetbeing";
    repo = "libdmg-hfsplus";
    rev = "7ac55ec64c96f7800d9818ce64c79670e7f02b67";
    hash = "sha256-5HHb08GEPzgLQC8y9YyhGoin1Oxy2UtOCx/4Xmb4ATQ=";
  };

  patches = [ ../patches/libdmg-hfsplus-untar-ustar-prefix.patch ];

  nativeBuildInputs = [ cmake ];
  cmakeFlags = [
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    "-DCMAKE_SKIP_BUILD_RPATH=ON"
  ];
  buildInputs = [ zlib ];

  # Only the hfsplus tool is needed; the dmg tool drags in extra deps.
  buildPhase = ''
    runHook preBuild
    make hfsplus
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 hfs/hfsplus $out/bin/hfsplus
    runHook postInstall
  '';

  meta = with lib; {
    description = "Userspace HFS+ image manipulation (hfsplus tool)";
    platforms = platforms.linux;
  };
}
