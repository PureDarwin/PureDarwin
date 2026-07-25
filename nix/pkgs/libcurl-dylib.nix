{ stdenv
, lib
, requireFile
, autoconf
, automake
, libtool
, gnum4
, gnumake
, darwinCrossToolchain
, nativeLd
, libSystem
, curl
, zlib
, openssl
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
  pname = "puredarwin-libcurl-dylib";
  inherit (curl) version;
  src = curl.src;

  nativeBuildInputs = [ autoconf automake libtool gnum4 gnumake ];
  buildInputs = [ zlib openssl ];

  # curl's own sources gate several real-Darwin-only features (CommonCrypto
  # SHA256, SystemConfiguration proxy auto-detection, Apple SecTrust) purely
  # on __APPLE__/SDK-version macros, which our SDK 11.3 headers legitimately
  # define even though none of those frameworks/dylibs actually exist on
  # PureDarwin - falls back to curl's own portable implementations instead.
  postPatch = ''
    substituteInPlace lib/curl_setup.h \
      --replace-fail '#    define CURL_MACOS_CALL_COPYPROXIES 1' \
                      '/* PureDarwin: no real SystemConfiguration.framework here */'
    substituteInPlace lib/sha256.c \
      --replace-fail '#elif (defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && \' \
                      '#elif 0 && (defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && \'
  '';

  configurePhase = ''
    runHook preConfigure

    autoreconf -fi

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang"
    export AR="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar"
    export RANLIB="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip"
    export NM="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-nm"
    export CPPFLAGS="-I${libSystem}/usr/include -I${zlib}/include -I${openssl}/include -isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector"
    export CFLAGS="$CPPFLAGS"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${zlib}/lib -L${openssl}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    ./configure \
      --host=x86_64-apple-darwin20.4 \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --with-openssl=${openssl} --disable-ldap --disable-ldaps --without-libpsl \
      --without-brotli --without-zstd --without-nghttp2 --without-nghttp3 \
      --disable-ares --disable-manual --without-libidn2 --without-gssapi \
      --disable-dependency-tracking --enable-shared --disable-static \
      --with-zlib=${zlib} --with-ca-bundle=/etc/ssl/cert.pem \
      ac_cv_func_accept4=no ac_cv_func_getpass_r=no

    # See curl.nix: configure detects a Darwin host triple and
    # unconditionally links against CoreFoundation/SystemConfiguration for
    # macOS proxy-autoconfig support - no real frameworks exist here.
    find . -name Makefile -exec sed -i \
      -e 's/-framework CoreFoundation//g' \
      -e 's/-framework SystemConfiguration//g' \
      -e 's/-framework CoreServices//g' \
      -e 's/-framework Security//g' \
      {} +

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES -C lib || true
    test -n "$(find lib -name '*.o' -path '*.libs*')"

    mapfile -t objs < <(find lib -name '*.o' -path '*.libs*')
    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib -L${zlib}/lib -L${openssl}/lib \
      -Wl,-force_load,${openssl}/lib/libssl.a -Wl,-force_load,${openssl}/lib/libcrypto.a \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -install_name /usr/lib/libcurl.4.dylib \
      -compatibility_version 7 \
      -current_version 9 \
      -lSystem -lz \
      "''${objs[@]}" \
      -o libcurl.4.dylib
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib/pkgconfig $out/usr/include
    cp libcurl.4.dylib $out/usr/lib/
    ln -sf libcurl.4.dylib $out/usr/lib/libcurl.dylib
    cp -r include/curl $out/usr/include/

    cat > $out/usr/lib/pkgconfig/libcurl.pc <<EOF
prefix=$out/usr
libdir=\''${prefix}/lib
includedir=\''${prefix}/include

Name: libcurl
Description: Library to transfer files with ftp, http, etc.
Version: ${curl.version}
Libs: -L\''${libdir} -lcurl
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Real /usr/lib/libcurl.4.dylib for PureDarwin";
    platforms = platforms.linux;
  };
}
