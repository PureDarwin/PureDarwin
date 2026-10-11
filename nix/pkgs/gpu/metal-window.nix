{ stdenv
, lib
, llvmPackages_21
, spirv-headers
, vulkan-headers
, vulkan-loader
, glfw
, mesa
, makeWrapper
}:

# the pdmtl window demo for Linux: Apple-compiled metallibs run by airjitd's executor (CPU JIT or
# Vulkan via air2spv) and shown in a GLFW window. metal-window [--cpu|--copy|--lavapipe|--xcode13] [demo args]
let
  llvm = llvmPackages_21.libllvm;
  cpu = stdenv.hostPlatform.parsed.cpu.name;
  # llvm/backend.c names the AArch64 target: the host's on x86
  targetDefs = lib.optionalString stdenv.hostPlatform.isx86_64 (lib.concatMapStringsSep " "
    (f: "-DLLVMInitializeAArch64${f}=LLVMInitializeX86${f}") [ "TargetInfo" "Target" "TargetMC" "AsmPrinter" ]);
in
stdenv.mkDerivation {
  pname = "puredarwin-metal-window";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src/Tools;
    fileset = lib.fileset.unions [
      ../../../src/Tools/airjitd
      ../../../src/Tools/air2spv
      ../../../src/Tools/pdmtl
    ];
  };

  nativeBuildInputs = [ makeWrapper ];
  buildInputs = [ llvm spirv-headers vulkan-headers vulkan-loader glfw ];

  buildPhase = ''
    runHook preBuild
    CFLAGS="-std=gnu11 -O2 -DAIRJITD_VULKAN ${targetDefs} -Ipdmtl/linux -Iairjitd -Ipdmtl \
      -I${lib.getDev llvm}/include -I${vulkan-headers}/include -I${spirv-headers}/include -I${lib.getDev glfw}/include"
    srcs=""
    for f in airjitd/*.c airjitd/llvm/*.c; do
      case $f in airjitd/main.c|airjitd/static_llvm_shim.c|airjitd/dump.c) continue ;; esac
      srcs="$srcs $f"
    done
    srcs="$srcs airjitd/vulkan/vk.c pdmtl/pdmtl.c pdmtl/pdmtl_window.c pdmtl/linux/darwin.c pdmtl/demo/demo.c"
    for f in spv translate iface air cfg a2s; do
      srcs="$srcs air2spv/$f.c"
    done
    objs=""
    for f in $srcs; do
      o=$(echo "$f" | tr / _); o=''${o%.c}.o
      $CC $CFLAGS -w -c "$f" -o "$o"
      objs="$objs $o"
    done
    $CC -o metal-window $objs -L${lib.getLib llvm}/lib -lLLVM -L${lib.getLib glfw}/lib -lglfw \
      -Wl,-rpath,${vulkan-loader}/lib -lpthread -ldl -lm
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 metal-window $out/libexec/metal-window
    install -Dm644 pdmtl/demo/demo-xcode26.metallib $out/share/metal-window/demo-xcode26.metallib
    install -Dm644 pdmtl/demo/demo-xcode13.metallib $out/share/metal-window/demo-xcode13.metallib
    install -Dm644 ${mesa}/share/vulkan/icd.d/lvp_icd.${cpu}.json $out/share/metal-window/lvp_icd.json
    mkdir -p $out/bin
    cat > $out/bin/metal-window <<EOF
    #!${stdenv.shell}
    # the loader comes from the wrapper, drivers from the system (/run/opengl-driver on NixOS)
    export LD_LIBRARY_PATH=${vulkan-loader}/lib\''${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}
    export AIRJITD_CACHE=\''${AIRJITD_CACHE:-\''${XDG_CACHE_HOME:-\$HOME/.cache}/metal-window}
    mkdir -p "\$AIRJITD_CACHE"
    lib=$out/share/metal-window/demo-xcode26.metallib
    export AIRJITD_VULKAN=1
    while [ \$# -gt 0 ]; do
      case "\$1" in
        --cpu) unset AIRJITD_VULKAN ;;
        --copy) export PDMTL_PRESENT=copy ;;
        --lavapipe) export VK_ICD_FILENAMES=$out/share/metal-window/lvp_icd.json ;;
        --xcode13) lib=$out/share/metal-window/demo-xcode13.metallib ;;
        *) break ;;
      esac
      shift
    done
    exec $out/libexec/metal-window -l "\$lib" "\$@"
    EOF
    chmod +x $out/bin/metal-window
    runHook postInstall
  '';

  meta = with lib; {
    description = "Metal shaders in a Linux window through airjitd's executor (pdmtl demo)";
    platforms = platforms.linux;
    mainProgram = "metal-window";
  };
}
