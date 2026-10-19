{ stdenv
, lib
, fetchFromGitHub
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, libcxxDylib
, libcxxabiDylib
, llvm
, iokit
, corefoundation
, libobjc
, foundation
, vulkanLoader
, vulkanHeaders
, glfw
, metalPd
, spirvCross
  # the MTKView is an NSView: in an AppKit window it presents into a Wayland subsurface from WindowServer
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
  # ModelIO, which MTKMesh and the MDLTexture loader sit on
, appFrameworks
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# metal for Objective-C on PureDarwin: Darling's Metal.framework (darling-metal, MPL-2.0 per file, patched by darling-metal-pd.patch) over indium-pd, our MetalKit (CAMetalLayer + MTKView on GLFW windows, X11 or Wayland, or an AppKit window's Wayland subsurface),
# Apple's AAPLRenderer triangle sample (MIT) as Triangle.app (GLFW, metal-objc-triangle) and TriangleAppKit.app (AppKit, metal-appkit-triangle), and a metal-cpp (Apache-2.0, header-only) sample (metal-cpp-triangle)
let
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  darlingMetal = fetchFromGitHub {
    owner = "darlinghq";
    repo = "darling-metal";
    rev = "ae20248dc144beab899e38752f5a530f28a0ea56";
    hash = "sha256-YyHGLJ/SBFN43UC6aNLfELlAOU0cVTflvTBr1y09JlM=";
  };
  indium = fetchFromGitHub {
    owner = "darlinghq";
    repo = "indium";
    rev = "8423a7d2f053167030d2bc4a227f96243c740667";
    hash = "sha256-yrGSbR7ZllQnHMN98YMkcn+IF2oOPvuDqP3cIU7BYVg=";
  };
  metalCpp = fetchFromGitHub {
    owner = "apple";
    repo = "metal-cpp";
    rev = "27c4382b7151d55a51692cdcb27aaa98752240de";
    hash = "sha256-DI/SLzv1Zm+X6wfzOCrIEIs24CNCVtHftLDNYEDo/bk=";
  };
  fw = "/System/Library/Frameworks";
in
stdenv.mkDerivation {
  pname = "puredarwin-metal-objc";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src/Tools/pdmtl;
    fileset = lib.fileset.unions [
      ../../../src/Tools/pdmtl/objc
      ../../../src/Tools/pdmtl/indium-pd/include
    ];
  };

  postUnpack = ''
    cp -r ${darlingMetal} $sourceRoot/darling-metal
    chmod -R u+w $sourceRoot/darling-metal
    (cd $sourceRoot/darling-metal && patch -p1 < ${./darling-metal-pd.patch})
  '';

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    CXX=${darwinCrossToolchain}/bin/${targetTriple}-clang++
    O=objc D=darling-metal
    BASE="-isysroot ${sdk} -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I${corefoundation}/include -I${libobjc}/usr/include -I${foundation}/usr/include"
    APPKIT="-I${onyx2d}/usr/include -I${coretext}/usr/include -I${quartzcore}/usr/include \
      -I${applicationservices}/usr/include -I${coregraphics}/usr/include -I${coreservices}/usr/include \
      -I${windowserver}/usr/include -I${mesa}/usr/include \
      -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks \
      -F${appFrameworks}/System/Library/Frameworks"
    APPKITLIBS="-L${appkit}/usr/lib -L${onyx2d}/usr/lib -L${coretext}/usr/lib -L${quartzcore}/usr/lib \
      -L${coregraphics}/usr/lib -L${windowserver}/usr/lib \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer"
    OBJC="$BASE -fblocks -fobjc-exceptions -O1 -g -DDARLING_METAL_ENABLED=1 -w -include $O/shim/pdmetal-shim.h \
      -I$O/include -I$O/shim -I$D/include -I$D/private-include -I${indium}/include -Iindium-pd/include \
      -I${vulkanHeaders}/include -I${glfw}/include"
    OBJCXX="$OBJC -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 -std=gnu++17"
    LINK="-fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot ${sdk} -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -L${llvm}/usr/lib \
      -L${vulkanLoader}/usr/lib -L${iokit}/usr/lib -L${corefoundation}/usr/lib -L${libobjc}/usr/lib \
      -L${foundation}/usr/lib -L${glfw}/usr/local/lib -L${mesa}/usr/lib -L${spirvCross}/usr/lib"

    objs=""
    for f in $D/src/Metal/*.mm; do
      o=dm_$(basename ''${f%.mm}).o
      $CXX $OBJCXX -c $f -o $o &
      objs="$objs $o"
    done
    $CXX $OBJCXX -c $O/MetalExtras.mm -o pd_MetalExtras.o &
    objs="$objs pd_MetalExtras.o"
    wait
    for o in $objs; do [ -f $o ] || { echo "compile failed: $o"; exit 1; }; done

    # indium-pd first: its C++ statics have to exist before Metal.mm's constructor runs Indium::init
    $CXX $LINK -dynamiclib -install_name ${fw}/Metal.framework/Versions/A/Metal \
      -Wl,-force_load,${metalPd}/lib/libmetal-pd.a $objs \
      -lEGL -lGL -lspirv-cross-c-shared -lglfw -lLLVM-21 -lvulkan -lIOKitCF -lFoundation -lCoreFoundation -lobjc -lc++ -lc++abi -lSystem -o Metal

    $CXX $OBJCXX $APPKIT -c $O/PDMetalKit.mm -o PDMetalKit.o
    # the MTKTextureLoader takes images through CoreGraphics' CGImageSource (Onyx2D) and reads KTX directly
    $CXX $OBJCXX $APPKIT -c $O/MTKTextureLoader.mm -o MTKTextureLoader.o
    $CXX $OBJCXX $APPKIT -c $O/MTKModel.mm -o MTKModel.o
    $CXX $LINK $APPKITLIBS -dynamiclib -install_name ${fw}/MetalKit.framework/Versions/A/MetalKit PDMetalKit.o MTKTextureLoader.o \
      MTKModel.o Metal -F${appFrameworks}/System/Library/Frameworks -framework ModelIO \
      -lglfw -lFoundation -lCoreFoundation -lobjc -lc++ -lc++abi -lSystem -o MetalKit

    R=$D/examples/triangle/Renderer
    $CC $OBJC $APPKIT -I$R -c $R/AAPLRenderer.m -o AAPLRenderer.o
    $CC $OBJC $APPKIT -I$R -c $O/samples/triangle/main.m -o triangle-main.o
    $CC $LINK $APPKITLIBS triangle-main.o AAPLRenderer.o MetalKit Metal -lFoundation -lCoreFoundation -lobjc -lSystem -o triangle
    # the same renderer in an AppKit window: NSApplication, an NSWindow and MTKView as its content view
    $CC $OBJC $APPKIT -I$R -c $O/samples/triangle/main-appkit.m -o appkit-main.o
    $CC $LINK $APPKITLIBS appkit-main.o AAPLRenderer.o MetalKit Metal -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o triangle-appkit
    # and Darling's own sample main (ISC), unchanged: the modern AppKit spellings it uses
    $CC $OBJC $APPKIT -I$D/examples/triangle -c $D/examples/triangle/main.m -o darling-main.o
    $CC $LINK $APPKITLIBS darling-main.o AAPLRenderer.o MetalKit Metal -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o triangle-darling
    # textures, samplers and depth: a textured cube on Indium's texturing shaders (Apple sample code, MIT)
    $CC $OBJC $APPKIT -c $O/samples/texcube/main.m -o texcube.o
    $CC $LINK $APPKITLIBS texcube.o MetalKit Metal -lFoundation -lCoreFoundation -lobjc -lSystem -o texcube
    # Metal textures over IOSurfaces shared between processes, and offscreen CAMetalLayer drawables
    $CC $OBJC $APPKIT -c $O/samples/iosurface/main.m -o metal-iosurface.o
    $CC $LINK $APPKITLIBS metal-iosurface.o MetalKit Metal -lFoundation -lCoreFoundation -lobjc -lSystem -o metal-iosurface-test

    # metal-cpp: PD CF's CF_ENUM spelling trips C++'s elaborated-enum check, and its weak_import constants
    # that PD's Metal and Foundation lack are left to dynamic lookup (metal-cpp checks their addresses)
    $CXX $BASE -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 -std=gnu++17 -O1 -g -Wno-elaborated-enum-base \
      -I$O/shim-cpp -I${metalCpp} -I${glfw}/include -c $O/samples/mtlcpp/main.cpp -o mtlcpp.o
    $CXX $LINK mtlcpp.o MetalKit Metal -Wl,-undefined,dynamic_lookup -lglfw -lFoundation -lCoreFoundation -lobjc \
      -lc++ -lc++abi -lSystem -o metal-cpp-triangle
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    for f in Metal MetalKit; do
      d=$out${fw}/$f.framework
      install -Dm755 $f $d/Versions/A/$f
      ln -s A $d/Versions/Current
      ln -s Versions/Current/$f $d/$f
    done
    mkdir -p $out${fw}/Metal.framework/Versions/A/Headers $out${fw}/MetalKit.framework/Versions/A/Headers
    cp darling-metal/include/Metal/*.h $out${fw}/Metal.framework/Versions/A/Headers/
    cp objc/include/MetalKit/*.h $out${fw}/MetalKit.framework/Versions/A/Headers/
    ln -s Versions/Current/Headers $out${fw}/Metal.framework/Headers
    ln -s Versions/Current/Headers $out${fw}/MetalKit.framework/Headers
    # the headers apps build against (metal-apple-samples): Metal, MetalKit, simd and ModelIO, CAMetalLayer
    # is QuartzCore's
    I=$out/usr/local/include/pdmetal
    mkdir -p $I/Metal $I/MetalKit $I/simd $I/ModelIO
    cp darling-metal/include/Metal/*.h $I/Metal/
    cp objc/include/MetalKit/*.h $I/MetalKit/
    cp objc/include/simd/simd.h $I/simd/
    cp ${appFrameworks}/System/Library/Frameworks/ModelIO.framework/Versions/A/Headers/ModelIO.h $I/ModelIO/

    A=$out/usr/local/share/metal-pd/Triangle.app/Contents
    install -Dm755 triangle $A/MacOS/triangle
    install -Dm644 objc/samples/triangle/default.metallib $A/Resources/default.metallib
    install -Dm755 metal-cpp-triangle $out/usr/local/libexec/metal-pd/metal-cpp-triangle
    B=$out/usr/local/share/metal-pd/TriangleAppKit.app/Contents
    install -Dm755 triangle-appkit $B/MacOS/triangle-appkit
    install -Dm755 triangle-darling $B/MacOS/triangle-darling
    C=$out/usr/local/share/metal-pd/TexCube.app/Contents
    install -Dm755 texcube $C/MacOS/texcube
    install -Dm644 ${indium}/test/texturing/shaders.metallib $C/Resources/default.metallib
    install -Dm644 objc/samples/triangle/default.metallib $B/Resources/default.metallib
    mkdir -p $out/usr/local/bin
    install -Dm755 metal-iosurface-test $out/usr/local/bin/metal-iosurface-test
    cat > $out/usr/local/bin/metal-objc-triangle <<'EOF'
    #!/bin/sh
    # apple's AAPLRenderer triangle through PD's ObjC Metal, on venus, Wayland when WAYLAND_DISPLAY is set
    if [ -n "$WAYLAND_DISPLAY" ]; then
      export XDG_SESSION_TYPE=wayland
    else
      : ''${DISPLAY:=:0}
      export DISPLAY XDG_SESSION_TYPE=x11
    fi
    export AIRJITD_VULKAN=1 MESA_VK_WSI_DEBUG=sw
    if [ "$1" = --opengl ]; then
      export AIRJITD_OPENGL=1
      unset AIRJITD_VULKAN
      shift
    fi
    export AIRJITD_CACHE=''${AIRJITD_CACHE:-/var/tmp/metal-pd-cache}
    mkdir -p "$AIRJITD_CACHE"
    icd=/usr/share/vulkan/icd.d
    [ -z "$VK_DRIVER_FILES" ] && [ -f $icd/virtio_icd.x86_64.json ] && export VK_DRIVER_FILES=$icd/virtio_icd.x86_64.json
    exec /usr/local/share/metal-pd/Triangle.app/Contents/MacOS/triangle "$@"
    EOF
    chmod +x $out/usr/local/bin/metal-objc-triangle
    sed 's|exec .*|exec /usr/local/libexec/metal-pd/metal-cpp-triangle "$@"|; s|# Apple.s AAPLRenderer triangle through PD.s ObjC Metal|# a metal-cpp triangle over PD'"'"'s ObjC Metal|' \
      $out/usr/local/bin/metal-objc-triangle > $out/usr/local/bin/metal-cpp-triangle
    chmod +x $out/usr/local/bin/metal-cpp-triangle
    sed 's|exec .*|exec /usr/local/share/metal-pd/TriangleAppKit.app/Contents/MacOS/triangle-appkit "$@"|; s|# Apple.s AAPLRenderer triangle through PD.s ObjC Metal|# the AAPLRenderer triangle in an AppKit window (MTKView on a Wayland subsurface)|' \
      $out/usr/local/bin/metal-objc-triangle > $out/usr/local/bin/metal-appkit-triangle
    chmod +x $out/usr/local/bin/metal-appkit-triangle
    sed 's|triangle-appkit "|triangle-darling "|' $out/usr/local/bin/metal-appkit-triangle > $out/usr/local/bin/metal-darling-triangle
    chmod +x $out/usr/local/bin/metal-darling-triangle
    sed 's|exec .*|exec /usr/local/share/metal-pd/TexCube.app/Contents/MacOS/texcube "$@"|' $out/usr/local/bin/metal-appkit-triangle \
      > $out/usr/local/bin/metal-texcube
    chmod +x $out/usr/local/bin/metal-texcube
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Objective-C Metal on PureDarwin: darling-metal over indium-pd, MetalKit on GLFW, AAPLRenderer sample";
    license = with licenses; [ mpl20 mit bsd0 asl20 ];
    platforms = platforms.unix;
  };
}
