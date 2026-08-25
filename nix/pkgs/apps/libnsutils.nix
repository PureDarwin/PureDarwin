{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libnsutils
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let

  srcs = [ "src/unistd.c" "src/base64.c" "src/time.c" ];
in
stdenv.mkDerivation {
  pname = "puredarwin-libnsutils";
  inherit (libnsutils) version src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -Iinclude -I${libSystem}/usr/include"

    for f in ${lib.concatStringsSep " " srcs}; do
      obj=$(basename "$f").o
      "$CC" $CFLAGS -c "$f" -o "$obj"
    done

    "$CC" -dynamiclib \
      -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=11.0 \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -install_name /lib/libnsutils.dylib \
      -lSystem \
      *.o \
      -o libnsutils.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out/lib/pkgconfig" "$out/include"
    install -m755 libnsutils.dylib "$out/lib/libnsutils.dylib"
    cp -r include/nsutils "$out/include/nsutils"

    cat > "$out/lib/pkgconfig/libnsutils.pc" <<EOF
prefix=$out
libdir=\''${prefix}/lib
includedir=\''${prefix}/include

Name: libnsutils
Description: Utility library for NetSurf
Version: ${libnsutils.version}
Libs: -L\''${libdir} -lnsutils
Cflags: -I\''${includedir}
EOF

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libnsutils (NetSurf utility library), cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
