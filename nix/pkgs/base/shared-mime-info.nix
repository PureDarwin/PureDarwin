# the freedesktop mime database in /usr/share/mime, which glib reads to tell gtk and webkit what a file is
# it is data under the gpl, shipped on its own with its licence and source note as the dtbs are
{ lib
, stdenvNoCC
, shared-mime-info
}:

stdenvNoCC.mkDerivation {
  pname = "puredarwin-shared-mime-info";
  inherit (shared-mime-info) version;

  dontUnpack = true;

  installPhase = ''
    runHook preInstall

    # the compiled database (mime.cache and the per-type xml) is architecture independent
    mkdir -p $out/usr/share $out/usr/share/licenses/shared-mime-info
    cp -rL ${shared-mime-info}/share/mime $out/usr/share/mime
    chmod -R u+w $out/usr/share/mime

    lic=$out/usr/share/licenses/shared-mime-info
    tar -xf ${shared-mime-info.src} --wildcards --strip-components=1 -C $lic '*/COPYING' 2>/dev/null \
      || cp ${shared-mime-info.src}/COPYING $lic/COPYING
    cat > $lic/SOURCE <<NOTE
/usr/share/mime is the database from shared-mime-info ${shared-mime-info.version},
built by nixpkgs with its own update-mime-database. It is GPL-2.0-or-later, the text is in COPYING.
Corresponding source: ${shared-mime-info.src.url or shared-mime-info.meta.homepage}
Source hash: ${shared-mime-info.src.outputHash or "unknown"}
NOTE

    runHook postInstall
  '';

  meta = {
    description = "freedesktop.org MIME database for PureDarwin";
    license = lib.licenses.gpl2Plus;
  };
}
