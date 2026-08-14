{ stdenv
, lib
, fetchFromGitHub
, cmake
, ninja
, python3
, mingwAarch64Cc
, mingwAarch64Pthreads
, fexVersion ? "2604"
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "puredarwin-fex-wow64";
  version = fexVersion;

  src = fetchFromGitHub {
    owner = "FEX-Emu";
    repo = "FEX";
    tag = "FEX-${finalAttrs.version}";
    hash = "sha256-E1AQUYu4hmpDAIWuHSPM4kjPSRlSffHp2gQ8Za8mH0s=";
    fetchSubmodules = true;
  };

  postPatch = ''
    substituteInPlace FEXHeaderUtils/FEXHeaderUtils/Syscalls.h \
      --replace-fail '#include <sched.h>' \
        '#ifndef _WIN32
#include <sched.h>
#endif'
  '';

  nativeBuildInputs = [
    cmake
    ninja
    (python3.withPackages (ps: [ ps.setuptools ]))
    mingwAarch64Cc
  ];

  configurePhase = ''
    runHook preConfigure
    export PATH="${mingwAarch64Cc}/bin:$PATH"

    # FEX uses pthreads for thread management (FEXCore/Source/Utils/Threads.cpp).
    # On mingw those live in winpthreads, which nixpkgs builds but does not put
    # on the compiler's default include/library path.
    export CFLAGS="-I${mingwAarch64Pthreads}/include $CFLAGS"
    export CXXFLAGS="-I${mingwAarch64Pthreads}/include $CXXFLAGS"
    export LDFLAGS="-L${mingwAarch64Pthreads}/lib $LDFLAGS"

    cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/Data/CMake/toolchain_mingw.cmake \
      -DMINGW_TRIPLE=aarch64-w64-mingw32 \
      -DENABLE_LTO=False \
      -DBUILD_TESTING=False \
      -DBUILD_TESTS=False \
      -DBUILD_THUNKS=False \
      -DENABLE_ASSERTIONS=False \
      -DOVERRIDE_VERSION=${finalAttrs.version} \
      `# GIT_HASH is split into a 20-byte array; the default "Unknown" is not` \
      `# 40 hex chars so it expands to 0xUn,0xkn,... and fails to compile.` \
      `# There is no .git in a nix source, so give it a valid placeholder.` \
      -DOVERRIDE_HASH=0000000000000000000000000000000000000000 \
      -DCMAKE_INSTALL_PREFIX=$out
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    # Only the WoW64 backend; the rest of FEX targets Linux.
    ninja -C build wow64fex
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    dll=$(find build -name 'libwow64fex.dll' | head -1)
    if [ -z "$dll" ]; then
      echo "fex-wow64: libwow64fex.dll was not produced" >&2
      find build -name '*.dll' >&2
      exit 1
    fi
    # Wine looks for the WoW64 CPU backend alongside its own PE modules.
    mkdir -p $out/usr/lib/wine/aarch64-windows
    cp "$dll" $out/usr/lib/wine/aarch64-windows/
    echo "fex-wow64: installed $dll"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "FEX x86-to-ARM64 translator as libwow64fex.dll, the CPU backend for Wine's new WoW64";
    platforms = platforms.unix;
  };
})
