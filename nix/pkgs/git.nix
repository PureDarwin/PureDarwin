{ stdenv
, lib
, requireFile
, gnumake
, darwinCrossToolchain
, nativeLd
, libSystem
, git
, zlib
, curl
, openssl
, targetTriple ? "x86_64-apple-darwin20.4"
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

  makeFlags = "uname_S=Darwin uname_R=20.5.0 uname_M=x86_64 uname_O=Darwin prefix=/usr"
    + " NO_GETTEXT=YesPlease NO_TCLTK=YesPlease NO_PYTHON=YesPlease NO_PERL=YesPlease"
    + " NO_ICONV=YesPlease NO_UNIX_SOCKETS=YesPlease NO_OPENSSL=YesPlease NO_EXPAT=YesPlease"
    + " NO_APPLE_COMMON_CRYPTO=YesPlease NO_INSTALL_HARDLINKS=YesPlease"
    + '' CURL_CFLAGS="-I${curl}/include" ''
    + '' CURL_LIBCURL="-Wl,-force_load,${curl}/lib/libcurl.a -Wl,-force_load,${openssl}/lib/libssl.a -Wl,-force_load,${openssl}/lib/libcrypto.a" ''
    + " PTHREAD_LIBS= SANE_TOOL_PATH=";
in
stdenv.mkDerivation {
  pname = "puredarwin-git";
  inherit (git) version;
  src = git.src;

  nativeBuildInputs = [ gnumake ];

  postPatch = ''
    sed -i \
      -e '/COMPAT_OBJS += compat\/precompose_utf8.o/d' \
      -e '/BASIC_CFLAGS += -DPRECOMPOSE_UNICODE/d' \
      -e '/BASIC_LDFLAGS += -framework CoreServices/d' \
      config.mak.uname
  '';

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"

    rm -rf "$DARWIN_SDK_ROOT/System/Library/Frameworks/Python.framework"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I${libSystem}/usr/include -I${zlib}/include -I${curl}/include"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${zlib}/lib -Wl,-force_load,${zlib}/lib/libz.a -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    make -j$NIX_BUILD_CORES \
      CC="$CC" AR="$AR" RANLIB="$RANLIB" \
      CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" \
      ${makeFlags} \
      all

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I${libSystem}/usr/include -I${zlib}/include -I${curl}/include"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${zlib}/lib -Wl,-force_load,${zlib}/lib/libz.a -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    make \
      CC="$CC" AR="$AR" RANLIB="$RANLIB" \
      CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" \
      DESTDIR="$out" \
      ${makeFlags} \
      install

    runHook postInstall
  '';

  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
