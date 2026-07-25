{ stdenv
, lib
, requireFile
, gnumake
, bison
, help2man
, perl
, darwinCrossToolchain
, nativeLd
, libSystem
, flex
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
  pname = "puredarwin-flex";
  inherit (flex) version src;

  nativeBuildInputs = [ gnumake bison help2man perl ];

  postPatch = ''
    perl -0pi -e 's/\@CROSS_TRUE\@am__append_1 = \\\n\@CROSS_TRUE\@\s+\.\.\/lib\/malloc\.c \\\n\@CROSS_TRUE\@\s+\.\.\/lib\/realloc\.c/\@CROSS_TRUE\@am__append_1 =/s' src/Makefile.in
    perl -0pi -e 's/\@CROSS_TRUE\@am__objects_3 = \.\.\/lib\/stage1flex-malloc\.\$\(OBJEXT\) \\\n\@CROSS_TRUE\@\s+\.\.\/lib\/stage1flex-realloc\.\$\(OBJEXT\)/\@CROSS_TRUE\@am__objects_3 =/s' src/Makefile.in
    perl -0pi -e 's/^\@AMDEP_TRUE\@\@am__include\@ \@am__quote\@\.\.\/lib\/\$\(DEPDIR\)\/stage1flex-(malloc|realloc)\.Po\@am__quote\@\n//mg' src/Makefile.in
    substituteInPlace src/Makefile.in \
      --replace-fail 'stage1flex_LDADD = ' 'stage1flex_LDADD = -lm'
  '';

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

    export ac_cv_func_malloc_0_nonnull=yes
    export ac_cv_func_realloc_0_nonnull=yes
    export gl_cv_func_malloc_posix=yes
    export gl_cv_func_realloc_posix=yes

    ./configure \
      --host=x86_64-apple-darwin20.4 \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --disable-nls \
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
    ln -sf flex "$out/usr/bin/lex"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Flex, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
