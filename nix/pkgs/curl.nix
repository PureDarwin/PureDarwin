{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, curl
, openssl
, zlib
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
  pname = "puredarwin-curl";
  inherit (curl) version;
  src = curl.src;

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
    export CPPFLAGS="-I${libSystem}/usr/include -I${openssl}/include -I${zlib}/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${openssl}/lib -L${zlib}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"
    export LIBS="-Wl,-force_load,${openssl}/lib/libssl.a -Wl,-force_load,${openssl}/lib/libcrypto.a -Wl,-force_load,${zlib}/lib/libz.a -lSystem"

    for fn in fchmod ftruncate getpeername getsockname recv send \
              strtoll poll fsetxattr; do
      export "ac_cv_func_''${fn}=yes"
    done
    export ac_cv_func_pipe2=no
    export ac_cv_func_accept4=no
    export ac_cv_func_getpass_r=no

    ./configure \
      --host=x86_64-apple-darwin20.4 \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --disable-shared \
      --enable-static \
      --with-openssl=${openssl} \
      --with-zlib=${zlib} \
      --with-ca-bundle=/etc/ssl/cert.pem \
      --without-nghttp2 \
      --without-nghttp3 \
      --without-libpsl \
      --without-libidn2 \
      --without-brotli \
      --without-zstd \
      --without-librtmp \
      --disable-ldap \
      --disable-ldaps \
      --disable-manual \
      --disable-threaded-resolver

    find . -name Makefile -exec sed -i \
      -e 's/-framework CoreFoundation//g' \
      -e 's/-framework SystemConfiguration//g' \
      -e 's/-framework CoreServices//g' \
      -e 's/-framework Security//g' \
      {} +

    sed -i -E 's/^(#\s*define\s+CURL_MACOS_CALL_COPYPROXIES\s+1)/\/* \1 disabled: no SystemConfiguration framework in this build *\//' \
      lib/curl_setup.h

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
