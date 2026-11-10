{ lib
, fetchurl
, dpkg
, stdenv
}:

# Debian's 32-bit shim, which 32-bit UEFI machines like bay trail tablets run with Secure Boot on
# it loads grubia32.efi from its own directory
stdenv.mkDerivation rec {
  pname = "shim-ia32";
  version = "16.1-2";

  src = fetchurl {
    url = "https://deb.debian.org/debian/pool/main/s/shim-signed/shim-signed_1.51~1+deb12u1+16.1-2~deb12u1_i386.deb";
    hash = "sha256-w0l9KiUn4E4HTGaM9KUk9sBbagWxUMjIiFR9TkONvKk=";
  };
  # MokManager, signed with Debian's key that the shim above trusts
  helpers = fetchurl {
    url = "https://deb.debian.org/debian/pool/main/s/shim-helpers-i386-signed/shim-helpers-i386-signed_1+16.1+2~deb12u1_i386.deb";
    hash = "sha256-azV+EeVYcb0n1/VdjAOEA5WX+gM7jd0y1DISk5HR1bc=";
  };

  nativeBuildInputs = [ dpkg ];

  unpackPhase = ''
    dpkg-deb -x $src .
    dpkg-deb -x $helpers .
  '';

  installPhase = ''
    mkdir -p $out/efi
    cp usr/lib/shim/shimia32.efi.signed $out/efi/shimia32.efi
    cp usr/lib/shim/mmia32.efi.signed $out/efi/mmia32.efi
  '';

  meta = {
    description = "Debian's Microsoft-signed 32-bit UEFI shim and MokManager";
    license = lib.licenses.bsd2;
    platforms = [ "x86_64-linux" ];
  };
}
