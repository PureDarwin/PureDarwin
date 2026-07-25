{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, pkgconf
, autoconf
, automake
, libtool
, gnumake
}:

let
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
  pname = "puredarwin-pkgconf";
  inherit (pkgconf) version src;

  nativeBuildInputs = [ autoconf automake libtool gnumake ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang"
    export AR="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar"
    export RANLIB="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip"
    export CPPFLAGS="-I${libSystem}/usr/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -lSystem"

    ./autogen.sh

    ./configure \
      --host=x86_64-apple-darwin20.4 \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --with-system-libdir=/usr/lib \
      --with-system-includedir=/usr/include \
      --disable-shared \
      --enable-static

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install DESTDIR="$out"
    mkdir -p "$out/usr/bin"
    ln -sf pkgconf "$out/usr/bin/pkg-config"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "pkgconf, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
