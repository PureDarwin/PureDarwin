{ stdenv
, lib
, requireFile
, pkg-config
, gnumake
, autoconf
, automake
, libtool
, gettext
, util-macros
, darwinCrossToolchain
, nativeLd
, libSystem
, dillo
, fltk
, openssl
, libX11
, libxcb
, libXau
, libXdmcp
, libXext
, libXrender
, libXfixes
, libXft
, libxcbcursor
, libICE
, libSM
, fontconfig
, freetype2
, expat
, xorgproto
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  xDeps = [
    xorgproto
    libX11
    libxcb
    libXau
    libXdmcp
    libXext
    libXrender
    libXfixes
    libXft
    libxcbcursor
    libICE
    libSM
    fontconfig
    freetype2
    expat
  ];
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-dillo";
  inherit (dillo) version;
  src = dillo.src;

  nativeBuildInputs = [
    pkg-config
    gnumake
    autoconf
    automake
    libtool
    gettext
  ];

  buildInputs = xDeps ++ [ fltk ];

  configurePhase = ''
    runHook preConfigure

    export PATH=${autoconf}/bin:${automake}/bin:${libtool}/bin:${gettext}/bin:$PATH
    autoreconf -fiv

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" (map lib.getDev xDeps)}:${lib.makeSearchPath "share/pkgconfig" (map lib.getDev xDeps)}:${util-macros}/share/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export CXX="${darwinCrossToolchain}/bin/${targetTriple}-clang++"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export FLTK_CONFIG="${fltk}/bin/fltk-config"
    export CPPFLAGS="-I${libSystem}/usr/include -I${openssl}/include ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") xDeps}"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector"
    export CXXFLAGS="$CFLAGS"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${openssl}/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") xDeps} -Wl,-force_load,${libxcb}/lib/libxcb.a -Wl,-force_load,${libXau}/lib/libXau.a -Wl,-force_load,${libXdmcp}/lib/libXdmcp.a -Wl,-force_load,${freetype2}/lib/libfreetype.a -Wl,-force_load,${fontconfig}/lib/libfontconfig.a -Wl,-force_load,${expat}/lib/libexpat.a -Wl,-force_load,${openssl}/lib/libssl.a -Wl,-force_load,${openssl}/lib/libcrypto.a -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --disable-png --disable-webp --disable-jpeg --disable-brotli \
      --enable-tls --enable-openssl --disable-mbedtls \
      --disable-threaded-dns \
      --with-ca-certs-file=/etc/ssl/cert.pem

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES AR="$AR" RANLIB="$RANLIB"
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install AR="$AR" RANLIB="$RANLIB"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Dillo web browser, cross-built for PureDarwin against the X11 FLTK port (no JPEG/PNG/WebP)";
    platforms = platforms.linux;
  };
}
