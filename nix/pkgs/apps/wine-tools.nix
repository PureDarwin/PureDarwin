{ stdenv
, lib
, pkg-config
, gnumake
, flex
, bison
, wine
, freetype
}:

stdenv.mkDerivation {
  pname = "wine-native-tools";
  inherit (wine) version;
  src = wine.src;

  patches = [ ./patches/wine-arm64ec-import-lib.patch ];

  nativeBuildInputs = [ pkg-config gnumake flex bison ];
  # sfnt2fon converts the bundled TTFs to .fon bitmaps and needs real freetype;
  # this is the build host's, since these tools only ever run here.
  buildInputs = [ freetype ];

  configurePhase = ''
    runHook preConfigure

    ./configure \
      --prefix="$out" \
      --enable-win64 \
      --without-x \
      --without-mingw \
      --without-alsa \
      --without-capi \
      --without-cups \
      --without-dbus \
      --without-fontconfig \
      --without-gnutls \
      --without-gssapi \
      --without-gstreamer \
      --without-krb5 \
      --without-netapi \
      --without-opencl \
      --without-oss \
      --without-pcap \
      --without-pulse \
      --without-sane \
      --without-sdl \
      --without-udev \
      --without-unwind \
      --without-usb \
      --without-v4l2 \
      --without-vulkan \
      --without-wayland

    runHook postConfigure
  '';

  # __tooldeps__ builds just the tools, not the whole of Wine.
  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES __tooldeps__
    runHook postBuild
  '';

  # The cross build wants the build tree itself (it looks for tools/winebuild
  # relative to what --with-wine-tools names), so keep the layout intact.
  installPhase = ''
    runHook preInstall
    mkdir -p "$out"
    cp -r . "$out/"
    runHook postInstall
  '';

  dontFixup = true;
}
