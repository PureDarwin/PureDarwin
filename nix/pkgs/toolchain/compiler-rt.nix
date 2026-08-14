{ stdenv
, lib
, requireFile
, cmake
, ninja
, python3
, darwinCrossToolchain
, nativeLd
, nativeMesonTools
, llvmSrc
, llvmVersion
, targetTriple ? "x86_64-apple-darwin20.4"
, targetArch ? "x86_64"
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-compiler-rt";
  version = llvmVersion;

  src = llvmSrc;

  nativeBuildInputs = [ cmake ninja python3 ];

  postPatch = ''
    substituteInPlace compiler-rt/cmake/Modules/CompilerRTDarwinUtils.cmake \
      --replace-fail "-fPIC -O3 -fvisibility=hidden -DVISIBILITY_HIDDEN -Wall" \
                     "-fPIC -O3 -Wall"
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"

    mkdir -p toolshim
    for cand in ${targetTriple}-lipo arm64-apple-darwin20.4-lipo; do
      if [ -x "${darwinCrossToolchain}/bin/$cand" ]; then
        ln -sf "${darwinCrossToolchain}/bin/$cand" toolshim/lipo
        break
      fi
    done
    if [ ! -e toolshim/lipo ]; then
      echo "compiler-rt: no lipo in the cross toolchain" >&2
      exit 1
    fi
    export PATH="$PWD/toolshim:$PATH"

    commonFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector"

    # PureDarwin exports the builtins from libSystem (they are named in
    # libSystem.exports), and -exported_symbols_list cannot promote a hidden
    # symbol. So force hidden off via DCOMPILER_RT_HAS_VISIBILITY_HIDDEN_FLAG.
    # libSystem must see it to export it.

    cmake -B build -G Ninja compiler-rt/lib/builtins \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_SYSTEM_PROCESSOR=${targetArch} \
      -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
      -DCMAKE_C_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -DCMAKE_C_COMPILER_TARGET=${targetTriple} \
      -DCMAKE_CXX_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang++ \
      -DCMAKE_CXX_COMPILER_TARGET=${targetTriple} \
      -DCMAKE_ASM_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -DCMAKE_ASM_COMPILER_TARGET=${targetTriple} \
      -DCMAKE_AR=${darwinCrossToolchain}/bin/${targetTriple}-ar \
      -DCMAKE_RANLIB=${darwinCrossToolchain}/bin/${targetTriple}-ranlib \
      -DCMAKE_INSTALL_NAME_TOOL=${nativeMesonTools}/bin/install_name_tool \
      -DCMAKE_C_FLAGS="$commonFlags" \
      -DCMAKE_ASM_FLAGS="$commonFlags" \
      -DCMAKE_CXX_FLAGS="$commonFlags" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$out \
      -DDARWIN_osx_SYSROOT="$DARWIN_SDK_ROOT" \
      -DDARWIN_osx_ARCHS=${targetArch} \
      -DDARWIN_osx_BUILTIN_ARCHS=${targetArch} \
      -DCOMPILER_RT_ENABLE_MACCATALYST=OFF \
      -DCOMPILER_RT_HAS_VISIBILITY_HIDDEN_FLAG=OFF \
      -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
      -DCOMPILER_RT_BUILD_BUILTINS=ON \
      -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
      -DCOMPILER_RT_BUILD_XRAY=OFF \
      -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
      -DCOMPILER_RT_BUILD_PROFILE=OFF \
      -DCOMPILER_RT_BUILD_MEMPROF=OFF \
      -DCOMPILER_RT_BUILD_ORC=OFF \
      -DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
      -DCOMPILER_RT_ENABLE_IOS=OFF \
      -DCOMPILER_RT_ENABLE_WATCHOS=OFF \
      -DCOMPILER_RT_ENABLE_TVOS=OFF \
      -DCOMPILER_RT_INCLUDE_TESTS=OFF \
      -DLLVM_ENABLE_ASSERTIONS=OFF

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

    # Darwin names the builtins archive after the platform (libclang_rt.osx.a),
    # not "builtins" as the generic layout does. Accept either and expose a
    # stable libcompiler_rt.a, so consumers do not have to know which.
    # libclang_rt.cc_kext.a is deliberately not used: it is the kext-safe
    # subset, and nothing here links kexts against compiler-rt.
    mkdir -p $out/lib
    found=$(find $out -name 'libclang_rt.osx.a' -o -name 'libclang_rt.builtins*.a' | head -1)
    if [ -z "$found" ]; then
      echo "compiler-rt: no builtins archive was produced" >&2
      find $out -name '*.a' >&2
      exit 1
    fi
    cp "$found" $out/lib/libcompiler_rt.a
    echo "compiler-rt: staged $found as libcompiler_rt.a"
    ${nativeMesonTools}/bin/otool -h "$found" || true

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "LLVM compiler-rt builtins cross-built for PureDarwin (${targetTriple})";
    platforms = platforms.unix;
  };
}
