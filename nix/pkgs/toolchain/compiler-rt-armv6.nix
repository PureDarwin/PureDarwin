{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, llvmSrc
, llvmVersion
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-compiler-rt-armv6";
  version = llvmVersion;
  src = llvmSrc;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC=${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang
    AR=${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar

    mkdir -p obj
    cd compiler-rt/lib/builtins

    for f in *.c; do
      case "$f" in
        emutls.c|enable_execute_stack.c|clear_cache.c|gcc_personality_v0.c) continue ;;
        atomic.c|atomic_*.c) continue ;;
      esac
      obj="$OLDPWD/obj/''${f%.c}.o"
      $CC -arch armv6 -isysroot "$SDK" -mmacosx-version-min=26.5 \
          -c "$f" -o "$obj" \
          -O2 -fno-builtin -fno-stack-protector -ffreestanding \
          -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
          -DCOMPILER_RT_HAS_FLOAT16=0 \
        || { echo "compiler-rt-armv6: skipping $f (does not build for this target)"; rm -f "$obj"; }
    done

    cd "$OLDPWD"
    $AR -crs libcompiler_rt.a obj/*.o
    echo "compiler-rt-armv6: archived $(ls obj/*.o | wc -l) objects"

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/lib
    cp libcompiler_rt.a $out/lib/libcompiler_rt.a
    runHook postInstall
  '';

  dontStrip = true;
  dontFixup = true;

  meta = {
    description = "compiler-rt builtins for ARMv6 Mach-O (Raspberry Pi Zero kernel)";
  };
}
