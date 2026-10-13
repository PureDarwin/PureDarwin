{ stdenv, lib, fetchurl, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, librsvg, libjpeg

, src ? ../../../src/Userspace/Dock
, targetTriple ? "x86_64-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-dock";
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
        -I${windowserver}/usr/include -I${mesa}/usr/include \
        -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks \
        -c "$f" -o "objs/$(basename "$f" .m).o"
    done

    $cc -isysroot "$SDK" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${appkit}/usr/lib -L${onyx2d}/usr/lib \
      -L${coretext}/usr/lib -L${quartzcore}/usr/lib -L${coregraphics}/usr/lib \
      -L${windowserver}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      objs/*.o \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o Dock

    # dock icons render at twice the 48px tile so they stay sharp when magnified
    for svg in Resources/*.svg; do
      ${librsvg}/bin/rsvg-convert -w 96 -h 96 "$svg" -o "png/$(basename "$svg" .svg).png"
    done
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/System/Library/CoreServices/Dock.app/Contents"
    install -Dm755 Dock "$app/MacOS/Dock"
    install -Dm644 Info.plist "$app/Info.plist"
    for l in *.lproj; do install -Dm644 -t "$app/Resources/$l" $l/*.strings; done
    install -Dm644 -t "$app/Resources" png/*.png
    install -Dm644 Resources/TRENTA-LICENSE "$app/Resources/TRENTA-LICENSE"
    install -Dm644 -t "$out/System/Library/Desktop Pictures" DesktopPictures/*.jpg
    # an eighth of the size for pickers, in the hidden folder macOS keeps its thumbnails in
    mkdir -p "$out/System/Library/Desktop Pictures/.thumbnails"
    for jpg in DesktopPictures/*.jpg; do
      ${libjpeg.bin or libjpeg}/bin/djpeg -scale 1/8 "$jpg" | ${libjpeg.bin or libjpeg}/bin/cjpeg -quality 88 \
        > "$out/System/Library/Desktop Pictures/.thumbnails/$(basename "$jpg")"
    done
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin Dock: the desktop picture and the dock bar";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.unix;
  };
}
