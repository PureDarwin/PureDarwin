{ stdenv, lib, librsvg, fetchurl, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, iokit, iokitHeaders

, src ? ../../../src/Userspace/Calculator
, targetTriple ? "x86_64-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-calculator";
  version = "0.1";
  inherit src;
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;

  buildPhase = ''
    runHook preBuild
    export SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cc=${darwinCrossToolchain}/bin/${targetTriple}-clang
    common="-isysroot $SDK -mmacosx-version-min=26.5 -D_DARWIN_C_SOURCE -fno-stack-protector -O2"

    mkdir -p objs png
    for f in main.m; do
      $cc $common -x objective-c -fno-objc-arc -fblocks -Wall -Wno-deprecated-declarations \
        -I. \
        -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
        -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
        -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
        -I${coregraphics}/usr/include -I${coreservices}/usr/include \
        -I${windowserver}/usr/include -I${iokitHeaders}/include -I${mesa}/usr/include \
        -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks \
        -c "$f" -o "objs/$(basename "$f" .m).o"
    done

    $cc -isysroot "$SDK" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${appkit}/usr/lib -L${onyx2d}/usr/lib \
      -L${coretext}/usr/lib -L${quartzcore}/usr/lib -L${coregraphics}/usr/lib \
      -L${windowserver}/usr/lib -L${iokit}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      objs/*.o \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -lIOKitCF -lCoreFoundation -lobjc -lSystem \
      -o Calculator

    # the bundle icon Lookout, Finder and the About panel show, from the Dock's artwork
    ${librsvg}/bin/rsvg-convert -w 256 -h 256 Resources/calculator.svg -o AppIcon.png
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/Applications/Calculator.app/Contents"
    install -Dm755 Calculator "$app/MacOS/Calculator"
    install -Dm644 Info.plist "$app/Info.plist"
    for l in *.lproj; do install -Dm644 -t "$app/Resources/$l" $l/*.strings; done
    install -Dm644 AppIcon.png "$app/Resources/AppIcon.png"
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin Calculator: the basic four-function calculator";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.unix;
  };
}
