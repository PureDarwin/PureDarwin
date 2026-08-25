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
, llvmSrc
, llvmVersion
, nativeTblgen
, nativeLlvmConfig
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-llvm";
  version = llvmVersion;

  src = llvmSrc;

  nativeBuildInputs = [ cmake ninja python3 ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    commonFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include"
    linkFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -lc++ -lc++abi -lSystem"

    cmake -B build -G Ninja llvm \
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
      -DLLVM_HOST_TRIPLE=${targetTriple} \
      -DLLVM_DEFAULT_TARGET_TRIPLE=${targetTriple} \
      -DLLVM_TARGET_ARCH=X86 \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DLLVM_TABLEGEN=${nativeTblgen} \
      -DLLVM_BUILD_LLVM_DYLIB=ON \
      -DLLVM_LINK_LLVM_DYLIB=ON \
      -DLLVM_ENABLE_PIC=ON \
      -DLLVM_ENABLE_RTTI=ON \
      -DLLVM_ENABLE_TERMINFO=OFF \
      -DLLVM_ENABLE_LIBXML2=OFF \
      -DLLVM_ENABLE_ZLIB=OFF \
      -DLLVM_ENABLE_ZSTD=OFF \
      -DLLVM_ENABLE_LIBEDIT=OFF \
      -DLLVM_ENABLE_LIBPFM=OFF \
      -DLLVM_ENABLE_BINDINGS=OFF \
      -DLLVM_ENABLE_OCAMLDOC=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF \
      -DLLVM_INCLUDE_EXAMPLES=OFF \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_INCLUDE_DOCS=OFF \
      -DLLVM_INCLUDE_UTILS=OFF \
      -DLLVM_BUILD_TOOLS=OFF \
      -DLLVM_BUILD_UTILS=OFF \
      -DLLVM_TOOL_LLVM_SHLIB_BUILD=ON

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

    # Mesa finds LLVM by *running* llvm-config, but the installed one is a
    # Darwin binary that cannot execute on the builder. Replace it with a shell
    # script that reports the cross target's paths and flags.
    mkdir -p "$out/usr/bin"
    cat > "$out/usr/bin/llvm-config" <<LLVMCONFIG
#!/bin/sh
prefix="$out/usr"
for arg in "\$@"; do
  case "\$arg" in
    --version)      echo "${llvmVersion}" ;;
    --prefix)       echo "\$prefix" ;;
    --bindir)       echo "\$prefix/bin" ;;
    --includedir)   echo "\$prefix/include" ;;
    --libdir)       echo "\$prefix/lib" ;;
    --cppflags)     echo "-I\$prefix/include -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS" ;;
    --cflags)       echo "-I\$prefix/include" ;;
    --cxxflags)     echo "-I\$prefix/include -std=c++17" ;;
    --ldflags)      echo "-L\$prefix/lib" ;;
    --libs|--libs-file|--system-libs) echo "-lLLVM-${lib.versions.major llvmVersion}" ;;
    --libnames)     echo "libLLVM-${lib.versions.major llvmVersion}.dylib" ;;
    --shared-mode)  echo "shared" ;;
    --link-shared)  ;;
    --targets-built) echo "X86" ;;
    --host-target)  echo "${targetTriple}" ;;
    --has-rtti)     echo "YES" ;;
    --components)   cat "$out/usr/lib/llvm-components.txt" ;;
    *) ;;
  esac
done
LLVMCONFIG
    chmod +x "$out/usr/bin/llvm-config"

    # meson's config-tool method validates each requested module against
    # --components. The list is a property of the LLVM sources, so take it from
    # the same-version native llvm-config and drop the target-specific entries
    # for backends this build does not include (LLVM_TARGETS_TO_BUILD=X86).
    ${nativeLlvmConfig} --components | tr ' ' '\n' \
      | grep -vE '^(aarch64|amdgpu|arm|arc|avr|bpf|csky|directx|hexagon|lanai|loongarch|m68k|mips|msp430|nvptx|powerpc|riscv|sparc|spirv|systemz|ve|webassembly|xcore|xtensa)' \
      | tr '\n' ' ' > "$out/usr/lib/llvm-components.txt"

    # LLVM's own dylib rules override CMAKE_INSTALL_NAME_DIR and stamp
    # @rpath/libLLVM.dylib, which nothing here sets an rpath for - Mesa would
    # link happily and then fail to load at runtime. Rewrite to the absolute
    # path dyld actually searches (/usr/local/lib then /usr/lib, never /lib).
    INT="${nativeMesonTools}/bin/install_name_tool"
    for dylib in "$out"/usr/lib/*.dylib; do
      [ -L "$dylib" ] && continue
      [ -e "$dylib" ] || continue
      base=$(basename "$dylib")
      "$INT" -id "/usr/lib/$base" "$dylib"
      for other in "$out"/usr/lib/*.dylib; do
        [ -L "$other" ] && continue
        [ -e "$other" ] || continue
        "$INT" -change "@rpath/$base" "/usr/lib/$base" "$other" 2>/dev/null || true
      done
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "LLVM ${llvmVersion} runtime libraries, cross-built for PureDarwin (llvmpipe/lavapipe backend)";
    platforms = platforms.linux;
  };
}
