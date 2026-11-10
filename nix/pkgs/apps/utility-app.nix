{ stdenv, lib, librsvg, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, iokit

# one of the built-in utility apps: its folder under src/Userspace, built with UtilityKit
, appName, appDir, executable ? appDir, installDir ? "Applications", iconSvg
, description
, src ? ../../../src/Userspace
, targetTriple ? "x86_64-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-" + lib.toLower executable;
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
    for f in ${appDir}/*.m UtilityKit/*.m; do
      $cc $common -x objective-c -fno-objc-arc -fblocks -Wall -Wno-deprecated-declarations \
        -I${appDir} -IUtilityKit \
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
      -L${windowserver}/usr/lib -F${iokit}/System/Library/Frameworks \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      objs/*.o \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -framework IOKit -lCoreFoundation -lobjc -lSystem \
      -o objs/${executable}

    # every svg in Resources at 32px for sidebars and sheets, the app icon at 256px for the bundle
    for svg in ${appDir}/Resources/*.svg; do
      name=$(basename "$svg" .svg)
      ${librsvg}/bin/rsvg-convert -w 96 -h 96 "$svg" -o "png/$name.png"
      ${librsvg}/bin/rsvg-convert -w 32 -h 32 "$svg" -o "png/$name-small.png"
    done
    ${librsvg}/bin/rsvg-convert -w 256 -h 256 ${appDir}/Resources/${iconSvg} -o AppIcon.png
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/${installDir}/${appName}.app/Contents"
    install -Dm755 objs/${executable} "$app/MacOS/${executable}"
    install -Dm644 ${appDir}/Info.plist "$app/Info.plist"
    for l in ${appDir}/*.lproj; do install -Dm644 -t "$app/Resources/$(basename $l)" $l/*.strings; done
    install -Dm644 -t "$app/Resources" png/*.png
    install -Dm644 AppIcon.png "$app/Resources/AppIcon.png"
    install -Dm644 ${appDir}/Resources/TRENTA-LICENSE "$app/Resources/TRENTA-LICENSE"
    runHook postInstall
  '';

  meta = with lib; {
    inherit description;
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.unix;
  };
}
