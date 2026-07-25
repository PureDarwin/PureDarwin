{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxabiDylib
, libcxxDylib
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };

  cc = "${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang++";

  testSrc = ''
    #include <vector>
    #include <string>
    #include <unordered_map>
    #include <algorithm>
    #include <thread>
    #include <mutex>
    #include <iostream>

    int main() {
        std::vector<int> v = { 5, 3, 9, 1, 7, 2 };
        std::sort(v.begin(), v.end());

        std::unordered_map<std::string, int> m;
        for (int i = 0; i < (int)v.size(); ++i) m["k" + std::to_string(v[i])] = v[i];

        std::mutex mtx;
        long sum = 0;
        auto worker = [&](int lo, int hi) {
            for (int i = lo; i < hi; ++i) {
                std::lock_guard<std::mutex> g(mtx);
                sum += v[i];
            }
        };
        std::thread t1(worker, 0, 3), t2(worker, 3, (int)v.size());
        t1.join(); t2.join();

        std::cout << "libc++ ok: min=" << v.front() << " max=" << v.back()
                  << " sum=" << sum << " map[k9]=" << m["k9"] << std::endl;
        return (v.front() == 1 && v.back() == 9 && sum == 27 && m["k9"] == 9) ? 0 : 1;
    }
  '';
in
stdenv.mkDerivation {
  pname = "puredarwin-libcxx-test";
  version = "1";

  dontUnpack = true;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    cat > cxx-test.cpp <<'EOF'
    ${testSrc}
    EOF

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -std=c++17 -nostdinc++ \
      -I${libcxxDylib}/usr/include/c++/v1 -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains \
      -lc++ -lc++abi -lSystem \
      -o cxx-test cxx-test.cpp
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/bin
    cp cxx-test $out/usr/bin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libc++.dylib STL smoke test binary (/usr/bin/cxx-test)";
    platforms = platforms.linux;
  };
}
