{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, libobjc
, corefoundation
, foundation
, metalObjc
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# MetalPerformanceShaders: the image kernels apps use, as compute kernels on PD's Metal. MPSKernels.metallib is
# MPSKernels.metal compiled by Apple's metal and metallib tools (xcrun -sdk macosx metal, then metallib)
let
  fw = "/System/Library/Frameworks";
in
stdenv.mkDerivation {
  pname = "puredarwin-metalperformanceshaders";
  version = "0.1";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -fno-objc-arc -fblocks -x objective-c \
      -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include -I${foundation}/usr/include \
      -I${metalObjc}/usr/local/include/pdmetal \
      -c $src/MPSImageGaussianBlur.m -o MPSImageGaussianBlur.o
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib -dynamiclib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -install_name ${fw}/MetalPerformanceShaders.framework/Versions/A/MetalPerformanceShaders \
      -compatibility_version 1 -current_version 1 \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib -L${foundation}/usr/lib \
      MPSImageGaussianBlur.o ${metalObjc}${fw}/Metal.framework/Versions/A/Metal \
      -lFoundation -lCoreFoundation -lobjc -lSystem -o MetalPerformanceShaders
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    F=$out${fw}/MetalPerformanceShaders.framework
    install -Dm755 MetalPerformanceShaders $F/Versions/A/MetalPerformanceShaders
    install -Dm644 $src/MetalPerformanceShaders.h $F/Versions/A/Headers/MetalPerformanceShaders.h
    install -Dm644 $src/MPSKernels.metallib $F/Versions/A/Resources/MPSKernels.metallib
    cat > $F/Versions/A/Resources/Info.plist <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>MetalPerformanceShaders</string>
	<key>CFBundleIdentifier</key>
	<string>com.apple.MetalPerformanceShaders.MetalPerformanceShaders</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
</dict>
</plist>
PLIST
    ln -s A $F/Versions/Current
    ln -s Versions/Current/MetalPerformanceShaders $F/MetalPerformanceShaders
    ln -s Versions/Current/Headers $F/Headers
    ln -s Versions/Current/Resources $F/Resources
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "MetalPerformanceShaders image kernels for PureDarwin";
    platforms = lib.platforms.unix;
  };
}
