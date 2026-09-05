{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, tinyxxd
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-xxd";
  inherit (tinyxxd) version;
  src = tinyxxd.src;

  configurePhase = ''
    runHook preConfigure
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -std=c11 \
      -isysroot "$DARWIN_SDK_ROOT" \
      -mmacosx-version-min=26.5 \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -Wl,-Z \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-undefined,dynamic_lookup \
      -o tinyxxd \
      main.c \
      -lSystem
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/bin" "$out/share/man/man1"
    install -m 755 tinyxxd "$out/bin/tinyxxd"
    ln -s tinyxxd "$out/bin/xxd"
    install -m 644 tinyxxd.1 "$out/share/man/man1/xxd.1"
    ln -s xxd.1 "$out/share/man/man1/tinyxxd.1"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Tiny xxd-compatible hex dump tool, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
