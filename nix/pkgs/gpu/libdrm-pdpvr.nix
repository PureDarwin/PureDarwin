{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, appleSdk
, libdrmSrc
, targetTriple ? "arm64-apple-darwin20.4"
}:

# libdrm's headers with a PDPowerVR-backed implementation of the calls the powervr winsys makes,
# the pkg-config file is named libdrm so mesa's dependency('libdrm') resolves to it
stdenv.mkDerivation {
  pname = "puredarwin-libdrm-pdpvr";
  version = "2.4.133";

  src = libdrmSrc;
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    shim=${../../../src/Tools/libdrm-pdpvr}
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    mkdir -p inc/libdrm
    cp xf86drm.h xf86drmMode.h libsync.h inc/
    cp include/drm/*.h inc/libdrm/
    cp ${../../../src/Kernel/Extensions/PDPowerVR/include/pd_pvr_uapi.h} inc/pd_pvr_uapi.h
    cp $shim/pd_pvr_drm.h $shim/pd_pvr_internal.h inc/

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -O2 -Wall \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I${corefoundation}/include -Iinc -Iinc/libdrm \
      -dynamiclib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -Wl,-install_name,/usr/lib/libdrm_pdpvr.dylib \
      $shim/pd_pvr_drm.c $shim/pd_kms.c -lIOKitCF -lCoreFoundation -lSystem \
      -o libdrm_pdpvr.dylib
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/lib/pkgconfig $out/include
    cp libdrm_pdpvr.dylib $out/usr/lib/
    cp -r inc/. $out/include/
    cat > $out/lib/pkgconfig/libdrm.pc <<EOF
prefix=$out
includedir=$out/include
libdir=$out/usr/lib

Name: libdrm
Description: libdrm interface over the PDPowerVR user client
Version: 2.4.133
Libs: -L$out/usr/lib -ldrm_pdpvr
Cflags: -I$out/include -I$out/include/libdrm
EOF
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libdrm calls for mesa's powervr winsys, routed to PDPowerVR";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
