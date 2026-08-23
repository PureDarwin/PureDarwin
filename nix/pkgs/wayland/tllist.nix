{ stdenv
, lib
, fetchurl
}:

# tllist is a single header implementing a typed linked list; fcft and foot
# both use it. Nothing is compiled, so this needs no cross toolchain - just the
# header and a pkg-config file for meson's dependency('tllist') to find.

stdenv.mkDerivation rec {
  pname = "puredarwin-tllist";
  version = "1.1.0";

  src = fetchurl {
    url = "https://codeberg.org/dnkl/tllist/archive/${version}.tar.gz";
    sha256 = "08sxgn11f9gmvzj2b9lb3l7hnmbi6vycyfr4ny0dsl15l2a70yqf";
  };

  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall

    install -Dm644 tllist.h "$out/include/tllist.h"

    mkdir -p "$out/lib/pkgconfig"
    cat > "$out/lib/pkgconfig/tllist.pc" <<EOF
prefix=$out
includedir=\''${prefix}/include

Name: tllist
Description: A C header file only implementation of a typed linked list
Version: ${version}
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Header-only typed linked list for C";
    homepage = "https://codeberg.org/dnkl/tllist";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
