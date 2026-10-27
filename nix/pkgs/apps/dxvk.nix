{ stdenv
, lib
, dxvk
}:

# DXVK's Windows DLLs (built by nixpkgs with mingw, zlib licensed) and pd-dxvk, which puts them into a Wine prefix
stdenv.mkDerivation {
  pname = "puredarwin-dxvk";
  inherit (dxvk) version;

  dontUnpack = true;
  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall
    for arch in x64 x32; do
      install -d $out/usr/lib/dxvk/$arch
      cp ${dxvk.bin}/$arch/*.dll $out/usr/lib/dxvk/$arch/
    done
    install -Dm755 ${./pd-dxvk.sh} $out/usr/bin/pd-dxvk
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Direct3D 8 to 11 over Vulkan for Wine, with a prefix setup tool";
    license = licenses.zlib;
    platforms = platforms.unix;
  };
}
