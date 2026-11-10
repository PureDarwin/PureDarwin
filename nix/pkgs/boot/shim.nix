{ lib
, fetchurl
, rpmextract
, stdenv
}:

# Fedora's shim, which firmware with the default keys runs with Secure Boot on
# it loads grubx64.efi from its own directory once that passes its checks
stdenv.mkDerivation rec {
  pname = "shim";
  version = "16.1-7";

  src = fetchurl {
    url = "https://kojipkgs.fedoraproject.org/packages/shim/16.1/7/x86_64/shim-x64-${version}.x86_64.rpm";
    hash = "sha256-ChGfNIjl2ifKVcGGTujhMa+l7PJ2iThboJuAXrAN1gg=";
  };

  nativeBuildInputs = [ rpmextract ];

  unpackPhase = ''
    rpmextract $src
  '';

  installPhase = ''
    mkdir -p $out/efi
    cp usr/lib/efi/shim/*/EFI/*/mmx64.efi $out/efi/
    cp usr/lib/efi/shim/*/EFI/*/shimx64.efi $out/efi/
  '';

  meta = {
    description = "Fedora's Microsoft-signed UEFI shim and MokManager";
    license = lib.licenses.bsd2;
    platforms = [ "x86_64-linux" ];
  };
}
