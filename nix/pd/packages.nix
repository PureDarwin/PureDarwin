# packages.nix - the PureDarwin packages, once for every target; callPackage fills PureDarwin dependencies from the
# target's set, the arguments given here are the nixpkgs inputs a recipe builds from
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
{ pkgs, lib, self, callPackage }:

{
  ncurses = callPackage ../pkgs/base/ncurses.nix {
    ncurses = pkgs.ncurses;
    appleSdk = pkgs.appleSdk;
  };
  libiconv = callPackage ../pkgs/base/libiconv.nix {
    libiconvReal = pkgs.libiconvReal;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  zsh = callPackage ../pkgs/base/zsh.nix {
    zsh = pkgs.zsh;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  bash = callPackage ../pkgs/base/bash.nix {
    bash = pkgs.bash;
    appleSdk = pkgs.appleSdk;
    bison = pkgs.bison;
    gnumake = pkgs.gnumake;
  };
  akff-test = callPackage ../pkgs/apple/akff-test.nix {
    src = ../../tests/appkit-ff;
  };
  appFrameworks = callPackage ../pkgs/apple/app-frameworks.nix {
    zlibHeaders = pkgs.zlib.dev;
    srcs = {
                    CoreMedia = ../../src/Frameworks/CoreMedia;
                    UniformTypeIdentifiers = ../../src/Frameworks/UniformTypeIdentifiers;
                    CFNetwork = ../../src/Frameworks/CFNetwork;
                    GSS = ../../src/Frameworks/GSS;
                    ColorSync = ../../src/Frameworks/ColorSync;
                    WebKit = ../../src/Frameworks/WebKit;
                    libcompression = ../../src/Libraries/libcompression;
                    ImageIO = ../../src/Frameworks/ImageIO;
                    UserNotifications = ../../src/Frameworks/UserNotifications;
                    ScreenCaptureKit = ../../src/Frameworks/ScreenCaptureKit;
                    GameKit = ../../src/Frameworks/GameKit;
                    GameController = ../../src/Frameworks/GameController;
                    ForceFeedback = ../../src/Frameworks/ForceFeedback;
                    CoreHaptics = ../../src/Frameworks/CoreHaptics;
                    ModelIO = ../../src/Frameworks/ModelIO;
                    VideoToolbox = ../../src/Frameworks/VideoToolbox;
                    AVFoundation = ../../src/Frameworks/AVFoundation;
                    MediaPlayer = ../../src/Frameworks/MediaPlayer;
                    CoreMIDI = ../../src/Frameworks/CoreMIDI;
                    AuthenticationServices = ../../src/Frameworks/AuthenticationServices;
                    LocalAuthentication = ../../src/Frameworks/LocalAuthentication;
                    CoreLocation = ../../src/Frameworks/CoreLocation;
                    Vision = ../../src/Frameworks/Vision;
                    ServiceManagement = ../../src/Frameworks/ServiceManagement;
                    IOBluetooth = ../../src/Frameworks/IOBluetooth;
                    Accessibility = ../../src/Frameworks/Accessibility;
                    AddressBook = ../../src/Frameworks/AddressBook;
                    HIServices = ../../src/Frameworks/HIServices;
                    libcups = ../../src/Libraries/libcups;
                  };
  };
  appkit = callPackage ../pkgs/apple/appkit.nix {
    src = ../../src/Frameworks/AppKit;
    appleSdk = pkgs.appleSdk;
  };
  applicationservices = callPackage ../pkgs/apple/applicationservices.nix {
    src = ../../src/Frameworks/ApplicationServices;
  };
  argon-gfx-test = callPackage ../pkgs/apple/argon-gfx-test.nix {
    src = ../../tests/argon-gfx;
  };
  asmjit-test = callPackage ../pkgs/apple/asmjit-test.nix {
    appleSdk = pkgs.appleSdk;
  };
  audiotoolbox = callPackage ../pkgs/apple/audiotoolbox.nix {
    src = ../../src/Frameworks/AudioToolbox;
    coreAudioSrc = ../../src/Frameworks/CoreAudio;
  };
  autoconf = callPackage ../pkgs/base/autoconf.nix {
    autoconf = pkgs.autoconf;
    perl = pkgs.perl;
  };
  bison = callPackage ../pkgs/base/bison.nix {
    bison = pkgs.bison;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  bmake = callPackage ../pkgs/base/bmake.nix {
    bmake = pkgs.bmake;
    appleSdk = pkgs.appleSdk;
  };
  bzip2 = callPackage ../pkgs/base/bzip2.nix {
    bzip2 = pkgs.bzip2;
    appleSdk = pkgs.appleSdk;
  };
  cabextract = callPackage ../pkgs/base/cabextract.nix {
    cabextract = pkgs.cabextract;
    appleSdk = pkgs.appleSdk;
  };
  cairoGobject = callPackage ../pkgs/gtk/cairo-gobject.nix {
    cairoReal = pkgs.cairo;
    appleSdk = pkgs.appleSdk;
  };
  calculator = callPackage ../pkgs/apps/calculator.nix {
    src = ../../src/Userspace/Calculator;
    appleSdk = pkgs.appleSdk;
    librsvg = pkgs.librsvg;
  };
  carbon = callPackage ../pkgs/apple/carbon.nix {
    src = ../../src/Frameworks/Carbon;
  };
  certificates-bundle = callPackage ../pkgs/apple/certificates-bundle.nix {
    src = ../../src/Libraries/Security/certificates;
    openssl = pkgs.openssl;
  };
  cg-ff-tests = callPackage ../pkgs/apple/cg-ff-tests.nix {
    src = ../../tests/cg-ff;
  };
  cg-screen-demo = callPackage ../pkgs/apps/cg-screen-demo.nix {
    src = ../../src/Userspace/cg-screen-demo;
    appleSdk = pkgs.appleSdk;
  };
  cocoa = callPackage ../pkgs/apple/cocoa.nix {
    src = ../../src/Frameworks/Cocoa;
  };
  coreaudiod = callPackage ../pkgs/apple/coreaudiod.nix { };
  coredata = callPackage ../pkgs/apple/coredata.nix {
    src = ../../src/Frameworks/CoreData;
    appleSdk = pkgs.appleSdk;
  };
  corefoundation = callPackage ../pkgs/apple/corefoundation.nix {
    src = "${self.coreFoundationSource}/src/Frameworks/CoreFoundation";
    pdCompatInclude = "${self.coreFoundationSource}/src/Libraries/libSystem/libc/pd-compat-include";
    foundationSrc = "${self.foundationSource}/src/Frameworks/Foundation";
    appleSdk = pkgs.appleSdk;
    compilerRt = null;
      cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  coregraphics = callPackage ../pkgs/apple/coregraphics.nix {
    src = ../../src/Frameworks/CoreGraphics;
    appleSdk = pkgs.appleSdk;
  };
  coreservices = callPackage ../pkgs/apple/coreservices.nix {
    src = self.coreServicesSource;
    appleSdk = pkgs.appleSdk;
  };
  coretext = callPackage ../pkgs/apple/coretext.nix {
    freetype = self.freetype2;
    harfbuzz = self.harfbuzzNoxBuild;
    src = ../../src/Frameworks/CoreText;
    appleSdk = pkgs.appleSdk;
  };
  corevideo = callPackage ../pkgs/apple/corevideo.nix {
    src = ../../src/Frameworks/CoreVideo;
    appleSdk = pkgs.appleSdk;
  };
  curl = callPackage ../pkgs/base/curl.nix {
    curl = pkgs.curl;
    appleSdk = pkgs.appleSdk;
  };
  dock = callPackage ../pkgs/apps/dock.nix {
    librsvg = pkgs.librsvg;
    libjpeg = pkgs.libjpeg;
    src = ../../src/Userspace/Dock;
    appleSdk = pkgs.appleSdk;
  };
  dxvk = callPackage ../pkgs/apps/dxvk.nix {
    dxvk = if pkgs.stdenv.hostPlatform.isx86 then pkgs.dxvk else pkgs.pkgsCross.gnu64.dxvk;
  };
  e2fsprogs = callPackage ../pkgs/base/e2fsprogs.nix {
    e2fsprogs = pkgs.e2fsprogs;
    appleSdk = pkgs.appleSdk;
  };
  elfaot = callPackage ../pkgs/apple/elfaot.nix { };
  elfaotDarwin = callPackage ../pkgs/apple/elfaot-darwin.nix {
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  elftrans = callPackage ../pkgs/apple/elftrans.nix {
    appleSdk = pkgs.appleSdk;
  };
  elftrans-stage = callPackage ../pkgs/apple/elftrans-stage.nix {
    e2fsprogs = pkgs.e2fsprogs;
  };
  epollShim = callPackage ../pkgs/wayland/pd-epoll-shim.nix {
    src = ../../src/Libraries/pd-epoll-shim;
    appleSdk = pkgs.appleSdk;
  };
  expat = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-expat";
    version = pkgs.expat.version;
    src = pkgs.expat.src;
    configureFlags = [
                    "--without-docbook"
                    "--without-examples"
                    "--without-tests"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  fastfetch = callPackage ../pkgs/apps/fastfetch.nix {
    fastfetch = pkgs.fastfetch;
    openglFramework = self.openglFrameworkBuild;
    glu = self.gluBuild;
    libX11 = self.libX11SharedBuild;
    libXext = self.libXextSharedBuild;
    libxcb = self.libxcbSharedBuild;
    libXau = self.libXauSharedBuild;
    libXdmcp = self.libXdmcpSharedBuild;
    mesa = self.mesaBuild;
    appleSdk = pkgs.appleSdk;
      cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  fcft = callPackage ../pkgs/wayland/fcft.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    freetype = self.freetype2;
    harfbuzz = self.harfbuzzNoxBuild;
    appleSdk = pkgs.appleSdk;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  fex-wow64 = callPackage ../pkgs/apps/fex-wow64.nix {
    mingwAarch64Cc = self.mingwAarch64.cc;
    mingwAarch64Pthreads = self.mingwAarch64.pthreads;
      cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  file = callPackage ../pkgs/base/file.nix {
    file = pkgs.file;
    appleSdk = pkgs.appleSdk;
  };
  finder = callPackage ../pkgs/apps/finder.nix {
    librsvg = pkgs.librsvg;
    src = ../../src/Userspace/Finder;
    appleSdk = pkgs.appleSdk;
  };
  flex = callPackage ../pkgs/base/flex.nix {
    flex = pkgs.flex;
    appleSdk = pkgs.appleSdk;
    bison = pkgs.bison;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  foot = callPackage ../pkgs/wayland/foot.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    xkbcommon = self.xkbcommonNoxBuild;
    freetype = self.freetype2;
    harfbuzz = self.harfbuzzNoxBuild;
    appleSdk = pkgs.appleSdk;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  framework-compat = callPackage ../pkgs/apple/framework-compat.nix { };
  frameworks-ff-tests = callPackage ../pkgs/apple/frameworks-ff-tests.nix {
    src = ../../tests/frameworks-ff;
  };
  gettext = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    guestPrefix = true;
    shared = true;
    pname = "puredarwin-gettext";
    version = pkgs.gettext.version;
    src = pkgs.gettext.src;
    deps = [ self.libiconv ];
    preConfigureExtra = ''
                    cd gettext-runtime
                  '';
    configureFlags = [
                    "--with-included-libintl"
                    "--with-libiconv-prefix=${self.libiconv}"
                    "--disable-java"
                    "--disable-csharp"
                    "--disable-libasprintf"
                    "--disable-rpath"
                    "--disable-dependency-tracking"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  git = callPackage ../pkgs/base/git.nix {
    git = pkgs.git;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  glib-networking = callPackage ../pkgs/gtk/glib-networking.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    glibNetworking = pkgs.glib-networking;
    appleSdk = pkgs.appleSdk;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  gnum4 = callPackage ../pkgs/base/gnum4.nix {
    gnum4 = pkgs.gnum4;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  gnumake = callPackage ../pkgs/base/gnumake.nix {
    gnumake = pkgs.gnumake;
    appleSdk = pkgs.appleSdk;
  };
  i3statusShim = callPackage ../pkgs/x11/i3status-shim.nix { };
  iceauth = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-iceauth";
    version = "1.0.11";
    src = pkgs.fetchurl {
                    url = "https://www.x.org/releases/individual/app/iceauth-1.0.11.tar.xz";
                    sha256 = "sha256-nWM88NTR2Y4+8C0YZgNylYtgpnAW6Kcs0ECTqNj41Ok=";
                  };
    deps = [ pkgs.xorgproto self.libX11 self.libICE ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  icu = callPackage ../pkgs/apple/icucore.nix {
    icuSrc = pkgs.icu.src;
    appleSdk = pkgs.appleSdk;
    compilerRt = null;
  };
  installer = callPackage ../pkgs/apps/installer.nix {
    src = ../../src/Userspace/Installer;
    appleSdk = pkgs.appleSdk;
    librsvg = pkgs.librsvg;
  };
  iokit = callPackage ../pkgs/apple/iokit.nix {
    appleSdk = pkgs.appleSdk;
  };
  iomediacheck = callPackage ../pkgs/apple/iomediacheck.nix {
    appleSdk = pkgs.appleSdk;
  };
  dtrace = callPackage ../pkgs/apple/dtrace.nix {
    appleSdk = pkgs.appleSdk;
    libcxxDylib = self.libcxxDylib;
    libzDylib = self.libzDylib;
    bison = pkgs.bison;
    flex = pkgs.flex;
  };
  ioreg = callPackage ../pkgs/apple/ioreg.nix {
    appleSdk = pkgs.appleSdk;
  };
  iosurface = callPackage ../pkgs/apple/iosurface.nix {
    src = ../../src/Frameworks/IOSurface;
  };
  iosurface-tests = callPackage ../pkgs/apple/iosurface-tests.nix {
    src = ../../src/Frameworks/IOSurface/tests;
  };
  launchctl = callPackage ../pkgs/apple/launchctl.nix {
    src = self.libSystemSource;
    appleSdk = pkgs.appleSdk;
  };
  launchd = callPackage ../pkgs/apple/launchd.nix {
    src = self.libSystemSource;
    appleSdk = pkgs.appleSdk;
    compilerRt = null;
  };
  libICE = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libICE";
    version = pkgs.libICE.version;
    src = pkgs.libICE.src;
    deps = [ pkgs.xorgproto pkgs.xtrans ];
    preConfigureExtra = ''
                    export ac_cv_func_arc4random_buf=yes
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libSM = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libSM";
    version = pkgs.libSM.version;
    src = pkgs.libSM.src;
    deps = [ pkgs.xorgproto pkgs.xtrans self.libICE ];
    configureFlags = [
                    "--without-libuuid"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libX11 = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libX11";
    version = pkgs.libX11.version;
    src = pkgs.libX11.src;
    deps = [
                    pkgs.xorgproto
                    pkgs.xtrans
                    self.libxcb
                    self.libXau
                    self.libXdmcp
                  ];
    configureFlags = [
                    "--disable-specs"
                    "--enable-xlocaledir"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXau = callPackage ../pkgs/x11/xvfb-stub-lib.nix {
    name = "Xau";
    version = pkgs.libxau.version or "1.0.12";
    pcName = "xau";
    pcDescription = "X authorization file management library";
    includeFrom = [ pkgs.libxau pkgs.xorgproto ];
    source = ''
                    void *XauGetBestAuthByAddr(unsigned int family, unsigned int address_length, const char *address, unsigned int number_length, const char *number, int types_length, char **types, const int *type_lengths) { (void)family; (void)address_length; (void)address; (void)number_length; (void)number; (void)types_length; (void)types; (void)type_lengths; return 0; }
                    void *XauReadAuth(const char *auth_file_name) { (void)auth_file_name; return 0; }
                    void XauDisposeAuth(void *auth) { (void)auth; }
                  '';
  };
  libXaw = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXaw";
    version = pkgs.libXaw.version;
    src = pkgs.libXaw.src;
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libXext
                    self.libXmu
                    self.libXpm
                    self.libXt
                    self.libSM
                    self.libICE
                  ];
    preConfigureExtra = ''
                    export CFLAGS="$CFLAGS -include limits.h"
                  '';
    postInstallExtra = ''
                    ln -sf libXaw7.a $out/lib/libXaw.a
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXcomposite = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXcomposite";
    version = pkgs.libXcomposite.version;
    src = pkgs.libXcomposite.src;
    deps = [ pkgs.xorgproto self.libX11 self.libXfixes ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXcursor = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXcursor";
    version = pkgs.libXcursor.version;
    src = pkgs.libXcursor.src;
    deps = [ pkgs.xorgproto self.libX11 self.libXfixes self.libXrender ];
    postInstallExtra = ''
                    mkdir -p .libXcursor-dylib
                    (
                      cd .libXcursor-dylib
                      ${self.darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar x "$out/lib/libXcursor.a"
                      ${self.darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
                        -isysroot "$DARWIN_SDK_ROOT" \
                        -mmacosx-version-min=26.5 \
                        -fuse-ld=${self.nativeLd}/bin/ld \
                        -nostdlib \
                        -dynamiclib \
                        -Wl,-install_name,/lib/libXcursor.1.dylib \
                        -Wl,-compatibility_version,1.0.0 \
                        -Wl,-current_version,1.0.2 \
                        -Wl,-undefined,dynamic_lookup \
                        -L${self.libSystem}/usr/lib \
                        -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${self.libSystem}/usr/lib/system/libdyld.dylib \
                        -o "$out/lib/libXcursor.1.dylib" \
                        ./*.o \
                        -lSystem
                    )
                    ln -sf libXcursor.1.dylib "$out/lib/libXcursor.dylib"
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXdamage = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXdamage";
    version = pkgs.libXdamage.version;
    src = pkgs.libXdamage.src;
    deps = [ pkgs.xorgproto self.libX11 self.libXfixes ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXdmcp = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXdmcp";
    version = pkgs.libxdmcp.version;
    src = pkgs.libxdmcp.src;
    deps = [ pkgs.xorgproto ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXext = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXext";
    version = pkgs.libXext.version;
    src = pkgs.libXext.src;
    deps = [ pkgs.xorgproto self.libX11 self.libXau ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXfixes = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXfixes";
    version = pkgs.libXfixes.version;
    src = pkgs.libXfixes.src;
    deps = [ pkgs.xorgproto self.libX11 ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXfont2 = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXfont2";
    version = pkgs.libxfont_2.version;
    src = pkgs.libxfont_2.src;
    deps = [
                    pkgs.xorgproto
                    pkgs.xtrans
                    self.zlib
                    self.freetype2
                    self.libfontenc
                  ];
    configureFlags = [
                    "--disable-devel-docs"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXft = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXft";
    version = pkgs.libXft.version;
    src = pkgs.libXft.src;
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libXrender
                    self.freetype2
                    self.fontconfig
                    self.expat
                  ];
    nativeDeps = [ pkgs.util-macros ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXi = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXi";
    version = pkgs.libXi.version;
    src = pkgs.libXi.src;
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libXext
                    self.libXfixes
                  ];
    configureFlags = [
                    "--disable-malloc0returnsnull"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXmu = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXmu";
    version = pkgs.libXmu.version;
    src = pkgs.libXmu.src;
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libXext
                    self.libXt
                    self.libSM
                    self.libICE
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXpm = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXpm";
    version = pkgs.libXpm.version;
    src = pkgs.libXpm.src;
    deps = [ pkgs.xorgproto self.libX11 ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXrandr = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXrandr";
    version = pkgs.libXrandr.version;
    src = pkgs.libXrandr.src;
    deps = [ pkgs.xorgproto self.libX11 self.libXrender self.libXext ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXrender = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXrender";
    version = pkgs.libXrender.version;
    src = pkgs.libXrender.src;
    deps = [ pkgs.xorgproto self.libX11 ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXres = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXres";
    version = pkgs.libXres.version;
    src = pkgs.libXres.src;
    deps = [ pkgs.xorgproto self.libX11 self.libXext ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libXt = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXt";
    version = pkgs.libXt.version;
    src = pkgs.libXt.src;
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libICE
                    self.libSM
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libapfsrw = callPackage ../pkgs/apple/libapfsrw.nix {
    cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  libcroco = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    guestPrefix = true;
    shared = true;
    pname = "puredarwin-libcroco";
    version = "0.6.13";
    src = pkgs.fetchurl {
                    url = "https://download.gnome.org/sources/libcroco/0.6/libcroco-0.6.13.tar.xz";
                    hash = "sha256-dn7CNK56poRpWzpzVUgiSIgTLgY/kttYV1m0IlcGIdQ=";
                  };
    deps = [ self.glib self.libxml2 self.pcre2 self.libffi self.libiconv self.zlib ];
    configureFlags = [ "--disable-Werror" "--disable-Bsymbolic" ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libcxxDylib = callPackage ../pkgs/apple/libcxx-dylib.nix {
    src = self.libcxxDylibSource;
    appleSdk = pkgs.appleSdk;
  };
  libcxxTest = callPackage ../pkgs/apple/libcxx-test.nix {
    appleSdk = pkgs.appleSdk;
  };
  libcxxabiDylib = callPackage ../pkgs/apple/libcxxabi-dylib.nix {
    src = self.libcxxDylibSource;
    appleSdk = pkgs.appleSdk;
  };
  libdrmVirtgpu = callPackage ../pkgs/gpu/libdrm-virtgpu.nix {
    libdrmSrc = pkgs.libdrm.src;
    vulkanHeaders = pkgs.vulkan-headers;
  };
  libelf = callPackage ../pkgs/gpu/elftoolchain-libelf.nix {
    appleSdk = pkgs.appleSdk;
    gnum4 = pkgs.gnum4;
  };
  libev = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libev";
    version = pkgs.libev.version;
    src = pkgs.libev.src;
    preConfigureExtra = ''
                    export ac_cv_func_poll=yes
                    export ac_cv_func_select=yes
                    export ac_cv_header_poll_h=yes
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libffi = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libffi";
    version = pkgs.libffiReal.version;
    src = pkgs.libffiReal.src;
    configureFlags = [
                    "--disable-docs"
                    "--disable-multi-os-directory"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libfontenc = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libfontenc";
    version = pkgs.libfontenc.version;
    src = pkgs.libfontenc.src;
    deps = [ pkgs.xorgproto self.zlib ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libgbm = callPackage ../pkgs/x11/libgbm.nix {
    appleSdk = pkgs.appleSdk;
  };
  libobjc = callPackage ../pkgs/apple/libobjc.nix {
    src = self.objcSource;
    appleSdk = pkgs.appleSdk;
    compilerRt = null;
  };
  librsvg = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    guestPrefix = true;
    shared = true;
    pname = "puredarwin-librsvg";
    version = "2.40.21";
    src = pkgs.fetchurl {
                    url = "https://download.gnome.org/sources/librsvg/2.40/librsvg-2.40.21.tar.xz";
                    hash = "sha256-92KJBfHK2oTofisUiD7VfYCU3KMoHVvLJOzkJ56akro=";
                  };
    deps = [
                    self.glib self.gdkPixbuf self.cairo self.cairoGobject self.pango
                    self.libxml2 self.libcroco self.libpng self.freetype2 self.fontconfig
                    self.fribidi self.harfbuzz self.expat self.pcre2 self.libffi
                    self.libiconv self.zlib self.pixman
                  ];
    preConfigureExtra = ''
                    # libxml2 installs under include/libxml2/libxml, and the deps
                    # mapping only contributes the include/ level.
                    export CFLAGS="$CFLAGS -I${self.libxml2}/include/libxml2"
                    export CFLAGS="$CFLAGS -include libxml/parser.h"
                    export CFLAGS="$CFLAGS -Wno-incompatible-function-pointer-types"
                    export ac_cv_path_GDK_PIXBUF_QUERYLOADERS="$(command -v true)"
                  '';
    configureFlags = [
                    "--disable-introspection"
                    "--disable-tools"
                    "--enable-pixbuf-loader"
                    "--disable-Bsymbolic"
                  ];
    postInstallExtra = ''
                    nested=$(find "$out/nix" -type d -name loaders 2>/dev/null | head -1)
                    if [ -n "$nested" ]; then
                      mkdir -p "$out/lib/gdk-pixbuf-2.0/2.10.0/loaders"
                      cp -a "$nested"/. "$out/lib/gdk-pixbuf-2.0/2.10.0/loaders/"
                      rm -rf "$out/nix"
                    fi
                    for so in "$out"/lib/gdk-pixbuf-2.0/2.10.0/loaders/*.so; do
                      [ -e "$so" ] || continue
                      ${self.nativeMesonToolsDir}/bin/install_name_tool \
                        -change //lib/librsvg-2.2.dylib /lib/librsvg-2.2.dylib "$so"
                    done
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libxcb = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb";
    version = pkgs.libxcb.version;
    src = pkgs.libxcb.src;
    deps = [
                    pkgs.xorgproto
                    self.libXau
                    self.libXdmcp
                  ];
    nativeDeps = [
                    pkgs.python3
                    pkgs.xcb-proto
                  ];
    configureFlags = [
                    "--disable-devel-docs"
                  ];
    preConfigureExtra = ''
                    export PYTHONPATH="${pkgs.xcb-proto}/${pkgs.python3.sitePackages}:$PYTHONPATH"
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libxcb-image = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-image";
    version = pkgs.libxcb-image.version;
    src = pkgs.libxcb-image.src;
    deps = [ pkgs.xorgproto self.libxcb self.libxcb-util self.libxcb-render-util ];
    postPatchExtra = ''
                    sed -i 's/^SUBDIRS = image test/SUBDIRS = image/' Makefile.in
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libxcb-keysyms = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-keysyms";
    version = pkgs.libxcb-keysyms.version;
    src = pkgs.libxcb-keysyms.src;
    deps = [ pkgs.xorgproto self.libxcb self.libxcb-util ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libxcb-render-util = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-render-util";
    version = pkgs.libxcb-render-util.version;
    src = pkgs.libxcb-render-util.src;
    deps = [ pkgs.xorgproto self.libxcb self.libxcb-util ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libxcb-util = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-util";
    version = pkgs.libxcb-util.version;
    src = pkgs.libxcb-util.src;
    deps = [ pkgs.xorgproto self.libxcb ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libxcbcursor = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-cursor";
    version = pkgs.libxcb-cursor.version;
    src = pkgs.libxcb-cursor.src;
    deps = [
                    pkgs.xorgproto
                    self.libxcb
                    self.libxcb-util
                    self.libxcb-keysyms
                    self.libxcb-image
                    self.libxcb-render-util
                  ];
    nativeDeps = [ pkgs.m4 ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libxkbfile = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxkbfile";
    version = pkgs.libxkbfile.version;
    src = pkgs.libxkbfile.src;
    deps = [ pkgs.xorgproto self.libX11 ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  localeData = callPackage ../pkgs/base/locale-data.nix {
    bison = pkgs.bison;
    flex = pkgs.flex;
  };
  loginwindow = callPackage ../pkgs/apps/loginwindow.nix {
    src = ../../src/Userspace/loginwindow;
    appleSdk = pkgs.appleSdk;
  };
  menubar = callPackage ../pkgs/apps/menubar.nix {
    librsvg = pkgs.librsvg;
    src = ../../src/Userspace/MenuBar;
    appleSdk = pkgs.appleSdk;
  };
  metal-apple-samples = callPackage ../pkgs/gpu/metal-apple-samples.nix {
    unzip = pkgs.unzip;
  };
  metalObjc = callPackage ../pkgs/gpu/metal-objc-pd.nix {
    vulkanHeaders = pkgs.vulkan-headers;
  };
  minibrowser-app = callPackage ../pkgs/apps/minibrowser-app.nix { };
  mps = callPackage ../pkgs/gpu/mps.nix {
    src = ../../src/Frameworks/MetalPerformanceShaders;
  };
  msdosCmds = callPackage ../pkgs/apple/msdos-cmds.nix {
    appleSdk = pkgs.appleSdk;
  };
  mslc = callPackage ../pkgs/gpu/mslc.nix { };
  mslc-pd = callPackage ../pkgs/gpu/mslc-pd.nix {
    clangHeaders = pkgs.llvmPackages_21.clang-unwrapped;
  };
  nano = callPackage ../pkgs/base/nano.nix {
    nano = pkgs.nano;
    appleSdk = pkgs.appleSdk;
  };
  neuwld = callPackage ../pkgs/wayland/neuwld.nix {
    freetype = self.freetype2;
    mesaNox = self.mesa;
    src = ../../src/ThirdParty/neuwld;
    appleSdk = pkgs.appleSdk;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  nvidia580 = callPackage ../pkgs/gpu/nvidia-580.nix { };
  nvidiaDrmSrc = callPackage ../pkgs/gpu/nvidia-drm-src.nix { };
  nvidiaUvmSrc = callPackage ../pkgs/gpu/nvidia-uvm-src.nix { };
  nvidiaLinuxRoot = callPackage ../pkgs/gpu/nvidia-linux-root.nix {
    libxcb = pkgs.libxcb;
      vulkan-tools = pkgs.vulkan-tools;
    virglrenderer = pkgs.virglrenderer;
    vulkan-loader = pkgs.vulkan-loader;
  };
  objcTest = callPackage ../pkgs/apple/objc-test.nix {
    appleSdk = pkgs.appleSdk;
  };
  onyx2d = callPackage ../pkgs/apple/onyx2d.nix {
    src = ../../src/Frameworks/Onyx2D;
    appleSdk = pkgs.appleSdk;
  };
  openssh = callPackage ../pkgs/base/openssh.nix {
    openssh = pkgs.openssh;
    appleSdk = pkgs.appleSdk;
  };
  openssl = callPackage ../pkgs/base/openssl.nix {
    openssl = pkgs.openssl;
    appleSdk = pkgs.appleSdk;
    perl = pkgs.perl;
  };
  pcmplay = callPackage ../pkgs/apple/pcmplay.nix {
    src = ../../src/Userspace/pcmplay;
  };
  pcre2 = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-pcre2";
    version = pkgs.pcre2.version;
    src = pkgs.pcre2.src;
    configureFlags = [
                    "--disable-pcre2-16"
                    "--disable-pcre2-32"
                    "--disable-jit"
                    "--disable-pcre2grep-jit"
                    "--disable-pcre2grep-callout"
                    "--disable-pcre2grep-callout-fork"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  pd-metal-surface = callPackage ../pkgs/gpu/pd-metal-surface.nix {
    vulkanHeaders = pkgs.vulkan-headers;
  };
  pdbless = callPackage ../pkgs/base/pdbless.nix {
    appleSdk = pkgs.appleSdk;
  };
  pddecor = callPackage ../pkgs/apps/pddecor.nix {
    waylandScanner = pkgs.wayland-scanner;
    src = ../../src/Userspace/pddecor;
    protocol = ../../src/ThirdParty/neuswc/protocol/pd-decoration-v1.xml;
    appleSdk = pkgs.appleSdk;
  };
  pdgltri = callPackage ../pkgs/gpu/pdgltri.nix {
    mesa = self.mesaDefaultBuild;
    appleSdk = pkgs.appleSdk;
  };
  pdnvprobe = callPackage ../pkgs/gpu/pdnvprobe.nix {
    appleSdk = pkgs.appleSdk;
  };
  pdnvvk = callPackage ../pkgs/gpu/pdnvvk.nix { };
  pdres = callPackage ../pkgs/apps/pdres.nix {
    src = ../../src/Userspace/pdres;
    appleSdk = pkgs.appleSdk;
  };
  pdsurface = callPackage ../pkgs/x11/pdsurface.nix {
    appleSdk = pkgs.appleSdk;
  };
  perl = callPackage ../pkgs/base/perl.nix {
    perl = pkgs.perl;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  pkgconf = callPackage ../pkgs/base/pkgconf.nix {
    pkgconf = pkgs.pkgconf-unwrapped;
    appleSdk = pkgs.appleSdk;
    autoconf = pkgs.autoconf;
    gnumake = pkgs.gnumake;
      automake = pkgs.automake;
  };
  protocolBuffer = callPackage ../pkgs/apple/protocolbuffer.nix {
    src = "${self.protocolBufferSource}/src/Libraries/ProtocolBuffer";
    appleSdk = pkgs.appleSdk;
  };
  python = callPackage ../pkgs/base/python.nix {
    python3 = pkgs.python3;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  quartzcore = callPackage ../pkgs/apple/quartzcore.nix {
    src = ../../src/Frameworks/QuartzCore;
    appleSdk = pkgs.appleSdk;
  };
  security = callPackage ../pkgs/apple/security.nix {
    mkPureDarwinBuild = self.mkPureDarwin;
    openssl = self.opensslThreadsBuild;
    src = self.securitySource;
  };
  settings = callPackage ../pkgs/apps/settings.nix {
    librsvg = pkgs.librsvg;
    src = ../../src/Userspace/Settings;
    appleSdk = pkgs.appleSdk;
  };
  shared-mime-info = callPackage ../pkgs/base/shared-mime-info.nix {
    shared-mime-info = pkgs.shared-mime-info;
  };
  shim = callPackage ../pkgs/boot/shim.nix { };
  shimIa32 = callPackage ../pkgs/boot/shim-ia32.nix { };
  spirvCross = callPackage ../pkgs/gpu/spirv-cross-pd.nix { };
  startup-notification = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-startup-notification";
    version = pkgs.libstartup_notification.version;
    src = pkgs.libstartup_notification.src;
    deps = [ pkgs.xorgproto self.libX11 self.libxcb self.libxcb-util ];
    configureFlags = [
                    "--x-includes=${lib.getDev self.libX11}/include"
                    "--x-libraries=${self.libX11}/lib"
                  ];
    preConfigureExtra = ''
                    export lf_cv_sane_realloc=yes
                  '';
    postPatchExtra = ''
                    sed -i 's/^SUBDIRS=libsn test doc/SUBDIRS=libsn/' Makefile.in
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  sway = callPackage ../pkgs/wayland/sway.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    freetype = self.freetype2;
    xcb = self.libxcb;
    src = ../../src/ThirdParty/sway;
    appleSdk = pkgs.appleSdk;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  swiftRuntime = callPackage ../pkgs/apple/swift-runtime.nix {
    overlaySrc = ../../src/Libraries/SwiftOverlays;
    objcApinotes = ../../src/Libraries/objc4/runtime/Module/ObjectiveC.apinotes;
    dispatchModulemap = ../../src/Libraries/libSystem/libdispatch/dispatch/darwin/module.modulemap;
    cfModulemap = ../../src/Frameworks/CoreFoundation/Base.subproj/module.modulemap;
    appleSdk = pkgs.appleSdk;
      cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  system-information = callPackage ../pkgs/apps/system-information.nix {
    src = ../../src/Userspace/SystemInformation;
    appleSdk = pkgs.appleSdk;
  };
  systemFonts = callPackage ../pkgs/base/system-fonts.nix { };
  systemStarter = callPackage ../pkgs/apple/systemstarter.nix {
    src = self.libSystemSource;
    appleSdk = pkgs.appleSdk;
  };
  terminal = callPackage ../pkgs/apps/terminal.nix {
    src = ../../src/Userspace/Terminal;
    appleSdk = pkgs.appleSdk;
    librsvg = pkgs.librsvg;
  };
  tllist = callPackage ../pkgs/wayland/tllist.nix { };
  toybox = callPackage ../pkgs/base/toybox.nix {
    toybox = pkgs.toybox;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  uigallery = callPackage ../pkgs/apps/uigallery.nix {
    src = ../../src/Tools/uigallery;
    appleSdk = pkgs.appleSdk;
  };
  unzip = callPackage ../pkgs/base/unzip.nix {
    unzip = pkgs.unzip;
    appleSdk = pkgs.appleSdk;
  };
  wayland = callPackage ../pkgs/wayland/wayland.nix {
    src = ../../src/ThirdParty/wayland;
    appleSdk = pkgs.appleSdk;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  waylandProtocols = callPackage ../pkgs/wayland/wayland-protocols.nix {
    src = ../../src/ThirdParty/wayland-protocols;
  };
  windowserver = callPackage ../pkgs/apple/windowserver.nix {
    waylandScanner = pkgs.wayland-scanner;
    src = ../../src/Frameworks/WindowServer;
    appleSdk = pkgs.appleSdk;
  };
  winetricks = callPackage ../pkgs/apps/winetricks.nix {
    winetricks = pkgs.winetricks;
  };
  wirelessDiagnostics = callPackage ../pkgs/apple/wirelessdiagnostics.nix {
    src = "${self.wirelessDiagnosticsSource}/src/Libraries/WirelessDiagnostics";
  };
  wlroots = callPackage ../pkgs/wayland/wlroots.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    pdgopSource = ../../src/Libraries/PDGOP;
    xcb = self.libxcb;
    src = ../../src/ThirdParty/wlroots;
    appleSdk = pkgs.appleSdk;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  xcb-util-xrm = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-xcb-util-xrm";
    version = pkgs.xcbutilxrm.version;
    src = pkgs.xcbutilxrm.src;
    deps = [ pkgs.xorgproto self.libX11 self.libxcb self.libxcb-util ];
    nativeDeps = [ pkgs.m4 pkgs.util-macros ];
    configureFlags = [
                    "--disable-devel-docs"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  xcbWm = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-wm";
    version = pkgs.libxcb-wm.version;
    src = pkgs.libxcb-wm.src;
    deps = [ pkgs.xorgproto self.libxcb self.libxcb-util ];
    nativeDeps = [ pkgs.m4 ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  xinit = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    guestPrefix = true;
    pname = "puredarwin-xinit";
    version = "1.4.4";
    src = pkgs.fetchurl {
                    url = "https://www.x.org/releases/individual/app/xinit-1.4.4.tar.xz";
                    sha256 = "sha256-QKR8ehZMf5gc43h7Szf35BH7QyMdzeVD1wCUB12s/vk=";
                  };
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libxcb
                    self.libXau
                    self.libXdmcp
                  ];
    preConfigureExtra = ''
                    export LIBS="-lX11 -lxcb -lXau -lXdmcp $LIBS"
                  '';
    configureFlags = [
                    "--with-xserver=/usr/bin/Xorg"
                    # launchd support here is the macOS org.x.startx plist machinery,
                    # which is unrelated to how PureDarwin starts X.
                    "--without-launchd"
                  ];
    postInstallExtra = ''
                    rm -f "$out/bin/startx"
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  xkbcommon = callPackage ../pkgs/x11/xkbcommon.nix {
    xkeyboard-config = self.xkeyboardConfig;
    appleSdk = pkgs.appleSdk;
    bison = pkgs.bison;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  xrandr = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-xrandr";
    version = "1.5.4";
    src = pkgs.fetchurl {
                    url = "https://www.x.org/releases/individual/app/xrandr-1.5.4.tar.xz";
                    hash = "sha256-LK/MsqrySRpAaGdhF6DU+QqzB3JLlv/8VM0dqVN3lAA=";
                  };
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libXrandr
                    self.libXrender
                    self.libXext
                    self.libxcb
                    self.libXau
                    self.libXdmcp
                  ];
    preConfigureExtra = ''
                    export LIBS="-lXrandr -lXrender -lXext -lX11 -lxcb -lXau -lXdmcp $LIBS"
                  '';
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  xvfbFonts = callPackage ../pkgs/x11/xvfb-fonts.nix { };
  xvfbLibXinerama = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXinerama";
    version = pkgs.libXinerama.version;
    src = pkgs.libXinerama.src;
    deps = [ pkgs.xorgproto self.libX11 self.libXext ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  xvfbLibXpresent = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXpresent";
    version = pkgs.libXpresent.version;
    src = pkgs.libXpresent.src;
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libXext
                    self.libXfixes
                    self.libXrandr
                    self.libXrender
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  xvfbLibXxf86vm = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXxf86vm";
    version = pkgs.libXxf86vm.version;
    src = pkgs.libXxf86vm.src;
    deps = [ pkgs.xorgproto self.libX11 self.libXext ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  xwayland = callPackage ../pkgs/x11/xwayland.nix {
    xwayland = pkgs.xwayland;
    xorgproto = pkgs.xorgproto;
    xtrans = pkgs.xtrans;
    xlib = self.libX11;
    xcb = self.libxcb;
    xvfbZlib = self.zlib;
    mesaGlHeaders = pkgs.mesa-gl-headers;
    mesa = self.mesaBuild;
    appleSdk = pkgs.appleSdk;
      meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  xz = callPackage ../pkgs/base/xz.nix {
    xz = pkgs.xz;
    appleSdk = pkgs.appleSdk;
  };
  atspi2Core = callPackage ../pkgs/gtk/at-spi2-core.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    inherit (pkgs) at-spi2-core meson ninja python3;
    appleSdk = pkgs.appleSdk;
  };
  automake = callPackage ../pkgs/base/automake.nix {
    automake = pkgs.automake;
    autoconf = pkgs.autoconf;
    perl = pkgs.perl;
  };
  cairo = callPackage ../pkgs/gtk/cairo.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    inherit (pkgs) cairo xorgproto;
    freetype = self.freetype2;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  cmake = callPackage ../pkgs/base/cmake.nix {
    libcurlDylib = self.libcurl;
    inherit (pkgs) cmake ninja;
    appleSdk = pkgs.appleSdk;
  };
  cursorTheme = callPackage ../pkgs/x11/cursor-theme.nix {
    inherit (pkgs) vanilla-dmz;
  };
  dbus = callPackage ../pkgs/gtk/dbus.nix {
    inherit (pkgs) dbus meson ninja python3 xorgproto;
    appleSdk = pkgs.appleSdk;
  };
  dillo = callPackage ../pkgs/apps/dillo.nix {
    inherit (pkgs) dillo util-macros;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    autoconf = pkgs.autoconf;
    automake = pkgs.automake;
    gettext = pkgs.gettext;
    gnumake = pkgs.gnumake;
  };
  dmenu = callPackage ../pkgs/x11/dmenu.nix {
    inherit (pkgs) dmenu;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  dtbTegra210P2371_2180 = callPackage ../pkgs/boot/linux-dtb.nix {
    linuxSrc = self.linux614Src;
    dts = "nvidia/tegra210-p2371-2180.dts";
    fixups = [ { node = "/gpu@57000000"; prop = "status"; value = "okay"; } ];
    xz = pkgs.xz;
  };
  fltk = callPackage ../pkgs/apps/fltk.nix {
    inherit (pkgs) fltk_1_3 util-macros;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    autoconf = pkgs.autoconf;
    automake = pkgs.automake;
    gettext = pkgs.gettext;
    gnumake = pkgs.gnumake;
  };
  fontconfig = callPackage ../pkgs/x11/fontconfig.nix {
    inherit (pkgs) fontconfig;
    nativeMesonTools = self.nativeMesonToolsDir;
    freetype = self.freetype2;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
    perl = pkgs.perl;
  };
  foundation = callPackage ../pkgs/apple/foundation.nix {
    dnssdInclude = ../../src/Libraries/mDNSResponder/mDNSShared;
    notifyInclude = ../../src/Libraries/XPC/notify;
    src = "${self.foundationSource}/src/Frameworks/Foundation";
    appleSdk = pkgs.appleSdk;
  };
  freetype2 = callPackage ../pkgs/x11/xvfb-freetype.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    inherit (pkgs) zlib freetype;
    appleSdk = pkgs.appleSdk;
    cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  fribidi = callPackage ../pkgs/gtk/fribidi.nix {
    inherit (pkgs) fribidi;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  gdkPixbuf = callPackage ../pkgs/gtk/gdk-pixbuf.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    inherit (pkgs) gdk-pixbuf meson ninja python3;
    appleSdk = pkgs.appleSdk;
  };
  glfw = callPackage ../pkgs/gpu/glfw-pd.nix {
    inherit (pkgs.glfw) src version;
    xHeaders = [ self.libX11SharedBuild self.libXextSharedBuild self.libXrenderSharedBuild self.libXrandrSharedBuild
                    self.libXfixesSharedBuild self.libXiSharedBuild self.libXcursorSharedBuild pkgs.libXinerama ];
    compatInclude = ../../nix/pkgs/wayland/pd-compat-include;
    inherit (pkgs) xorgproto;
  };
  glib = callPackage ../pkgs/gtk/glib.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    inherit (pkgs) glib;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
    perl = pkgs.perl;
  };
  gluBuild = callPackage ../pkgs/mesa/glu.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    mesa = self.mesaBuild;
    inherit (pkgs) meson ninja pkg-config;
    appleSdk = pkgs.appleSdk;
  };
  gtk3 = callPackage ../pkgs/gtk/gtk3.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    glibNative = pkgs.glib.dev;
    mesa = self.mesaBuild;
    inherit (pkgs) gtk3 xorgproto;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  gtkLayerShell = callPackage ../pkgs/wayland/gtk-layer-shell.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    glibNative = pkgs.glib.dev;
    mesa = self.mesaBuild;
    inherit (pkgs) gtk-layer-shell xorgproto;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  harfbuzz = callPackage ../pkgs/gtk/harfbuzz.nix {
    inherit (pkgs) harfbuzz;
    freetype = self.freetype2;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  i3 = callPackage ../pkgs/x11/i3.nix {
    inherit (pkgs) i3;
    inherit (pkgs) xorgproto;
    libxcb-wm = self.xcbWm;
    libxcb-cursor = self.libxcbcursor;
    freetype = self.freetype2;
    appleSdk = pkgs.appleSdk;
    bash = pkgs.bash;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
    perl = pkgs.perl;
  };
  jsonc = callPackage ../pkgs/apps/json-c.nix {
    src = pkgs.json_c.src;
    inherit (pkgs) cmake ninja pkg-config;
    appleSdk = pkgs.appleSdk;
  };
  libcss = callPackage ../pkgs/apps/libcss.nix {
    inherit (pkgs) libcss perl python3;
    appleSdk = pkgs.appleSdk;
  };
  libcurl = callPackage ../pkgs/base/libcurl-dylib.nix {
    inherit (pkgs) curl;
    appleSdk = pkgs.appleSdk;
    autoconf = pkgs.autoconf;
    automake = pkgs.automake;
    gnum4 = pkgs.gnum4;
    gnumake = pkgs.gnumake;
  };
  libdisplayInfo = callPackage ../pkgs/mesa/libdisplay-info.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    inherit (pkgs) libdisplay-info hwdata;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  libdom = callPackage ../pkgs/apps/libdom.nix {
    inherit (pkgs) libdom;
    appleSdk = pkgs.appleSdk;
  };
  libdrm = callPackage ../pkgs/x11/libdrm.nix {
    puredarwinSource = ../../src/Libraries/libdrm;
    src = pkgs.libdrm.src;
    inherit (pkgs) meson ninja pkg-config python3;
    appleSdk = pkgs.appleSdk;
  };
  libepoxy = callPackage ../pkgs/gtk/libepoxy.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    mesa = self.mesaBuild;
    inherit (pkgs) libepoxy xorgproto meson ninja python3;
    appleSdk = pkgs.appleSdk;
  };
  libgcrypt = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libgcrypt";
    inherit (pkgs.libgcrypt) version src;
    deps = [ self.libgpg-error ];
    configureFlags = [
                    "--disable-doc"
                    "--disable-tests"
                    "--disable-asm"
                    "--with-libgpg-error-prefix=${self.libgpg-error}"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libgpg-error = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libgpg-error";
    inherit (pkgs.libgpg-error) version src;
    postPatchExtra = ''
                    cp src/syscfg/lock-obj-pub.x86_64-apple-darwin.h \
                       src/syscfg/lock-obj-pub.darwin20.4.h
                  '';
    preConfigureExtra = ''
                    export CFLAGS="$CFLAGS -include sys/stat.h"
                  '';
    configureFlags = [
                    "--disable-nls"
                    "--disable-doc"
                    "--disable-tests"
                    "--disable-languages"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libhubbub = callPackage ../pkgs/apps/libhubbub.nix {
    inherit (pkgs) libhubbub perl gperf gnused;
    appleSdk = pkgs.appleSdk;
  };
  libjpeg = callPackage ../pkgs/base/libjpeg-turbo.nix {
    inherit (pkgs) libjpeg_turbo cmake ninja;
    appleSdk = pkgs.appleSdk;
  };
  libnsbmp = callPackage ../pkgs/apps/libnsbmp.nix {
    inherit (pkgs) libnsbmp;
    appleSdk = pkgs.appleSdk;
  };
  libnsgif = callPackage ../pkgs/apps/libnsgif.nix {
    inherit (pkgs) libnsgif;
    appleSdk = pkgs.appleSdk;
  };
  libnsutils = callPackage ../pkgs/apps/libnsutils.nix {
    inherit (pkgs) libnsutils;
    appleSdk = pkgs.appleSdk;
  };
  libparserutils = callPackage ../pkgs/apps/libparserutils.nix {
    inherit (pkgs) libparserutils perl;
    appleSdk = pkgs.appleSdk;
  };
  libpng = callPackage ../pkgs/gtk/libpng.nix {
    inherit (pkgs) libpng;
    appleSdk = pkgs.appleSdk;
  };
  libpsl = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libpsl";
    inherit (pkgs.libpsl) version;
    src = pkgs.runCommand "libpsl-src-${pkgs.libpsl.version}" { nativeBuildInputs = [ pkgs.lzip ]; } ''
                    lzip -dc ${pkgs.libpsl.src} | tar -x
                    mkdir -p $out
                    cp -R libpsl-*/. $out/
                    chmod -R u+w $out
                  '';
    nativeDeps = [ pkgs.python3 ];
    configureFlags = [ "--disable-runtime" "--disable-builtin" "--disable-man" ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libsoup = callPackage ../pkgs/base/libsoup.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    glibNative = pkgs.glib.dev;
    inherit (pkgs) meson ninja pkg-config python3;
    libsoup = pkgs.libsoup_3;
    appleSdk = pkgs.appleSdk;
  };
  libtasn1 = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libtasn1";
    inherit (pkgs.libtasn1) version src;
    configureFlags = [ "--disable-doc" "--disable-gtk-doc" ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  libutf8proc = callPackage ../pkgs/base/libutf8proc.nix {
    inherit (pkgs) libutf8proc;
    appleSdk = pkgs.appleSdk;
  };
  libwapcaplet = callPackage ../pkgs/apps/libwapcaplet.nix {
    inherit (pkgs) libwapcaplet;
    appleSdk = pkgs.appleSdk;
  };
  libwebp = callPackage ../pkgs/base/libwebp.nix {
    inherit (pkgs) libwebp cmake ninja;
    appleSdk = pkgs.appleSdk;
  };
  libwnck = callPackage ../pkgs/xfce/libwnck.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    glibNative = pkgs.glib.dev;
    startupNotification = self.startup-notification;
    inherit (pkgs) libwnck xorgproto;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  libxcvt = callPackage ../pkgs/x11/xvfb-libxcvt.nix {
    inherit (pkgs) libxcvt;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  libxml2 = callPackage ../pkgs/base/libxml2.nix {
    inherit (pkgs) libxml2 meson ninja python3 git;
    appleSdk = pkgs.appleSdk;
  };
  libzDylib = callPackage ../pkgs/base/libz-dylib.nix {
    inherit (pkgs) zlib;
    appleSdk = pkgs.appleSdk;
  };
  mesaBuild = callPackage ../pkgs/mesa/mesa.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    libX11 = self.libX11SharedBuild;
    libXext = self.libXextSharedBuild;
    libxcb = self.libxcbSharedBuild;
    libXau = self.libXauSharedBuild;
    libXdmcp = self.libXdmcpSharedBuild;
    virglWinsysSrc = ../../nix/pkgs/mesa/virgl-puredarwin;
    virglAbiHeader = ../../src/Kernel/Extensions/IOVirtIOGPU/IOVirtIOGPU3DShared.h;
    inherit (pkgs) meson ninja pkg-config python3 bison flex xorgproto xtrans;
    appleSdk = pkgs.appleSdk;
    libelf = pkgs.libelf;
      libdrmAmdgpu = null;
  };
  mesaDemos = callPackage ../pkgs/mesa/mesa-demos.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    mesa = self.mesaBuild;
    openglFramework = self.openglFrameworkBuild;
    inherit (pkgs) meson ninja pkg-config xorgproto xtrans;
    appleSdk = pkgs.appleSdk;
    wayland = pkgs.wayland;
    waylandProtocols = null;
    waylandScanner = null;
    xkbcommon = null;
  };
  meson = callPackage ../pkgs/base/meson.nix {
    inherit (pkgs) meson;
  };
  metal-pd = callPackage ../pkgs/gpu/metal-pd.nix {
    vulkanHeaders = pkgs.vulkan-headers;
    inherit (pkgs) spirv-headers glm stb xxd;
  };
  netsurf = callPackage ../pkgs/apps/netsurf.nix {
    glibNative = pkgs.glib.dev;
    gdkPixbufNative = pkgs.gdk-pixbuf.dev;
    inherit (pkgs) inetutils;
    xorgproto = pkgs.xorgproto;
    inherit (pkgs) perl pkg-config nsgenbind;
    appleSdk = pkgs.appleSdk;
  };
  neuswc = callPackage ../pkgs/wayland/neuswc.nix {
    pdwmSource = ../../src/Userspace/pdwm/pdwm.c;
    freetype = self.freetype2;
    mesaNox = self.mesa;
    xkbcommon = self.xkbcommonNoxBuild;
    src = ../../src/ThirdParty/neuswc;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  nghttp2 = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-nghttp2";
    inherit (pkgs.nghttp2) version src;
    configureFlags = [ "--enable-lib-only" "--disable-python-bindings" ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  ninja = callPackage ../pkgs/base/ninja.nix {
    inherit (pkgs) ninja;
    appleSdk = pkgs.appleSdk;
  };
  openglFrameworkBuild = callPackage ../pkgs/apple/opengl-framework.nix {
    mesa = self.mesaBuild;
    glu = self.gluBuild;
    libX11 = self.libX11SharedBuild;
    inherit (pkgs) xorgproto;
    libXext = self.libXextSharedBuild;
    libxcb = self.libxcbSharedBuild;
    libXau = self.libXauSharedBuild;
    libXdmcp = self.libXdmcpSharedBuild;
    src = ../../src/Frameworks/OpenGL;
    appleSdk = pkgs.appleSdk;
  };
  pango = callPackage ../pkgs/gtk/pango.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    inherit (pkgs) pango;
    freetype = self.freetype2;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
    perl = pkgs.perl;
  };
  pixman = callPackage ../pkgs/x11/xvfb-pixman.nix {
    inherit (pkgs) pixman;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  sqlite = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-sqlite";
    inherit (pkgs.sqlite) version;
    src = pkgs.runCommand "sqlite-src-${pkgs.sqlite.version}" { nativeBuildInputs = [ pkgs.unzip ]; } ''
                    unzip -q ${pkgs.sqlite.src}
                    mkdir -p $out
                    cp -R sqlite-src-*/. $out/
                    chmod -R u+w $out
                  '';
    configureFlags = [ "--disable-readline" "--disable-editline" ];
    deps = [ self.zlib ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  venus-tests = callPackage ../pkgs/gpu/venus-tests.nix {
    vulkanHeaders = pkgs.vulkan-headers;
    inherit (pkgs) glslang;
  };
  vte = callPackage ../pkgs/gtk/vte.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    version = "0.70.6";
    src = self.vteSrc;
    glibNative = pkgs.glib.dev;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  vulkan-tools = callPackage ../pkgs/mesa/vulkan-tools.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    vulkanTools = pkgs.vulkan-tools;
    vulkanHeaders = pkgs.vulkan-headers;
    libX11 = self.libX11SharedBuild;
    libXext = self.libXextSharedBuild;
    libxcb = self.libxcbSharedBuild;
    libXau = self.libXauSharedBuild;
    libXdmcp = self.libXdmcpSharedBuild;
    libXrandr = self.libXrandrSharedBuild;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  vulkanLoader = callPackage ../pkgs/mesa/vulkan-loader.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    vulkanLoader = pkgs.vulkan-loader;
    vulkanHeaders = pkgs.vulkan-headers;
    libX11 = self.libX11SharedBuild;
    libxcb = self.libxcbSharedBuild;
    libXau = self.libXauSharedBuild;
    libXdmcp = self.libXdmcpSharedBuild;
    libXrandr = self.libXrandrSharedBuild;
    libXrender = self.libXrenderSharedBuild;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  waylandScanner = callPackage ../pkgs/wayland/wayland-scanner.nix {
    src = ../../src/ThirdParty/wayland;
    inherit (pkgs) expat libxml2;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  webkitgtk = callPackage ../pkgs/apps/webkitgtk.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    glibNative = pkgs.glib.dev;
    libxml2Native = pkgs.libxml2;
    inherit (pkgs) cmake ninja pkg-config python3 perl ruby gperf unifdef;
    webkitgtk = pkgs.webkitgtk_6_0;
    mesa = self.mesaBuild;
    deps = [
                    self.glib self.pcre2 self.libffi self.zlib self.libiconv
                    self.cairo self.cairoGobject self.pixman
                    self.pango self.fribidi self.harfbuzz self.freetype2
                    self.fontconfig self.expat
                    self.gdkPixbuf self.libepoxy self.atspi2Core self.dbus self.libpng
                    self.gtk3 self.icu self.libxml2
                    self.libsoup self.sqlite self.libpsl self.nghttp2
                    self.libgcrypt self.libgpg-error self.libtasn1 self.libjpeg self.libwebp
                    self.wayland self.waylandProtocols self.xkbcommon self.mesaBuild
                    self.libX11 self.libxcb self.libXau self.libXdmcp
                    self.libXext self.libXrender self.libXfixes
                    self.libXcomposite self.libXdamage
                    pkgs.xorgproto
                  ];
    appleSdk = pkgs.appleSdk;
  };
  wine = callPackage ../pkgs/apps/wine.nix {
    metalObjc = self.metalObjcPdNoxBuild;
    inherit (pkgs) perl;
    mingwGcc = pkgs.pkgsCross.mingwW64.buildPackages.gcc;
    mingwBintools = pkgs.pkgsCross.mingwW64.buildPackages.bintools;
    mingwGcc32 = pkgs.pkgsCross.mingw32.buildPackages.gcc;
    mingwBintools32 = pkgs.pkgsCross.mingw32.buildPackages.bintools;
    inherit (pkgs) python3;
    wine = pkgs.wine64;
    inherit (pkgs) xorgproto flex bison;
    libX11 = self.libX11SharedBuild;
    libxcb = self.libxcbSharedBuild;
    libXau = self.libXauSharedBuild;
    libXdmcp = self.libXdmcpSharedBuild;
    libXext = self.libXextSharedBuild;
    libXrender = self.libXrenderSharedBuild;
    libXfixes = self.libXfixesSharedBuild;
    libXi = self.libXiSharedBuild;
    libXcursor = self.libXcursorSharedBuild;
    libXrandr = self.libXrandrSharedBuild;
    freetype = self.freetype2;
    mesa = self.mesaBuild;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  wineTools = callPackage ../pkgs/apps/wine-tools.nix {
    wine = pkgs.wine64;
    inherit (pkgs) flex bison freetype;
    peCc = if pkgs.stdenv.buildPlatform.isAarch then self.mingwAarch64.cc else null;
    gnumake = pkgs.gnumake;
  };
  xcalc = callPackage ../pkgs/x11/xcalc.nix {
    xcalc = pkgs.xcalc;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  xclock = callPackage ../pkgs/x11/xclock.nix {
    xclock = pkgs.xclock;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    autoconf = pkgs.autoconf;
    automake = pkgs.automake;
    gettext = pkgs.gettext;
    gnumake = pkgs.gnumake;
  };
  xeyes = callPackage ../pkgs/x11/xeyes.nix {
    xeyes = pkgs.xeyes;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  xkbcomp = callPackage ../pkgs/x11/xvfb-xkbcomp.nix {
    inherit (pkgs) xkbcomp xorgproto;
    appleSdk = pkgs.appleSdk;
    bison = pkgs.bison;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  xmessage = callPackage ../pkgs/x11/xmessage.nix {
    xmessage = pkgs.xmessage;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  xorg = callPackage ../pkgs/x11/xorg.nix {
    xorg-server = pkgs.xorg-server;
    xvfbZlib = self.zlib;
    inherit (pkgs) xorgproto xtrans;
    libXdmcp = pkgs.libxdmcp;
    mesa = self.mesaBuild;
    mesaGlHeaders = pkgs.mesa-gl-headers;
    glHeaders = pkgs.libglvnd.dev;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  xrdb = callPackage ../pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-xrdb";
    version = "1.2.3";
    src = pkgs.fetchurl {
                    url = "https://www.x.org/releases/individual/app/xrdb-1.2.3.tar.xz";
                    sha256 = "sha256-yI9WAkMnjIls5PySrlpForUFoxb/pCf+VbAuXVkUxOQ=";
                  };
    deps = [
                    pkgs.xorgproto
                    self.libX11
                    self.libXmu
                    self.libXt
                    self.libXext
                    self.libSM
                    self.libICE
                    self.libxcb
                    self.libXau
                    self.libXdmcp
                  ];
    preConfigureExtra = ''
                    export LIBS="-lXmu -lXt -lXext -lX11 -lxcb -lXau -lXdmcp -lSM -lICE $LIBS"
                  '';
    configureFlags = [
                    "--with-cpp=/usr/bin/cpp,/bin/cpp"
                  ];
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
    perl = pkgs.perl;
  };
  xterm = callPackage ../pkgs/x11/xterm.nix {
    xterm = pkgs.xterm;
    inherit (pkgs) xorgproto;
    appleSdk = pkgs.appleSdk;
    gnumake = pkgs.gnumake;
  };
  xvfb = callPackage ../pkgs/x11/xvfb.nix {
    xorg-server = pkgs.xorg-server;
    xvfbZlib = self.zlib;
    inherit (pkgs) xorgproto xtrans;
    libXdmcp = pkgs.libxdmcp;
    appleSdk = pkgs.appleSdk;
    meson = pkgs.meson;
    ninja = pkgs.ninja;
  };
  yajl = callPackage ../pkgs/base/yajl.nix {
    inherit (pkgs) yajl;
    appleSdk = pkgs.appleSdk;
    cmake = pkgs.cmake;
  };
  zlib = callPackage ../pkgs/x11/xvfb-zlib.nix {
    inherit (pkgs) zlib;
    appleSdk = pkgs.appleSdk;
    cmake = pkgs.cmake;
    ninja = pkgs.ninja;
  };
  atspi2CoreNox = self.atspi2Core.override { dbus = self.dbusNox; };
  cairoGobjectNox = self.cairoGobject.override { cairo = self.cairoNox; };
  cairoNox = self.cairo.override {
                withX11 = false;
                xorgproto = null; libX11 = null; libXext = null;
                libXrender = null; libxcb = null; libXau = null; libXdmcp = null;
              };
  dbusNox = self.dbus.override {
                withX11 = false;
                libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
                xorgproto = null;
              };
  fastfetchNox = self.fastfetch.override {
                withX11 = false;
                mesa = self.mesaNox;
                glu = self.gluNox;
                openglFramework = self.openglFrameworkNox;
                libX11 = null; libXext = null; libxcb = null;
                libXau = null; libXdmcp = null;
              };
  gluNox = self.gluBuild.override { mesa = self.mesa; };
  gtk3Nox = self.gtk3.override {
                withX11 = false;
                cairo = self.cairoNox;
                cairoGobject = self.cairoGobjectNox;
                pango = self.pangoNox;
                harfbuzz = self.harfbuzzNox;
                libepoxy = self.libepoxyNox;
                mesa = self.mesaNox;
                dbus = self.dbusNox;
                atspi2Core = self.atspi2CoreNox;
                xkbcommon = self.xkbcommonNox;
                libX11 = null;
                libxcb = null;
                libXau = null;
                libXdmcp = null;
                libXext = null;
                libXi = null;
                libXrender = null;
                libXrandr = null;
                libXfixes = null;
                libXcursor = null;
                xorgproto = null;
              };
  gtkLayerShellNox = self.gtkLayerShell.override {
                gtk3 = self.gtk3Nox;
                withX11 = false;
                atspi2Core = self.atspi2CoreNox;
                xkbcommon = self.xkbcommonNox;
                cairo = self.cairoNox;
                cairoGobject = self.cairoGobjectNox;
                pango = self.pangoNox;
                harfbuzz = self.harfbuzzNox;
                libepoxy = self.libepoxyNox;
                dbus = self.dbusNox;
                mesa = self.mesaNox;
                libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
                libXext = null; libXi = null; libXrender = null; libXrandr = null;
                libXfixes = null; libXcursor = null; xorgproto = null;
              };
  harfbuzzNox = self.harfbuzz.override { cairo = self.cairoNox; };
  libdrmAmdgpu = self.libdrm.override { amdgpu = true; nouveau = true; };
  libepoxyNox = self.libepoxy.override {
                withX11 = false;
                mesa = self.mesaNox;
                libX11 = null; xorgproto = null;
              };
  librsvgNox = self.librsvg.override {
                deps = [
                  self.glib self.gdkPixbuf self.cairoNox self.cairoGobjectNoxBuild self.pangoNoxBuild
                  self.libxml2 self.libcroco self.libpng self.freetype2 self.fontconfig
                  self.fribidi self.harfbuzzNoxBuild self.expat self.pcre2 self.libffi
                  self.libiconv self.zlib self.pixman
                ];
              };
  mesaAmdgpu = self.mesaNox.override {
                withAmdgpuDrm = true;
                libdrmAmdgpu = self.libdrmAmdgpu;
                libelf = self.libelf;
              };
  mesaDemosNox = self.mesaDemos.override {
                withX11 = false;
                mesa = self.mesaNox;
                wayland = self.wayland;
                xkbcommon = self.xkbcommonNoxBuild;
                waylandScanner = self.waylandScanner;
                waylandProtocols = self.waylandProtocols;
                libX11 = null; libXext = null; libxcb = null; libXau = null;
                libXdmcp = null; xorgproto = null; xtrans = null;
              };
  mesaGpu = self.mesaAmdgpu.override {
              withIntelDrm = true;
              withNouveauDrm = true;
              withVirgl = true;
              mesaClcTools = pkgs.mesa.cross_tools;
            };
  mesaNox = self.mesaBuild.override {
                withX11 = false;
                libX11 = null; libXext = null; libxcb = null; libXau = null;
                libXdmcp = null; libXxf86vm = null; xorgproto = null; xtrans = null;
              };
  metalAppleSamplesNox = self.metal-apple-samples.override { metalObjc = self.metalObjcPdNox; };
  metalObjcPdNox = self.metalObjc.override {
                vulkanLoader = self.vulkanLoaderNox;
                metalPd = self.metalPdNox;
              };
  metalPdNox = self.metal-pd.override { vulkanLoader = self.vulkanLoaderNox; };
  mpsNox = self.mps.override { metalObjc = self.metalObjcPdNox; };
  netsurfNox = self.netsurf.override {
                withX11 = false;
                gtk3 = self.gtk3NoxBuild;
                cairo = self.cairoNox;
                cairoGobject = self.cairoGobjectNoxBuild;
                pango = self.pangoNoxBuild;
                harfbuzz = self.harfbuzzNoxBuild;
                libepoxy = self.libepoxyNoxBuild;
                dbus = self.dbusNox;
                atspi2Core = self.atspi2CoreNoxBuild;
                libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
                libXext = null; libXi = null; libXrender = null; libXrandr = null;
                libXfixes = null; libXcursor = null; xorgproto = null;
              };
  openglFrameworkNox = self.openglFrameworkBuild.override {
                withX11 = false;
                mesa = self.mesaNox;
                glu = self.gluNox;
                libX11 = null; xorgproto = null; libXext = null;
                libxcb = null; libXau = null; libXdmcp = null;
              };
  opensslThreads = self.openssl.override { withThreads = true; };
  pangoNox = self.pango.override {
                withX11 = false;
                cairo = self.cairoNox;
                harfbuzz = self.harfbuzzNox;
                libX11 = null; libxcb = null; libXext = null; libXrender = null;
                xorgproto = null;
              };
  swayNox = self.sway.override {
                withXwayland = false;
                wlroots = self.wlrootsNox;
                cairo = self.cairoNox;
                xkbcommon = self.xkbcommonNox;
                pango = self.pangoNox;
                harfbuzz = self.harfbuzzNox;
                xcb = null;
                xcbWm = null;
              };
  venusTestsNox = self.venus-tests.override { vulkanLoader = self.vulkanLoaderNox; };
  vulkanLoaderNox = self.vulkanLoader.override {
                withX11 = false;
                libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
                libXrandr = null; libXrender = null; xorgproto = null;
              };
  vulkanToolsNox = self.vulkan-tools.override {
                withX11 = false;
                vulkanLoader = self.vulkanLoaderNox;
                libX11 = null; libXext = null; libxcb = null; libXau = null;
                libXdmcp = null; libXrandr = null; xorgproto = null;
              };
  wlrootsNox = self.wlroots.override {
                withXwayland = false;
                xcb = null;
                xcbWm = null;
                xwayland = null;
                xkbcommon = self.xkbcommonNox;
              };
  xkbcommonNox = self.xkbcommon.override {
                withX11 = false;
                libxcb = null; libXau = null; libXdmcp = null;
              };
  activity = self.utilityApp { appName = "Activity"; appDir = "Activity"; installDir = "Applications/Utilities"; iconSvg = "activity.svg"; description = "PureDarwin Activity: processes, cores and memory"; };
  argon-session = pkgs.runCommand "puredarwin-argon-session" { } ''
                install -Dm755 ${../../src/Userspace/argon-session/argon-session} \
                  $out/usr/libexec/argon-session
                install -Dm644 ${../../src/Userspace/argon-session/org.puredarwin.argon.plist} \
                  $out/System/Library/LaunchDaemons/org.puredarwin.argon.plist
                install -Dm755 ${../../src/Userspace/argon-session/pd-user-session} \
                  $out/usr/libexec/pd-user-session
              '';
  cctools = self.mkPureDarwin {
              pname = "puredarwin-cctools";
              src = self.cctoolsSource;
              buildTargets = [
                "lipo_selfhost" "size_selfhost" "strings_selfhost" "checksyms_selfhost"
                "iig_selfhost" "ld64_selfhost"
                "ar_selfhost" "nm_selfhost" "libtool_selfhost" "ranlib_selfhost"
                "otool_selfhost"
                "redo_prebinding_selfhost"
                "seg_hack_selfhost" "install_name_tool_selfhost"
                "indr_selfhost" "strip_selfhost" "segedit_selfhost" "pagestuff_selfhost"
                "codesign_allocate_selfhost" "bitcode_strip_selfhost" "ctf_insert_selfhost"
                "check_dylib_selfhost" "cmpdylib_selfhost" "inout_selfhost"
                "nmedit_selfhost"
              ];
              enableProjects = false;
              enableKernel = false;
              enableLibraries = false;
              enableUserspace = false;
              enableTools = true;
              installUserland = true;
              installKernel = false;
              prebuiltLibSystem = self.libSystem;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_SELFHOST_CCTOOLS=ON"
                "-DPUREDARWIN_IIG_SOURCE=${self.iig-tools}"
                # libc++ 21 keeps basic_string, to_string and operator new in the dylib
                "-DPUREDARWIN_SELFHOST_LIBCXX_DIR=${self.libcxxDylib}/usr/lib"
                "-DPUREDARWIN_SELFHOST_LIBCXXABI_DIR=${self.libcxxabiDylib}/usr/lib"
              ];
            };
  diskArbitration = (self.mkPureDarwin {
                pname = "puredarwin-diskarbitration";
                src = self.diskArbitrationSource;
                buildTargets = [ "DiskArbitration" "diskarbitrationd" ];
                enableProjects = false;
                enableKernel = false;
                enableUserspace = false;
                installUserland = false;
                installKernel = false;
                extraCmakeFlags = [
                  "-DPUREDARWIN_ENABLE_DISKARBITRATION=ON"
                  "-DPUREDARWIN_COREFOUNDATION_PREFIX=${self.corefoundation}"
                  "-DPUREDARWIN_IOKIT_PREFIX=${self.iokit}"
                  "-DPUREDARWIN_SECURITY_PREFIX=${self.security}"
                  "-DPUREDARWIN_SYSTEMCONFIGURATION_PREFIX=${self.systemConfiguration}"
                ];
              }).overrideAttrs (old: {
                installPhase = ''
                  runHook preInstall
                  fw="$out/System/Library/Frameworks/DiskArbitration.framework"
                  mkdir -p "$fw/Versions/A/Headers"
                  cp build-nix/src/Libraries/DiskArbitration/libDiskArbitration.dylib \
                    "$fw/Versions/A/DiskArbitration"
                  cp -a src/Libraries/DiskArbitration/include/DiskArbitration/. \
                    "$fw/Versions/A/Headers/"
                  ln -s A "$fw/Versions/Current"
                  ln -s Versions/Current/DiskArbitration "$fw/DiskArbitration"
                  ln -s Versions/Current/Headers "$fw/Headers"
                  mkdir -p "$out/usr/lib"
                  ln -s "../../System/Library/Frameworks/DiskArbitration.framework/Versions/A/DiskArbitration" \
                    "$out/usr/lib/libDiskArbitration.dylib"
                  mkdir -p "$out/include"
                  cp -a src/Libraries/DiskArbitration/include/DiskArbitration "$out/include/"
                  mkdir -p "$out/usr/libexec" "$out/System/Library/LaunchDaemons"
                  cp build-nix/src/Libraries/DiskArbitration/diskarbitrationd \
                    "$out/usr/libexec/diskarbitrationd"
                  cp src/Libraries/DiskArbitration/diskarbitrationd/com.apple.diskarbitrationd.plist \
                    "$out/System/Library/LaunchDaemons/"
                  runHook postInstall
                '';
              });
  disks = self.utilityApp { appName = "Disks"; appDir = "Disks"; installDir = "Applications/Utilities"; iconSvg = "disks.svg"; description = "PureDarwin Disks: drives, partitions and volumes"; };
  fbdoom = (self.mkPureDarwin {
              pname = "puredarwin-fbdoom";
              src = self.fbdoomSource;
              buildTargets = [ "fbdoom" ];
              enableProjects = false;
              enableKernel = false;
              installUserland = false;
              installKernel = false;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_FBDOOM=ON"
                "-DPUREDARWIN_COREFOUNDATION_PREFIX=${self.corefoundation}"
                "-DPUREDARWIN_FBDOOM_SOURCE=${self.fbdoomExternalSrc}"
                "-DPUREDARWIN_AUDIOTOOLBOX_PREFIX=${self.audiotoolbox}"
                "-DPUREDARWIN_CHOCOLATE_DOOM_SOURCE=${self.chocolateDoomPatchedSrc}"
              ];
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                mkdir -p $out/usr/bin
                cp build-nix/src/Userspace/fbdoom/fbdoom $out/usr/bin/fbdoom
                runHook postInstall
              '';
            });
  gnutls = self.mkSharedXorgLib {
              pname = "puredarwin-gnutls";
              inherit (pkgs.gnutls) version src;
              deps = [ self.nettle-shared ];
              postPatchExtra = ''
                substituteInPlace configure lib/Makefile.in \
                  --replace ' -framework Security -framework CoreFoundation' ""
                # The keychain trust store goes with those frameworks. The header
                # is included whether or not that code path is compiled, and here
                # it is not: DEFAULT_TRUST_STORE_FILE wins the #if chain.
                substituteInPlace lib/system/certs.c \
                  --replace-fail '#ifdef __APPLE__' '#if 0'
              '';
              configureFlags = [
                "--with-default-trust-store-file=/etc/ssl/cert.pem"
                "--with-nettle-mini"
                "--with-included-libtasn1"
                "--with-included-unistring"
                "--without-p11-kit"
                "--without-idn"
                "--without-tpm"
                "--without-tpm2"
                "--without-brotli"
                "--without-zstd"
                "--without-zlib"
                "--disable-doc"
                "--disable-tools"
                "--disable-tests"
                "--disable-cxx"
                "--disable-nls"
                "--disable-libdane"
                "--disable-guile"
                "--disable-hardware-acceleration"
              ];
            };
  iconThemes = pkgs.runCommand "puredarwin-icon-themes" { } ''
                mkdir -p "$out/share/icons"
                cp -a ${pkgs.hicolor-icon-theme}/share/icons/hicolor "$out/share/icons/"
                cp -a ${pkgs.adwaita-icon-theme}/share/icons/Adwaita "$out/share/icons/"
                chmod -R u+w "$out/share/icons"
              '';
  iokitCFStatic = (self.mkPureDarwin {
                pname = "puredarwin-iokitcf-static";
                src = self.iokitCFSource;
                buildTargets = [ "IOKitCF" ];
                enableProjects = false;
                enableKernel = false;
                enableUserspace = false;
                installUserland = false;
                installKernel = false;
                extraCmakeFlags = [
                  "-DPUREDARWIN_ENABLE_IOKITCF=ON"
                  "-DPUREDARWIN_COREFOUNDATION_PREFIX=${self.corefoundation}"
                  "-DPUREDARWIN_LIBOBJC_PREFIX=${self.libobjc}"
                  "-DPUREDARWIN_FOUNDATION_PREFIX=${self.foundation}"
                ];
              }).overrideAttrs (old: {
                installPhase = ''
                  runHook preInstall
                  mkdir -p "$out/usr/lib/system"
                  cp build-nix/src/Libraries/IOKit/libIOKitCF.a "$out/usr/lib/system/"
                  mkdir -p "$out/include"
                  cp -a src/Libraries/IOKit/iokituser/include/IOKit "$out/include/"
                  runHook postInstall
                '';
              });
  libSystem = self.mkPureDarwin {
              pname = "puredarwin-libsystem";
              src = self.libSystemSource;
              buildTargets = [ "libSystem_B_stub" "libDER_static" "dyld" "libsystem_kernel_static" "libdispatch_static" "XPC_libnv_static" "XPC_libinfo_static" "XPC_libxpc_static" "XPC_launchd_static" "XPC_launchd_mig_static" "XPC_notify_client_static" "notifyd" "logd" ];
              enableUserspace = false;
              enableKernel = false;
              installUserland = false;
              installKernel = false;
              installLibSystem = true;
            };
  libX11SharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libX11";
              inherit (pkgs.libX11) version src;
              deps = [ pkgs.xorgproto pkgs.xtrans self.libxcbSharedBuild self.libXauSharedBuild self.libXdmcpSharedBuild ];
              configureFlags = [ "--disable-specs" "--enable-xlocaledir" ];
            };
  libXauSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libXau";
              inherit (pkgs.libXau) version src;
              deps = [ pkgs.xorgproto ];
            };
  libXcursorSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libXcursor";
              inherit (pkgs.libXcursor) version src;
              deps = [ pkgs.xorgproto self.libX11SharedBuild self.libXrenderSharedBuild self.libXfixesSharedBuild ];
            };
  libXdmcpSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libXdmcp";
              inherit (pkgs.libXdmcp) version src;
              deps = [ pkgs.xorgproto ];
            };
  libXextSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libXext";
              inherit (pkgs.libXext) version src;
              deps = [ pkgs.xorgproto self.libX11SharedBuild self.libXauSharedBuild ];
            };
  libXfixesSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libXfixes";
              inherit (pkgs.libXfixes) version src;
              deps = [ pkgs.xorgproto self.libX11SharedBuild self.libXextSharedBuild ];
            };
  libXiSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libXi";
              inherit (pkgs.libXi) version src;
              deps = [ pkgs.xorgproto self.libX11SharedBuild self.libXextSharedBuild self.libXfixesSharedBuild ];
            };
  libXrandrSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libXrandr";
              inherit (pkgs.libXrandr) version src;
              deps = [ pkgs.xorgproto self.libX11SharedBuild self.libXextSharedBuild self.libXrenderSharedBuild ];
            };
  libXrenderSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libXrender";
              inherit (pkgs.libXrender) version src;
              deps = [ pkgs.xorgproto self.libX11SharedBuild ];
            };
  libXxf86vm = self.mkSharedXorgLib {
              pname = "puredarwin-libXxf86vm";
              inherit (pkgs.libXxf86vm) version src;
              deps = [ pkgs.xorgproto self.libX11SharedBuild self.libXextSharedBuild ];
            };
  libxcbSharedBuild = self.mkSharedXorgLib {
              pname = "puredarwin-libxcb";
              inherit (pkgs.libxcb) version src;
              deps = [ pkgs.xorgproto self.libXauSharedBuild self.libXdmcpSharedBuild ];
              nativeDeps = [ pkgs.python3 pkgs.xcb-proto ];
              configureFlags = [ "--disable-devel-docs" ];
              preConfigureExtra = ''
                export PYTHONPATH="${pkgs.xcb-proto}/${pkgs.python3.sitePackages}:$PYTHONPATH"
              '';
            };
  libxshmfence = self.mkSharedXorgLib {
              pname = "puredarwin-libxshmfence";
              inherit (pkgs.libxshmfence) version src;
              deps = [ pkgs.xorgproto ];
              configureFlags = [ "--with-shared-memory-dir=/tmp" ];
            };
  logs = self.utilityApp { appName = "Logs"; appDir = "Logs"; installDir = "Applications/Utilities"; iconSvg = "logs.svg"; description = "PureDarwin Logs: the kernel buffer and /var/log, followed live"; };
  nettle-shared = self.mkSharedXorgLib {
              pname = "puredarwin-nettle";
              inherit (pkgs.nettle) version src;
              configureFlags = [
                "--enable-mini-gmp"
                "--disable-documentation"
                "--disable-assembler"
                "--disable-openssl"
              ];
            };
  pdVirglShim = (self.mkPureDarwin {
                pname = "puredarwin-pd-virgl-shim";
                src = self.userlandSource;
                buildTargets = [ "pd_virgl_shim" ];
                enableProjects = false;
                enableKernel = false;
                enableLibraries = false;
                installUserland = false;
                installKernel = false;
                prebuiltLibSystem = self.libSystem;
                extraCmakeFlags = [
                  "-DPUREDARWIN_COREFOUNDATION_PREFIX=${self.corefoundation}"
                ];
              }).overrideAttrs (old: {
                installPhase = ''
                  runHook preInstall
                  mkdir -p $out/usr/lib $out/include
                  ar=${self.darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar
                  mkdir -p repack && ( cd repack && \
                    "$ar" x ../build-nix/src/Userspace/pd-virgl-shim/libpd_virgl_shim.a )
                  ${self.darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
                    -dynamiclib -fuse-ld=${self.nativeLd}/bin/ld -nostdlib \
                    -L${self.libSystem}/usr/lib \
                    -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${self.libSystem}/usr/lib/system/libdyld.dylib \
                    -Wl,-install_name,/usr/lib/libpd_virgl_shim.dylib \
                    -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
                    repack/*.o -lSystem \
                    -o $out/usr/lib/libpd_virgl_shim.dylib
                  cp src/Libraries/PDVirglShim/include/pd_virgl_shim.h $out/include/
                  runHook postInstall
                '';
              });
  screenshot = self.utilityApp { appName = "Screenshot"; appDir = "Screenshot"; installDir = "Applications/Utilities"; iconSvg = "screenshot.svg"; description = "PureDarwin Screenshot: capture the screen or a selection"; };
  symptomReporter = (self.mkPureDarwin {
                pname = "puredarwin-symptomreporter";
                src = self.symptomReporterSource;
                buildTargets = [ "SymptomReporter" ];
                enableProjects = false;
                enableKernel = false;
                enableUserspace = false;
                installUserland = false;
                installKernel = false;
                extraCmakeFlags = [
                  "-DPUREDARWIN_ENABLE_SYMPTOMREPORTER=ON"
                ];
              }).overrideAttrs (old: {
                installPhase = ''
                  runHook preInstall
                  fw="$out/System/Library/PrivateFrameworks/SymptomReporter.framework"
                  mkdir -p "$fw/Versions/A/Headers"
                  cp build-nix/src/Libraries/SymptomReporter/libSymptomReporter.dylib \
                    "$fw/Versions/A/SymptomReporter"
                  cp -a src/Libraries/SymptomReporter/include/SymptomReporter/. \
                    "$fw/Versions/A/Headers/"
                  ln -s A "$fw/Versions/Current"
                  ln -s Versions/Current/SymptomReporter "$fw/SymptomReporter"
                  ln -s Versions/Current/Headers "$fw/Headers"
                  mkdir -p "$out/include"
                  cp -a src/Libraries/SymptomReporter/include/SymptomReporter "$out/include/"
                  runHook postInstall
                '';
              });
  systemConfiguration = self.mkSystemConfiguration {
                corefoundation = self.corefoundation;
                libobjc = self.libobjc;
                security = self.security;
                iokit = self.iokit;
                symptomReporter = self.symptomReporter;
              };
  tcc = self.mkPureDarwin {
              pname = "puredarwin-tcc";
              src = self.userlandSource;
              buildTargets = [ "tcc" ];
              enableProjects = false;
              enableKernel = false;
              enableLibraries = false;
              enableTools = false;
              enableTcc = true;
              installUserland = true;
              installKernel = false;
              prebuiltLibSystem = self.libSystem;
            };
  text-editor = self.utilityApp { appName = "Text Editor"; appDir = "TextEditor"; iconSvg = "texteditor.svg"; description = "PureDarwin Text Editor: plain text documents"; };
  userland = (self.mkPureDarwin {
              pname = "puredarwin-userland";
              src = self.userlandSource;
              buildTargets = [ "sw_vers" "doubleagentd" "ps" "killall" "pkill" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "cpuctl" "mean" "reboot" "halt" "poweroff" "shutdown" "netsetup" "wslinit" "ping" "wifiutil" "startx" "mousemon" "mount" "umount" "ext4tool" "ext4_util" "mdnsd" "pdpowerd" "pd-dmesg-save" "pdkext" "drmprobe" "sh" "gpt" ]
                # shell_cmds (+ tsort/uuencode/uudecode)
                ++ [ "basename" "chown" "dirname" "echo" "false" "getopt" "hostname" "jot" "kill" "logname" "mktemp" "nice" "nohup" "passwd" "sudo" "printenv" "pwd" "renice" "seq" "shlock" "sleep" "tee" "test_cmd" "true" "tsort" "uname" "yes" "uuencode" "uudecode" ]
                # text_cmds
                ++ [ "banner" "cat" "colrm" "comm" "cut" "expand" "fold" "head" "lam" "look" "nl" "paste" "rev" "split" "tail" "tr" "unexpand" "uniq" "wc" ]
                ++ [ "puredarwingop_drv" "puredarwininput_drv" ];
              enableProjects = false;
              enableKernel = false;
              enableLibraries = false;
              enableTools = false;
              installUserland = true;
              installKernel = false;
              prebuiltLibSystem = self.libSystem;
              extraCmakeFlags = [ "-DPUREDARWIN_COREFOUNDATION_PREFIX=${self.corefoundation}" ];
              xorgDriverIncludes = [
                "${self.xorg}/usr/include/xorg"
                "${self.xorg}/usr/include"
                "${lib.getDev pkgs.xorgproto}/include"
                "${self.pixman}/include/pixman-1"
              ];
            }).overrideAttrs (old: {
              # the kernel only takes an aux KC from a process holding this entitlement
              nativeBuildInputs = (old.nativeBuildInputs or [ ]) ++ [ pkgs.ldid ];
              postInstall = (old.postInstall or "") + ''
                ldid -S${../../src/Userspace/pdkext/pdkext.entitlements} "$out"/usr/sbin/pdkext
              '';
            });
  userlandNox = self.mkPureDarwin {
              pname = "puredarwin-userland-nox";
              src = self.userlandSource;
              buildTargets = [ "sw_vers" "doubleagentd" "ps" "killall" "pkill" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "cpuctl" "mean" "reboot" "halt" "poweroff" "shutdown" "netsetup" "wslinit" "ping" "wifiutil" "startx" "mousemon" "mount" "umount" "ext4tool" "ext4_util" "mdnsd" "pdpowerd" "pd-dmesg-save" "pdkext" "drmprobe" "sh" "gpt" ]
                ++ [ "basename" "chown" "dirname" "echo" "false" "getopt" "hostname" "jot" "kill" "logname" "mktemp" "nice" "nohup" "passwd" "sudo" "printenv" "pwd" "renice" "seq" "shlock" "sleep" "tee" "test_cmd" "true" "tsort" "uname" "yes" "uuencode" "uudecode" ]
                ++ [ "banner" "cat" "colrm" "comm" "cut" "expand" "fold" "head" "lam" "look" "nl" "paste" "rev" "split" "tail" "tr" "unexpand" "uniq" "wc" ];
              enableProjects = false;
              enableKernel = false;
              enableLibraries = false;
              enableTools = false;
              installUserland = true;
              installKernel = false;
              prebuiltLibSystem = self.libSystem;
              extraCmakeFlags = [ "-DPUREDARWIN_COREFOUNDATION_PREFIX=${self.corefoundation}" ];
            };
  xkeyboardConfig = pkgs.runCommand "puredarwin-xkeyboard-config" { } ''
                mkdir -p "$out/usr/share"
                cp -a ${pkgs.xkeyboard_config}/share/X11 "$out/usr/share/X11"
                chmod -R u+w "$out/usr/share/X11"
                cp -a ${pkgs.xkeyboard_config}/share/xkeyboard-config-2 "$out/usr/share/xkeyboard-config-2"
                chmod -R u+w "$out/usr/share/xkeyboard-config-2"
                if [ -L "$out/usr/share/X11/xkb" ]; then
                  rm "$out/usr/share/X11/xkb"
                  cp -a "$out/usr/share/xkeyboard-config-2" "$out/usr/share/X11/xkb"
                  chmod -R u+w "$out/usr/share/X11/xkb"
                fi
              '';
  xlibLocale = pkgs.runCommand "puredarwin-libx11-locale" { } ''
                mkdir -p "$out/usr/share/X11"
                cp -a ${pkgs.libX11}/share/X11/locale "$out/usr/share/X11/locale"
                chmod -R u+w "$out/usr/share/X11/locale"
              '';
}
