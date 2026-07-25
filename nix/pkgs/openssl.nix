{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, openssl
, perl
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
  pname = "puredarwin-openssl";
  inherit (openssl) version;
  src = openssl.src;

  nativeBuildInputs = [ perl ];

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
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DOPENSSL_NO_APPLE_CRYPTO_RANDOM"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    perl ./Configure darwin64-x86_64-cc \
      no-asm no-shared no-tests no-async no-engine no-dso no-threads \
      --prefix=$out \
      --openssldir=$out/etc/ssl \
      $CFLAGS

    # OpenSSL's generated Makefile hardcodes its own CC/AR/RANLIB/CROSS_COMPILE
    # detection from the Configure target name (darwin64-x86_64-cc assumes
    # Xcode's cc) - force our cross tools in unconditionally.
    sed -i \
      -e "s|^CC=.*|CC=$CC|" \
      -e "s|^AR=.*|AR=$AR|" \
      -e "s|^RANLIB=.*|RANLIB=$RANLIB|" \
      Makefile

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES build_sw
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install_sw install_ssldirs
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
