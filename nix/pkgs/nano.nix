{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, nano
, ncurses
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
in
stdenv.mkDerivation {
  pname = "puredarwin-nano";
  inherit (nano) version;
  src = nano.src;

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include -I${ncurses}/include/ncursesw -I${ncurses}/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${ncurses}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"
    export LIBS="-Wl,-force_load,${ncurses}/lib/libncursesw.a -lSystem"

    for fn in mempcpy stpcpy stpncpy strchrnul memrchr rawmemchr \
              canonicalize_file_name secure_getenv getrandom reallocarray \
              explicit_bzero timegm futimens utimensat fdatasync \
              posix_fadvise dup3 pipe2 accept4 mkostemp mkostemps \
              getdelim getline error error_at_line vasprintf asprintf \
              argz_create argz_count _set_invalid_parameter_handler \
              futimesat wmempcpy utimens lutimens futimes lutimes; do
      export "ac_cv_func_''${fn}=no"
    done
    export ac_cv_header_error_h=no

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=$out \
      --disable-nls \
      --disable-utf8 \
      --disable-libmagic \
      --disable-speller \
      --disable-wrapping-as-root \
      --without-slang \
      --with-curses-dir=${ncurses}

    find . -name Makefile -exec sed -i -E 's/-lncursesw?\b//g' {} +

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

  meta = with lib; {
    platforms = platforms.linux;
  };
}
