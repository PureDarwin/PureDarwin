{ stdenv
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, libobjc
, corefoundation
, foundation
, coregraphics
, onyx2d
, dejavu_fonts
, inter
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# the CoreGraphics calls the official Firefox build imports, run once at boot by a launch daemon
stdenv.mkDerivation {
  pname = "puredarwin-cg-ff-tests";
  version = "0.1";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    $CC -isysroot $SDK -mmacosx-version-min=26.5 -O1 -Wall -Wno-deprecated-declarations -fblocks \
      -Icf-headers -I${corefoundation}/include -I${libSystem}/usr/include -I${coregraphics}/usr/include \
      -I${onyx2d}/usr/include -I${foundation}/usr/include -I${libobjc}/usr/include -x objective-c -c $src/cg_test.c -o cg_test.o
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib -L${corefoundation}/usr/lib \
      cg_test.o ${coregraphics}/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics \
      -lCoreFoundation -lSystem -o cg_test
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/local/libexec/cgtests $out/System/Library/LaunchDaemons
    install -m755 cg_test $src/run.sh $out/usr/local/libexec/cgtests/
    install -m644 ${dejavu_fonts}/share/fonts/truetype/DejaVuSans.ttf ${inter}/share/fonts/truetype/InterVariable.ttf \
      $out/usr/local/libexec/cgtests/
    install -m644 $src/org.puredarwin.cgtests.plist $out/System/Library/LaunchDaemons/
    runHook postInstall
  '';

  dontFixup = true;
}
