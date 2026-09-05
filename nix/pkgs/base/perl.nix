{ stdenv
, lib
, gnumake
, darwinCrossToolchain
, nativeLd
, libSystem
, perl
, zlib
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-perl";
  inherit (perl) version src;

  nativeBuildInputs = [ gnumake perl ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/usr/include -I${zlib}/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${zlib}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -Wl,-undefined,dynamic_lookup -lSystem"

    sh Configure -des \
      -Dusecrosscompile \
      -Dtargethost=${targetTriple} \
      -Dtargetarch=x86_64 \
      -Dcc="$CC" \
      -Dld="$CC" \
      -Dar="$AR" \
      -Dranlib="$RANLIB" \
      -Dccflags="$CPPFLAGS $CFLAGS" \
      -Dldflags="$LDFLAGS" \
      -Dprefix=/usr \
      -Dvendorprefix=/usr \
      -Dsiteprefix=/usr \
      -Dman1dir=none \
      -Dman3dir=none \
      -Duseshrplib=false \
      -Duseithreads=false \
      -Dusemultiplicity=false \
      -Dusedtrace=false \
      -Duselargefiles=true \
      -Uusesfio \
      -Ui_db \
      -Ui_gdbm \
      -Ui_ndbm

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
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Perl, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
