{ stdenv
, lib
, requireFile
, bison
, flex
, darwinCrossToolchain
, nativeLd
, libSystem
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

  migcomSrcs = [
    "error.c" "global.c" "header.c" "mig.c" "routine.c" "server.c"
    "statement.c" "string.c" "type.c" "user.c" "utils.c"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-migcom";
  version = "0.1";

  src = ../../tools/mig;

  nativeBuildInputs = [ bison flex ];

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    yacc -d -b y parser.y
    lex -o lexxer.yy.c lexxer.l

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-std=gnu89 -D__private_extern__= -DMIG_VERSION=\"0.1\" -isysroot $DARWIN_SDK_ROOT -I. -I${libSystem}/usr/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"

    mkdir -p out
    objs=""
    for src in ${lib.concatStringsSep " " migcomSrcs} y.tab.c lexxer.yy.c; do
      name="$(basename "$src" .c)"
      obj="out/$name.o"
      $CC $CFLAGS -c "$src" -o "$obj"
      objs="$objs $obj"
    done
    $CC $LDFLAGS -o out/migcom $objs
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/libexec
    cp out/migcom $out/usr/libexec/migcom
    mkdir -p $out/usr/bin
    install -m755 mig.sh $out/usr/bin/mig
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Real Mach-O migcom + mig.sh, for PureDarwin's own image (not the native host-tool build used during our Nix build)";
    platforms = platforms.linux;
  };
}
