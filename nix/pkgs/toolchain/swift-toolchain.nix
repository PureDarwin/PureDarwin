{ stdenv
, lib
, fetchurl
, autoPatchelfHook
, xar
, cpio
, zlib
, libuuid
, libxml2
, ncurses
, sqlite
, curl
, libedit
, python3
}:

# swift.org's prebuilt toolchain, host tool only: its swift-frontend and
# clang (Apple's llvm fork) emit Mach-O for every Darwin arch
let
  version = "6.3.3";
  isDarwin = stdenv.hostPlatform.isDarwin;
  # one universal installer serves both mac architectures
  macos = {
    url = "https://download.swift.org/swift-${version}-release/xcode/swift-${version}-RELEASE/swift-${version}-RELEASE-osx.pkg";
    hash = "sha256-7oLld3TWZQ+UqgYwJDXW9EoFW5QRaY247LhdmjvMkdA=";
  };
  tarballs = {
    x86_64-linux = {
      url = "https://download.swift.org/swift-${version}-release/ubuntu2404/swift-${version}-RELEASE/swift-${version}-RELEASE-ubuntu24.04.tar.gz";
      hash = "sha256-2oJypf3czWWxUp7Q5S4EUm4urdQjfVjWIg7+uXPGzRk=";
    };
    aarch64-linux = {
      url = "https://download.swift.org/swift-${version}-release/ubuntu2404-aarch64/swift-${version}-RELEASE/swift-${version}-RELEASE-ubuntu24.04-aarch64.tar.gz";
      hash = "sha256-RxJjlUKWU/p2jTcGVYduwbaPapXHiE9eTxeXABQcm38=";
    };
    aarch64-darwin = macos;
    x86_64-darwin = macos;
  };
  src = tarballs.${stdenv.hostPlatform.system}
    or (throw "swift-toolchain: no swift.org tarball wired for ${stdenv.hostPlatform.system}");
in
stdenv.mkDerivation ({
  pname = "swift-toolchain";
  inherit version;

  src = fetchurl src;

  # the mac binaries are already Mach-O against the system's own libraries
  nativeBuildInputs = if isDarwin then [ xar cpio ] else [ autoPatchelfHook ];
  buildInputs = lib.optionals (!isDarwin) [ stdenv.cc.cc.lib zlib libuuid libxml2 ncurses sqlite curl libedit python3 ];
  # lldb, lld and FoundationXML pull in libraries the runtime build never runs
  autoPatchelfIgnoreMissingDeps = [ "libpython3*" "libedit.so.2" "libncurses.so.6" "libtinfo.so.6" "libpanel.so.6" "libxml2.so.2" ];

  dontConfigure = true;
  dontBuild = true;
  dontStrip = true;

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp -a usr/. $out/
    runHook postInstall
  '';

  meta = {
    description = "swift.org Swift ${version} toolchain (host compiler for the PD Swift runtime)";
    homepage = "https://www.swift.org";
    license = lib.licenses.asl20;
    platforms = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin" ];
  };
} // lib.optionalAttrs isDarwin {
  # a flat installer: a gzip cpio payload of the .xctoolchain inside a xar archive
  unpackPhase = ''
    runHook preUnpack
    xar -xf $src
    gzip -dc swift-${version}-RELEASE-osx-package.pkg/Payload | cpio -idm --quiet
    sourceRoot=.
    runHook postUnpack
  '';
})
