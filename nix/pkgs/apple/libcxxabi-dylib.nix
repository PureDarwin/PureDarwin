{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, src
, appleSdk
}:

let

  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";

  # libc++abi ABI-layer sources (mirrors _abi_srcs in the CMakeLists).
  abiSrcs = [
    "cxa_demangle" "stdlib_new_delete" "cxa_aux_runtime" "cxa_handlers"
    "cxa_default_handlers" "cxa_exception" "cxa_exception_storage"
    "cxa_personality" "cxa_virtual" "cxa_guard" "private_typeinfo"
    "stdlib_typeinfo" "stdlib_exception" "stdlib_stdexcept" "fallback_malloc"
    "abort_message" "pd_bootstrap_runtime" "cxa_vector"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-libcxxabi-dylib";
  version = "1";

  inherit src;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    L=src/Libraries
    ABI=$L/libcxxabi
    CXX=$L/libcxx
    CONFIG=$ABI/config

    # Common include/flag block for the libc++abi + libc++ subset. Matches
    # _abi_flags in src/Libraries/libcxxabi/CMakeLists.txt. The nix toolchain
    # (vanilla nixpkgs clang) uses ordinary include ordering, so libcxx/include
    # is a plain -I (see the long comment in that CMakeLists).
    ABI_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include \
      -std=c++23 -nostdinc++ -funwind-tables -fexceptions -fPIC -Os -DNDEBUG \
      -I $CONFIG -I $ABI/include -I $ABI/src -I $CXX/src -I $CXX/include \
      -D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY \
      -DLIBCXX_BUILDING_LIBCXXABI"

    objs=""

    # the exception globals are per thread here, this dylib is the process's only c++ abi
    for s in ${lib.concatStringsSep " " abiSrcs}; do
      ${cc} $ABI_FLAGS -c "$ABI/src/$s.cpp" -o "$s.o"
      objs="$objs $s.o"
    done

    # the unwinder lives in libSystem and libc++ owns every std:: library symbol,
    # this dylib is the c++ abi only and imports _Unwind_* from libSystem

    # Link everything into one dylib. -fixup_chains: same eager-bind fix as
    # corefoundation.nix/icucore.nix (PD's dyld lazy-bind path is fragile).
    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,/usr/lib/libc++abi.dylib \
      -Wl,-fixup_chains \
      -lSystem \
      -o libc++abi.dylib $objs

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/usr/include
    # install_name is already baked in at link time (-Wl,-install_name);
    # no install_name_tool pass (llvm's chokes on LC_DYLD_CHAINED_FIXUPS).
    cp libc++abi.dylib $out/usr/lib/
    # cxxabi.h header for consumers that #include it.
    cp -a src/Libraries/libcxxabi/include/. $out/usr/include/ 2>/dev/null || true
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin libc++abi.dylib (the c++ abi runtime, unwinding comes from libSystem)";
    platforms = platforms.unix;
  };
}
