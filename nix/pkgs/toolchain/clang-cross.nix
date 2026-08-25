{ stdenv
, lib
, cmake
, ninja
, python3
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, nativeMesonTools
, llvm
, llvmSrc
, llvmVersion
, nativeTblgen
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-clang";
  version = llvmVersion;

  src = llvmSrc;

  nativeBuildInputs = [ cmake ninja python3 ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # clang-tblgen runs on the build machine and there is no packaged one, so
    # build it here from the same source revision as the LLVM being linked
    # against. Only that one target is built, not the whole native LLVM.
    cmake -S llvm -B nativebuild -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_PROJECTS=clang \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF \
      -DLLVM_INCLUDE_EXAMPLES=OFF \
      -DLLVM_ENABLE_ZLIB=OFF \
      -DLLVM_ENABLE_ZSTD=OFF \
      -DLLVM_ENABLE_TERMINFO=OFF \
      -DLLVM_ENABLE_LIBXML2=OFF
    ninja -C nativebuild clang-tblgen

    commonFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include"
    linkFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -L${llvm}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -lc++ -lc++abi -lSystem"

    cmake -S clang -B build -G Ninja \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
      -DCMAKE_C_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -DCMAKE_CXX_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang++ \
      -DCMAKE_AR=${darwinCrossToolchain}/bin/${targetTriple}-ar \
      -DCMAKE_RANLIB=${darwinCrossToolchain}/bin/${targetTriple}-ranlib \
      -DCMAKE_INSTALL_NAME_TOOL=${nativeMesonTools}/bin/install_name_tool \
      -DCMAKE_C_FLAGS="$commonFlags" \
      -DCMAKE_CXX_FLAGS="$commonFlags -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1" \
      -DCMAKE_EXE_LINKER_FLAGS="$linkFlags" \
      -DCMAKE_SHARED_LINKER_FLAGS="$linkFlags" \
      -DCMAKE_INSTALL_PREFIX=$out/usr \
      -DCMAKE_INSTALL_NAME_DIR=/usr/lib \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CROSSCOMPILING=ON \
      -DLLVM_DIR=${llvm}/usr/lib/cmake/llvm \
      -DLLVM_TABLEGEN_EXE=${nativeTblgen} \
      -DCLANG_TABLEGEN=$PWD/nativebuild/bin/clang-tblgen \
      -DLLVM_LINK_LLVM_DYLIB=ON \
      -DCLANG_LINK_CLANG_DYLIB=ON \
      -DLLVM_ENABLE_PIC=ON \
      -DLLVM_ENABLE_RTTI=ON \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DCLANG_INCLUDE_TESTS=OFF \
      -DCLANG_INCLUDE_DOCS=OFF \
      -DCLANG_ENABLE_STATIC_ANALYZER=OFF \
      -DCLANG_ENABLE_ARCMT=OFF \
      -DLLVM_ENABLE_ZLIB=OFF \
      -DLLVM_ENABLE_ZSTD=OFF \
      -DLLVM_ENABLE_TERMINFO=OFF \
      -DLLVM_ENABLE_LIBXML2=OFF \
      -DDEFAULT_SYSROOT=/ \
      -DCLANG_DEFAULT_LINKER=ld

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    ninja -C build install

    rm -rf "$out/usr/lib"/*.a "$out/usr/lib/cmake" "$out/usr/include"
    # c-index-test is a 36M libclang test driver, and the offload tools drive
    # GPU toolchains that do not exist here.
    rm -f "$out/usr/bin"/c-index-test "$out/usr/bin"/amdgpu-arch \
          "$out/usr/bin"/nvptx-arch "$out/usr/bin"/clang-nvlink-wrapper \
          "$out/usr/bin"/clang-offload-bundler "$out/usr/bin"/clang-offload-packager \
          "$out/usr/bin"/clang-sycl-linker

    INSTALL_NAME_TOOL="${nativeMesonTools}/bin/install_name_tool"
    for dylib in "$out"/usr/lib/*.dylib; do
      [ -e "$dylib" ] || continue
      base=$(basename "$dylib")
      "$INSTALL_NAME_TOOL" -id "/usr/lib/$base" "$dylib" 2>/dev/null || true
    done
    for f in "$out"/usr/bin/* "$out"/usr/lib/*.dylib; do
      [ -f "$f" ] || continue
      for dylib in "$out"/usr/lib/*.dylib; do
        [ -e "$dylib" ] || continue
        base=$(basename "$dylib")
        "$INSTALL_NAME_TOOL" -change "@rpath/$base" "/usr/lib/$base" "$f" 2>/dev/null || true
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "clang, cross-built to run on PureDarwin (links the cross-built libLLVM, drives cctools ld)";
    platforms = platforms.linux;
  };
}
