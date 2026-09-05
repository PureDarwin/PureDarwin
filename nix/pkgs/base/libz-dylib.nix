{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, zlib
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
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
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    # unistd.h for lseek()/off_t use in gzlib.c - not pulled in transitively
    # under our SDK header set the way it apparently is on real macOS.
    CFLAGS="-isysroot ${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -I${libSystem}/usr/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -O2 -include unistd.h"

    mkdir -p obj
    for f in ${lib.concatStringsSep " " srcs}; do
      $CC $CFLAGS -c "$f" -o "obj/$(basename "$f" .c).o"
    done

    $CC -dynamiclib \
      -isysroot "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk" \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
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
