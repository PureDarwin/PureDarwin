{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, src
}:

# Builds a real /usr/lib/libc++abi.dylib for PureDarwin.
#
# The same vendored LLVM libc++abi / libunwind / libc++ subtrees that
# src/Libraries/libcxxabi/CMakeLists.txt compiles into *static* archives (for
# ICU) are compiled here with the identical raw-clang flags, but linked into a
# single dynamic library. libunwind and the small libc++ subset are folded in
# so that a downstream `-lc++abi` (e.g. libobjc) resolves the C++ ABI,
# exception, and unwind symbols in one shot.
#
# Kept deliberately in lock-step with that CMakeLists: if the source subset or
# flags there change, mirror them here.

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

  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";

  # libc++abi ABI-layer sources (mirrors _abi_srcs in the CMakeLists).
  abiSrcs = [
    "cxa_demangle" "stdlib_new_delete" "cxa_aux_runtime" "cxa_handlers"
    "cxa_default_handlers" "cxa_exception" "cxa_exception_storage"
    "cxa_personality" "cxa_virtual" "cxa_guard" "private_typeinfo"
    "stdlib_typeinfo" "stdlib_exception" "stdlib_stdexcept" "fallback_malloc"
    "abort_message" "pd_bootstrap_runtime"
  ];
  cxxSrcs = [ ];
in
stdenv.mkDerivation {
  pname = "puredarwin-libcxxabi-dylib";
  version = "1";

  inherit src;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    L=src/Libraries
    ABI=$L/libcxxabi
    CXX=$L/libcxx
    UW=$L/libunwind
    CONFIG=$ABI/config

    # Common include/flag block for the libc++abi + libc++ subset. Matches
    # _abi_flags in src/Libraries/libcxxabi/CMakeLists.txt. The nix toolchain
    # (vanilla nixpkgs clang) uses ordinary include ordering, so libcxx/include
    # is a plain -I (see the long comment in that CMakeLists).
    ABI_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include \
      -std=c++17 -nostdinc++ -funwind-tables -fexceptions -fPIC -Os -DNDEBUG \
      -I $CONFIG -I $ABI/include -I $ABI/src -I $CXX/src -I $CXX/include \
      -D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY \
      -DLIBCXX_BUILDING_LIBCXXABI"

    objs=""

    for s in ${lib.concatStringsSep " " abiSrcs}; do
      extra=""
      [ "$s" = "cxa_exception_storage" ] && extra="-D_LIBCXXABI_HAS_NO_THREADS"
      ${cc} $ABI_FLAGS $extra -c "$ABI/src/$s.cpp" -o "$s.o"
      objs="$objs $s.o"
    done

    for s in ${lib.concatStringsSep " " cxxSrcs}; do
      extra=""
      [ "$s" = "string" ] && extra="-DPD_LIBCXX_NARROW_STRING_ONLY"
      # operator new/delete already come from libcxxabi stdlib_new_delete.cpp.
      [ "$s" = "new" ] && extra="-D_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS"
      ${cc} $ABI_FLAGS -D_LIBCPP_BUILDING_LIBRARY $extra -c "$CXX/src/$s.cpp" -o "cxx_$s.o"
      objs="$objs cxx_$s.o"
    done

    # libunwind subset (mirrors _uw_* in the CMakeLists).
    UW_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include \
      -nostdinc++ -funwind-tables -fPIC -Os -DNDEBUG \
      -I $UW/include -I $UW/src -I $CONFIG -I $CXX/include \
      -D_LIBUNWIND_IS_NATIVE_ONLY -D_LIBUNWIND_BUILDING_LIBUNWIND=1"

    ${cc} $UW_FLAGS -std=c++17 -fno-exceptions -fno-rtti -c "$UW/src/libunwind.cpp" -o "uw_libunwind.o"
    objs="$objs uw_libunwind.o"
    for s in UnwindLevel1 UnwindLevel1-gcc-ext; do
      ${cc} $UW_FLAGS -std=c11 -c "$UW/src/$s.c" -o "uw_$s.o"
      objs="$objs uw_$s.o"
    done
    for s in UnwindRegistersRestore UnwindRegistersSave; do
      ${cc} $UW_FLAGS -c "$UW/src/$s.S" -o "uw_$s.o"
      objs="$objs uw_$s.o"
    done

    # Link everything into one dylib. -fixup_chains: same eager-bind fix as
    # corefoundation.nix/icucore.nix (PD's dyld lazy-bind path is fragile).
    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
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
    description = "PureDarwin libc++abi.dylib (libc++abi + libunwind + libc++ subset, cross-built)";
    platforms = platforms.linux;
  };
}
