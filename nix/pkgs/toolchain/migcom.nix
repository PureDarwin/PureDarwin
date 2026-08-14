{ stdenv
, lib
, requireFile
, bison
, flex
}:

let
  onDarwin = stdenv.hostPlatform.isDarwin;

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

  # On a Darwin host the compiler's own SDK already provides the real mach/,
  # i386/ and machine/ headers mig's sources expect, so none of the staging or
  # glibc-vs-Apple typedef reconciliation below is needed - just compile.
  sdkShimPhase = lib.optionalString (!onDarwin) ''
    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    SDK="$PWD/sdk/MacOSX11.3.sdk"

    # Stage just the SDK's mach/ header subtree into an isolated shim dir
    # rather than adding the whole SDK usr/include to the search path -
    # see tools/mig/CMakeLists.txt for the full explanation.
    mkdir -p mach_shim
    cp -r "$SDK/usr/include/mach" mach_shim/mach
    cp -r "$SDK/usr/include/i386" mach_shim/i386
    sed -i \
      -e "/typedef long long *__int64_t;/d" \
      -e "/typedef unsigned long long *__uint64_t;/d" \
      -e "/^typedef union {/,/} __mbstate_t;/d" \
      mach_shim/i386/_types.h
    mkdir -p mach_shim/sys
    cp "$SDK/usr/include/sys/_types.h" \
       "$SDK/usr/include/sys/_endian.h" \
       "$SDK/usr/include/sys/appleapiopts.h" \
       mach_shim/sys/
    cp -r "$SDK/usr/include/sys/_types" mach_shim/sys/_types
    cp -r "$SDK/usr/include/sys/_pthread" mach_shim/sys/_pthread
    cp -r "$SDK/usr/include/machine" mach_shim/machine
    cp -r "$SDK/usr/include/architecture" mach_shim/architecture
    cp -r "$SDK/usr/include/_types" mach_shim/_types
    cp -r "$SDK/usr/include/libkern" mach_shim/libkern
    cp "$SDK/usr/include/Availability.h" \
       "$SDK/usr/include/AvailabilityInternal.h" \
       "$SDK/usr/include/AvailabilityVersions.h" \
       "$SDK/usr/include/AvailabilityMacros.h" \
       mach_shim/
  '';

  hostCflags =
    if onDarwin
    then ''-I . -include sys/types.h''
    else ''-I . -I mach_shim -include sys/types.h -include bits/types/__mbstate_t.h'';
in
stdenv.mkDerivation {
  pname = "puredarwin-migcom-native";
  version = "0.1";

  src = ../../../tools/mig;

  nativeBuildInputs = [ bison flex ];

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    yacc -d -b y parser.y
    lex -o lexxer.yy.c lexxer.l

    ${sdkShimPhase}

    mkdir -p migcom_native
    objs=""
    for src in ${lib.concatStringsSep " " migcomSrcs} y.tab.c lexxer.yy.c; do
      name="$(basename "$src" .c)"
      obj="migcom_native/$name.o"
      $CC -std=gnu89 -D__private_extern__= -D__LITTLE_ENDIAN__ -DMIG_VERSION=\"\" \
        ${hostCflags} \
        -c "$src" -o "$obj"
      objs="$objs $obj"
    done
    $CC -o migcom_native/migcom.native $objs
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp migcom_native/migcom.native $out/bin/migcom
    runHook postInstall
  '';

  meta = with lib; {
    description = "Native build of PureDarwin's tools/mig migcom, for use as a build-time host tool";
    platforms = platforms.unix;
  };
}
