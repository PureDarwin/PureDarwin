{ stdenv, lib, fetchurl, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, librsvg, iokit, iokitHeaders

, src ? ../../../src/Userspace/Settings
, targetTriple ? "x86_64-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-settings";
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
    for f in main.m SettingsView.m SettingsData.m; do
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
      -o Settings

    # tile icons at twice their 30px, symbolic ones and the logo turned white for coloured backs
    for svg in Resources/colour/*.svg; do
      ${librsvg}/bin/rsvg-convert -w 60 -h 60 "$svg" -o "png/$(basename "$svg" .svg).png"
    done
    for svg in Resources/symbolic/*.svg; do
      name=$(basename "$svg" .svg)
      # the logo colours its path itself, the symbolic icons inherit the root's fill
      if [ "$name" = beak ]; then
        sed -e 's/fill="black"/fill="#ffffff"/g' "$svg" > white.svg
      elif [ "$name" = search ]; then
        cp "$svg" white.svg
      else
        sed -e 's/<svg /<svg fill="#ffffff" /' "$svg" > white.svg
      fi
      if [ "$name" = beak ]; then name=beak-white; size=400; else size=48; fi
      ${librsvg}/bin/rsvg-convert -w $size -h $size --keep-aspect-ratio white.svg -o "png/$name.png"
    done
    # the bundle icon Lookout, Finder and the About panel show, from the Dock's artwork
    ${librsvg}/bin/rsvg-convert -w 256 -h 256 Resources/colour/settings.svg -o AppIcon.png
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/Applications/Settings.app/Contents"
    install -Dm755 Settings "$app/MacOS/Settings"
    install -Dm644 Info.plist "$app/Info.plist"
    install -Dm644 AppIcon.png "$app/Resources/AppIcon.png"
    install -Dm644 -t "$app/Resources" png/*.png
    install -Dm644 Resources/TRENTA-LICENSE "$app/Resources/TRENTA-LICENSE"
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin Settings";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.unix;
  };
}
