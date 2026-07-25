{ stdenv
, lib
, requireFile
, gnumake
, darwinCrossToolchain
, nativeLd
, libSystem
, perl
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
  pname = "puredarwin-perl";
  inherit (perl) version src;

  nativeBuildInputs = [ gnumake perl ];

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
    export CPPFLAGS="-I${libSystem}/usr/include -I${zlib}/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${zlib}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    sh Configure -des \
      -Dusecrosscompile \
      -Dtargethost=x86_64-apple-darwin20.4 \
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
