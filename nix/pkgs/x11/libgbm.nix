{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, pdsurface
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
in
stdenv.mkDerivation {
  pname = "puredarwin-libgbm";
  version = "1";
  src = ../../../src/Libraries/libgbm;

  dontConfigure = true;
  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -D_DARWIN_C_SOURCE -Iinclude \
      -I${pdsurface}/usr/include -I${libSystem}/usr/include \
      -dynamiclib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${pdsurface}/usr/lib \
      -Wl,-install_name,/usr/lib/libgbm.dylib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -lPDSurface -lSystem \
      -o libgbm.dylib gbm.c
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 libgbm.dylib $out/usr/lib/libgbm.dylib
    ln -s libgbm.dylib $out/usr/lib/libgbm.1.dylib
    install -Dm644 include/gbm.h $out/usr/include/gbm.h

    # Consumers find GBM through pkg-config and nothing else; without this
    # meson's dependency('gbm') fails and the whole renderer is skipped.
    mkdir -p $out/usr/lib/pkgconfig
    cat > $out/usr/lib/pkgconfig/gbm.pc <<EOF
    prefix=/usr
    libdir=$out/usr/lib
    includedir=$out/usr/include

    Name: gbm
    Description: Generic Buffer Manager, PureDarwin implementation over PDSurface
    Version: 21.3.0
    Libs: -L\''${libdir} -lgbm
    Cflags: -I\''${includedir}
    EOF
    sed -i 's/^    //' $out/usr/lib/pkgconfig/gbm.pc
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "GBM-shaped veneer over PDSurface so unmodified ports link";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
