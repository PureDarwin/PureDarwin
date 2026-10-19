{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, libdrmAmdgpu
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# one sdma submission through libdrm_amdgpu, checked from the cpu
stdenv.mkDerivation {
  pname = "puredarwin-amdcs";
  version = "0.1";

  src = ../../../src/Userspace/amdcs;
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -O2 -Wall \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -fno-stack-protector -I${libSystem}/usr/include -I${libdrmAmdgpu}/include -I${libdrmAmdgpu}/include/libdrm \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${libdrmAmdgpu}/lib -Wl,-rpath,/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      amdcs.c -ldrm_amdgpu -ldrm -lSystem -o amdcs
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/sbin
    cp amdcs $out/usr/sbin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "libdrm_amdgpu command submission check for PureDarwin's gpu drivers";
    license = lib.licenses.mit;
  };
}
