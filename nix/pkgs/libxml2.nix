{ stdenv
, lib
, requireFile
, meson
, ninja
, pkg-config
, python3
, git
, darwinCrossToolchain
, nativeLd
, libSystem
, libxml2
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
in
stdenv.mkDerivation {
  pname = "puredarwin-libxml2";
  inherit (libxml2) version src;

  nativeBuildInputs = [ meson ninja pkg-config python3 git ];

  postPatch = ''
    patchShebangs .
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang'
cpp = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang++'
ar = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar'
strip = '${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-mmacosx-version-min=11.0', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylinker_install_name,/usr/lib/dyld', '-Wl,-platform_version,macos,11.0,11.5', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
needs_exe_wrapper = true
EOF

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=static \
      -Ddocs=disabled \
      -Dhttp=disabled \
      -Dtls=disabled \
      -Dicu=disabled \
      -Dpython=disabled \
      -Diconv=disabled \
      -Dzlib=disabled \
      -Dhistory=disabled \
      -Dreadline=disabled

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    ninja -C build install
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libxml2, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
