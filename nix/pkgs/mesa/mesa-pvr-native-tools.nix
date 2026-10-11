{ stdenv
, lib
, src
, version
, meson
, ninja
, pkg-config
, python3
, bison
, flex
, glslang
, llvmPackages
, spirv-llvm-translator
, spirv-tools
, libdrm
, zlib
, expat
}:

# the powervr driver's internal shaders are built by mesa_clc, vtn_bindgen2 and pco_clc, which
# must run on the build machine: a native build of the same source supplies them
let
  pythonEnv = python3.withPackages (ps: [ ps.mako ps.pyyaml ps.packaging ]);
  clangLib = lib.getLib llvmPackages.clang-unwrapped;
in
stdenv.mkDerivation {
  pname = "mesa-pvr-native-tools";
  inherit version src;

  nativeBuildInputs = [
    meson ninja pkg-config pythonEnv bison flex glslang
    llvmPackages.libllvm.dev
  ];
  buildInputs = [
    llvmPackages.clang-unwrapped
    llvmPackages.libclc
    llvmPackages.libllvm
    spirv-llvm-translator
    spirv-tools
    libdrm
    zlib
    expat
  ];

  # clang's libraries and opencl headers live outside llvm's libdir in nixpkgs
  postPatch = ''
    patchShebangs .
    substituteInPlace meson.build \
      --replace-fail "dirs : llvm_libdir" "dirs : [llvm_libdir, '${clangLib}/lib']"
    substituteInPlace src/compiler/clc/meson.build \
      --replace-fail "join_paths(llvm_libdir, 'clang', clang_version_dir, 'include')" \
                     "join_paths('${clangLib}/lib', 'clang', clang_version_dir, 'include')"
  '';

  mesonFlags = [
    "-Dauto_features=auto"
    "-Dvulkan-drivers=imagination"
    "-Dgallium-drivers="
    "-Dplatforms="
    "-Dopengl=false"
    "-Dgles1=disabled"
    "-Dgles2=disabled"
    "-Degl=disabled"
    "-Dglx=disabled"
    "-Dgbm=disabled"
    "-Dglvnd=disabled"
    "-Dllvm=enabled"
    "-Dshared-llvm=enabled"
    "-Dmesa-clc=enabled"
    "-Dinstall-mesa-clc=true"
    "-Dmesa-clc-bundle-headers=enabled"
    "-Dprecomp-compiler=enabled"
    "-Dinstall-precomp-compiler=true"
    "-Dvalgrind=disabled"
    "-Dlibunwind=disabled"
    "-Dlmsensors=disabled"
    "-Dzstd=disabled"
    "-Dbuild-tests=false"
  ];

  # only the build-time tools are wanted, the native driver is a by-product
  postInstall = ''
    find $out -mindepth 1 -maxdepth 1 ! -name bin -exec rm -rf {} +
    find $out/bin -type f ! -name mesa_clc ! -name vtn_bindgen2 ! -name pco_clc -delete
  '';

  meta = with lib; {
    description = "mesa_clc, vtn_bindgen2 and pco_clc from Mesa, for cross-building the PowerVR Vulkan driver";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
