{ stdenv
, lib
, llvmPackages_21
, makeWrapper
}:

# mslc (prototype): Metal Shading Language source to AIR for air2spv, built for the host. A clang plugin (MSL
# attributes, entry descriptors), PD's own metal_stdlib, and airify (AIR entry functions, metadata, intrinsics)
let
  llvm = llvmPackages_21.libllvm;
  clang = llvmPackages_21.clang-unwrapped;
in
stdenv.mkDerivation {
  pname = "puredarwin-mslc";
  version = "0.1";

  src = lib.fileset.toSource {
    root = ../../../src/Tools/mslc;
    fileset = ../../../src/Tools/mslc;
  };

  nativeBuildInputs = [ makeWrapper ];
  buildInputs = [ llvm clang ];

  buildPhase = ''
    runHook preBuild
    CXXFLAGS="-std=c++17 -O2 -fno-rtti -Wall -Wno-unused-parameter -I${lib.getDev clang}/include -I${lib.getDev llvm}/include"
    $CXX $CXXFLAGS -fPIC -shared plugin.cpp -o mslc-plugin.so
    $CXX $CXXFLAGS airify.cpp -o airify -L${lib.getLib llvm}/lib -lLLVM
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 airify $out/bin/airify
    install -Dm755 mslc-plugin.so $out/lib/mslc-plugin.so
    mkdir -p $out/share/mslc
    cp -r include $out/share/mslc/include
    install -Dm755 mslc.sh $out/libexec/mslc
    makeWrapper $out/libexec/mslc $out/bin/mslc \
      --set CLANG ${clang}/bin/clang --set PLUGIN $out/lib/mslc-plugin.so \
      --set AIRIFY $out/bin/airify --set MSLC_INCLUDE $out/share/mslc/include
    runHook postInstall
  '';

  meta = with lib; {
    description = "Prototype MSL to AIR compiler feeding air2spv";
    platforms = platforms.linux;
  };
}
