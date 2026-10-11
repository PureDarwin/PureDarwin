{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, appleSdk
, libdrmSrc
, vulkanHeaders
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# the DRM virtio-gpu calls mesa's venus driver makes, on IOVirtIOGPU's venus user client, plus vnsmoke, the kext check that runs without mesa.
# the pkg-config file is named libdrm so mesa's dependency('libdrm') resolves to it
stdenv.mkDerivation {
  pname = "puredarwin-libdrm-virtgpu";
  version = "0.1";

  src = libdrmSrc;
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    shim=${../../../src/Tools/libdrm-virtgpu}

    mkdir -p inc/libdrm
    cp xf86drm.h libsync.h inc/
    cp include/drm/*.h inc/libdrm/
    cp ${../../../src/Kernel/Extensions/IOVirtIOGPU/IOVirtIOGPU3DShared.h} inc/IOVirtIOGPU3DShared.h
    cp $shim/pd_virtgpu_drm.h inc/

    cc() {
      ${darwinCrossToolchain}/bin/${targetTriple}-clang \
        -isysroot "$SDK" -mmacosx-version-min=26.5 -O2 -Wall \
        -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
        -I${libSystem}/usr/include -I${corefoundation}/include -Iinc -Iinc/libdrm \
        -fuse-ld=${nativeLd}/bin/ld -nostdlib \
        -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
        -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
        -Wl,-platform_version,macos,26.5,26.5 "$@"
    }
    cc -dynamiclib -Wl,-fixup_chains -Wl,-install_name,/usr/lib/libdrm_virtgpu.dylib \
      $shim/pd_virtgpu_drm.c -lIOKitCF -lCoreFoundation -lSystem -o libdrm_virtgpu.dylib
    cc $shim/vnsmoke.c -lIOKitCF -lCoreFoundation -lSystem -o vnsmoke
    cc -I${vulkanHeaders}/include $shim/vnprobe.c -lSystem -o vnprobe
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    shim=${../../../src/Tools/libdrm-virtgpu}
    install -Dm755 libdrm_virtgpu.dylib $out/usr/lib/libdrm_virtgpu.dylib
    install -Dm755 vnsmoke $out/usr/local/bin/vnsmoke
    install -Dm755 vnprobe $out/usr/local/bin/vnprobe
    install -Dm755 $shim/vntest.sh $out/usr/local/bin/vntest
    install -Dm644 $shim/org.puredarwin.vnsmoke.plist \
      $out/System/Library/LaunchDaemons/org.puredarwin.vnsmoke.plist
    mkdir -p $out/lib/pkgconfig $out/include
    cp -r inc/. $out/include/
    cat > $out/lib/pkgconfig/libdrm.pc <<PC
prefix=$out
includedir=$out/include
libdir=$out/usr/lib

Name: libdrm
Description: libdrm interface over IOVirtIOGPU's venus client
Version: 2.4.133
Libs: -L$out/usr/lib -ldrm_virtgpu
Cflags: -I$out/include -I$out/include/libdrm
PC
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libdrm virtio-gpu calls over IOVirtIOGPU's venus client";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
