{ stdenv, lib, fetchFromGitHub, darwinCrossToolchain, nativeLd, libSystem
, libcxxDylib, libcxxabiDylib, appleSdk
, targetTriple ? "x86_64-apple-darwin20.4"
}:
let sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
in stdenv.mkDerivation {
  pname = "puredarwin-spirv-cross";
  version = "1.4.341.0";
  src = fetchFromGitHub {
    owner = "KhronosGroup";
    repo = "SPIRV-Cross";
    rev = "vulkan-sdk-1.4.341.0";
    hash = "sha256-F3/j2+B/qv3sDiOiOy1OhR9G+DnM7I4LJqljZXL4S7Q=";
  };
  buildPhase = ''
    runHook preBuild
    CXX=${darwinCrossToolchain}/bin/${targetTriple}-clang++
    FLAGS="-isysroot ${sdk} -mmacosx-version-min=26.5 -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 \
      -I${libSystem}/usr/include -std=c++17 -O2 -fPIC -DSPIRV_CROSS_C_API_GLSL=1 \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector"
    objs=""
    for f in spirv_cross spirv_parser spirv_cross_parsed_ir spirv_cfg spirv_glsl spirv_cross_c; do
      $CXX $FLAGS -c $f.cpp -o $f.o
      objs="$objs $f.o"
    done
    $CXX -fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot ${sdk} -mmacosx-version-min=26.5 \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -dynamiclib -install_name /usr/lib/libspirv-cross-c-shared.dylib $objs \
      -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -L${libSystem}/usr/lib \
      -lc++ -lc++abi -lSystem -o libspirv-cross-c-shared.dylib
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    install -Dm755 libspirv-cross-c-shared.dylib $out/usr/lib/libspirv-cross-c-shared.dylib
    install -Dm644 spirv_cross_c.h $out/usr/include/spirv_cross/spirv_cross_c.h
    install -Dm644 spirv.h $out/usr/include/spirv_cross/spirv.h
    runHook postInstall
  '';
  meta.license = lib.licenses.asl20;
}
