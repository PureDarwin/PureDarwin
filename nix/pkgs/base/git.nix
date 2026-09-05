{ stdenv
, lib
, gnumake
, darwinCrossToolchain
, nativeLd
, libSystem
, git
, zlib
, curl
, openssl
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let

  makeFlags = "uname_S=Darwin uname_R=26.5.0 uname_M=x86_64 uname_O=Darwin prefix=/usr"
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
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I${libSystem}/usr/include -I${zlib}/include -I${curl}/include"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${zlib}/lib -Wl,-force_load,${zlib}/lib/libz.a -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -Wl,-undefined,dynamic_lookup -lSystem"

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
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${zlib}/lib -Wl,-force_load,${zlib}/lib/libz.a -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -Wl,-undefined,dynamic_lookup -lSystem"

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
