{ stdenv
, lib
, gnumake
, bison
, help2man
, perl
, darwinCrossToolchain
, nativeLd
, libSystem
, flex
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
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
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -lSystem"

    export ac_cv_func_malloc_0_nonnull=yes
    export ac_cv_func_realloc_0_nonnull=yes
    export gl_cv_func_malloc_posix=yes
    export gl_cv_func_realloc_posix=yes

    ./configure \
      --host=${targetTriple} \
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
