{ stdenv
, lib
, src
, version
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, xorgproto
  # headers only: GLFW dlopens the libraries
, xHeaders
  # the Wayland backend: client and xkbcommon headers, protocol code from the native wayland-scanner
, wayland
, xkbcommon
, waylandScanner
, compatInclude
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# GLFW (zlib licence) with its X11 and Wayland backends for PureDarwin: X11 under Xorg, Wayland when WAYLAND_DISPLAY is set and DISPLAY is not.
# The window-system libraries and the Vulkan loader are dlopened at run time by their darwin names, as GLFW does with sonames on Linux
let
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  srcs = [
    "context" "init" "input" "monitor" "platform" "vulkan" "window" "egl_context" "osmesa_context"
    "null_init" "null_monitor" "null_window" "null_joystick" "cocoa_time" "posix_module" "posix_thread"
    "posix_poll" "x11_init" "x11_monitor" "x11_window" "xkb_unicode" "glx_context"
    "wl_init" "wl_monitor" "wl_window"
  ];
  protocols = [ "wayland" "viewporter" "xdg-shell" "idle-inhibit-unstable-v1" "pointer-constraints-unstable-v1"
    "relative-pointer-unstable-v1" "fractional-scale-v1" "xdg-activation-v1" "xdg-decoration-unstable-v1" ];
in
stdenv.mkDerivation {
  pname = "puredarwin-glfw";
  inherit version src;

  # libX11.so.6 -> libX11.6.dylib and so on
  postPatch = ''
    sed -i -E 's/"lib([A-Za-z0-9_-]+)\.so\.([0-9]+)"/"lib\1.\2.dylib"/' src/*.c
    # PD's xkbcommon is a static library: it is linked in, and GLFW looks its symbols up in itself
    sed -i 's|"libxkbcommon.0.dylib"|"/usr/local/lib/libglfw.3.dylib"|' src/wl_init.c
  '';

  nativeBuildInputs = [ waylandScanner ];

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    for p in ${lib.concatStringsSep " " protocols}; do
      wayland-scanner client-header deps/wayland/$p.xml src/$p-client-protocol.h
      wayland-scanner private-code deps/wayland/$p.xml src/$p-client-protocol-code.h
    done

    # darwin has no memfd, posix_fallocate, pipe2 or timerfd: shm_open'd objects, and no timers (poll skips fd -1,
    # so key repeat and animated cursors are off)
    mkdir -p compat/sys
    cat > compat/glfw-pd-compat.h <<EOF
    #include <errno.h>
    #include <fcntl.h>
    #include <stdio.h>
    #include <sys/mman.h>
    #include <unistd.h>
    #define HAVE_MEMFD_CREATE 1
    #define MFD_CLOEXEC 0
    #define MFD_ALLOW_SEALING 0
    #define F_ADD_SEALS 1033
    #define F_SEAL_SEAL 1
    #define F_SEAL_SHRINK 2
    static inline int memfd_create(const char *name, unsigned flags) {
      static unsigned n;
      char path[64];
      int fd;
      (void)name, (void)flags;
      snprintf(path, sizeof(path), "/glfw-%d-%u", (int)getpid(), n++);
      fd = shm_open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
      if (fd >= 0)
        shm_unlink(path);
      return fd;
    }
    static inline int posix_fallocate(int fd, off_t offset, off_t len) {
      return ftruncate(fd, offset + len) ? errno : 0;
    }
    static inline int pipe2(int fds[2], int flags) {
      if (pipe(fds))
        return -1;
      if (flags & O_CLOEXEC) {
        fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);
      }
      return 0;
    }
    EOF
    cat > compat/sys/timerfd.h <<EOF
    #include <time.h>
    #define TFD_CLOEXEC 0
    #define TFD_NONBLOCK 0
    struct itimerspec {
      struct timespec it_interval, it_value;
    };
    static inline int timerfd_create(int clock, int flags) { (void)clock, (void)flags; return -1; }
    static inline int timerfd_settime(int fd, int flags, const struct itimerspec *v, struct itimerspec *o) {
      (void)fd, (void)flags, (void)v, (void)o;
      return -1;
    }
    EOF
    for f in ${lib.concatStringsSep " " srcs}; do
      $CC -isysroot ${sdk} -mmacosx-version-min=26.5 -O2 -fPIC -w \
        -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
        -D_GLFW_X11 -D_GLFW_WAYLAND -D_GLFW_BUILD_DLL -Isrc -Icompat -I${compatInclude} \
        -I${lib.getDev wayland}/include -I${lib.getDev xkbcommon}/include \
        $(case $f in wl_*) echo "-include compat/glfw-pd-compat.h" ;; esac) '-D_GLFW_VULKAN_LIBRARY="libvulkan.1.dylib"' \
        -I${libSystem}/usr/include ${lib.concatMapStringsSep " " (d: "-I${lib.getDev d}/include") xHeaders} -I${xorgproto}/include -Iinclude \
        -c src/$f.c -o $f.o
    done
    $CC -isysroot ${sdk} -mmacosx-version-min=26.5 -dynamiclib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -install_name /usr/local/lib/libglfw.3.dylib -current_version ${version} -compatibility_version 3 \
      *.o -Wl,-force_load,${xkbcommon}/lib/libxkbcommon.a -lSystem -o libglfw.3.dylib
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 libglfw.3.dylib $out/usr/local/lib/libglfw.3.dylib
    ln -s libglfw.3.dylib $out/usr/local/lib/libglfw.dylib
    mkdir -p $out/include
    cp -r include/GLFW $out/include/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "GLFW with the X11 backend, cross-built for PureDarwin";
    license = licenses.zlib;
    platforms = platforms.unix;
  };
}
