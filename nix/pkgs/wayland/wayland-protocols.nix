{ stdenv
, lib
, src
}:

stdenv.mkDerivation {
  pname = "puredarwin-wayland-protocols";
  version = "1.48";
  inherit src;

  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/share" "$out/include"
    cp -a share/wayland-protocols "$out/share/"
    mkdir -p "$out/share/pkgconfig"
    cp -a share/pkgconfig/wayland-protocols.pc "$out/share/pkgconfig/"
    sed -i "s|^prefix=.*$|prefix=$out|" \
      "$out/share/pkgconfig/wayland-protocols.pc"
    # Meson is configuring a Darwin cross build.  There is no target sysroot
    # prefix to prepend to an absolute Nix store path here; doing so produces
    # //nix/store/... and makes wl.find_protocol reject valid XML files.
    substituteInPlace "$out/share/pkgconfig/wayland-protocols.pc" \
      --replace-fail 'pkgdatadir=''${pc_sysrootdir}''${datarootdir}/wayland-protocols' \
      'pkgdatadir=''${datarootdir}/wayland-protocols'
    cp -a include/wayland-protocols "$out/include/"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Wayland protocol XML descriptions for PureDarwin compositor builds";
    homepage = "https://gitlab.freedesktop.org/wayland/wayland-protocols";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
