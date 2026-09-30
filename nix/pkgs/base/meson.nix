{ stdenv
, lib
, meson
, python
}:

stdenv.mkDerivation {
  pname = "puredarwin-meson";
  inherit (meson) version src;

  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/usr/lib/meson" "$out/usr/bin"
    cp -a mesonbuild "$out/usr/lib/meson/"
    cp -a meson.py "$out/usr/lib/meson/"
    [ ! -d data ] || cp -a data "$out/usr/lib/meson/"

    cat > "$out/usr/bin/meson" <<'EOF'
#!/bin/sh
exec /usr/bin/python3 /usr/lib/meson/meson.py "$@"
EOF
    chmod +x "$out/usr/bin/meson"

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Meson build system, running under PureDarwin's CPython";
    homepage = "https://mesonbuild.com/";
    license = licenses.asl20;
    platforms = platforms.unix;
  };
}
