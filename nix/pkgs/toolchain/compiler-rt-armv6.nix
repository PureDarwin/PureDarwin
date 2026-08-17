{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, llvmSrc
, llvmVersion
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
  pname = "puredarwin-compiler-rt-armv6";
  version = llvmVersion;
  src = llvmSrc;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    SDK="$PWD/sdk/MacOSX11.3.sdk"

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
      $CC -arch armv6 -isysroot "$SDK" -mmacosx-version-min=11.0 \
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
