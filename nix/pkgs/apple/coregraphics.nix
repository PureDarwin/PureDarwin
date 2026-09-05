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
, freetype2
, libpng
, libjpeg
, zlib
, src
, appleSdk
}:

# CoreGraphics.framework is a thin CG* -> O2* shim over Onyx2D, in the Cocotron
# layering ravynOS uses. Everything here is software rendering into a bitmap;
# nothing talks to a window server.

let

  installName = "/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics";

  # PureDarwin has no WindowServer, so the sources that are pure clients of it
  # stay out until there is something to talk to. KTFont+PDF belongs to
  # CoreText, which is a separate framework and not in upstream's SRCS either.
  # Everything else here is self-contained software rendering.
  excludedSrcs = [
    "CGLPixelSurface.m" "CGDirectDisplay.m" "CGEvent.m" "CGWindow.m"
    "KTFont+PDF.m"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-coregraphics";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # Sources #import <CoreGraphics/X.h>, so stage the headers under that name.
    mkdir -p staged/CoreGraphics
    cp *.h staged/CoreGraphics/

    # corefoundation.nix installs its headers flattened into $out/include.
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    cflags="
      -isysroot $DARWIN_SDK_ROOT
      -mmacosx-version-min=26.5
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
      -fno-stack-protector
      -fobjc-arc-exceptions -fblocks
      -Wno-deprecated-objc-isa-usage -Wno-objc-root-class
      -I$PWD/staged
      -I$PWD/cf-headers
      -I${corefoundation}/include
      -I${libSystem}/usr/include
      -I${libobjc}/usr/include
      -I${foundation}/usr/include
      -I${onyx2d}/usr/include
      -I${freetype2}/include/freetype2
    "

    objs=""
    for s in *.m; do
      case " ${lib.concatStringsSep " " excludedSrcs} " in
        *" $s "*) continue ;;
      esac
      echo "  CC $s"
      $cc $cflags -c "$s" -o "''${s%.m}.o"
      objs="$objs ''${s%.m}.o"
    done

    $cc \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -L${libobjc}/usr/lib \
      -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib \
      -L${onyx2d}/usr/lib \
      -L${freetype2}/lib -L${libpng}/lib -L${libjpeg}/lib -L${zlib}/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylib_file,/usr/lib/libOnyx2D.dylib:${onyx2d}/usr/lib/libOnyx2D.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,${installName} \
      $objs \
      -lOnyx2D -lFoundation -lCoreFoundation -lobjc \
      -lfreetype -lpng -ljpeg -lz \
      -lSystem \
      -o CoreGraphics

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/CoreGraphics.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers" "$frameworkDir/Versions/A/Resources"
    cp CoreGraphics "$frameworkDir/Versions/A/CoreGraphics"
    cp *.h "$frameworkDir/Versions/A/Headers/"
    cp Info.plist "$frameworkDir/Versions/A/Resources/"

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/CoreGraphics "$frameworkDir/CoreGraphics"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"
    ln -s Versions/Current/Resources "$frameworkDir/Resources"

    mkdir -p "$out/usr/lib" "$out/usr/include"
    ln -s "../../System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics" \
      "$out/usr/lib/libCoreGraphics.dylib"
    ln -s "../../System/Library/Frameworks/CoreGraphics.framework/Versions/A/Headers" \
      "$out/usr/include/CoreGraphics"

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "CoreGraphics.framework: the CG API over Onyx2D's software rasteriser";
    platforms = platforms.unix;
  };
}
