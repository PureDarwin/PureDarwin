{ stdenv
, lib
, requireFile
, meson
, ninja
, python3
, pkg-config
, darwinCrossToolchain
, nativeLd
, libSystem
, dbus
, expat
, libX11
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  targetInfo = import ../lib/target-info.nix targetTriple;

  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
  rawClang = "/nix/store/h6wfr7hsc4013lzp1igizkcd1awx8mcm-clang-21.1.8/bin/clang";
  rawClangxx = "/nix/store/h6wfr7hsc4013lzp1igizkcd1awx8mcm-clang-21.1.8/bin/clang++";
in
stdenv.mkDerivation {
  pname = "puredarwin-dbus";
  inherit (dbus) version;
  src = dbus.src;

  nativeBuildInputs = [ meson ninja python3 pkg-config ];
  buildInputs = [ expat libX11 ];

  postPatch = ''
    for fn in accept4 getrandom close_range clearenv prlimit setresuid \
              getresuid closefrom inotify_init1 prctl getpeerucred pipe2; do
      sed -i "/^    '$fn',\$/d" meson.build
    done
    sed -i "/^subdir('test')\$/d" meson.build
    patchShebangs .
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PKG_CONFIG_PATH="${expat}/lib/pkgconfig:${libX11}/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > cross.txt <<CROSSFILE
[binaries]
c = '${rawClang}'
cpp = '${rawClangxx}'
ar = '${darwinCrossToolchain}/bin/${targetTriple}-ar'
strip = '${darwinCrossToolchain}/bin/${targetTriple}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[host_machine]
system = 'darwin'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[built-in options]
c_args = ['-target', '${targetInfo.clangTarget}', '-isysroot', '$DARWIN_SDK_ROOT', '-I${libSystem}/usr/include', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector']
c_link_args = ['-target', '${targetInfo.clangTarget}', '-isysroot', '$DARWIN_SDK_ROOT', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-undefined,dynamic_lookup', '-lSystem']

[properties]
needs_exe_wrapper = true
CROSSFILE

    meson setup build --cross-file cross.txt \
      -Dsystemd=disabled -Dselinux=disabled -Dapparmor=disabled -Dlibaudit=disabled \
      -Depoll=disabled -Dkqueue=disabled -Dx11_autolaunch=disabled \
      -Ddoxygen_docs=disabled -Dxml_docs=disabled -Dqt_help=disabled \
      -Dasserts=false -Dmodular_tests=disabled \
      -Dsystem_socket=/var/run/dbus/system_bus_socket \
      -Dsystem_pid_file=/var/run/dbus/pid \
      --prefix=$out

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
    substituteInPlace $out/share/dbus-1/system.conf --replace-fail "$out" ""
    substituteInPlace $out/share/dbus-1/session.conf --replace-fail "$out" "" || true

    INSTALL_NAME_TOOL="${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool"
    "$INSTALL_NAME_TOOL" -id /lib/libdbus-1.3.dylib "$out/lib/libdbus-1.3.dylib"
    for bin in dbus-daemon dbus-send dbus-monitor dbus-launch dbus-run-session \
               dbus-update-activation-environment dbus-uuidgen \
               dbus-cleanup-sockets dbus-test-tool; do
      f="$out/bin/$bin"
      if [ -f "$f" ]; then
        "$INSTALL_NAME_TOOL" -change @rpath/libdbus-1.3.dylib /lib/libdbus-1.3.dylib "$f"
      fi
    done
    if [ -f "$out/libexec/dbus-daemon-launch-helper" ]; then
      "$INSTALL_NAME_TOOL" -change @rpath/libdbus-1.3.dylib /lib/libdbus-1.3.dylib \
        "$out/libexec/dbus-daemon-launch-helper"
    fi

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "D-Bus (libdbus + dbus-daemon), cross-built for PureDarwin (no systemd/SELinux/AppArmor/audit, kqueue/epoll off)";
    platforms = platforms.linux;
  };
}
