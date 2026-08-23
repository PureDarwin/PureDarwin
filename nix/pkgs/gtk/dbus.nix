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
, libX11 ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, xorgproto ? null
  # x11_autolaunch is the only thing pulling X into dbus; a Wayland-only image
  # has no X display to autolaunch on.
, withX11 ? true
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;

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
  pname = "puredarwin-dbus${lib.optionalString (!withX11) "-nox"}";
  inherit (dbus) version;
  src = dbus.src;

  nativeBuildInputs = [ meson ninja python3 pkg-config ];
  buildInputs = [ expat ] ++ lib.optionals withX11 [ libX11 libxcb libXau libXdmcp xorgproto ];

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
    # x11.pc pulls xcb/Xau/Xdmcp through Requires.private; without their .pc files
    # on the path pkg-config cannot resolve x11 at all and the x11_autolaunch
    # feature check fails with "X11 autolaunch support requested but not found".
    export PKG_CONFIG_PATH="${expat}/lib/pkgconfig${lib.optionalString withX11 ":${libX11}/lib/pkgconfig:${libxcb}/lib/pkgconfig:${libXau}/lib/pkgconfig:${libXdmcp}/lib/pkgconfig:${xorgproto}/share/pkgconfig"}"
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
c_link_args = ['-target', '${targetInfo.clangTarget}', '-isysroot', '$DARWIN_SDK_ROOT', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-platform_version,macos,11.0,11.5', '-Wl,-undefined,dynamic_lookup', ${lib.optionalString withX11 "'-L${libX11}/lib', '-L${libxcb}/lib', '-L${libXau}/lib', '-L${libXdmcp}/lib', '-lX11', '-lxcb', '-lXau', '-lXdmcp', "}'-lSystem']

[properties]
needs_exe_wrapper = true
CROSSFILE

    # The X libraries are named explicitly on the link line. x11.pc lists xcb/Xau/
    # Xdmcp under Requires.private, which pkg-config does not expand for a dynamic
    # link, so dbus-launch came out with 16 unresolved xcb_* symbols. -undefined
    # dynamic_lookup let that link succeed anyway and pushed the failure to
    # runtime: "lazy symbol binding failed: _xcb_parse_display", which aborts
    # autolaunch with SIGABRT and takes xfconf init down with it.
    #
    # x11_autolaunch is enabled (libX11 is already a dependency): without it
    # dbus-launch --autolaunch fails, and since nothing here starts a session bus,
    # every GTK/XFCE client that talks to D-Bus dies with "Error spawning command
    # line dbus-launch --autolaunch=<machine-id>". Only the system bus is started
    # at boot, by pd-dbus-launch.
    #
    # session_socket_dir is set explicitly because it otherwise defaults to the
    # build-time temporary directory, so session.conf ships
    # "<listen>unix:tmpdir=/build</listen>" and the session bus fails in the guest
    # with 'Failed to bind socket "/build/dbus-XXXXXX"'.
    #
    # --prefix=/ with DESTDIR at install, not --prefix=$out: dbus compiles its
    # own paths in, so a store prefix makes dbus-launch exec
    # /nix/store/.../bin/dbus-daemon and dbus-daemon read
    # /nix/store/.../share/dbus-1/session.conf - neither of which exists in the
    # guest ("Failed to execute message bus daemon"). The staged layout is
    # unchanged, since DESTDIR=$out still yields $out/{bin,lib,share}.
    meson setup build --cross-file cross.txt \
      -Dsystemd=disabled -Dselinux=disabled -Dapparmor=disabled -Dlibaudit=disabled \
      -Depoll=disabled -Dkqueue=disabled -Dx11_autolaunch=${if withX11 then "enabled" else "disabled"} \
      -Ddoxygen_docs=disabled -Dxml_docs=disabled -Dqt_help=disabled \
      -Dasserts=false -Dmodular_tests=disabled \
      -Dsystem_socket=/var/run/dbus/system_bus_socket \
      -Dsystem_pid_file=/var/run/dbus/pid \
      -Dsession_socket_dir=/tmp \
      --prefix=/

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    DESTDIR="$out" ninja -C build install

    # The .pc files inherit prefix=/ from the configure prefix, which is right for
    # the guest but useless to anything cross-compiling against dbus here (picom
    # got -I/include/dbus-1.0). Point them back into the store; only build-time
    # consumers read these, never the guest.
    for pc in "$out"/lib/pkgconfig/*.pc; do
      [ -f "$pc" ] && substituteInPlace "$pc" --replace-quiet "prefix=/" "prefix=$out"
    done

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
