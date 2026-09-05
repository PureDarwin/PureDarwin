{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, asmjitSrc
, targetTriple ? "x86_64-apple-darwin20.4"
, targetArch ? "x86_64"
, appleSdk
}:

let
  cxx = "${darwinCrossToolchain}/bin/${targetTriple}-clang++";
  isArm64 = targetArch == "arm64";
in
stdenv.mkDerivation {
  pname = "puredarwin-asmjit-test";
  version = "1";
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cp -r ${asmjitSrc} asmjit-src
    chmod -R u+w asmjit-src

    # ASMJIT_STATIC: no dylib export machinery. Only the backend for this
    # target is compiled; the other one is dead weight and doubles build time.
    defs="-DASMJIT_STATIC ${if isArm64 then "-DASMJIT_NO_X86" else "-DASMJIT_NO_AARCH64"}"

    common="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 \
      -std=c++17 -fno-rtti -O2 \
      -nostdinc++ -isystem ${libcxxDylib}/usr/include/c++/v1 \
      -I${libSystem}/usr/include \
      -Iasmjit-src \
      $defs"

    objs=""
    # core/ is mandatory; the arm/ or x86/ backend is selected by $defs above,
    # but the unused one still has to be excluded from the file list or its
    # translation units fail on the missing backend defines.
    srcdirs="asmjit-src/asmjit/core asmjit-src/asmjit/support"
    if ${if isArm64 then "true" else "false"}; then
      srcdirs="$srcdirs asmjit-src/asmjit/arm"
    else
      srcdirs="$srcdirs asmjit-src/asmjit/x86"
    fi

    for d in $srcdirs; do
      [ -d "$d" ] || continue
      for f in "$d"/*.cpp; do
        [ -e "$f" ] || continue
        o="$(echo "$f" | tr '/' '_').o"
        ${cxx} $common -c "$f" -o "$o"
        objs="$objs $o"
      done
    done
    echo "asmjit: compiled $(echo $objs | wc -w) objects"

    ${cxx} $common -c ${./asmjit-test.cpp} -o asmjit-test.o
    objs="$objs asmjit-test.o"

    ${cxx} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -lc++ -lc++abi -lSystem \
      -o asmjit-test $objs
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/bin
    cp asmjit-test $out/usr/bin/
    runHook postInstall
  '';

  dontFixup = true;
  meta = with lib; {
    description = "asmjit smoke test (/usr/bin/asmjit-test): a real JIT library assembling and running code";
    platforms = platforms.unix;
  };
}
