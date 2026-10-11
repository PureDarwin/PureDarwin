{ stdenv
, lib
, fetchFromGitHub
, llvmPackages_21
, spirv-headers
, vulkan-headers
, vulkan-loader
, glfw
, glm
, stb
, xxd
, mesa
}:

# indium-pd: Indium's Metal-like C++ interface (darlinghq/indium, 0BSD: headers and tests only, its Vulkan backend is not used) over airjitd's executor,
# plus Indium's seven tests, whose shaders are Apple sample code (MIT) precompiled by Apple's metal. Run them as indium-test-NAME [--cpu|--copy|--lavapipe]
let
  llvm = llvmPackages_21.libllvm;
  cpu = stdenv.hostPlatform.parsed.cpu.name;
  targetDefs = lib.optionalString stdenv.hostPlatform.isx86_64 (lib.concatMapStringsSep " "
    (f: "-DLLVMInitializeAArch64${f}=LLVMInitializeX86${f}") [ "TargetInfo" "Target" "TargetMC" "AsmPrinter" ]);
  indium = fetchFromGitHub {
    owner = "darlinghq";
    repo = "indium";
    rev = "8423a7d2f053167030d2bc4a227f96243c740667";
    hash = "sha256-yrGSbR7ZllQnHMN98YMkcn+IF2oOPvuDqP3cIU7BYVg=";
  };
  tests = [ "basic-compute" "triangle" "sampler" "cube" "lighting" "texturing" "cubemap" ];
in
stdenv.mkDerivation {
  pname = "puredarwin-indium-pd";
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
  buildInputs = [ llvm spirv-headers vulkan-headers vulkan-loader glfw glm stb ];

  buildPhase = ''
    runHook preBuild
    INC="-Ipdmtl/linux -Iairjitd -Ipdmtl -I${lib.getDev llvm}/include -I${vulkan-headers}/include \
      -I${spirv-headers}/include -I${lib.getDev glfw}/include"
    objs=""
    for f in airjitd/*.c airjitd/llvm/*.c airjitd/vulkan/vk.c pdmtl/pdmtl.c pdmtl/pdmtl_window.c pdmtl/linux/darwin.c \
             air2spv/spv.c air2spv/translate.c air2spv/iface.c air2spv/air.c air2spv/cfg.c air2spv/a2s.c; do
      case $f in airjitd/main.c|airjitd/static_llvm_shim.c|airjitd/dump.c) continue ;; esac
      o=$(echo "$f" | tr / _); o=''${o%.c}.o
      $CC -std=gnu11 -O2 -DAIRJITD_VULKAN ${targetDefs} $INC -w -c "$f" -o "$o"
      objs="$objs $o"
    done
    for f in pdmtl/indium-pd/*.cpp; do
      o=ipd_$(basename "''${f%.cpp}").o
      $CXX -std=c++17 -O2 -Wall -Wno-unused-parameter -Wno-overloaded-virtual -Wno-mismatched-tags \
        -Ipdmtl/indium-pd/include -I${indium}/include $INC -c "$f" -o "$o"
      objs="$objs $o"
    done
    ar rcs libindium-pd.a $objs
    LIBS="-L${lib.getLib llvm}/lib -lLLVM -L${lib.getLib glfw}/lib -lglfw -Wl,-rpath,${vulkan-loader}/lib -lpthread -ldl -lm"

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
      $CXX -std=c++17 -O1 -w -DGLFW_INCLUDE_NONE -DGLM_ENABLE_EXPERIMENTAL -Igen-$t -Ipdmtl/indium-pd/include \
        -I${indium}/include -I${vulkan-headers}/include -I${lib.getDev glfw}/include -I${glm}/include -I${stb}/include/stb \
        $d/$t.cpp libindium-pd.a $LIBS -o indium-test-$t
    done
    $CC -shared -fPIC -O1 -DGLFW_INCLUDE_NONE -I${lib.getDev glfw}/include pdmtl/indium-pd/tests/xvfb-refresh.c -o xvfb-refresh.so -ldl
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm644 libindium-pd.a $out/lib/libindium-pd.a
    install -Dm755 xvfb-refresh.so $out/lib/indium-pd/xvfb-refresh.so
    install -Dm644 ${mesa}/share/vulkan/icd.d/lvp_icd.${cpu}.json $out/share/indium-pd/lvp_icd.json
    mkdir -p $out/include $out/bin
    cp -r pdmtl/indium-pd/include/indium $out/include/
    for t in ${lib.concatStringsSep " " tests}; do
      install -Dm755 indium-test-$t $out/libexec/indium-pd/indium-test-$t
      cat > $out/bin/indium-test-$t <<EOF
    #!${stdenv.shell}
    # loader from here, drivers from the system: --cpu runs the CPU JIT, --copy copies frames, --lavapipe uses Mesa's CPU Vulkan
    export LD_LIBRARY_PATH=${vulkan-loader}/lib\''${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}
    export AIRJITD_CACHE=\''${AIRJITD_CACHE:-\''${XDG_CACHE_HOME:-\$HOME/.cache}/indium-pd}
    mkdir -p "\$AIRJITD_CACHE"
    export AIRJITD_VULKAN=1
    while [ \$# -gt 0 ]; do
      case "\$1" in
        --cpu) unset AIRJITD_VULKAN ;;
        --copy) export PDMTL_PRESENT=copy ;;
        --lavapipe) export VK_ICD_FILENAMES=$out/share/indium-pd/lvp_icd.json ;;
        *) break ;;
      esac
      shift
    done
    exec $out/libexec/indium-pd/indium-test-$t "\$@"
    EOF
      chmod +x $out/bin/indium-test-$t
    done
    runHook postInstall
  '';

  meta = with lib; {
    description = "Indium's Metal-like C++ API over airjitd's executor, with Indium's tests";
    license = with licenses; [ bsd0 ];
    platforms = platforms.linux;
  };
}
