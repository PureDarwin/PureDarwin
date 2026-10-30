{ stdenv
, lib
, fetchurl
, unzip
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, libcxxDylib
, libcxxabiDylib
, compilerRt
, corefoundation
, libobjc
, foundation
, metalObjc
, appkit
, quartzcore
, coregraphics
, coretext
, onyx2d
, applicationservices
, coreservices
, windowserver
, openglFramework
, mesa
, mslc
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# apple's Metal sample code (MIT, downloaded from developer.apple.com, sources unchanged) built for PureDarwin: storyboards read by AppKit's NSStoryboard, MTKView and MTKTextureLoader from PD's MetalKit,
# shaders compiled here by mslc, the MSL compiler Metal.framework uses for newLibraryWithSource:
let
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  fw = "/System/Library/Frameworks";

  # "Migrating OpenGL Code to Metal": an OBJ temple (the sample's own loader) with PNG textures through
  # MTKTextureLoader (mipmaps, bottom-left origin), a reflective quad rendered to a texture, depth
  openGLToMetal = fetchurl {
    url = "https://docs-assets.developer.apple.com/published/7eb52e0c29cd/MigratingOpenGLCodeToMetal.zip";
    hash = "sha256-qTfp5UeaQbLUY9bmze2TneJO5yTeP7ITyyMcElwMME0=";
  };
  # "Creating and Sampling Textures": a TGA read by the sample's AAPLImage, sampled on a quad
  basicTexturing = fetchurl {
    url = "https://docs-assets.developer.apple.com/published/6f670b0b5d94/CreatingAndSamplingTextures.zip";
    hash = "sha256-9X6HYo5/vYgJskz8w7dF0vHCTJLK6B0DlzhS+7Q87s0=";
  };

in
stdenv.mkDerivation {
  pname = "puredarwin-metal-apple-samples";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src;
    fileset = lib.fileset.unions [
      ../../../src/Tools/pdmtl/objc/samples/source
    ];
  };

  nativeBuildInputs = [ unzip ];

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    CXX=${darwinCrossToolchain}/bin/${targetTriple}-clang++
    mkdir -p inc/Cocoa
    # cocoa without CoreData (PD's Cocoa.h pulls it in)
    printf '#import <Foundation/Foundation.h>\n#import <AppKit/AppKit.h>\n' > inc/Cocoa/Cocoa.h

    unzip -q ${openGLToMetal} -d gl
    unzip -q ${basicTexturing} -d tex

    BASE="-isysroot ${sdk} -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${metalObjc}/usr/local/include/pdmetal -Iinc \
      -I${libSystem}/usr/include -I${corefoundation}/include -I${libobjc}/usr/include -I${foundation}/usr/include \
      -I${onyx2d}/usr/include -I${coretext}/usr/include -I${quartzcore}/usr/include \
      -I${applicationservices}/usr/include -I${coregraphics}/usr/include -I${coreservices}/usr/include \
      -I${windowserver}/usr/include -I${mesa}/usr/include \
      -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks"
    # the projects' settings: ARC, TARGET_MACOS, -O2 like Release
    OBJC="$BASE -fobjc-arc -fblocks -fobjc-exceptions -O2 -g -DTARGET_MACOS=1 -Wno-everything"
    OBJCXX="$OBJC -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 -std=gnu++17"
    # apple's clang links the builtins archive too (half conversions libSystem doesn't export)
    LINK="-fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot ${sdk} -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -L${corefoundation}/usr/lib \
      -L${libobjc}/usr/lib -L${foundation}/usr/lib -L${appkit}/usr/lib -L${onyx2d}/usr/lib -L${coretext}/usr/lib \
      -L${quartzcore}/usr/lib -L${coregraphics}/usr/lib -L${windowserver}/usr/lib \
      ${metalObjc}${fw}/Metal.framework/Versions/A/Metal ${metalObjc}${fw}/MetalKit.framework/Versions/A/MetalKit \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -lCoreFoundation -lobjc -lc++ -lc++abi -lSystem \
      ${compilerRt}/lib/darwin/libclang_rt.osx.a"

    # each app's default.metallib, its shaders compiled by mslc
    (cd gl && ${mslc}/bin/mslc Metal/AAPLShaders.metal -I Common -o ../gl-default.metallib)
    (cd tex && ${mslc}/bin/mslc Renderer/AAPLShaders.metal -o ../tex-default.metallib)

    # PD's headers have no module maps: an @import is the framework's umbrella header
    find gl tex \( -name '*.h' -o -name '*.m' -o -name '*.mm' \) -exec \
      sed -i -E 's|^@import ([A-Za-z0-9_]+);|#import <\1/\1.h>|' {} +

    # migrating OpenGL Code to Metal, its Metal target
    G=gl
    for f in Common/main.m Common/AAPLMathUtilities.m Metal/AAPLMetalRenderer.m Metal/AAPLMetalViewController.m; do
      $CC $OBJC -I$G/Common -I$G/Metal -c $G/$f -o gl-$(basename $f).o
    done
    $CXX $OBJCXX -I$G/Common -I$G/Metal -c $G/Common/AAPLMeshData.mm -o gl-AAPLMeshData.o
    $CXX $LINK gl-*.o -o OpenGLToMetal

    # creating and Sampling Textures
    T=tex
    for f in Application/main.m Application/AAPLViewController.m Renderer/AAPLRenderer.m Renderer/AAPLImage.m; do
      $CC $OBJC -I$T/Application -I$T/Renderer -c $T/$f -o tex-$(basename $f).o
    done
    $CXX $LINK tex-*.o -o BasicTexturing

    # newLibraryWithSource: through the source cache and mslc
    $CC $OBJC -c Tools/pdmtl/objc/samples/source/main.m -o source.o
    $CXX $LINK source.o -o metal-source-test
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    S=$out/usr/local/share/metal-pd

    app() { # NAME EXE INFOPLIST STORYBOARD
      local A=$S/$1.app/Contents
      install -Dm755 $2 $A/MacOS/$2
      sed -e "s|\$(EXECUTABLE_NAME)|$2|; s|\$(PRODUCT_BUNDLE_IDENTIFIER)|com.example.apple-samplecode.$2|" \
          -e "s|\$(PRODUCT_NAME)|$1|; s|\$(MACOSX_DEPLOYMENT_TARGET)|14.0|; s|\$(DEVELOPMENT_LANGUAGE)|en|" \
          $3 > Info.plist.tmp
      install -Dm644 Info.plist.tmp $A/Info.plist
      install -Dm644 $4 $A/Resources/Base.lproj/Main.storyboard
    }
    app OpenGLToMetal OpenGLToMetal gl/Common/Application/macOS/Info.plist gl/Metal/Application/macOS/Base.lproj/Main.storyboard
    install -Dm644 gl-default.metallib $S/OpenGLToMetal.app/Contents/Resources/default.metallib
    cp -r gl/Common/Meshes $S/OpenGLToMetal.app/Contents/Resources/
    app BasicTexturing BasicTexturing tex/Application/macOS/Info.plist tex/Application/macOS/Base.lproj/Main.storyboard
    install -Dm644 tex-default.metallib $S/BasicTexturing.app/Contents/Resources/default.metallib
    install -Dm644 tex/Renderer/Image.tga $S/BasicTexturing.app/Contents/Resources/Image.tga

    # metal-source-test, its source compiled by mslc on the first newLibraryWithSource:
    install -Dm755 metal-source-test $S/MetalSourceTest.app/Contents/MacOS/metal-source-test
    install -Dm644 Tools/pdmtl/objc/samples/source/add.metal $S/MetalSourceTest.app/Contents/Resources/add.metal

    # runners: venus, the window on Wayland when WAYLAND_DISPLAY is set
    mkdir -p $out/usr/local/bin
    for r in OpenGLToMetal:OpenGLToMetal:metal-opengl-to-metal BasicTexturing:BasicTexturing:metal-basic-texturing \
             MetalSourceTest:metal-source-test:metal-source-test; do
      IFS=: read -r name exe cmd <<< "$r"
      cat > $out/usr/local/bin/$cmd <<EOF
    #!/bin/sh
    # apple's Metal sample $name on PD (venus, AppKit window)
    export AIRJITD_VULKAN=1 MESA_VK_WSI_DEBUG=sw
    export AIRJITD_CACHE=\''${AIRJITD_CACHE:-/var/tmp/metal-pd-cache}
    mkdir -p "\$AIRJITD_CACHE"
    icd=/usr/share/vulkan/icd.d
    [ -z "\$VK_DRIVER_FILES" ] && [ -f \$icd/virtio_icd.x86_64.json ] && export VK_DRIVER_FILES=\$icd/virtio_icd.x86_64.json
    exec $S/$name.app/Contents/MacOS/$exe "\$@"
    EOF
      sed -i "s|$out||" $out/usr/local/bin/$cmd
      chmod +x $out/usr/local/bin/$cmd
    done
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Apple's Metal sample code (MIT) built unchanged for PureDarwin's Metal, MetalKit and AppKit";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
