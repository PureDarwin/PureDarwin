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
  };
  fcft = callPackage ../pkgs/wayland/fcft.nix {
    nativeMesonTools = self.nativeMesonToolsDir;
    freetype = self.freetype2;
    harfbuzz = self.harfbuzzNoxBuild;
    appleSdk = pkgs.appleSdk;
  };
  fex-wow64 = callPackage ../pkgs/apps/fex-wow64.nix {
    mingwAarch64Cc = self.mingwAarch64.cc;
    mingwAarch64Pthreads = self.mingwAarch64.pthreads;
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
  libapfsrw = callPackage ../pkgs/apple/libapfsrw.nix { };
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
  };
  nvidia580 = callPackage ../pkgs/gpu/nvidia-580.nix { };
  nvidiaLinuxRoot = callPackage ../pkgs/gpu/nvidia-linux-root.nix {
    libxcb = pkgs.libxcb;
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
  };
  swiftRuntime = callPackage ../pkgs/apple/swift-runtime.nix {
    overlaySrc = ../../src/Libraries/SwiftOverlays;
    objcApinotes = ../../src/Libraries/objc4/runtime/Module/ObjectiveC.apinotes;
    dispatchModulemap = ../../src/Libraries/libSystem/libdispatch/dispatch/darwin/module.modulemap;
    cfModulemap = ../../src/Frameworks/CoreFoundation/Base.subproj/module.modulemap;
    appleSdk = pkgs.appleSdk;
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
  };
  xz = callPackage ../pkgs/base/xz.nix {
    xz = pkgs.xz;
    appleSdk = pkgs.appleSdk;
  };
}
