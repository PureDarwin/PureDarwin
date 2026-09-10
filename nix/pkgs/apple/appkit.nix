{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, libobjc
, corefoundation
, foundation
, onyx2d
, coregraphics
, coretext
, quartzcore
, applicationservices
, coreservices
, openglFramework
, windowserver
, freetype2
, fontconfig
, mesa
, glu
, src
, appleSdk
}:

let
  installName = "/System/Library/Frameworks/AppKit.framework/Versions/A/AppKit";

  # CoreData_/  needs CoreData, which we don't have yet
  excludedDirs = [ "CoreData_" ];
in
stdenv.mkDerivation {
  pname = "puredarwin-appkit";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # Sources #import <AppKit/X.h> for headers that live in subdirectories, so
    # stage every header flat under that one name.
    mkdir -p staged/AppKit cf-headers
    find . -name '*.h' -not -path './staged/*' -exec cp -n {} staged/AppKit/ \;
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    cflags="
      -isysroot $DARWIN_SDK_ROOT
      -mmacosx-version-min=26.5
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
      -fno-stack-protector
      -fobjc-exceptions -fblocks -fno-objc-arc
      -Wno-deprecated-objc-isa-usage -Wno-objc-root-class
      -Wno-nullability-completeness -Wno-int-conversion
      -I$PWD/staged
      -I$PWD/cf-headers
      -I${libSystem}/usr/include
      -I${libobjc}/usr/include
      -I${corefoundation}/include
      -I${foundation}/usr/include
      -I${onyx2d}/usr/include
      -I${coregraphics}/usr/include
      -I${coretext}/usr/include
      -I${quartzcore}/usr/include
      -I${applicationservices}/usr/include
      -I${coreservices}/usr/include
      -I${windowserver}/usr/include
      -I${freetype2}/include/freetype2
      -I${fontconfig}/include
      -I${fontconfig}/include/fontconfig
      -I${mesa}/usr/include
      -F${openglFramework}/System/Library/Frameworks
    "

    objects=""
    failed=""
    for source in $(find . -name '*.m' | sed 's|^\./||' | sort); do
      skip=
      for dir in ${lib.concatStringsSep " " excludedDirs}; do
        case "$source" in "$dir"/*) skip=1 ;; esac
      done
      [ -n "$skip" ] && continue

      object="objs/$(echo "$source" | tr '/' '_')"
      object="''${object%.m}.o"
      mkdir -p objs
      if $cc $cflags -c "$source" -o "$object" 2> "compile.log"; then
        objects="$objects $object"
      else
        echo "  SKIP $source"
        sed 's/^/      /' compile.log | grep -m2 "error:" || true
        failed="$failed $source"
      fi
    done

    echo "AppKit: $(echo $objects | wc -w) objects, $(echo $failed | wc -w) skipped"

    $cc \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -L${libobjc}/usr/lib \
      -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib \
      -L${onyx2d}/usr/lib \
      -L${coregraphics}/usr/lib \
      -L${coretext}/usr/lib \
      -L${quartzcore}/usr/lib \
      -L${windowserver}/usr/lib \
      -L${freetype2}/lib \
      -L${fontconfig}/lib \
      -F${openglFramework}/System/Library/Frameworks \
      -F${coreservices}/System/Library/Frameworks \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib \
      -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,${installName} \
      $objects \
      -framework OpenGL \
      -framework CoreServices \
      -lWindowServer -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D \
      -lFoundation -lCoreFoundation -lobjc -lSystem \
      -lfreetype -lfontconfig \
      -o AppKit

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/AppKit.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers" "$frameworkDir/Versions/A/Resources"
    cp AppKit "$frameworkDir/Versions/A/AppKit"
    cp staged/AppKit/*.h "$frameworkDir/Versions/A/Headers/"
    cp Info.plist "$frameworkDir/Versions/A/Resources/" || true

    # The control artwork AppKit draws with is found through
    # -[NSBundle pathForImageResource:] against the framework bundle, so it has
    # to be installed alongside the binary or every +imageNamed: returns nil.
    find "$src" -name '*.tiff' -exec cp {} "$frameworkDir/Versions/A/Resources/" \; 2>/dev/null || true

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/AppKit "$frameworkDir/AppKit"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"

    mkdir -p "$out/usr/lib" "$out/usr/include"
    ln -s ../../System/Library/Frameworks/AppKit.framework/Versions/A/AppKit \
      "$out/usr/lib/libAppKit.dylib"
    ln -s ../../System/Library/Frameworks/AppKit.framework/Versions/A/Headers \
      "$out/usr/include/AppKit"

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Cocotron/ravynOS AppKit for PureDarwin";
    platforms = platforms.linux;
  };
}
