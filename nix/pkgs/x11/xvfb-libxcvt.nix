{ stdenv
, lib
, meson
, ninja
, pkg-config
, python3
, darwinCrossToolchain
, nativeLd
, libSystem
, libxcvt
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
in
stdenv.mkDerivation {
  pname = "puredarwin-libxcvt";
  version = libxcvt.version or "0";

  src = libxcvt.src;

  nativeBuildInputs = [ meson ninja pkg-config python3 ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cat > puredarwin-cross.ini <<EOF
[binaries]
c = '${darwinCrossToolchain}/bin/${targetTriple}-clang'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
c_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include']
c_link_args = ['-isysroot', '$DARWIN_SDK_ROOT', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,26.5,26.5', '-lSystem']

[host_machine]
system = 'darwin'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'
EOF

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out \
      --libdir=lib \
      --buildtype=debug \
      -Ddefault_library=static

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

    # libxcvt only builds a shared lib (its meson ignores default_library), so
    # give the dylib a real, on-image install-name. Anything that links it -
    # notably Xorg - records this as its LC_LOAD_DYLIB, so it must be a path
    # that exists at runtime. (Without this, ld64 stamped a garbage id and Xorg
    # ended up with LC_LOAD_DYLIB /usr/lib/dyld, which is MH_DYLINKER not a
    # dylib -> "wrong filetype" load failure.) Then stage a copy at
    # $out/usr/lib so the image places it at /usr/lib/libxcvt.0.dylib.
    _int=${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool
    mkdir -p $out/usr/lib
    for _f in $out/lib/libxcvt.*.dylib; do
      [ -L "$_f" ] && continue
      "$_int" -id /usr/lib/$(basename "$_f") "$_f"
      cp "$_f" $out/usr/lib/
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
