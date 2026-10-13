{ stdenv, lib, librsvg, fetchurl, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, iokit, iokitHeaders

, src ? ../../../src/Userspace/Installer
, wifiSrc ? ../../../src/Userspace/wifiutil
, wifiHeaders ? ../../../src/Kernel/Extensions/IO80211Family
, cryptoHeaders ? ../../../src/Libraries/CommonCrypto/include
, targetTriple ? "x86_64-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-installer";
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
    # the Wi-Fi backend and screens the menu bar, Settings and the installer share
    cp ${wifiSrc}/pdwifi.c ${wifiSrc}/pdwifi.h ${wifiSrc}/PDWiFiUI.m ${wifiSrc}/PDWiFiUI.h .
    for f in main.m pdwifi.c PDWiFiUI.m; do
      $cc $common -x objective-c -fno-objc-arc -fblocks -Wall -Wno-deprecated-declarations \
        -I. -I${wifiHeaders} -I${cryptoHeaders} \
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
      -o Installer

    # the bundle icon, the software-update artwork the demo uses for the installer
    ${librsvg}/bin/rsvg-convert -w 256 -h 256 Resources/installer.svg -o AppIcon.png
    # the beak in white for the banner and its watermark, in grey for the sidebar
    printf '* { fill: #ffffff !important; stroke: #ffffff !important; }' > white.css
    printf '* { fill: #8a8a8a !important; stroke: #8a8a8a !important; }' > grey.css
    ${librsvg}/bin/rsvg-convert -w 112 -h 112 --keep-aspect-ratio -s white.css Resources/beak.svg -o beak-white.png
    ${librsvg}/bin/rsvg-convert -w 360 -h 360 --keep-aspect-ratio -s white.css Resources/beak.svg -o beak-mark.png
    ${librsvg}/bin/rsvg-convert -w 36 -h 36 --keep-aspect-ratio -s grey.css Resources/beak.svg -o beak-grey.png
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/Applications/Install PureDarwin.app/Contents"
    install -Dm755 Installer "$app/MacOS/Installer"
    install -Dm644 Info.plist "$app/Info.plist"
    install -Dm644 AppIcon.png "$app/Resources/AppIcon.png"
    for p in beak-white beak-mark beak-grey; do install -Dm644 $p.png "$app/Resources/$p.png"; done
    runHook postInstall
  '';

  meta = with lib; {
    description = "Install PureDarwin: the installer app over pdinstall";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.unix;
  };
}
