{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, zlib
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
  srcs = [
    "adler32.c" "compress.c" "crc32.c" "deflate.c" "gzclose.c" "gzlib.c"
    "gzread.c" "gzwrite.c" "infback.c" "inffast.c" "inflate.c" "inftrees.c"
    "trees.c" "uncompr.c" "zutil.c"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-libz-dylib";
  inherit (zlib) version;
  src = zlib.src;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    # unistd.h for lseek()/off_t use in gzlib.c - not pulled in transitively
    # under our SDK header set the way it apparently is on real macOS.
    CFLAGS="-isysroot $PWD/sdk/MacOSX11.3.sdk -I${libSystem}/usr/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -O2 -include unistd.h"

    mkdir -p obj
    for f in ${lib.concatStringsSep " " srcs}; do
      $CC $CFLAGS -c "$f" -o "obj/$(basename "$f" .c).o"
    done

    $CC -dynamiclib \
      -isysroot "$PWD/sdk/MacOSX11.3.sdk" \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -install_name /usr/lib/libz.1.dylib \
      -compatibility_version 1.0.0 \
      -current_version 1.2.11 \
      -lSystem \
      obj/*.o \
      -o libz.1.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib
    cp libz.1.dylib $out/usr/lib/
    ln -sf libz.1.dylib $out/usr/lib/libz.dylib
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Real /usr/lib/libz.1.dylib for PureDarwin, matching the SDK's libz.tbd stub so SDK-linked -lz binaries actually resolve at runtime";
    platforms = platforms.linux;
  };
}
