{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, libcxxDylib
, libcxxabiDylib
, llvm
, clang
, clangHeaders
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# mslc on PureDarwin, for Metal.framework's newLibraryWithSource: the plugin loads into PD's own clang (its
# libclang-cpp and libLLVM), airify links the same libLLVM. clangHeaders is the host's clang 21 of the same
# release, since PD's clang install drops its headers. The clang driver and libclang-cpp come along, so images
# without the clang package still compile
stdenv.mkDerivation {
  pname = "puredarwin-mslc";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src/Tools/mslc;
    fileset = ../../../src/Tools/mslc;
  };

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CXX=${darwinCrossToolchain}/bin/${targetTriple}-clang++
    CXXFLAGS="-isysroot $SDK -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 -std=c++17 -O2 -fno-rtti -w \
      -I${llvm}/usr/include -I${lib.getDev clangHeaders}/include"
    LINK="-fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -L${llvm}/usr/lib -L${clang}/usr/lib"
    $CXX $CXXFLAGS -fPIC -c plugin.cpp -o plugin.o
    $CXX $LINK -dynamiclib -install_name /usr/lib/mslc-plugin.dylib plugin.o \
      -lclang-cpp -lLLVM-21 -lc++ -lc++abi -lSystem -o mslc-plugin.dylib
    $CXX $CXXFLAGS -c airify.cpp -o airify.o
    $CXX $LINK airify.o -lLLVM-21 -lc++ -lc++abi -lSystem -o mslc-airify
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 mslc.sh $out/usr/libexec/mslc
    install -Dm755 mslc-airify $out/usr/libexec/mslc-airify
    install -Dm755 mslc-plugin.dylib $out/usr/lib/mslc-plugin.dylib
    install -Dm755 ${clang}/usr/bin/clang-21 $out/usr/libexec/mslc-clang
    install -Dm755 ${clang}/usr/lib/libclang-cpp.dylib $out/usr/lib/libclang-cpp.dylib
    mkdir -p $out/usr/share/mslc
    cp -r include $out/usr/share/mslc/include
    # Metal.framework keeps each compiler build's metallibs apart, so a new mslc never reuses an old one's
    cat plugin.cpp airify.cpp mslc.sh $(find include -type f | sort) | sha256sum | cut -c1-16 > $out/usr/share/mslc/version
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "MSL to AIR compiler that Metal.framework runs for newLibraryWithSource:";
    platforms = platforms.unix;
  };
}
