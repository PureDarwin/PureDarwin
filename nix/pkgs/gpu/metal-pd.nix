{ stdenv
, lib
, fetchFromGitHub
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, libcxxDylib
, libcxxabiDylib
, llvm
, iokit
, corefoundation
, vulkanLoader
, vulkanHeaders
, spirv-headers
, glfw
, glm
, stb
, xxd
, mesa
, spirvCross
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# metal-window and Indium's tests (see metal-window.nix and indium-pd.nix) on PureDarwin's own userland: airjitd's executor with the Vulkan backend, PD libSystem/libc++/libLLVM and the vulkan loader,
# windows from GLFW on Xorg. Run them as metal-pd window|cube|lighting|... [--cpu|--copy|--lavapipe] [args]
let
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  indium = fetchFromGitHub {
    owner = "darlinghq";
    repo = "indium";
    rev = "8423a7d2f053167030d2bc4a227f96243c740667";
    hash = "sha256-yrGSbR7ZllQnHMN98YMkcn+IF2oOPvuDqP3cIU7BYVg=";
  };
  tests = [ "basic-compute" "triangle" "sampler" "cube" "lighting" "texturing" "cubemap" ];
  # llvm/backend.c names the AArch64 target: the host's on x86
  targetDefs = lib.optionalString (lib.hasPrefix "x86_64" targetTriple) (lib.concatMapStringsSep " "
    (f: "-DLLVMInitializeAArch64${f}=LLVMInitializeX86${f}") [ "TargetInfo" "Target" "TargetMC" "AsmPrinter" ]);
in
stdenv.mkDerivation {
  pname = "puredarwin-metal-pd";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src/Tools;
    fileset = lib.fileset.unions [
      ../../../src/Tools/airjitd
      ../../../src/Tools/air2spv
      ../../../src/Tools/pdmtl
    ];
  };

  nativeBuildInputs = [ xxd ];

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    CXX=${darwinCrossToolchain}/bin/${targetTriple}-clang++
    BASE="-isysroot ${sdk} -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I${corefoundation}/include"
    INC="-Iairjitd -Ipdmtl -I${llvm}/usr/include -I${vulkanHeaders}/include -I${spirv-headers}/include -I${glfw}/include -I${mesa}/usr/include -I${spirvCross}/usr/include"
    LINK="-fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot ${sdk} -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -L${llvm}/usr/lib \
      -L${vulkanLoader}/usr/lib -L${iokit}/usr/lib -L${corefoundation}/usr/lib -L${glfw}/usr/local/lib -L${mesa}/usr/lib -L${spirvCross}/usr/lib"
    LIBS="-lEGL -lGL -lspirv-cross-c-shared -lglfw -lLLVM-21 -lvulkan -lIOKitCF -lCoreFoundation -lc++ -lc++abi -lSystem"

    # PD libSystem exports dispatch_apply but not dispatch_apply_f (an export-list gap)
    cat > pd_apply_f.c <<'EOF'
    #include <dispatch/dispatch.h>
    void dispatch_apply_f(size_t n, dispatch_queue_t q, void *ctx, void (*fn)(void *, size_t))
    {
    	dispatch_apply(n, q, ^(size_t i) { fn(ctx, i); });
    }
    EOF

    objs=""
    for f in airjitd/*.c airjitd/llvm/*.c airjitd/vulkan/vk.c airjitd/opengl/*.c pdmtl/pdmtl.c pdmtl/pdmtl_window.c pd_apply_f.c \
             air2spv/spv.c air2spv/translate.c air2spv/iface.c air2spv/air.c air2spv/cfg.c air2spv/a2s.c; do
      case $f in airjitd/main.c|airjitd/static_llvm_shim.c|airjitd/dump.c) continue ;; esac
      o=$(echo "$f" | tr / _); o=''${o%.c}.o
      $CC $BASE -std=gnu11 -O2 -fblocks -DAIRJITD_VULKAN -DAIRJITD_OPENGL ${targetDefs} $INC -w -c "$f" -o "$o" &
      objs="$objs $o"
    done
    wait
    for f in pdmtl/indium-pd/*.cpp; do
      o=ipd_$(basename "''${f%.cpp}").o
      $CXX $BASE -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 -std=c++17 -O2 -w \
        -Ipdmtl/indium-pd/include -I${indium}/include $INC -c "$f" -o "$o" &
      objs="$objs $o"
    done
    wait
    for o in $objs; do [ -f $o ] || { echo "compile failed: $o"; exit 1; }; done
    ${darwinCrossToolchain}/bin/${targetTriple}-ar rcs libmetal-pd.a $objs

    $CC $BASE -std=gnu11 -O2 $INC -c pdmtl/demo/demo.c -o demo.o
    $CXX $LINK demo.o libmetal-pd.a $LIBS -o metal-window
    $CC $BASE -std=gnu11 -O2 $INC -c pdmtl/demo/native.c -o native.o
    $CXX $LINK native.o libmetal-pd.a $LIBS -o metal-native

    # each test's binary assets become headers as Indium's CMake does with xxd -i
    for t in ${lib.concatStringsSep " " tests}; do
      d=${indium}/test/$t; mkdir -p gen-$t
      case $t in basic-compute) v=compute_add ;; triangle|sampler) v=shader ;; *) v=shaders ;; esac
      for m in $d/*.metallib; do xxd -i -n $v $m > gen-$t/$(basename $m .metallib).h; done
      [ -f $d/Image.tga ] && xxd -i -n image $d/Image.tga > gen-$t/Image.h
      [ -f $d/teapot.obj ] && xxd -i -n teapot $d/teapot.obj > gen-$t/teapot.h
      if [ -f $d/spot.obj ]; then
        xxd -i -n spot $d/spot.obj > gen-$t/spot.h
        xxd -i -n spot_texture $d/spot_texture.png > gen-$t/spot_texture.h
      fi
      if [ -d $d/textures ]; then
        for p in px nx py ny pz nz; do xxd -i -n texture_$p $d/textures/$p.png > gen-$t/texture_$p.h; done
      fi
      $CXX $BASE -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 -std=c++17 -O1 -w -DGLFW_INCLUDE_NONE \
        -DGLM_ENABLE_EXPERIMENTAL -Igen-$t -Ipdmtl/indium-pd/include -I${indium}/include $INC \
        -I${glm}/include -I${stb}/include/stb -c $d/$t.cpp -o test-$t.o
      $CXX $LINK test-$t.o libmetal-pd.a $LIBS -o indium-test-$t
    done
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    L=$out/usr/local/libexec/metal-pd
    install -Dm755 metal-window $L/metal-window
    install -Dm755 metal-native $L/metal-native
    for t in ${lib.concatStringsSep " " tests}; do
      install -Dm755 indium-test-$t $L/indium-test-$t
    done
    install -Dm644 pdmtl/demo/demo-xcode26.metallib $out/usr/local/share/metal-pd/demo-xcode26.metallib
    install -Dm644 pdmtl/demo/demo-xcode13.metallib $out/usr/local/share/metal-pd/demo-xcode13.metallib
    mkdir -p $out/usr/local/bin
    cat > $out/usr/local/bin/metal-pd <<'EOF'
    #!/bin/sh
    # metal-pd window|basic-compute|triangle|sampler|cube|lighting|texturing|cubemap [--cpu|--copy|--lavapipe|--xcode13] [args]
    # Vulkan on venus by default, frames reach X or Wayland through Mesa's software wsi (no DRI3/dmabuf on PD)
    L=/usr/local/libexec/metal-pd
    S=/usr/local/share/metal-pd
    name=$1
    [ -n "$name" ] || { echo "usage: metal-pd window|basic-compute|triangle|sampler|cube|lighting|texturing|cubemap [opts]"; exit 2; }
    shift
    # wayland when a compositor is named, else Xorg on :0
    if [ -n "$WAYLAND_DISPLAY" ]; then
      export XDG_SESSION_TYPE=wayland
    else
      : ''${DISPLAY:=:0}
      export DISPLAY XDG_SESSION_TYPE=x11
    fi
    export AIRJITD_VULKAN=1 MESA_VK_WSI_DEBUG=sw
    export AIRJITD_CACHE=''${AIRJITD_CACHE:-/var/tmp/metal-pd-cache}
    mkdir -p "$AIRJITD_CACHE"
    icd=/usr/share/vulkan/icd.d
    [ -z "$VK_DRIVER_FILES" ] && [ -f $icd/virtio_icd.x86_64.json ] && export VK_DRIVER_FILES=$icd/virtio_icd.x86_64.json
    lib=$S/demo-xcode26.metallib
    while [ $# -gt 0 ]; do
      case "$1" in
        --cpu) unset AIRJITD_VULKAN AIRJITD_OPENGL ;;
        --opengl) export AIRJITD_OPENGL=1; unset AIRJITD_VULKAN ;;
        --copy) export PDMTL_PRESENT=copy ;;
        --lavapipe) export VK_DRIVER_FILES=$(ls $icd/lvp_icd*.json | head -1) ;;
        --xcode13) lib=$S/demo-xcode13.metallib ;;
        *) break ;;
      esac
      shift
    done
    case $name in
      native) exec $L/metal-native "$@" ;;
      window) exec $L/metal-window -l "$lib" "$@" ;;
      *) exec $L/indium-test-$name "$@" ;;
    esac
    EOF
    chmod +x $out/usr/local/bin/metal-pd
    install -Dm644 libmetal-pd.a $out/lib/libmetal-pd.a
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Metal demos (pdmtl window, Indium tests) on PureDarwin through airjitd's Vulkan executor";
    license = with licenses; [ bsd0 ];
    platforms = platforms.unix;
  };
}
