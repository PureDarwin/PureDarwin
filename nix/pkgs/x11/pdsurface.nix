{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
in
stdenv.mkDerivation {
  pname = "puredarwin-pdsurface";
  version = "1";
  src = ../../../src/Libraries/PDSurface;

  dontConfigure = true;
  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -D_DARWIN_C_SOURCE -Iinclude \
      -I${libSystem}/usr/include -I${corefoundation}/usr/include \
      -I${iokit}/usr/include \
      -dynamiclib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-install_name,/usr/lib/libPDSurface.dylib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -lIOKitCF -lCoreFoundation -lSystem \
      -o libPDSurface.dylib PDSurface.c
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 libPDSurface.dylib $out/usr/lib/libPDSurface.dylib
    install -Dm644 include/PDSurface.h $out/usr/include/PDSurface.h
    install -Dm644 include/PDSurfaceProtocol.h $out/usr/include/PDSurfaceProtocol.h

    mkdir -p $out/usr/lib/pkgconfig
    cat > $out/usr/lib/pkgconfig/pdsurface.pc <<EOF
    prefix=/usr
    libdir=$out/usr/lib
    includedir=$out/usr/include

    Name: PDSurface
    Description: PureDarwin shareable graphics buffers
    Version: 1
    Libs: -L\''${libdir} -lPDSurface
    Cflags: -I\''${includedir}
    EOF
    sed -i 's/^    //' $out/usr/lib/pkgconfig/pdsurface.pc
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "PureDarwin shareable graphics buffers, driver independent";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
