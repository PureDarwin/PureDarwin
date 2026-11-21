# arm64 cross-compiled package set.
#
# Every binding here targets arm64-apple-darwin20.4 via mkArm64Build, which
# injects the arm64 toolchain, target triple and arm64 libSystem into each
# package's own argument set. Packages whose derivation only differs by
# toolchain use .override on the x86 build rather than a second call.
{ lib
, pkgs
# the PureDarwin package set builder (nix/pd/scope.nix)
, pdScope
, isDarwin
, arm64CrossToolchain
, armv6CrossToolchain
, coreFoundationBuild
, darwinCrossToolchain
, fbdoomSource
, foundationSource
, hostOtoolBuild
, i3statusShimBuild
, icuCoreBuild
, iokitBuild
, iokitCFStaticBuild
, kernelSource
, kextsSource
, kextsOnlySource
, kextsOnlyArgs
, xnuHeadersArm64Build
, launchctlBuild
, launchdBuild
, libSystemBuild
, libcxxDylibBuild
, libcxxabiDylibBuild
, libiconvBuild
, libobjcBuild
, mkPureDarwinBuild
, mkSystemConfigurationBuild
, symptomReporterBuild
, nativeLd
, nativeMesonToolsDir
, tllistBuild
, localeDataBuild
, applicationservicesBuild
, systemFontsBuild
, waylandSessionBuild
, ncursesBuild
, coreServicesSource
, asmjitSrc
, wineToolsBuild
, fexWow64Build
, mingwAarch64Cc
, mingwArm64ecCc
, vteSrc
, waylandScannerBuild
, waylandProtocolsBuild
, diskArbitrationSource
, securitySource
, userlandBuild
, userlandSource
, xkeyboardConfigBuild
, xlibLocaleBuild
, xvfbFontsBuild
, zshBuild
, xfconfArm64
, libxfce4utilArm64
, libxfce4uiArm64
, libxfce4windowingArm64
, libwnckArm64Build
, garconArm64
, exoArm64
, xfwm4Arm64
, xfce4SessionArm64
, xfce4PanelArm64
, xfdesktopArm64
, xfce4AppfinderArm64
, thunarArm64
, xfce4SettingsArm64
, xfce4TerminalArm64
}:

let
  atspi2CoreArm64Build = mkArm64Build ./pkgs/gtk/at-spi2-core.nix {
    nativeMesonTools = nativeMesonToolsDir;
    glib = glibArm64Build;
    libxml2 = libxml2Arm64Build;
    dbus = dbusArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
    inherit (pkgs) at-spi2-core meson ninja python3;
  };
  autoconfArm64Build = pdArm64.autoconf;
  automakeArm64Build = pdArm64.automake;
  cairoArm64Build = pdArm64.cairo;
  cairoGobjectArm64Build = pdArm64.cairoGobject;
  curlArm64Build = mkArm64Build ./pkgs/base/curl.nix {
    curl = pkgs.curl;
    openssl = opensslArm64Build;
    zlib = xvfbZlibArm64Build;
    corefoundation = coreFoundationArm64Build;
    systemConfiguration = systemConfigurationArm64Build;
  };
  dbusArm64Build = mkArm64Build ./pkgs/gtk/dbus.nix {
    expat = expatArm64Build;
    libX11 = xlibArm64Build;
    inherit (pkgs) dbus meson ninja python3;
  };
  dilloArm64Build = pdArm64.dillo;
  dmenuArm64Build = pdArm64.dmenu;
  fastfetchArm64Build = mkArm64Build ./pkgs/apps/fastfetch.nix {
    fastfetch = pkgs.fastfetch;
    corefoundation = coreFoundationArm64Build;
    foundation = foundationArm64Build;
    libobjc = libobjcArm64Build;
    iokit = iokitArm64Build;
    openglFramework = openglFrameworkArm64Build;
    libX11 = xlibArm64Build;
    libXext = xvfbLibXextArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    mesa = mesaArm64Build;
    glu = gluArm64Build;
  };
  # For boards with no GL stack (the Pi), so the image does not have to carry
  # OpenGL.framework and Mesa just to satisfy dyld at launch.
  fastfetchNoGLArm64Build = fastfetchArm64Build.override { withOpenGL = false; };
  fltkArm64Build = pdArm64.fltk;
  foundationArm64Build = pdArm64.foundation;
  fribidiArm64Build = pdArm64.fribidi;
  gdkPixbufArm64Build = pdArm64.gdkPixbuf;
  gitArm64Build = mkArm64Build ./pkgs/base/git.nix {
    git = pkgs.git;
    zlib = xvfbZlibArm64Build;
    curl = curlArm64Build;
    openssl = opensslArm64Build;
  };
  gtk3Arm64Build = mkArm64Build ./pkgs/gtk/gtk3.nix {
    nativeMesonTools = nativeMesonToolsDir;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
    cairo = cairoArm64Build;
    cairoGobject = cairoGobjectArm64Build;
    pixman = xvfbPixmanArm64Build;
    pango = pangoArm64Build;
    fribidi = fribidiArm64Build;
    harfbuzz = harfbuzzArm64Build;
    freetype2 = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    expat = expatArm64Build;
    gdkPixbuf = gdkPixbufArm64Build;
    libepoxy = libepoxyArm64Build;
    atspi2Core = atspi2CoreArm64Build;
    dbus = dbusArm64Build;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXi = xvfbLibXiArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXrandr = xvfbLibXrandrArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXcursor = xvfbLibXcursorArm64Build;
    libpng = libpngArm64Build;
    glibNative = pkgs.glib.dev;
    # Without these gtk3 configures with the X11 backend only, so it ships no
    # gdk/gdkwayland.h or gdk-wayland-3.0.pc and every Wayland consumer
    # (gtk-layer-shell, libxfce4windowing) fails to configure.
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    xkbcommon = xkbcommonArm64Build;
    mesa = mesaArm64Build;
    inherit (pkgs) gtk3 xorgproto;
  };
  harfbuzzArm64Build = pdArm64.harfbuzz;
  i3Arm64Build = pdArm64.i3;
  ioregArm64Build = pdArm64.ioreg;
  libXftArm64Build = pdArm64.libXft;
  libcssArm64Build = pdArm64.libcss;
  libcurlDylibArm64Build = mkArm64Build ./pkgs/base/libcurl-dylib.nix {
    zlib = xvfbZlibArm64Build;
    openssl = opensslArm64Build;
    corefoundation = coreFoundationArm64Build;
    systemConfiguration = systemConfigurationArm64Build;
    inherit (pkgs) curl;
  };
  libcxxTestArm64Build = pdArm64.libcxxTest;
  libdomArm64Build = pdArm64.libdom;
  libepoxyArm64Build = mkArm64Build ./pkgs/gtk/libepoxy.nix {
    nativeMesonTools = nativeMesonToolsDir;
    libX11 = xlibArm64Build;
    mesa = mesaArm64Build;
    inherit (pkgs) libepoxy xorgproto meson ninja python3;
  };
  libfontencArm64Build = pdArm64.libfontenc;
  libhubbubArm64Build = pdArm64.libhubbub;
  libnsbmpArm64Build = pdArm64.libnsbmp;
  libnsgifArm64Build = pdArm64.libnsgif;
  libnsutilsArm64Build = pdArm64.libnsutils;
  libparserutilsArm64Build = pdArm64.libparserutils;
  libutf8procArm64Build = pdArm64.libutf8proc;
  libwapcapletArm64Build = pdArm64.libwapcaplet;
  libzDylibArm64Build = pdArm64.libzDylib;
  libgbmArm64Build = pdArm64.libgbm;
  libdrmPdpvrArm64Build = mkArm64Build ./pkgs/gpu/libdrm-pdpvr.nix {
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
    libdrmSrc = pkgs.libdrm.src;
  };
  libdrmArm64Build = pdArm64.libdrm;
  xwaylandArm64Build = pdArm64.xwayland;
  mesonArm64Build = pdArm64.meson;
  cmakeArm64Build = mkArm64Build ./pkgs/base/cmake.nix {
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    corefoundation = coreFoundationArm64Build;
    libcurlDylib = libcurlDylibArm64Build;
    inherit (pkgs) cmake ninja;
  };
  ninjaArm64Build = pdArm64.ninja;
  clangCrossArm64Build = mkArm64Build ./pkgs/toolchain/clang-cross.nix {
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    nativeMesonTools = nativeMesonToolsDir;
    llvm = llvmCrossArm64Build;
    llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
    llvmVersion = pkgs.llvmPackages_21.llvm.version;
    nativeTblgen = "${pkgs.llvmPackages_21.llvm}/bin/llvm-tblgen";
  };
  # Wine for arm64, built with new WoW64: --enable-archs=aarch64,x86_64.
  # The aarch64 half is native PE (ntdll's PE part, wow64.dll, wow64cpu.dll);
  # the x86_64 half is the emulated one and needs a CPU backend - FEX's
  # libwow64fex.dll - before any x86 Windows binary will actually run. Without
  # that backend this Wine runs ARM64 Windows binaries only.
  wineArm64Build = mkArm64Build ./pkgs/apps/wine.nix {
    targetArch = "arm64";
    mingwAarch64Cc = mingwAarch64Cc;
    mingwArm64ecCc = mingwArm64ecCc;
    wineTools = wineToolsBuild;
    fexWow64 = fexWow64Build;
    mingwGcc = pkgs.pkgsCross.mingwW64.buildPackages.gcc;
    mingwBintools = pkgs.pkgsCross.mingwW64.buildPackages.bintools;
    mingwGcc32 = pkgs.pkgsCross.mingw32.buildPackages.gcc;
    mingwBintools32 = pkgs.pkgsCross.mingw32.buildPackages.bintools;
    inherit (pkgs) python3;
    wine = pkgs.wine64;
    inherit (pkgs) xorgproto flex bison;
    libX11 = libX11SharedArm64Build;
    libxcb = libxcbSharedArm64Build;
    libXau = libXauSharedArm64Build;
    libXdmcp = libXdmcpSharedArm64Build;
    libXext = libXextSharedArm64Build;
    libXrender = libXrenderSharedArm64Build;
    libXfixes = libXfixesSharedArm64Build;
    libXi = libXiSharedArm64Build;
    libXcursor = libXcursorSharedArm64Build;
    libXrandr = libXrandrSharedArm64Build;
    freetype = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    expat = expatArm64Build;
    gnutls = gnutlsSharedArm64Build;
    mesa = mesaArm64Build;
    coreservices = coreServicesArm64Build;
    security = securityArm64Build;
    diskArbitration = diskArbitrationArm64Build;
    systemConfiguration = systemConfigurationArm64Build;
    iokit = iokitArm64Build;
    corefoundation = coreFoundationArm64Build;
    inherit (pkgs) perl;
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    xkbcommon = xkbcommonArm64Build;
    libxml2 = libxml2Arm64Build;
  };
  jsoncArm64Build = pdArm64.jsonc;
  wlrootsArm64Build = pdArm64.wlroots;
  swayArm64Build = pdArm64.sway;
  libdisplayInfoArm64Build = pdArm64.libdisplayInfo;
  gtkLayerShellArm64Build = mkArm64Build ./pkgs/wayland/gtk-layer-shell.nix {
    nativeMesonTools = nativeMesonToolsDir;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
    cairo = cairoArm64Build;
    cairoGobject = cairoGobjectArm64Build;
    pixman = xvfbPixmanArm64Build;
    pango = pangoArm64Build;
    fribidi = fribidiArm64Build;
    harfbuzz = harfbuzzArm64Build;
    freetype2 = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    expat = expatArm64Build;
    gdkPixbuf = gdkPixbufArm64Build;
    libepoxy = libepoxyArm64Build;
    atspi2Core = atspi2CoreArm64Build;
    dbus = dbusArm64Build;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXi = xvfbLibXiArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXrandr = xvfbLibXrandrArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXcursor = xvfbLibXcursorArm64Build;
    libpng = libpngArm64Build;
    glibNative = pkgs.glib.dev;
    gtk3 = gtk3Arm64Build;
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    xkbcommon = xkbcommonArm64Build;
    mesa = mesaArm64Build;
    inherit (pkgs) gtk-layer-shell xorgproto;
  };
  asmjitTestArm64Build = mkArm64Build ./pkgs/apple/asmjit-test.nix {
    inherit asmjitSrc;
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    targetArch = "arm64";
  };
  vmprobeArm64Build = mkArm64Build ./pkgs/apple/vmprobe.nix { };
  compilerRtArm64Build = mkArm64Build ./pkgs/toolchain/compiler-rt.nix {
    nativeMesonTools = nativeMesonToolsDir;
    llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
    llvmVersion = pkgs.llvmPackages_21.llvm.version;
    targetArch = "arm64";
  };
  # ARMv6 has no integer divide instruction and no 64-bit ALU, so the kernel
  # genuinely needs the compiler-rt builtins (__divdi3, __divsi3, __moddi3,
  # the float/int conversions) that a 64-bit target never references.
  compilerRtArmv6Build = pkgs.callPackage ./pkgs/toolchain/compiler-rt-armv6.nix {
    inherit darwinCrossToolchain nativeLd;
    llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
    llvmVersion = pkgs.llvmPackages_21.llvm.version;
  };
  llvmCrossArm64Build = mkArm64Build ./pkgs/toolchain/llvm-cross.nix {
    llvmTargetArch = "AArch64";
    llvmTargets = "AArch64";
    cmakeSystemProcessor = "arm64";
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    nativeMesonTools = nativeMesonToolsDir;
    llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
    llvmVersion = pkgs.llvmPackages_21.llvm.version;
    nativeTblgen = "${pkgs.llvmPackages_21.llvm}/bin/llvm-tblgen";
    nativeLlvmConfig = "${pkgs.llvmPackages_21.llvm.dev}/bin/llvm-config";
  };
  airjitdArm64Build = mkArm64Build ./pkgs/gpu/airjitd.nix {
    llvm = llvmCrossArm64Build;
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
  };
  xvfbLibXxf86vmArm64Build = pdArm64.xvfbLibXxf86vm;
  waylandArm64Build = pdArm64.wayland;
  gluArm64Build = pdArm64.gluBuild;
  mesaArm64Build = mkArm64Build ./pkgs/mesa/mesa.nix {
    nativeMesonTools = nativeMesonToolsDir;
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    llvm = llvmCrossArm64Build;
    libxshmfence = libxshmfenceSharedArm64Build;
    libXxf86vm = xvfbLibXxf86vmArm64Build;
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    zlib = xvfbZlibArm64Build;
    expat = expatArm64Build;
    libX11 = xlibArm64Build;
    libXext = xvfbLibXextArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    pdVirglShim = pdVirglShimArm64Build;
    virglWinsysSrc = ./pkgs/mesa/virgl-puredarwin;
    virglAbiHeader = ../src/Kernel/Extensions/IOVirtIOGPU/IOVirtIOGPU3DShared.h;
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
    inherit (pkgs) meson ninja pkg-config python3 bison flex xorgproto xtrans;
  };
  # freedreno on the adreno msm driver through /dev/dri (PDDRMmsm), for the msm8916 stick: no x11 there
  libdrmDriArm64Build = libdrmArm64Build.override { amdgpu = true; };
  mesaMsmArm64Build = mesaArm64Build.override {
    withMsmDrm = true;
    withX11 = false;
    libdrmAmdgpu = libdrmDriArm64Build;
  };
  pdgltriMsmArm64Build = mkArm64Build ./pkgs/gpu/pdgltri.nix { mesa = mesaMsmArm64Build; };
  # argon on the msm8916 stick: neuswc over freedreno, drawing into PDVirtualFramebuffer
  neuwldMsmArm64Build = mkArm64Build ./pkgs/wayland/neuwld.nix {
    fontconfig = fontconfigArm64Build;
    freetype = freetype2Arm64Build;
    pixman = xvfbPixmanArm64Build;
    mesaNox = mesaMsmArm64Build;
    src = ../src/ThirdParty/neuwld;
  };
  # argon's desktop on the stick: the AppKit stack over the freedreno mesa, from the x86 wiring
  iosurfaceArm64Build = pdArm64.iosurface;
  gluMsmArm64Build = gluArm64Build.override { mesa = mesaMsmArm64Build; };
  onyx2dArm64Build = pdArm64.onyx2d;
  coregraphicsArm64Build = pdArm64.coregraphics;
  coretextArm64Build = mkArm64Build ./pkgs/apple/coretext.nix {
    libobjc = libobjcArm64Build;
    corefoundation = coreFoundationArm64Build;
    foundation = foundationArm64Build;
    onyx2d = onyx2dArm64Build;
    coregraphics = coregraphicsArm64Build;
    freetype = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    harfbuzz = harfbuzzArm64NoxBuild;
    fribidi = fribidiArm64Build;
    icu = icuCoreArm64Build;
    src = ../src/Frameworks/CoreText;
  };
  corevideoMsmArm64Build = mkArm64Build ./pkgs/apple/corevideo.nix {
    libobjc = libobjcArm64Build;
    corefoundation = coreFoundationArm64Build;
    foundation = foundationArm64Build;
    openglFramework = openglFrameworkArm64NoxBuild;
    mesa = mesaMsmArm64Build;
    glu = gluMsmArm64Build;
    iosurface = iosurfaceArm64Build;
    coregraphics = coregraphicsArm64Build;
    src = ../src/Frameworks/CoreVideo;
  };
  quartzcoreMsmArm64Build = mkArm64Build ./pkgs/apple/quartzcore.nix {
    libobjc = libobjcArm64Build;
    corefoundation = coreFoundationArm64Build;
    foundation = foundationArm64Build;
    onyx2d = onyx2dArm64Build;
    coregraphics = coregraphicsArm64Build;
    coretext = coretextArm64Build;
    openglFramework = openglFrameworkArm64NoxBuild;
    corevideo = corevideoMsmArm64Build;
    applicationservices = applicationservicesBuild;
    mesa = mesaMsmArm64Build;
    glu = gluMsmArm64Build;
    src = ../src/Frameworks/QuartzCore;
  };
  windowserverArm64Build = pdArm64.windowserver;
  appkitMsmArm64Build = mkArm64Build ./pkgs/apple/appkit.nix {
    libobjc = libobjcArm64Build;
    corefoundation = coreFoundationArm64Build;
    foundation = foundationArm64Build;
    onyx2d = onyx2dArm64Build;
    coregraphics = coregraphicsArm64Build;
    coretext = coretextArm64Build;
    quartzcore = quartzcoreMsmArm64Build;
    applicationservices = applicationservicesBuild;
    coreservices = coreServicesArm64Build;
    carbon = carbonArm64Build;
    openglFramework = openglFrameworkArm64NoxBuild;
    windowserver = windowserverArm64Build;
    freetype2 = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    mesa = mesaMsmArm64Build;
    glu = gluMsmArm64Build;
    src = ../src/Frameworks/AppKit;
  };
  coredataArm64Build = pdArm64.coredata;
  cocoaMsmArm64Build = mkArm64Build ./pkgs/apple/cocoa.nix {
    foundation = foundationArm64Build;
    appkit = appkitMsmArm64Build;
    coredata = coredataArm64Build;
    src = ../src/Frameworks/Cocoa;
  };
  carbonArm64Build = pdArm64.carbon;
  pddecorMsmArm64Build = mkArm64Build ./pkgs/apps/pddecor.nix ({
    iokit = iokitArm64Build;
    wayland = waylandArm64Build;
    waylandScanner = pkgs.wayland-scanner;
    src = ../src/Userspace/pddecor;
    protocol = ../src/ThirdParty/neuswc/protocol/pd-decoration-v1.xml;
  } // argonAppDeps);
  dockMsmArm64Build = mkArm64Build ./pkgs/apps/dock.nix ({
    librsvg = pkgs.librsvg;
    libjpeg = pkgs.libjpeg;
    src = ../src/Userspace/Dock;
  } // argonAppDeps);
  # CoreAudio and AudioToolbox, the menubar's volume control links them
  audiotoolboxArm64Build = pdArm64.audiotoolbox;
  menubarMsmArm64Build = mkArm64Build ./pkgs/apps/menubar.nix ({
    librsvg = pkgs.librsvg;
    iokit = iokitArm64Build;
    audiotoolbox = audiotoolboxArm64Build;
    src = ../src/Userspace/MenuBar;
  } // argonAppDeps);
  terminalMsmArm64Build = mkArm64Build ./pkgs/apps/terminal.nix ({
    src = ../src/Userspace/Terminal;
  } // argonAppDeps);
  finderMsmArm64Build = mkArm64Build ./pkgs/apps/finder.nix ({
    librsvg = pkgs.librsvg;
    src = ../src/Userspace/Finder;
  } // argonAppDeps);
  calculatorMsmArm64Build = mkArm64Build ./pkgs/apps/calculator.nix ({
    iokit = iokitArm64Build;
    src = ../src/Userspace/Calculator;
  } // argonAppDeps);
  # argon's desktop for the stick, on top of argonMsmArm64Bundle and pdroot's own Foundation
  argonDesktopMsmArm64Bundle = pkgs.runCommand "puredarwin-argon-desktop-msm-arm64" { } ''
    mkdir -p $out
    for d in ${onyx2dArm64Build} ${coregraphicsArm64Build} ${coretextArm64Build} \
             ${harfbuzzArm64NoxBuild} ${fribidiArm64Build} ${libpngArm64Build} ${libjpegArm64Build} \
             ${xvfbZlibArm64Build} ${iosurfaceArm64Build} ${gluMsmArm64Build} ${openglFrameworkArm64NoxBuild} \
             ${corevideoMsmArm64Build} ${quartzcoreMsmArm64Build} ${windowserverArm64Build} \
             ${coreServicesArm64Build} ${appkitMsmArm64Build} ${coredataArm64Build} ${cocoaMsmArm64Build} \
             ${carbonArm64Build} ${pddecorMsmArm64Build} ${dockMsmArm64Build} ${menubarMsmArm64Build} \
             ${terminalMsmArm64Build} ${finderMsmArm64Build} ${calculatorMsmArm64Build} \
             ${systemFontsBuild} ${waylandSessionBuild}; do
      cp -rL --no-preserve=mode $d/. $out/ 2>/dev/null || cp -rL --no-preserve=mode $d/. $out/ || true
    done
    # what only a build needs, and what the stick has already
    rm -rf $out/usr/include $out/include $out/nix-support $out/usr/lib/pkgconfig $out/lib/pkgconfig $out/usr/share/man
    find $out -name '*.a' -delete
    find $out -path '*/Headers' -prune -exec rm -rf {} +
    find $out -type f -path '*/bin/*' -exec chmod 755 {} +
    find $out -type f \( -name '*.dylib' -o -path '*/MacOS/*' -o -path '*.framework/*' \) -exec chmod 755 {} + 2>/dev/null || true
    chmod 755 $out/usr/libexec/argon-session 2>/dev/null || true
  '';
  # what every AppKit app above links
  argonAppDeps = {
    libobjc = libobjcArm64Build;
    corefoundation = coreFoundationArm64Build;
    foundation = foundationArm64Build;
    appkit = appkitMsmArm64Build;
    onyx2d = onyx2dArm64Build;
    coretext = coretextArm64Build;
    quartzcore = quartzcoreMsmArm64Build;
    coregraphics = coregraphicsArm64Build;
    applicationservices = applicationservicesBuild;
    coreservices = coreServicesArm64Build;
    windowserver = windowserverArm64Build;
    openglFramework = openglFrameworkArm64NoxBuild;
    mesa = mesaMsmArm64Build;
  };

  # foot, a terminal to run in argon on the stick
  pdEpollShimArm64Build = pdArm64.epollShim;
  fcftArm64Build = mkArm64Build ./pkgs/wayland/fcft.nix {
    nativeMesonTools = nativeMesonToolsDir;
    fontconfig = fontconfigArm64Build;
    freetype = freetype2Arm64Build;
    pixman = xvfbPixmanArm64Build;
    harfbuzz = harfbuzzArm64NoxBuild;
    libutf8proc = libutf8procArm64Build;
    tllist = tllistBuild;
    expat = expatArm64Build;
    zlib = xvfbZlibArm64Build;
    libpng = libpngArm64Build;
    libiconv = libiconvArm64Build;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
  };
  footArm64Build = mkArm64Build ./pkgs/wayland/foot.nix {
    nativeMesonTools = nativeMesonToolsDir;
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    xkbcommon = xkbcommonArm64NoxBuild;
    fontconfig = fontconfigArm64Build;
    freetype = freetype2Arm64Build;
    pixman = xvfbPixmanArm64Build;
    harfbuzz = harfbuzzArm64NoxBuild;
    libutf8proc = libutf8procArm64Build;
    tllist = tllistBuild;
    fcft = fcftArm64Build;
    epollShim = pdEpollShimArm64Build;
    expat = expatArm64Build;
    zlib = xvfbZlibArm64Build;
    libpng = libpngArm64Build;
    libiconv = libiconvArm64Build;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    ncurses = ncursesArm64Build;
  };
  # what the stick needs on top of pdroot to run neuswc: copied in with adb, not an image
  argonMsmArm64Bundle = pkgs.runCommand "puredarwin-argon-msm-arm64" { } ''
    mkdir -p $out/bin $out/usr/bin $out/usr/lib/pd-msm-mesa $out/etc
    cp ${neuswcMsmArm64Build}/bin/neuswc $out/bin/
    cp ${neuswcMsmArm64Build}/bin/swcsnap $out/usr/bin/
    cp -P ${waylandArm64Build}/lib/*.dylib $out/usr/lib/
    # their usr/lib entries point at ../../lib in the store, so take the files
    cp -L ${fontconfigArm64Build}/usr/lib/*.dylib ${freetype2Arm64Build}/usr/lib/*.dylib $out/usr/lib/
    cp -rP --no-preserve=mode ${mesaMsmArm64Build}/usr/lib/. $out/usr/lib/pd-msm-mesa/
    rm -rf $out/usr/lib/pd-msm-mesa/pkgconfig
    cp -r ${fontconfigArm64Build}/etc/fonts $out/etc/
    for d in ${xkeyboardConfigArm64Build} ${xvfbFontsArm64Build} ${localeDataBuild}; do
      cp -rn --no-preserve=mode $d/. $out/
    done
    # foot and what it brings, utf8proc's install name is /lib, so these are found by DYLD_LIBRARY_PATH
    mkdir -p $out/usr/lib/argon
    cp ${footArm64Build}/bin/foot $out/usr/bin/
    cp -r --no-preserve=mode ${footArm64Build}/share/foot $out/usr/share/
    mkdir -p $out/etc/xdg/foot
    # a first size larger than the screen leaves foot waiting on a resize that never lands
    printf '[main]\nfont=DejaVu Sans Mono:size=9\ninitial-window-size-chars=60x16\n' > $out/etc/xdg/foot/foot.ini
    cp -L ${fcftArm64Build}/usr/lib/libfcft.4.dylib ${pdEpollShimArm64Build}/usr/lib/libepoll-shim.dylib $out/usr/lib/argon/
    cp -L $(find ${libutf8procArm64Build} -name libutf8proc.dylib | head -n 1) $out/usr/lib/argon/
    # terminfo for foot's TERM and the usual fallbacks, under both the letter and the hex directory names
    for t in foot foot-direct xterm xterm-256color vt100 dumb linux; do
      src=$(find ${pkgs.ncurses}/share/terminfo -name "$t" | head -n 1)
      [ -n "$src" ] || continue
      c=$(printf %s "$t" | cut -c1)
      h=$(printf %s "$c" | od -An -tx1 | tr -d ' ')
      mkdir -p $out/usr/share/terminfo/$c $out/usr/share/terminfo/$h
      cp $src $out/usr/share/terminfo/$c/$t
      cp $src $out/usr/share/terminfo/$h/$t
    done
    rm -rf $out/nix-support $out/include $out/lib/pkgconfig
  '';
  neuswcMsmArm64Build = mkArm64Build ./pkgs/wayland/neuswc.nix {
    pdwmSource = ../src/Userspace/pdwm/pdwm.c;
    corefoundation = coreFoundationArm64Build;
    fontconfig = fontconfigArm64Build;
    freetype = freetype2Arm64Build;
    iokit = iokitArm64Build;
    neuwld = neuwldMsmArm64Build;
    mesaNox = mesaMsmArm64Build;
    pdsurface = pdsurfaceArm64Build;
    pixman = xvfbPixmanArm64Build;
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    xkbcommon = xkbcommonArm64NoxBuild;
    src = ../src/ThirdParty/neuswc;
  };
  # gallium nouveau (nvc0) on PDDRMnouveau through /dev/dri, for the jetson tx1's gm20b
  libdrmNouveauArm64Build = libdrmArm64Build.override { amdgpu = true; nouveau = true; };
  mesaNouveauArm64Build = mesaArm64Build.override {
    withNouveauDrm = true;
    withX11 = false;
    libdrmAmdgpu = libdrmNouveauArm64Build;
  };
  pdgltriNouveauArm64Build = mkArm64Build ./pkgs/gpu/pdgltri.nix { mesa = mesaNouveauArm64Build; };
  # the tx1 sd root's gl stack: mesa's dylibs, libdrm, libLLVM for llvmpipe and pdgltri
  tx1GlRootArm64Bundle = pkgs.runCommand "puredarwin-tx1-gl-root-arm64" { } ''
    mkdir -p $out/usr/lib $out/usr/sbin
    cp -P ${mesaNouveauArm64Build}/usr/lib/*.dylib $out/usr/lib/
    cp -P ${libdrmNouveauArm64Build}/lib/*.dylib $out/usr/lib/
    cp -P ${llvmCrossArm64Build}/usr/lib/libLLVM.dylib ${llvmCrossArm64Build}/usr/lib/libLLVM-21.dylib $out/usr/lib/
    cp -r --no-preserve=mode ${mesaNouveauArm64Build}/usr/share $out/usr/
    cp ${pdgltriNouveauArm64Build}/usr/sbin/pdgltri $out/usr/sbin/
    # libEGL asks for /lib/libwayland-client.0.dylib
    mkdir -p $out/lib
    cp -P ${waylandArm64Build}/lib/libwayland-client*.dylib $out/lib/
  '';
  # the tx1 sd root's desktop: argon from the stick bundles, composited on the cpu into PDTegraDisplay
  tx1DesktopArm64Bundle = pkgs.runCommand "puredarwin-tx1-desktop-arm64" { } ''
    mkdir -p $out/etc
    # fontconfig's conf.d links are relative and some dangle, so links are kept as they are
    cp -rP --no-preserve=mode ${argonMsmArm64Bundle}/. $out/
    cp -rP --no-preserve=mode ${argonDesktopMsmArm64Bundle}/. $out/
    chmod 755 $out/bin/* $out/usr/bin/* $out/usr/libexec/* 2>/dev/null || true
    find $out -type f \( -name '*.dylib' -o -path '*/MacOS/*' \) -exec chmod 755 {} +
    # pdtegradisp comes up late, the log stays readable from the acm shell
    printf '%s\n' 'NEUSWC_ENV="ARGON_GFX=cpu SWC_WAIT_HW_FB=120"' 'NEUSWC_LOG=/var/log/neuswc.log' > $out/etc/neuswc.env
  '';
  # the powervr vulkan driver for PDPowerVR, on the mesa release the linux reference runs
  mesaPowervrVersion = "26.0.3";
  mesaPowervrSrc = pkgs.fetchurl {
    url = "https://archive.mesa3d.org/mesa-${mesaPowervrVersion}.tar.xz";
    sha256 = "0d71d6vslcmyr94bma7cvacz0dzrgc3hzf5nni2sm2cf68yl9dyx";
  };
  mesaPvrNativeToolsBuild = pkgs.callPackage ./pkgs/mesa/mesa-pvr-native-tools.nix {
    src = mesaPowervrSrc;
    version = mesaPowervrVersion;
  };
  mesaPowervrArm64Build = mkArm64Build ./pkgs/mesa/mesa-powervr.nix {
    src = mesaPowervrSrc;
    version = mesaPowervrVersion;
    nativeMesonTools = nativeMesonToolsDir;
    zlib = xvfbZlibArm64Build;
    libdrmPdpvr = libdrmPdpvrArm64Build;
    mesaPvrNativeTools = mesaPvrNativeToolsBuild;
    inherit (pkgs) meson ninja pkg-config python3 bison flex glslang;
  };
  vulkanLoaderArm64Build = mkArm64Build ./pkgs/mesa/vulkan-loader.nix {
    nativeMesonTools = nativeMesonToolsDir;
    corefoundation = coreFoundationArm64Build;
    vulkanLoader = pkgs.vulkan-loader;
    vulkanHeaders = pkgs.vulkan-headers;
    withX11 = false;
  };
  pdvktriArm64Build = mkArm64Build ./pkgs/gpu/pdvktri.nix {
    vulkanHeaders = pkgs.vulkan-headers;
    vulkanLoader = vulkanLoaderArm64Build;
    libdrmPdpvr = libdrmPdpvrArm64Build;
    inherit (pkgs) glslang;
  };
  pdvkspinArm64Build = mkArm64Build ./pkgs/gpu/pdvkspin.nix {
    vulkanHeaders = pkgs.vulkan-headers;
    vulkanLoader = vulkanLoaderArm64Build;
    iokit = iokitArm64Build;
    corefoundation = coreFoundationArm64Build;
    inherit (pkgs) glslang;
  };
  # everything the triangle needs beyond the minimal root, laid out for the image
  pvrVulkanArm64 = pkgs.runCommand "puredarwin-pvr-vulkan-arm64" { } ''
    mkdir -p $out
    for p in ${libdrmPdpvrArm64Build} ${mesaPowervrArm64Build} ${vulkanLoaderArm64Build} ${pdvktriArm64Build} ${pdvkspinArm64Build}; do
      for d in usr/lib usr/share/vulkan usr/local/bin System/Library/LaunchDaemons; do
        [ -d $p/$d ] && mkdir -p $out/$d && cp -a $p/$d/. $out/$d/ && chmod -R u+w $out/$d
      done
    done
    rm -rf $out/usr/lib/pkgconfig $out/usr/lib/cmake
  '';
  # the driver and its libdrm shim for airjitd inside the stock macOS userland: no libIOKitCF or
  # /usr/lib/libCoreFoundation there, so they bind to Apple's frameworks, and live in /usr/local/lib/pdvk
  pvrVulkanIpdArm64 = pkgs.runCommand "puredarwin-pvr-vulkan-ipd-arm64" { } ''
    d=$out/usr/local/lib/pdvk
    mkdir -p $d
    cp ${libdrmPdpvrArm64Build}/usr/lib/libdrm_pdpvr.dylib ${mesaPowervrArm64Build}/usr/lib/libvulkan_powervr_mesa.dylib $d/
    chmod u+w $d/*.dylib
    int=${nativeMesonToolsDir}/bin/install_name_tool
    $int -id /usr/local/lib/pdvk/libdrm_pdpvr.dylib \
      -change /usr/lib/libIOKitCF.dylib /System/Library/Frameworks/IOKit.framework/Versions/A/IOKit \
      -change /usr/lib/libCoreFoundation.dylib /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation \
      $d/libdrm_pdpvr.dylib
    $int -id /usr/local/lib/pdvk/libvulkan_powervr_mesa.dylib \
      -change /usr/lib/libdrm_pdpvr.dylib /usr/local/lib/pdvk/libdrm_pdpvr.dylib \
      $d/libvulkan_powervr_mesa.dylib
    for f in $d/*.dylib; do
      ${pkgs.rcodesign}/bin/rcodesign sign "$f"
    done
    ${nativeMesonToolsDir}/bin/otool -L $d/*.dylib
  '';
  # venus (virtio-gpu vulkan) on IOVirtIOGPU's venus client, for arm64 guests under KVM
  libdrmVirtgpuArm64Build = pdArm64.libdrmVirtgpu;
  mesaVenusArm64Build = mkArm64Build ./pkgs/mesa/mesa-venus.nix {
    src = mesaPowervrSrc;
    version = mesaPowervrVersion;
    nativeMesonTools = nativeMesonToolsDir;
    zlib = xvfbZlibArm64Build;
    libdrmVirtgpu = libdrmVirtgpuArm64Build;
    inherit (pkgs) meson ninja pkg-config python3 bison flex glslang;
  };
  # the venus icd and its shim for the stock macOS userland (IPD), shipped as frameworks over PD's dylibs
  venusVulkanIpdArm64 = pkgs.runCommand "puredarwin-venus-vulkan-ipd-arm64" { } ''
    d=$out/usr/local/lib/pdvk
    mkdir -p $d
    cp ${libdrmVirtgpuArm64Build}/usr/lib/libdrm_virtgpu.dylib ${mesaVenusArm64Build}/usr/lib/libvulkan_virtio.dylib $d/
    chmod u+w $d/*.dylib
    int=${nativeMesonToolsDir}/bin/install_name_tool
    $int -id /usr/local/lib/pdvk/libdrm_virtgpu.dylib \
      -change /usr/lib/libIOKitCF.dylib /System/Library/Frameworks/IOKit.framework/Versions/A/IOKit \
      -change /usr/lib/libCoreFoundation.dylib /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation \
      $d/libdrm_virtgpu.dylib
    $int -id /usr/local/lib/pdvk/libvulkan_virtio.dylib \
      -change /usr/lib/libdrm_virtgpu.dylib /usr/local/lib/pdvk/libdrm_virtgpu.dylib \
      $d/libvulkan_virtio.dylib
    # the kext check, run from a launchd job in the guest
    cp ${libdrmVirtgpuArm64Build}/usr/local/bin/vnsmoke ${libdrmVirtgpuArm64Build}/usr/local/bin/vnprobe $d/
    chmod u+w $d/vnsmoke $d/vnprobe
    $int -change /usr/lib/libIOKitCF.dylib /System/Library/Frameworks/IOKit.framework/Versions/A/IOKit \
      -change /usr/lib/libCoreFoundation.dylib /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation \
      $d/vnsmoke
    for f in $d/*.dylib $d/vnsmoke $d/vnprobe; do
      ${pkgs.rcodesign}/bin/rcodesign sign "$f"
    done
    ${nativeMesonToolsDir}/bin/otool -L $d/*.dylib
  '';
  # the same driver and loader with x11 wsi, linked to the shared guest x libraries
  mesaPowervrX11Arm64Build = mkArm64Build ./pkgs/mesa/mesa-powervr.nix {
    src = mesaPowervrSrc;
    version = mesaPowervrVersion;
    nativeMesonTools = nativeMesonToolsDir;
    zlib = xvfbZlibArm64Build;
    libdrmPdpvr = libdrmPdpvrArm64Build;
    mesaPvrNativeTools = mesaPvrNativeToolsBuild;
    withX11 = true;
    libX11 = libX11SharedArm64Build;
    libxcb = libxcbSharedArm64Build;
    libXau = libXauSharedArm64Build;
    libXdmcp = libXdmcpSharedArm64Build;
    libxshmfence = libxshmfenceSharedArm64Build;
    inherit (pkgs) meson ninja pkg-config python3 bison flex glslang xorgproto;
  };
  vulkanLoaderX11Arm64Build = mkArm64Build ./pkgs/mesa/vulkan-loader.nix {
    nativeMesonTools = nativeMesonToolsDir;
    corefoundation = coreFoundationArm64Build;
    vulkanLoader = pkgs.vulkan-loader;
    vulkanHeaders = pkgs.vulkan-headers;
    libX11 = libX11SharedArm64Build;
    libxcb = libxcbSharedArm64Build;
    libXau = libXauSharedArm64Build;
    libXdmcp = libXdmcpSharedArm64Build;
    libXrandr = libXrandrSharedArm64Build;
    libXrender = libXrenderSharedArm64Build;
    inherit (pkgs) xorgproto;
  };
  vulkanToolsArm64Build = mkArm64Build ./pkgs/mesa/vulkan-tools.nix {
    nativeMesonTools = nativeMesonToolsDir;
    vulkanTools = pkgs.vulkan-tools;
    vulkanHeaders = pkgs.vulkan-headers;
    vulkanLoader = vulkanLoaderX11Arm64Build;
    libX11 = libX11SharedArm64Build;
    libXext = libXextSharedArm64Build;
    libxcb = libxcbSharedArm64Build;
    libXau = libXauSharedArm64Build;
    libXdmcp = libXdmcpSharedArm64Build;
    libXrandr = libXrandrSharedArm64Build;
    inherit (pkgs) xorgproto;
  };
  puredarwingopArm64Build = mkArm64Build ./pkgs/x11/xf86-video-puredarwingop.nix {
    xorg = xorgNoGlxArm64Build;
    pixman = xvfbPixmanArm64Build;
    iokit = iokitArm64Build;
    corefoundation = coreFoundationArm64Build;
    inherit (pkgs) xorgproto;
  };
  # xorg on the puredarwingop driver plus the x11 vulkan stack, laid out over a minimal root,
  # Xvfb rides along so the client side can be checked on a board with no display attached
  x11PvrArm64 = pkgs.runCommand "puredarwin-x11-pvr-arm64" { } ''
    mkdir -p $out
    for p in ${xorgNoGlxArm64Build} ${xkbcompArm64Build} ${xkeyboardConfigArm64Build} ${xeyesArm64Build} \
        ${libdrmPdpvrArm64Build} ${mesaPowervrX11Arm64Build} ${vulkanLoaderX11Arm64Build} ${vulkanToolsArm64Build} \
        ${libX11SharedArm64Build} ${libxcbSharedArm64Build} ${libXauSharedArm64Build} ${libXdmcpSharedArm64Build} \
        ${libXextSharedArm64Build} ${libXrenderSharedArm64Build} ${libXrandrSharedArm64Build} ${libxshmfenceSharedArm64Build} \
        ${freetype2Arm64Build} ${xvfbLibxcvtArm64Build} ${xvfbArm64Build}; do
      for d in usr/bin usr/share/X11 usr/share/vulkan; do
        [ -d $p/$d ] && mkdir -p $out/$d && cp -a $p/$d/. $out/$d/ && chmod -R u+w $out/$d
      done
      # some packages link bin/ back into usr/bin, so take bin/ only when usr/bin is absent
      [ -d $p/bin ] && [ ! -d $p/usr/bin ] && mkdir -p $out/usr/bin && cp -a $p/bin/. $out/usr/bin/
      mkdir -p $out/usr/lib
      # the shared guest libraries keep their dylibs in lib/ and only symlink them from usr/lib
      [ -d $p/lib ] && find $p/lib -maxdepth 1 -name '*.dylib' -exec cp -a {} $out/usr/lib/ \;
      [ -d $p/usr/lib ] && find $p/usr/lib -mindepth 1 -maxdepth 1 ! -lname '../../lib/*' -exec cp -a {} $out/usr/lib/ \;
      chmod -R u+w $out
    done
    mkdir -p $out/usr/lib/xorg/modules/drivers
    cp ${puredarwingopArm64Build}/usr/lib/xorg/modules/drivers/puredarwingop_drv.so $out/usr/lib/xorg/modules/drivers/
    # glx would pull the llvmpipe mesa in, and the server runs without it
    rm -rf $out/usr/lib/xorg/modules/extensions $out/usr/lib/xorg/modules/input $out/usr/lib/pkgconfig $out/usr/lib/cmake $out/usr/include
    rm -f $out/usr/bin/vkcubepp $out/usr/bin/gtf
    rm -f $out/usr/lib/*.a $out/usr/lib/*.la
    # libxcb's extension libraries name it by its configure libdir, /lib
    for f in $out/usr/lib/libxcb-*.dylib; do
      [ -L "$f" ] || ${nativeMesonToolsDir}/bin/install_name_tool -change /lib/libxcb.1.dylib /usr/lib/libxcb.1.dylib "$f"
    done
    install -Dm644 ${./pkgs/x11/a733-xorg.conf} $out/etc/X11/xorg.conf
    install -Dm755 ${./pkgs/x11/pdx11-demo.sh} $out/usr/local/bin/pdx11-demo
    install -Dm644 ${./pkgs/x11/org.puredarwin.xorg.plist} $out/System/Library/LaunchDaemons/org.puredarwin.xorg.plist
    install -Dm644 ${./pkgs/x11/org.puredarwin.pdx11demo.plist} $out/System/Library/LaunchDaemons/org.puredarwin.pdx11demo.plist
    # scanout: the cube through VK_KHR_display, flipped by PDSun60iDisplay
    install -Dm755 ${pdvkspinArm64Build}/usr/local/bin/pdvkdisplay $out/usr/local/bin/pdvkdisplay
    install -Dm755 ${pdvkspinArm64Build}/usr/local/bin/pdvkspin $out/usr/local/bin/pdvkspin
    install -Dm755 ${./pkgs/gpu/pddisplay-demo.sh} $out/usr/local/bin/pddisplay-demo
    install -Dm644 ${./pkgs/gpu/org.puredarwin.pddisplay.plist} $out/System/Library/LaunchDaemons/org.puredarwin.pddisplay.plist
  '';
  mesaDemosArm64Build = mkArm64Build ./pkgs/mesa/mesa-demos.nix {
    nativeMesonTools = nativeMesonToolsDir;
    mesa = mesaArm64Build;
    openglFramework = openglFrameworkArm64Build;
    libX11 = xlibArm64Build;
    libXext = xvfbLibXextArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    inherit (pkgs) meson ninja pkg-config xorgproto xtrans;
  };
  migcomDarwinArm64Build = mkArm64Build ./pkgs/toolchain/migcom-darwin.nix {
  };
  netsurfArm64Build = mkArm64Build ./pkgs/apps/netsurf.nix {
    hostOtool = hostOtoolArm64Build;
    glibNative = pkgs.glib.dev;
    gdkPixbufNative = pkgs.gdk-pixbuf.dev;
    inherit (pkgs) inetutils;
    gtk3 = gtk3Arm64Build;
    glib = glibArm64Build;
    cairo = cairoArm64Build;
    cairoGobject = cairoGobjectArm64Build;
    pango = pangoArm64Build;
    gdkPixbuf = gdkPixbufArm64Build;
    libepoxy = libepoxyArm64Build;
    atspi2Core = atspi2CoreArm64Build;
    dbus = dbusArm64Build;
    libcurl = libcurlDylibArm64Build;
    openssl = opensslArm64Build;
    zlib = xvfbZlibArm64Build;
    libpng = libpngArm64Build;
    libiconv = libiconvArm64Build;
    libwapcaplet = libwapcapletArm64Build;
    libparserutils = libparserutilsArm64Build;
    libhubbub = libhubbubArm64Build;
    libcss = libcssArm64Build;
    libdom = libdomArm64Build;
    libnsgif = libnsgifArm64Build;
    libnsbmp = libnsbmpArm64Build;
    libnsutils = libnsutilsArm64Build;
    libutf8proc = libutf8procArm64Build;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXi = xvfbLibXiArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXrandr = xvfbLibXrandrArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXcursor = xvfbLibXcursorArm64Build;
    xorgproto = pkgs.xorgproto;
    expat = expatArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    fribidi = fribidiArm64Build;
    harfbuzz = harfbuzzArm64Build;
    freetype2 = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    inherit (pkgs) perl pkg-config nsgenbind;
  };
  openglFrameworkArm64Build = mkArm64Build ./pkgs/apple/opengl-framework.nix {
    mesa = mesaArm64Build;
    glu = gluArm64Build;
    libX11 = xlibArm64Build;
    xorgproto = pkgs.xorgproto;
    libXext = xvfbLibXextArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = pkgs.libxdmcp;
    src = ../src/Frameworks/OpenGL;
  };
  opensshArm64Build = pdArm64.openssh;
  pangoArm64Build = pdArm64.pango;
  pythonArm64Build = pdArm64.python;
  libcrocoArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libcroco";
    version = "0.6.13";
    src = pkgs.fetchurl {
      url = "https://download.gnome.org/sources/libcroco/0.6/libcroco-0.6.13.tar.xz";
      hash = "sha256-dn7CNK56poRpWzpzVUgiSIgTLgY/kttYV1m0IlcGIdQ=";
    };
    deps = [
      glibArm64Build libxml2Arm64Build pcre2Arm64Build libffiArm64Build
      libiconvArm64Build xvfbZlibArm64Build
    ];
    configureFlags = [ "--disable-Werror" "--disable-Bsymbolic" ];
  };
  librsvgArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-librsvg";
    version = "2.40.21";
    src = pkgs.fetchurl {
      url = "https://download.gnome.org/sources/librsvg/2.40/librsvg-2.40.21.tar.xz";
      hash = "sha256-92KJBfHK2oTofisUiD7VfYCU3KMoHVvLJOzkJ56akro=";
    };
    deps = [
      glibArm64Build gdkPixbufArm64Build cairoArm64Build cairoGobjectArm64Build
      pangoArm64Build libxml2Arm64Build libcrocoArm64Build libpngArm64Build
      freetype2Arm64Build fontconfigArm64Build fribidiArm64Build
      harfbuzzArm64Build expatArm64Build pcre2Arm64Build libffiArm64Build
      libiconvArm64Build xvfbZlibArm64Build xvfbPixmanArm64Build
    ];
    preConfigureExtra = ''
      # libxml2 installs under include/libxml2/libxml, and the deps mapping
      # only contributes the include/ level.
      export CFLAGS="$CFLAGS -I${libxml2Arm64Build}/include/libxml2"
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
        ${nativeMesonToolsDir}/bin/install_name_tool \
          -change //lib/librsvg-2.2.dylib /lib/librsvg-2.2.dylib "$so"
      done
    '';
  };
  gnutlsSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-gnutls";
    inherit (pkgs.gnutls) version src;
    deps = [ nettleSharedArm64Build ];
    postPatchExtra = ''
      substituteInPlace configure \
        --replace ' -framework Security -framework CoreFoundation' ""
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
  libsoupArm64Build = mkArm64Build ./pkgs/base/libsoup.nix {
    nativeMesonTools = nativeMesonToolsDir;
    glibNative = pkgs.glib.dev;
    inherit (pkgs) meson ninja pkg-config python3;
    libsoup = pkgs.libsoup_3;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
    sqlite = sqliteArm64Build;
    libpsl = libpslArm64Build;
    nghttp2 = nghttp2Arm64Build;
    libxml2 = libxml2Arm64Build;
  };
  vteArm64Build = mkArm64Build ./pkgs/gtk/vte.nix {
    nativeMesonTools = nativeMesonToolsDir;
    version = "0.70.6";
    src = vteSrc;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
    cairo = cairoArm64Build;
    cairoGobject = cairoGobjectArm64Build;
    pixman = xvfbPixmanArm64Build;
    pango = pangoArm64Build;
    fribidi = fribidiArm64Build;
    gnutls = gnutlsSharedArm64Build;
    harfbuzz = harfbuzzArm64Build;
    freetype2 = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    expat = expatArm64Build;
    gdkPixbuf = gdkPixbufArm64Build;
    libepoxy = libepoxyArm64Build;
    atspi2Core = atspi2CoreArm64Build;
    dbus = dbusArm64Build;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXi = xvfbLibXiArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXrandr = xvfbLibXrandrArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXcursor = xvfbLibXcursorArm64Build;
    libpng = libpngArm64Build;
    glibNative = pkgs.glib.dev;
    gtk3 = gtk3Arm64Build;
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    inherit (pkgs) xorgproto;
  };
  libwnckArm64Build = mkArm64Build ./pkgs/xfce/libwnck.nix {
    nativeMesonTools = nativeMesonToolsDir;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
    cairo = cairoArm64Build;
    cairoGobject = cairoGobjectArm64Build;
    pixman = xvfbPixmanArm64Build;
    pango = pangoArm64Build;
    fribidi = fribidiArm64Build;
    harfbuzz = harfbuzzArm64Build;
    freetype2 = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    expat = expatArm64Build;
    gdkPixbuf = gdkPixbufArm64Build;
    libepoxy = libepoxyArm64Build;
    atspi2Core = atspi2CoreArm64Build;
    dbus = dbusArm64Build;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXi = xvfbLibXiArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXrandr = xvfbLibXrandrArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXcursor = xvfbLibXcursorArm64Build;
    libXres = xvfbLibXresArm64Build;
    libpng = libpngArm64Build;
    glibNative = pkgs.glib.dev;
    gtk3 = gtk3Arm64Build;
    startupNotification = startupNotificationArm64Build;
    inherit (pkgs) libwnck xorgproto;
  };
  coreServicesArm64Build = pdArm64.coreservices;
  iomediacheckArm64Build = pdArm64.iomediacheck;
  pdsurfaceArm64Build = pdArm64.pdsurface;
  diskArbitrationArm64Build =
    (mkPureDarwinBuild {
      pname = "puredarwin-diskarbitration-arm64";
      src = diskArbitrationSource;
      buildTargets = [ "DiskArbitration" "diskarbitrationd" ];
      enableProjects = false;
      enableKernel = false;
      enableUserspace = false;
      installUserland = false;
      installKernel = false;
      # No prebuiltLibSystem here: src/CMakeLists.txt treats a prebuilt
      # libSystem and add_subdirectory(Libraries) as mutually exclusive
      # branches, so passing one would leave the DiskArbitration target
      # undefined. SystemConfiguration is built the same way for the same
      # reason.
      puredarwinArch = "arm64";
      inherit arm64CrossToolchain;
      extraCmakeFlags = [
        "-DPUREDARWIN_ENABLE_DISKARBITRATION=ON"
        "-DPUREDARWIN_COREFOUNDATION_PREFIX=${coreFoundationArm64Build}"
        "-DPUREDARWIN_IOKIT_PREFIX=${iokitArm64Build}"
        "-DPUREDARWIN_SECURITY_PREFIX=${securityArm64Build}"
        "-DPUREDARWIN_SYSTEMCONFIGURATION_PREFIX=${systemConfigurationArm64Build}"
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
  opensslThreadsArm64Build = opensslArm64Build.override { withThreads = true; };
  securityArm64Build = mkArm64Build ./pkgs/apple/security.nix {
    pname = "puredarwin-security-arm64";
    puredarwinArch = "arm64";
    inherit mkPureDarwinBuild arm64CrossToolchain;
    corefoundation = coreFoundationArm64Build;
    libobjc = libobjcArm64Build;
    foundation = foundationArm64Build;
    sqlite = sqliteArm64Build;
    src = securitySource;
    openssl = opensslThreadsArm64Build;
    libz = libzDylibArm64Build;
  };
  symptomReporterArm64Build =
    if symptomReporterBuild == null then null
    else symptomReporterBuild.override {
      puredarwinArch = "arm64";
      inherit arm64CrossToolchain;
    };
  systemConfigurationArm64Build =
  let base = mkSystemConfigurationBuild {
    corefoundation = coreFoundationArm64Build;
    libobjc = libobjcArm64Build;
    security = securityArm64Build;
    iokit = iokitArm64Build;
    symptomReporter = symptomReporterArm64Build;
  };
  in if base == null then null else base.override {
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
  };
  startupNotificationArm64Build = pdArm64.startup-notification;
  xcalcArm64Build = pdArm64.xcalc;
  xcbArm64Build = pdArm64.libxcb;
  xcbCursorArm64Build = pdArm64.libxcbcursor;
  xcbImageArm64Build = pdArm64.libxcb-image;
  xcbKeysymsArm64Build = pdArm64.libxcb-keysyms;
  xcbRenderUtilArm64Build = pdArm64.libxcb-render-util;
  xcbUtilArm64Build = pdArm64.libxcb-util;
  xcbWmArm64Build = pdArm64.xcbWm;
  xcbXrmArm64Build = pdArm64.xcb-util-xrm;
  xclockArm64Build = pdArm64.xclock;
  xeyesArm64Build = pdArm64.xeyes;
  xkbcommonArm64Build = pdArm64.xkbcommon;
  xkbcompArm64Build = pdArm64.xkbcomp;
  xlibArm64Build = pdArm64.libX11;
  xmessageArm64Build = pdArm64.xmessage;
  xorgArm64Build = mkArm64Build ./pkgs/x11/xorg.nix {
    xorg-server = pkgs.xorg-server;
    pixman = xvfbPixmanArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXfont2 = xvfbLibXfont2Arm64Build;
    zlib = xvfbZlibArm64Build;
    freetype2 = freetype2Arm64Build;
    libfontenc = libfontencArm64Build;
    xvfbZlib = xvfbZlibArm64Build;
    inherit (pkgs) xorgproto xtrans;
    libxkbfile = xvfbLibXkbfileArm64Build;
    libXdmcp = pkgs.libxdmcp;
    libxcvt = xvfbLibxcvtArm64Build;
    mesaGlHeaders = pkgs.mesa-gl-headers;
    glHeaders = pkgs.libglvnd.dev;
    mesa = mesaArm64Build;
  };
  # the a733 server: no glx, so no llvmpipe mesa underneath it
  xorgNoGlxArm64Build = mkArm64Build ./pkgs/x11/xorg.nix {
    xorg-server = pkgs.xorg-server;
    pixman = xvfbPixmanArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXfont2 = xvfbLibXfont2Arm64Build;
    zlib = xvfbZlibArm64Build;
    freetype2 = freetype2Arm64Build;
    libfontenc = libfontencArm64Build;
    xvfbZlib = xvfbZlibArm64Build;
    inherit (pkgs) xorgproto xtrans;
    libxkbfile = xvfbLibXkbfileArm64Build;
    libXdmcp = pkgs.libxdmcp;
    libxcvt = xvfbLibxcvtArm64Build;
    mesaGlHeaders = pkgs.mesa-gl-headers;
    glHeaders = pkgs.libglvnd.dev;
    mesa = null;
    withGlx = false;
  };
  xtermArm64Build = pdArm64.xterm;
  xvfbArm64Build = pdArm64.xvfb;
  xvfbLibICEArm64Build = pdArm64.libICE;
  xvfbLibSMArm64Build = pdArm64.libSM;
  xvfbLibXauArm64Build = pdArm64.libXau;
  xvfbLibXawArm64Build = pdArm64.libXaw;
  xvfbLibXcursorArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXcursor";
    version = pkgs.libXcursor.version;
    src = pkgs.libXcursor.src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXfixesArm64Build xvfbLibXrenderArm64Build ];
    postInstallExtra = ''
      mkdir -p .libXcursor-dylib
      (
        cd .libXcursor-dylib
        ${arm64CrossToolchain}/bin/arm64-apple-darwin20.4-ar x "$out/lib/libXcursor.a"
        ${arm64CrossToolchain}/bin/arm64-apple-darwin20.4-clang \
          -isysroot "$DARWIN_SDK_ROOT" \
          -mmacosx-version-min=26.5 \
          -fuse-ld=${nativeLd}/bin/ld \
          -nostdlib \
          -dynamiclib \
          -Wl,-install_name,/lib/libXcursor.1.dylib \
          -Wl,-compatibility_version,1.0.0 \
          -Wl,-current_version,1.0.2 \
          -Wl,-undefined,dynamic_lookup \
          -L${libSystemArm64Build}/usr/lib \
          -o "$out/lib/libXcursor.1.dylib" \
          ./*.o \
          -lSystem
      )
      ln -sf libXcursor.1.dylib "$out/lib/libXcursor.dylib"
    '';
  };
  xvfbLibXdmcpArm64Build = mkArm64Build ./pkgs/x11/xvfb-stub-lib.nix {
    name = "Xdmcp";
    version = pkgs.libxdmcp.version or "1.1.5";
    pcName = "xdmcp";
    pcDescription = "X Display Manager Control Protocol library";
    includeFrom = [ pkgs.libxdmcp pkgs.xorgproto ];
    source = ''
      int XdmcpWrap(const unsigned char *input, unsigned char *wrapper, const unsigned char *key) { (void)input; (void)wrapper; (void)key; return 0; }
      int XdmcpUnwrap(const unsigned char *input, unsigned char *wrapper, const unsigned char *key) { (void)input; (void)wrapper; (void)key; return 0; }
    '';
  };
  xvfbLibXextArm64Build = pdArm64.libXext;
  xvfbLibXfixesArm64Build = pdArm64.libXfixes;
  xvfbLibXfont2Arm64Build = pdArm64.libXfont2;
  xvfbLibXiArm64Build = pdArm64.libXi;
  xvfbLibXkbfileArm64Build = pdArm64.libxkbfile;
  xvfbLibXmuArm64Build = pdArm64.libXmu;
  xvfbLibXpmArm64Build = pdArm64.libXpm;
  xvfbLibXrandrArm64Build = pdArm64.libXrandr;
  xvfbLibXrenderArm64Build = pdArm64.libXrender;
  xvfbLibXtArm64Build = pdArm64.libXt;
  xvfbLibxcvtArm64Build = pdArm64.libxcvt;
  xvfbPixmanArm64Build = pdArm64.pixman;
  pdVirglShimArm64Build =
  (mkPureDarwinBuild {
      pname = "puredarwin-pd-virgl-shim-arm64";
      src = userlandSource;
      buildTargets = [ "pd_virgl_shim" ];
      enableProjects = false;
      enableKernel = false;
      enableLibraries = false;
      installUserland = false;
      installKernel = false;
      prebuiltLibSystem = libSystemArm64Build;
      puredarwinArch = "arm64";
      inherit arm64CrossToolchain;
      extraCmakeFlags = [
        "-DPUREDARWIN_COREFOUNDATION_PREFIX=${coreFoundationArm64Build}"
      ];
  }).overrideAttrs (old: {
      installPhase = ''
        runHook preInstall
        mkdir -p $out/usr/lib $out/include
        ar=${arm64CrossToolchain}/bin/arm64-apple-darwin20.4-ar
        mkdir -p repack && ( cd repack && \
          "$ar" x ../build-nix/src/Userspace/pd-virgl-shim/libpd_virgl_shim.a )
        ${arm64CrossToolchain}/bin/arm64-apple-darwin20.4-clang \
          -dynamiclib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
          -L${libSystemArm64Build}/usr/lib \
          -Wl,-install_name,/usr/lib/libpd_virgl_shim.dylib \
          -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
          repack/*.o -lSystem \
          -o $out/usr/lib/libpd_virgl_shim.dylib
        cp src/Libraries/PDVirglShim/include/pd_virgl_shim.h $out/include/
        runHook postInstall
      '';
  });
  # Arch-independent: a host-side tool and a pure data package, so the
  # x86 builds are reused rather than duplicated.
  # Arch-independent: a host-side tool and a pure data package, so the
  # x86 builds are reused rather than duplicated.
  hostOtoolArm64Build = hostOtoolBuild;
  xkeyboardConfigArm64Build = xkeyboardConfigBuild;

  xlibLocaleArm64Build = xlibLocaleBuild;
  xvfbFontsArm64Build = xvfbFontsBuild;
  i3statusShimArm64Build = i3statusShimBuild;

  libSystemArm64Build = libSystemBuild.override {
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
  };
  # First-stage 32-bit ARM userland for the BCM2835/Pi Zero target.  It needs
  # an ARMv6 libSystem/dyld rather than the x86_64 or ARM64 runtime.
  libSystemArmv6Build = libSystemBuild.override {
    puredarwinArch = "armv6";
    compilerRt = compilerRtArmv6Build;
    extraCmakeFlags = [
      "-DCMAKE_OSX_ARCHITECTURES=armv6"
      "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
    ];
  };
  icuCoreArm64Build = pdArm64.icu;
  libcxxabiDylibArm64Build = pdArm64.libcxxabiDylib;
  libcxxDylibArm64Build = pdArm64.libcxxDylib;
  libobjcArm64Build = pdArm64.libobjc;
  coreFoundationArm64Build = pdArm64.corefoundation;
  # The static archive this force-loads has to be the arm64 one too. Overriding
  # only the toolchain leaves it pulling the x86_64 libIOKitCF.a, whose objects
  # the linker skips as wrong-architecture - producing a valid, and completely
  # empty, arm64 dylib rather than any error.
  iokitCFStaticArm64Build = iokitCFStaticBuild.override {
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
  };
  iokitArm64Build = pdArm64.iokit;
  # ARMv6 (Pi Zero). launchd links CoreFoundation and IOKitCF, which in turn
  # need ICU, libobjc and libc++abi, so the whole chain gets an armv6 build.
  libcxxabiDylibArmv6Build = libcxxabiDylibBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
  };
  libcxxDylibArmv6Build = libcxxDylibBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
    libcxxabiDylib = libcxxabiDylibArmv6Build;
  };
  libobjcArmv6Build = libobjcBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
    libcxxabiDylib = libcxxabiDylibArmv6Build;
    libcxxDylib = libcxxDylibArmv6Build;
    compilerRt = compilerRtArmv6Build;
  };
  icuCoreArmv6Build = icuCoreBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
    compilerRt = compilerRtArmv6Build;
    libcxxabiDylib = libcxxabiDylibArmv6Build;
    libcxxDylib = libcxxDylibArmv6Build;
  };
  coreFoundationArmv6Build = coreFoundationBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
    icu = icuCoreArmv6Build;
    libobjc = libobjcArmv6Build;
    compilerRt = compilerRtArmv6Build;
  };
  # No prebuiltLibSystem here: that takes the prebuilt_libsystem.cmake branch in
  # src/CMakeLists.txt, which skips Libraries/ entirely and with it IOKitCF.
  iokitCFStaticArmv6Build = iokitCFStaticBuild.override {
    puredarwinArch = "armv6";
    compilerRt = compilerRtArmv6Build;
    extraCmakeFlags = [
      "-DPUREDARWIN_ENABLE_IOKITCF=ON"
      "-DPUREDARWIN_COREFOUNDATION_PREFIX=${coreFoundationArmv6Build}"
      "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjcArmv6Build}"
      "-DCMAKE_OSX_ARCHITECTURES=armv6"
      "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
    ];
  };
  iokitArmv6Build = iokitBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
    corefoundation = coreFoundationArmv6Build;
    iokitCFStatic = iokitCFStaticArmv6Build;
  };
  launchdArmv6Build = launchdBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
    corefoundation = coreFoundationArmv6Build;
    iokit = iokitArmv6Build;
    compilerRt = compilerRtArmv6Build;
  };
  launchdArm64Build = pdArm64.launchd;
  launchctlArm64Build = pdArm64.launchctl;
  mkArm64Build = file: deps:
  let
  f = import file;
  common = {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
    # c++ recipes that link with -nostdlib take the runtime explicitly
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    inherit nativeLd;
  };
  in
  pkgs.callPackage f
  (builtins.intersectAttrs (builtins.functionArgs f) common // deps);

  # Counterpart of flake.nix's mkSharedXorgLib: the guest-prefixed, shared-library
  # flavour of the same generic autotools wrapper. Kept here so the arm64 set can
  # mirror the x86 "SharedBuild" packages without repeating the five common args.
  mkSharedArm64XorgLib = args: mkArm64Build ./pkgs/x11/xorg-cross-lib.nix ({
    nativeMesonTools = nativeMesonToolsDir;
    guestPrefix = true;
    shared = true;
  } // args);

  xvfbZlibArm64Build = pdArm64.zlib;
  toyboxArm64Build = pdArm64.toybox;
  pdadbdArm64Build = mkArm64Build ./pkgs/apps/pdadbd.nix { };
  xzArm64Build = pdArm64.xz;
  fileArm64Build = pdArm64.file;
  opensslArm64Build = pdArm64.openssl;

  sqliteArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-sqlite";
    inherit (pkgs.sqlite) version;
    # nixpkgs' sqlite src is a .zip the default unpacker cannot read.
    src = pkgs.runCommand "sqlite-src-${pkgs.sqlite.version}" { nativeBuildInputs = [ pkgs.unzip ]; } ''
      unzip -q ${pkgs.sqlite.src}
      mkdir -p $out
      cp -R sqlite-src-*/. $out/
      chmod -R u+w $out
    '';
    configureFlags = [ "--disable-readline" "--disable-editline" ];
  };
  libjpegArm64Build = pdArm64.libjpeg;
  libwebpArm64Build = pdArm64.libwebp;
  libgpgErrorArm64Build = pdArm64.libgpg-error;
  libgcryptArm64Build = pdArm64.libgcrypt;
  libtasn1Arm64Build = pdArm64.libtasn1;
  nghttp2Arm64Build = pdArm64.nghttp2;
  libpslArm64Build = pdArm64.libpsl;
  gettextArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-gettext";
    inherit (pkgs.gettext) version src;
    deps = [ libiconvArm64Build ];
    preConfigureExtra = ''
      cd gettext-runtime
    '';
    configureFlags = [
      "--with-included-libintl"
      "--with-libiconv-prefix=${libiconvArm64Build}"
      "--disable-java"
      "--disable-csharp"
      "--disable-libasprintf"
      "--disable-rpath"
      "--disable-dependency-tracking"
    ];
  };
  libxshmfenceSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libxshmfence";
    inherit (pkgs.libxshmfence) version src;
    deps = [ pkgs.xorgproto ];
    configureFlags = [ "--with-shared-memory-dir=/tmp" ];
  };
  xvfbLibXineramaArm64Build = pdArm64.xvfbLibXinerama;
  xvfbLibXresArm64Build = pdArm64.libXres;
  xvfbLibXcompositeArm64Build = pdArm64.libXcomposite;
  xvfbLibXdamageArm64Build = pdArm64.libXdamage;
  xvfbLibXpresentArm64Build = pdArm64.xvfbLibXpresent;
  iceauthArm64Build = pdArm64.iceauth;
  xrandrArm64Build = pdArm64.xrandr;
  xrdbArm64Build = pdArm64.xrdb;
  xinitArm64Build = pdArm64.xinit;
  # Wine links the X libraries dynamically, so it needs the shared, guest-prefixed
  # flavour rather than the static ones the rest of the tree uses.
  libXauSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libXau";
    inherit (pkgs.libXau) version src;
    deps = [ pkgs.xorgproto ];
  };
  libXdmcpSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libXdmcp";
    inherit (pkgs.libXdmcp) version src;
    deps = [ pkgs.xorgproto ];
  };
  libxcbSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libxcb";
    inherit (pkgs.libxcb) version src;
    deps = [ pkgs.xorgproto libXauSharedArm64Build libXdmcpSharedArm64Build ];
    nativeDeps = [ pkgs.python3 pkgs.xcb-proto ];
    configureFlags = [ "--disable-devel-docs" ];
    preConfigureExtra = ''
      export PYTHONPATH="${pkgs.xcb-proto}/${pkgs.python3.sitePackages}:$PYTHONPATH"
    '';
  };
  libX11SharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libX11";
    inherit (pkgs.libX11) version src;
    deps = [ pkgs.xorgproto pkgs.xtrans libxcbSharedArm64Build libXauSharedArm64Build libXdmcpSharedArm64Build ];
    configureFlags = [ "--disable-specs" "--enable-xlocaledir" ];
  };
  libXextSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libXext";
    inherit (pkgs.libXext) version src;
    deps = [ pkgs.xorgproto libX11SharedArm64Build libXauSharedArm64Build ];
  };
  libXrenderSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libXrender";
    inherit (pkgs.libXrender) version src;
    deps = [ pkgs.xorgproto libX11SharedArm64Build ];
  };
  libXfixesSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libXfixes";
    inherit (pkgs.libXfixes) version src;
    deps = [ pkgs.xorgproto libX11SharedArm64Build libXextSharedArm64Build ];
  };
  libXiSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libXi";
    inherit (pkgs.libXi) version src;
    deps = [ pkgs.xorgproto libX11SharedArm64Build libXextSharedArm64Build libXfixesSharedArm64Build ];
  };
  libXrandrSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libXrandr";
    inherit (pkgs.libXrandr) version src;
    deps = [ pkgs.xorgproto libX11SharedArm64Build libXextSharedArm64Build libXrenderSharedArm64Build ];
  };
  libXcursorSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-libXcursor";
    inherit (pkgs.libXcursor) version src;
    deps = [ pkgs.xorgproto libX11SharedArm64Build libXrenderSharedArm64Build libXfixesSharedArm64Build ];
  };
  nettleSharedArm64Build = mkSharedArm64XorgLib {
    pname = "puredarwin-nettle";
    inherit (pkgs.nettle) version src;
    configureFlags = [
      "--enable-mini-gmp"
      "--disable-documentation"
      "--disable-assembler"
      "--disable-openssl"
    ];
  };

  # Core tools. autoconf/automake are host-side scripts with no
  # cross-compiled component, so they are shared with the x86 build
  # rather than re-instantiated.
  bmakeArm64Build = pdArm64.bmake;
  gnumakeArm64Build = pdArm64.gnumake;
  gnum4Arm64Build = pdArm64.gnum4;
  pkgconfArm64Build = pdArm64.pkgconf;
  bisonArm64Build = pdArm64.bison;
  flexArm64Build = pdArm64.flex;
  xxdArm64Build = mkArm64Build ./pkgs/toolchain/xxd.nix { };
  nanoArm64Build = pdArm64.nano;

  # Core libraries.
  libffiArm64Build = pdArm64.libffi;
  expatArm64Build = pdArm64.expat;
  pcre2Arm64Build = pdArm64.pcre2;
  libevArm64Build = pdArm64.libev;
  libpngArm64Build = pdArm64.libpng;
  freetype2Arm64Build = pdArm64.freetype2;
  fontconfigArm64Build = pdArm64.fontconfig;
  libxml2Arm64Build = pdArm64.libxml2;
  yajlArm64Build = pdArm64.yajl;
  glibArm64Build = pdArm64.glib;

  # the boards and their kernels (see nix/boards.nix)
  boardKernels = (import ./boards.nix).kernels { inherit lib mkPureDarwinBuild kernelSource arm64CrossToolchain; };
  # the arm64 PureDarwin package set, the same recipes as x86_64 (see nix/pd/scope.nix)
  pdArm64 = pdScope {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    inherit nativeLd;
    # the arm64 builds of the packages not yet in nix/pd/packages.nix, by their scope names
    bridge = {
      atspi2CoreNoxBuild = throw "nix/pd: no arm64 build of atspi2CoreNoxBuild";
      cairoGobjectNoxBuild = throw "nix/pd: no arm64 build of cairoGobjectNoxBuild";
      clang = clangCrossArm64Build;
      compilerRt = compilerRtArm64Build;
      darwinCrossToolchain = darwinCrossToolchain;
      glu = throw "nix/pd: no arm64 build of glu";
      gtk3NoxBuild = throw "nix/pd: no arm64 build of gtk3NoxBuild";
      harfbuzzNoxBuild = throw "nix/pd: no arm64 build of harfbuzzNoxBuild";
      hostOtool = hostOtoolArm64Build;
      libepoxyNoxBuild = throw "nix/pd: no arm64 build of libepoxyNoxBuild";
      llvm = llvmCrossArm64Build;
      mesa = throw "nix/pd: no arm64 build of mesa";
      mesaDefaultBuild = throw "nix/pd: no arm64 build of mesaDefaultBuild";
      metalObjcPdNoxBuild = throw "nix/pd: no arm64 build of metalObjcPdNoxBuild";
      metalPd = throw "nix/pd: no arm64 build of metalPd";
      nativeLd = nativeLd;
      openglFramework = throw "nix/pd: no arm64 build of openglFramework";
      opensslThreadsBuild = opensslThreadsArm64Build;
      pangoNoxBuild = throw "nix/pd: no arm64 build of pangoNoxBuild";
      utilityApp = throw "nix/pd: no arm64 build of utilityApp";
      xkbcommonNoxBuild = throw "nix/pd: no arm64 build of xkbcommonNoxBuild";
    };
    # what arm64 builds differently from the shared definitions
    overrides = {
      asmjit-test = asmjitTestArm64Build;
      atspi2Core = atspi2CoreArm64Build;
      cmake = cmakeArm64Build;
      coretext = coretextArm64Build;
      curl = curlArm64Build;
      dbus = dbusArm64Build;
      diskArbitration = diskArbitrationArm64Build;
      fastfetch = fastfetchArm64Build;
      fcft = fcftArm64Build;
      foot = footArm64Build;
      gettext = gettextArm64Build;
      git = gitArm64Build;
      gnutls = gnutlsSharedArm64Build;
      gtk3 = gtk3Arm64Build;
      gtkLayerShell = gtkLayerShellArm64Build;
      i3statusShim = i3statusShimArm64Build;
      iokitCFStatic = iokitCFStaticArm64Build;
      libSystem = libSystemArm64Build;
      libX11SharedBuild = libX11SharedArm64Build;
      libXauSharedBuild = libXauSharedArm64Build;
      libXcursor = xvfbLibXcursorArm64Build;
      libXcursorSharedBuild = libXcursorSharedArm64Build;
      libXdmcp = xvfbLibXdmcpArm64Build;
      libXdmcpSharedBuild = libXdmcpSharedArm64Build;
      libXextSharedBuild = libXextSharedArm64Build;
      libXfixesSharedBuild = libXfixesSharedArm64Build;
      libXiSharedBuild = libXiSharedArm64Build;
      libXrandrSharedBuild = libXrandrSharedArm64Build;
      libXrenderSharedBuild = libXrenderSharedArm64Build;
      libcroco = libcrocoArm64Build;
      libcurl = libcurlDylibArm64Build;
      libepoxy = libepoxyArm64Build;
      librsvg = librsvgArm64Build;
      libsoup = libsoupArm64Build;
      libwnck = libwnckArm64Build;
      libxcbSharedBuild = libxcbSharedArm64Build;
      libxshmfence = libxshmfenceSharedArm64Build;
      mesaBuild = mesaArm64Build;
      mesaDemos = mesaDemosArm64Build;
      netsurf = netsurfArm64Build;
      nettle-shared = nettleSharedArm64Build;
      openglFrameworkBuild = openglFrameworkArm64Build;
      opensslThreads = opensslThreadsArm64Build;
      pdVirglShim = pdVirglShimArm64Build;
      security = securityArm64Build;
      sqlite = sqliteArm64Build;
      symptomReporter = symptomReporterArm64Build;
      systemConfiguration = systemConfigurationArm64Build;
      userland = userlandArm64Build;
      vte = vteArm64Build;
      vulkan-tools = vulkanToolsArm64Build;
      vulkanLoader = vulkanLoaderArm64Build;
      wine = wineArm64Build;
      xkeyboardConfig = xkeyboardConfigArm64Build;
      xlibLocale = xlibLocaleArm64Build;
      xorg = xorgArm64Build;
      xvfbFonts = xvfbFontsArm64Build;
    };
  };
  libiconvArm64Build = pdArm64.libiconv;
  ncursesArm64Build = pdArm64.ncurses;
  zshArm64Build = pdArm64.zsh;
  userlandArm64Sg2002Build = userlandBuild.override {
    pname = "puredarwin-userland-arm64-sg2002";
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    prebuiltLibSystem = libSystemArm64Build;
    xorgDriverIncludes = null;
    buildTargets = [ "sw_vers" "doubleagentd" "ps" "killall" "pkill" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "cpuctl" "mean" "reboot" "halt" "poweroff" "shutdown" "netsetup" "wslinit" "ping" "wifiutil" "startx" "mousemon" "mount" "umount" "ext4tool" "ext4_util" "mdnsd" "pdpowerd" "pd-dmesg-save" "pdkext" "sh" "gpt" ]
      ++ [ "basename" "chown" "dirname" "echo" "false" "getopt" "hostname" "jot" "kill" "logname" "mktemp" "nice" "nohup" "passwd" "printenv" "pwd" "renice" "seq" "shlock" "sleep" "tee" "test_cmd" "true" "tsort" "uname" "yes" "uuencode" "uudecode" ]
      ++ [ "banner" "cat" "colrm" "comm" "cut" "expand" "fold" "head" "lam" "look" "nl" "paste" "rev" "split" "tail" "tr" "unexpand" "uniq" "wc" ];
  };
  userlandArm64Build = userlandBuild.override {
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    prebuiltLibSystem = libSystemArm64Build;
    # the xorg drivers build against the arm64 server sdk
    xorgDriverIncludes = [
      "${xorgNoGlxArm64Build}/usr/include/xorg"
      "${xorgNoGlxArm64Build}/usr/include"
      "${lib.getDev pkgs.xorgproto}/include"
      "${xvfbPixmanArm64Build}/include/pixman-1"
    ];
  };

  # ARM64 counterpart of the x86 Wayland-only package set. Keep the no-X
  # variants here so their dependencies remain arm64 rather than accidentally
  # reusing the x86 cross builds.
  cairoArm64NoxBuild = cairoArm64Build.override {
    withX11 = false;
    xorgproto = null; libX11 = null; libXext = null; libXrender = null;
    libxcb = null; libXau = null; libXdmcp = null;
  };
  dbusArm64NoxBuild = dbusArm64Build.override {
    withX11 = false;
    libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
    xorgproto = null;
  };
  mesaArm64NoxBuild = mesaArm64Build.override {
    withX11 = false;
    libX11 = null; libXext = null; libxcb = null; libXau = null;
    libXdmcp = null; libXxf86vm = null; xorgproto = null; xtrans = null;
    libxshmfence = null;
  };
  openglFrameworkArm64NoxBuild = openglFrameworkArm64Build.override {
    withX11 = false;
    mesa = mesaArm64NoxBuild;
    libX11 = null; xorgproto = null; libXext = null;
    libxcb = null; libXau = null; libXdmcp = null;
  };
  mesaDemosArm64NoxBuild = mesaDemosArm64Build.override {
    withX11 = false;
    mesa = mesaArm64NoxBuild;
    wayland = waylandArm64Build;
    xkbcommon = xkbcommonArm64NoxBuild;
    waylandScanner = waylandScannerBuild;
    waylandProtocols = waylandProtocolsBuild;
    libX11 = null; libXext = null; libxcb = null; libXau = null;
    libXdmcp = null; xorgproto = null; xtrans = null;
  };
  libepoxyArm64NoxBuild = libepoxyArm64Build.override {
    withX11 = false;
    mesa = mesaArm64NoxBuild;
    libX11 = null; xorgproto = null;
  };
  fastfetchArm64NoxBuild = fastfetchArm64Build.override {
    withX11 = false;
    mesa = mesaArm64NoxBuild;
    openglFramework = openglFrameworkArm64NoxBuild;
    libX11 = null; libXext = null; libxcb = null;
    libXau = null; libXdmcp = null;
  };
  harfbuzzArm64NoxBuild = harfbuzzArm64Build.override {
    cairo = cairoArm64NoxBuild;
  };
  atspi2CoreArm64NoxBuild = atspi2CoreArm64Build.override {
    dbus = dbusArm64NoxBuild;
  };
  cairoGobjectArm64NoxBuild = cairoGobjectArm64Build.override {
    cairo = cairoArm64NoxBuild;
  };
  xkbcommonArm64NoxBuild = xkbcommonArm64Build.override {
    withX11 = false;
    libxcb = null; libXau = null; libXdmcp = null;
  };
  pangoArm64NoxBuild = pangoArm64Build.override {
    withX11 = false;
    cairo = cairoArm64NoxBuild;
    harfbuzz = harfbuzzArm64NoxBuild;
    libX11 = null; libxcb = null; libXext = null; libXrender = null;
    xorgproto = null;
  };
  gtk3Arm64NoxBuild = gtk3Arm64Build.override {
    withX11 = false;
    cairo = cairoArm64NoxBuild;
    cairoGobject = cairoGobjectArm64NoxBuild;
    pango = pangoArm64NoxBuild;
    harfbuzz = harfbuzzArm64NoxBuild;
    libepoxy = libepoxyArm64NoxBuild;
    mesa = mesaArm64NoxBuild;
    dbus = dbusArm64NoxBuild;
    atspi2Core = atspi2CoreArm64NoxBuild;
    xkbcommon = xkbcommonArm64NoxBuild;
    libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
    libXext = null; libXi = null; libXrender = null; libXrandr = null;
    libXfixes = null; libXcursor = null; xorgproto = null;
  };
  gtkLayerShellArm64NoxBuild = gtkLayerShellArm64Build.override {
    gtk3 = gtk3Arm64NoxBuild;
    withX11 = false;
    atspi2Core = atspi2CoreArm64NoxBuild;
    xkbcommon = xkbcommonArm64NoxBuild;
    cairo = cairoArm64NoxBuild;
    cairoGobject = cairoGobjectArm64NoxBuild;
    pango = pangoArm64NoxBuild;
    harfbuzz = harfbuzzArm64NoxBuild;
    libepoxy = libepoxyArm64NoxBuild;
    dbus = dbusArm64NoxBuild;
    mesa = mesaArm64NoxBuild;
    libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
    libXext = null; libXi = null; libXrender = null; libXrandr = null;
    libXfixes = null; libXcursor = null; xorgproto = null;
  };
  wlrootsArm64NoxBuild = wlrootsArm64Build.override {
    withXwayland = false;
    xcb = null; xcbWm = null; xwayland = null;
  };
  swayArm64NoxBuild = swayArm64Build.override {
    withXwayland = false;
    wlroots = wlrootsArm64NoxBuild;
    xcb = null; xcbWm = null;
  };
  netsurfArm64NoxBuild = netsurfArm64Build.override {
    withX11 = false;
    gtk3 = gtk3Arm64NoxBuild;
    cairo = cairoArm64NoxBuild;
    cairoGobject = cairoGobjectArm64NoxBuild;
    pango = pangoArm64NoxBuild;
    harfbuzz = harfbuzzArm64NoxBuild;
    libepoxy = libepoxyArm64NoxBuild;
    dbus = dbusArm64NoxBuild;
    atspi2Core = atspi2CoreArm64NoxBuild;
    libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
    libXext = null; libXi = null; libXrender = null; libXrandr = null;
    libXfixes = null; libXcursor = null; xorgproto = null;
  };
  librsvgArm64NoxBuild = librsvgArm64Build.override {
    deps = [
      glibArm64Build gdkPixbufArm64Build cairoArm64NoxBuild
      cairoGobjectArm64NoxBuild pangoArm64NoxBuild libxml2Arm64Build
      libcrocoArm64Build libpngArm64Build freetype2Arm64Build
      fontconfigArm64Build fribidiArm64Build harfbuzzArm64NoxBuild
      expatArm64Build pcre2Arm64Build libffiArm64Build libiconvArm64Build
      xvfbZlibArm64Build xvfbPixmanArm64Build
    ];
  };
  imageExtraPackageSetArm64Nox =
    (removeAttrs imageExtraPackageSetArm64 [
      "iceauth" "xrandr" "xrdb" "xinit" "xorg" "xwayland" "xvfb"
      "libxcvt" "xeyes" "xclock" "xcalc" "xmessage" "dillo" "xterm"
      "xkbcomp" "dmenu" "i3" "i3status" "startup-notification"
      "libX11" "libxcb" "libxcb-util" "libxcb-keysyms" "libxcb-wm"
      "libxcb-render-util" "libxcb-image" "libxcb-cursor" "xcb-util-xrm"
      "libXau" "libXdmcp" "libXext" "libXrender" "libXfixes" "libXcursor"
      "libXrandr" "libXft" "libICE" "libSM" "libXinerama" "libXres"
      "libXcomposite" "libXdamage" "libXpresent" "libxshmfence"
      "libX11-shared" "libxcb-shared" "libXau-shared" "libXdmcp-shared"
      "libXext-shared" "libXrender-shared" "libXfixes-shared" "libXi-shared"
      "libXcursor-shared" "libXrandr-shared" "libx11-locale"
      "libxfce4util" "xfconf" "libwnck" "libxfce4ui" "xfwm4"
      "libxfce4windowing" "garcon" "exo" "xfce4-session" "xfce4-panel"
      "xfdesktop" "xfce4-appfinder" "thunar" "xfce4-settings"
      "xfce4-terminal" "vte" "wine"
    ]) // {
      cairo = cairoArm64NoxBuild;
      cairo-gobject = cairoGobjectArm64NoxBuild;
      dbus = dbusArm64NoxBuild;
      fastfetch = fastfetchArm64NoxBuild;
      gtk3 = gtk3Arm64NoxBuild;
      gtk-layer-shell = gtkLayerShellArm64NoxBuild;
      harfbuzz = harfbuzzArm64NoxBuild;
      libepoxy = libepoxyArm64NoxBuild;
      librsvg = librsvgArm64NoxBuild;
      mesa = mesaArm64NoxBuild;
      mesa-demos = mesaDemosArm64NoxBuild;
      netsurf = netsurfArm64NoxBuild;
      opengl-framework = openglFrameworkArm64NoxBuild;
      pango = pangoArm64NoxBuild;
      sway = swayArm64NoxBuild;
      wlroots = wlrootsArm64NoxBuild;
      xkbcommon = xkbcommonArm64NoxBuild;
    };
  # licheerv nano (sg2002): the minimal root plus configd, whose InterfaceNamer and
  # IPConfiguration plugins name en0 and run dhcp, and fastfetch without gl
  imageExtraPackagesArm64Sg2002 = [
    zshArm64Build libiconvArm64Build toyboxArm64Build
    securityArm64Build symptomReporterArm64Build systemConfigurationArm64Build
    fastfetchNoGLArm64Build
  ];
  imageExtraPackagesArm64Nox = [
    zshArm64Build libiconvArm64Build toyboxArm64Build asmjitTestArm64Build
  ] ++ lib.attrValues imageExtraPackageSetArm64Nox;
  userlandArm32Bcm2835Build = userlandBuild.override {
    pname = "puredarwin-userland-arm32-bcm2835";
    puredarwinArch = "armv6";
    compilerRt = compilerRtArmv6Build;
    prebuiltLibSystem = libSystemArmv6Build;
    buildTargets = [
      "sw_vers" "dmesg" "reboot" "halt" "poweroff"
      "echo" "false" "hostname" "kill" "printenv" "pwd" "sleep"
      "test_cmd" "true" "uname" "cat" "head" "tail" "wc"
    ];
    installUserlandTargetsOnly = true;
    # The ARMv6 bootstrap image does not build Xorg input/video modules.
    # Keeping the generic include path here unnecessarily pulls Mesa/Xorg into
    # the closure and can exhaust the build host before userland is packaged.
    xorgDriverIncludes = null;
    extraCmakeFlags = [
      "-DCMAKE_OSX_ARCHITECTURES=armv6"
      "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
    ];
  };
  kernelArm64Build = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm64";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "RELEASE";
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
  };
  kernelArm64VirtBuild = boardKernels.virt.release;
  kernelArm64VirtDebugBuild = boardKernels.virt.debug;
  # VIRT with mixed core types (Orange Pi Zero 4 A733: A55 E cluster, A76 P cluster), AMP scheduling
  kernelArm64VirtAmpBuild = boardKernels.virtamp.release;
  kernelArm64Bcm2837Build = boardKernels.bcm2837.release;
  kernelArm64Bcm2837DebugBuild = boardKernels.bcm2837.debug;
  # VIRT with 4KB pages on a Cortex-A53 model (see MakeInc VIRT4K), the SUN50I
  # page-table geometry where QEMU can single-step it
  kernelArm64Virt4kDebugBuild = boardKernels.virt4k.debug;
  kernelArm64Sun50iBuild = boardKernels.sun50i.release;
  kernelArm64Sun50iDebugBuild = boardKernels.sun50i.debug;
  # Apple A10 (T8010) - iPad 6th gen / iPhone 7, booted by pongoOS over
  # checkm8 rather than by xnu-loader. The board config, AIC and the s5l
  # and dockchannel UARTs are Apple's own in-tree code, only the machine
  # config selection differs from the QEMU virt kernel.
  kernelArm64T8010Build = boardKernels.t8010.release;
  kernelArm64T8010DebugBuild = boardKernels.t8010.debug;
  # Apple A9 (S8000) - iPhone 6s, for the Inferno emulator's s8000 machine
  kernelArm64S8000Build = boardKernels.s8000.release;
  kernelArm64S8000DebugBuild = boardKernels.s8000.debug;
  # Apple M1 (T8103) - Mac mini, for darwin-vm's qemu
  kernelArm64T8103Build = boardKernels.t8103.release;
  kernelArm64T8103DebugBuild = boardKernels.t8103.debug;
  kernelArm64Sc8280xpBuild = boardKernels.sc8280xp.release;
  kernelArm64Sc8280xpDebugBuild = boardKernels.sc8280xp.debug;
  kernelArm64Sdm670Build = boardKernels.sdm670.release;
  kernelArm32Bcm2835Build = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm32-bcm2835";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "RELEASE";
    puredarwinArch = "armv6";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [
      "-DPUREDARWIN_ARM32_MACHINE_CONFIG=BCM2835"
      "-DPUREDARWIN_KERNEL_COMPILER_RT=${compilerRtArmv6Build}/lib/libcompiler_rt.a"
    ];
  };
  kernelArm32Bcm2835DevBuild = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm32-bcm2835-dev";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "DEVELOPMENT";
    puredarwinArch = "armv6";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [
      "-DPUREDARWIN_ARM32_MACHINE_CONFIG=BCM2835"
      "-DPUREDARWIN_KERNEL_COMPILER_RT=${compilerRtArmv6Build}/lib/libcompiler_rt.a"
    ];
  };
  kernelArm32Bcm2835DebugBuild = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm32-bcm2835-debug";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "DEBUG";
    puredarwinArch = "armv6";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [
      "-DPUREDARWIN_ARM32_MACHINE_CONFIG=BCM2835"
      "-DPUREDARWIN_KERNEL_COMPILER_RT=${compilerRtArmv6Build}/lib/libcompiler_rt.a"
    ];
  };
  kextsArm32Bcm2835Build = mkPureDarwinBuild {
    pname = "puredarwin-kexts-arm32-bcm2835";
    src = kextsSource;
    buildTargets = [
      "corecrypto.kext" "pthread.kext" "ext4.kext"
      "IOStorageFamily" "Ext4FileSystemDriver.kext" "PDBcm2835SD.kext"
      "PDArmPlatformExpert"
    ];
    enableUserspace = false;
    installUserland = false;
    installKernel = false;
    installKexts = true;
    installKextNames = [
      "corecrypto.kext" "pthread.kext" "ext4.kext"
      "IOStorageFamily.kext" "Ext4FileSystemDriver.kext" "PDBcm2835SD.kext"
      "PDArmPlatformExpert.kext"
    ];
    puredarwinArch = "armv6";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [
      "-DPUREDARWIN_ARM32_MACHINE_CONFIG=BCM2835"
      "-DPUREDARWIN_KERNEL_COMPILER_RT=${compilerRtArmv6Build}/lib/libcompiler_rt.a"
    ];
  };
  kextsArm64Build = mkPureDarwinBuild ({
    pname = "puredarwin-kexts-arm64";
    src = kextsOnlySource;
    buildTargets = [
      "IOPCIFamily" "IOStorageFamily" "IOCDStorageFamily.kext"
      "IODVDStorageFamily.kext" "IOBDStorageFamily.kext" "IOVirtIOFamily.kext"
      "IOVirtIONet.kext" "IONetworkingFamily.kext" "IOHIDFamily.kext"
      "RavynAHCIPort.kext" "ext4.kext" "Ext4FileSystemDriver.kext"
      "ApfsFileSystemDriver.kext"
      "AppleFileSystemDriver.kext" "corecrypto.kext" "pthread.kext" "amfi.kext"
      "Sandbox.kext" "PDWatchdog.kext" "PDIOSurface.kext" "PDDiskImages2.kext" "PDLifs.kext" "PDSerialHID.kext" "PDKeyStore.kext"
      "PDArmPlatformExpert" "PDArmPCI" "PDEcamPCI"
      # Arch-neutral drivers, matching what x86 builds: filesystems,
      # USB, the rest of VirtIO, and the remaining storage families.
      "msdosfs.kext" "apfs.kext" "hfs.kext" "HFSEncodings.kext"
      "IOUSBFamily" "AppleUSBEHCI.kext" "AppleUSBOHCI.kext"
      "IOUSBCompositeDriver.kext"
      "IOUSBHIDDriver.kext" "AppleUSBMergeNub.kext"
      "RavynXHCIPort.kext" "IOVirtIOGPU.kext" "IOVirtIOBlock.kext"
      "IOGraphicsFamily.kext" "IOGOPFramebuffer.kext" "PDVirtualFramebuffer.kext"
      "IONVMEFamily.kext"
      "RavynHDAudio.kext" "PDE1000.kext" "PDRealtek8111.kext"
      "PDFBSDem.kext" "PDFBSDigc.kext" "PDFBSDrge.kext"
      "IO80211Family.kext" "PDQwx.kext"
      "PDBcm2835SD.kext" "PDTegraSDMMC.kext" "PDSg2002SD.kext" "PDSg2002Eth.kext"
      "PDI2CFamily.kext" "PDQcomTLMM.kext" "PDQcomGeniI2C.kext" "PDGadgetFamily.kext" "PDQcomUSB.kext" "PDTegraUSB.kext" "PDI2CHID.kext"
      "PDPowerVR.kext" "PDSun60iDisplay.kext" "PDTegraDisplay.kext" "PDTegraXUSB.kext"
      # the bundle ids Inferno's s8000 kernel patcher looks up
      "PDStubAppleMobileFileIntegrity.kext" "PDStubAppleImage4.kext" "PDStubImage4.kext" "PDStubAppleFirmwareUpdateKext.kext" "PDStubAppleS5L8960XWatchDogTimer.kext"
    ];
    enableUserspace = false;
    installUserland = false;
    installKernel = false;
    installKexts = true;
    installKextNames = [
      "PDStubAppleMobileFileIntegrity.kext" "PDStubAppleImage4.kext" "PDStubImage4.kext" "PDStubAppleFirmwareUpdateKext.kext" "PDStubAppleS5L8960XWatchDogTimer.kext"
      "PDBcm2835SD.kext" "PDTegraSDMMC.kext" "PDSg2002SD.kext" "PDSg2002Eth.kext"
      "PDI2CFamily.kext" "PDQcomTLMM.kext" "PDQcomGeniI2C.kext" "PDGadgetFamily.kext" "PDQcomUSB.kext" "PDTegraUSB.kext" "PDI2CHID.kext"
      "PDPowerVR.kext" "PDSun60iDisplay.kext" "PDTegraDisplay.kext" "PDTegraXUSB.kext"
      "IOPCIFamily.kext" "IOStorageFamily.kext" "IOCDStorageFamily.kext"
      "IODVDStorageFamily.kext" "IOBDStorageFamily.kext" "IOVirtIOFamily.kext"
      "IOVirtIONet.kext" "IONetworkingFamily.kext" "IOHIDFamily.kext"
      "RavynAHCIPort.kext" "ext4.kext" "Ext4FileSystemDriver.kext"
      "ApfsFileSystemDriver.kext"
      "AppleFileSystemDriver.kext" "corecrypto.kext" "pthread.kext" "amfi.kext"
      "Sandbox.kext" "PDWatchdog.kext" "PDIOSurface.kext" "PDDiskImages2.kext" "PDLifs.kext" "PDSerialHID.kext" "PDKeyStore.kext"
      "PDArmPlatformExpert.kext" "PDArmPCI.kext" "PDEcamPCI.kext"
      "msdosfs.kext" "apfs.kext" "hfs.kext" "HFSEncodings.kext"
      "IOUSBFamily.kext" "AppleUSBEHCI.kext" "AppleUSBOHCI.kext"
      "IOUSBCompositeDriver.kext"
      "IOUSBHIDDriver.kext" "AppleUSBMergeNub.kext"
      "RavynXHCIPort.kext" "IOVirtIOGPU.kext" "IOVirtIOBlock.kext"
      "IOGraphicsFamily.kext" "IOGOPFramebuffer.kext" "PDVirtualFramebuffer.kext"
      "IONVMEFamily.kext"
      "RavynHDAudio.kext" "PDE1000.kext" "PDRealtek8111.kext"
      "PDFBSDem.kext" "PDFBSDigc.kext" "PDFBSDrge.kext"
      "IO80211Family.kext" "PDQwx.kext"
    ];
    enableIOGraphicsFamily = true;
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
  } // kextsOnlyArgs xnuHeadersArm64Build);
  # ARM64 minimal mirrors the x86 minimal split: native libSystem,
  # launchd/launchctl, and the CLI userland are present alongside the
  # ARM64 kernel and kexts. Keep the kernel payload separate so this
  # composition cannot accidentally pull the x86 kernel into the ARM
  # image.
  splitBaseSystemArm64VirtMinimal = pkgs.runCommand "puredarwin-basesystem-arm64-virt-minimal-0.1" { } ''
    mkdir -p "$out"
    cp -a ${libSystemArm64Build}/. "$out/"
    chmod -R u+w "$out"
    # launchd is dynamically linked against the native framework and
    # runtime libraries. Keep these in the minimal image so dyld can
    # resolve launchd before any service jobs are submitted.
    cp -a ${icuCoreArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libcxxabiDylibArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libcxxDylibArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libobjcArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${coreFoundationArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${foundationArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${iokitArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${launchdArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${launchctlArm64Build}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
    cp -a ${userlandArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kernelArm64VirtDebugBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kextsArm64Build}/. "$out/"
    chmod -R u+w "$out"
  '';

  # licheerv nano: the minimal base with a userland that skips the xorg drivers, so a kernel
  # edit does not rebuild llvm, mesa and xorg, and the sun50i kernel the board actually runs
  splitBaseSystemArm64Sg2002 = pkgs.runCommand "puredarwin-basesystem-arm64-sg2002-0.1" { } ''
    mkdir -p "$out"
    cp -a ${libSystemArm64Build}/. "$out/"
    chmod -R u+w "$out"
    # launchd links the native frameworks and runtime libraries dynamically,
    # dyld has to resolve them before any job is submitted
    cp -a ${icuCoreArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libcxxabiDylibArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libcxxDylibArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libobjcArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${coreFoundationArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${foundationArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${iokitArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${launchdArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${launchctlArm64Build}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
    cp -a ${userlandArm64Sg2002Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kernelArm64Sun50iDebugBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kextsArm64Build}/. "$out/"
    chmod -R u+w "$out"
  '';

  splitBaseSystemArm64VirtMinimalRelease = pkgs.runCommand "puredarwin-basesystem-arm64-virt-minimal-release-0.1" { } ''
    mkdir -p "$out"
    cp -a ${libSystemArm64Build}/. "$out/"
    chmod -R u+w "$out"
    # Keep the release image's runtime closure identical to debug.
    cp -a ${icuCoreArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libcxxabiDylibArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libcxxDylibArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libobjcArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${coreFoundationArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${foundationArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${iokitArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${launchdArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${launchctlArm64Build}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
    cp -a ${userlandArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kernelArm64VirtBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kextsArm64Build}/. "$out/"
    chmod -R u+w "$out"
  '';

  # The minimal ARM64 base already contains no X11 components. The Wayland
  # image layers its compositor and clients separately, so it can share this
  # base without introducing the full X11 split.
  splitBaseSystemArm64VirtWayland = splitBaseSystemArm64VirtMinimal;

  # The same package set as imageExtraPackageSet, resolved to the arm64
  # builds, so the two architectures ship the same userland.
  # The same package set as imageExtraPackageSet, resolved to the arm64
  # builds, so the two architectures ship the same userland.
  imageExtraPackageSetArm64 = {
    wine = wineArm64Build;
    sqlite = sqliteArm64Build;
    libjpeg = libjpegArm64Build;
    libwebp = libwebpArm64Build;
    libgcrypt = libgcryptArm64Build;
    libtasn1 = libtasn1Arm64Build;
    nghttp2 = nghttp2Arm64Build;
    libpsl = libpslArm64Build;
    gettext = gettextArm64Build;
    libxshmfence = libxshmfenceSharedArm64Build;
    nettle = nettleSharedArm64Build;
    libXinerama = xvfbLibXineramaArm64Build;
    libXres = xvfbLibXresArm64Build;
    libXcomposite = xvfbLibXcompositeArm64Build;
    libXdamage = xvfbLibXdamageArm64Build;
    libXpresent = xvfbLibXpresentArm64Build;
    iceauth = iceauthArm64Build;
    xrandr = xrandrArm64Build;
    xrdb = xrdbArm64Build;
    xinit = xinitArm64Build;
    coreServices = coreServicesArm64Build;
    iomediacheck = iomediacheckArm64Build;
    pdsurface = pdsurfaceArm64Build;
    diskArbitration = diskArbitrationArm64Build;
    libcroco = libcrocoArm64Build;
    librsvg = librsvgArm64Build;
    gnutls = gnutlsSharedArm64Build;
    libsoup = libsoupArm64Build;
    vte = vteArm64Build;
    llvm = llvmCrossArm64Build;
    libXxf86vm = xvfbLibXxf86vmArm64Build;
    wayland = waylandArm64Build;
    libgbm = libgbmArm64Build;
    jsonc = jsoncArm64Build;
    meson = mesonArm64Build;
    cmake = cmakeArm64Build;
    ninja = ninjaArm64Build;
    clang = clangCrossArm64Build;
    wlroots = wlrootsArm64Build;
    sway = swayArm64Build;
    libdrm = libdrmArm64Build;
    xwayland = xwaylandArm64Build;
    xvfb = xvfbArm64Build;
    xorg = xorgArm64Build;
    libxcvt = xvfbLibxcvtArm64Build;
    xeyes = xeyesArm64Build;
    xclock = xclockArm64Build;
    xcalc = xcalcArm64Build;
    xmessage = xmessageArm64Build;
    dillo = dilloArm64Build;
    libz-dylib = libzDylibArm64Build;
    libcurl-dylib = libcurlDylibArm64Build;
    dbus = dbusArm64Build;
    libxml2 = libxml2Arm64Build;
    at-spi2-core = atspi2CoreArm64Build;
    libepoxy = libepoxyArm64Build;
    gdk-pixbuf = gdkPixbufArm64Build;
    cairo-gobject = cairoGobjectArm64Build;
    libXrandr = xvfbLibXrandrArm64Build;
    gtk3 = gtk3Arm64Build;
    libpng = libpngArm64Build;
    libwapcaplet = libwapcapletArm64Build;
    libparserutils = libparserutilsArm64Build;
    libnsutils = libnsutilsArm64Build;
    libnsgif = libnsgifArm64Build;
    libnsbmp = libnsbmpArm64Build;
    libutf8proc = libutf8procArm64Build;
    libhubbub = libhubbubArm64Build;
    libcss = libcssArm64Build;
    libdom = libdomArm64Build;
    netsurf = netsurfArm64Build;
    xxd = xxdArm64Build;
    xz = xzArm64Build;
    xterm = xtermArm64Build;
    xkbcomp = xkbcompArm64Build;
    xkeyboard-config = xkeyboardConfigArm64Build;
    libx11-locale = xlibLocaleArm64Build;
    fonts = xvfbFontsArm64Build;
    libiconv = libiconvArm64Build;
    nano = nanoArm64Build;
    bmake = bmakeArm64Build;
    gnumake = gnumakeArm64Build;
    pkgconf = pkgconfArm64Build;
    gnum4 = gnum4Arm64Build;
    autoconf = autoconfArm64Build;
    automake = automakeArm64Build;
    bison = bisonArm64Build;
    flex = flexArm64Build;
    python = pythonArm64Build;
    #perl = perlBuild;
    zsh = zshArm64Build;
    toybox = toyboxArm64Build;
    file = fileArm64Build;
    openssl = opensslArm64Build;
    curl = curlArm64Build;
    openssh = opensshArm64Build;
    git = gitArm64Build;
    migcomDarwin = migcomDarwinArm64Build;
    ioreg = ioregArm64Build;
    xkbcommon = xkbcommonArm64Build;
    fastfetch = fastfetchArm64Build;
    corefoundation = coreFoundationArm64Build;
    icucore = icuCoreArm64Build;
    libcxxabi-dylib = libcxxabiDylibArm64Build;
    libcxx-dylib = libcxxDylibArm64Build;
    libcxx-test = libcxxTestArm64Build;
    mesa = mesaArm64Build;
    mesa-demos = mesaDemosArm64Build;
    pd-virgl-shim = pdVirglShimArm64Build;
    libobjc = libobjcArm64Build;
    foundation = foundationArm64Build;
    iokit = iokitArm64Build;
    security = securityArm64Build;
    # configd links against SymptomReporter, so it has to ship wherever
    # systemConfiguration does - x86 gets it via the same pairing.
    symptomReporter = symptomReporterArm64Build;
    systemConfiguration = systemConfigurationArm64Build;
    opengl-framework = openglFrameworkArm64Build;
    i3 = i3Arm64Build;
    i3status = i3statusShimArm64Build;
    startup-notification = startupNotificationArm64Build;
    xfconf = xfconfArm64;
    libxfce4util = libxfce4utilArm64;
    libxfce4ui = libxfce4uiArm64;
    libxfce4windowing = libxfce4windowingArm64;
    libwnck = libwnckArm64Build;
    garcon = garconArm64;
    exo = exoArm64;
    xfwm4 = xfwm4Arm64;
    xfce4-session = xfce4SessionArm64;
    xfce4-panel = xfce4PanelArm64;
    xfdesktop = xfdesktopArm64;
    xfce4-appfinder = xfce4AppfinderArm64;
    thunar = thunarArm64;
    xfce4-settings = xfce4SettingsArm64;
    xfce4-terminal = xfce4TerminalArm64;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libxcb-util = xcbUtilArm64Build;
    libxcb-keysyms = xcbKeysymsArm64Build;
    libxcb-wm = xcbWmArm64Build;
    libxcb-render-util = xcbRenderUtilArm64Build;
    libxcb-image = xcbImageArm64Build;
    libxcb-cursor = xcbCursorArm64Build;
    xcb-util-xrm = xcbXrmArm64Build;
    libev = libevArm64Build;
    pcre2 = pcre2Arm64Build;
    yajl = yajlArm64Build;
    cairo = cairoArm64Build;
    libffi = libffiArm64Build;
    glib = glibArm64Build;
    fribidi = fribidiArm64Build;
    harfbuzz = harfbuzzArm64Build;
    expat = expatArm64Build;
    fontconfig = fontconfigArm64Build;
    freetype2 = freetype2Arm64Build;
    pango = pangoArm64Build;
    libXft = libXftArm64Build;
    dmenu = dmenuArm64Build;
    zlib = xvfbZlibArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXcursor = xvfbLibXcursorArm64Build;
    libICE = xvfbLibICEArm64Build;
    libSM = xvfbLibSMArm64Build;
  };

  imageExtraPackagesArm64 = lib.attrValues imageExtraPackageSetArm64;
in
{
  # the arm64 PureDarwin package set itself
  inherit pdArm64;
  inherit
    atspi2CoreArm64Build
    autoconfArm64Build
    automakeArm64Build
    cairoArm64Build
    cairoGobjectArm64Build
    curlArm64Build
    dbusArm64Build
    dilloArm64Build
    dmenuArm64Build
    fastfetchArm64Build
    fastfetchNoGLArm64Build
    vmprobeArm64Build
    fltkArm64Build
    foundationArm64Build
    fribidiArm64Build
    gdkPixbufArm64Build
    gitArm64Build
    gtk3Arm64Build
    harfbuzzArm64Build
    i3Arm64Build
    ioregArm64Build
    libXftArm64Build
    libcssArm64Build
    libcurlDylibArm64Build
    libcxxTestArm64Build
    libdomArm64Build
    libepoxyArm64Build
    libfontencArm64Build
    libhubbubArm64Build
    libnsbmpArm64Build
    libnsgifArm64Build
    libnsutilsArm64Build
    libparserutilsArm64Build
    libutf8procArm64Build
    libwapcapletArm64Build
    libzDylibArm64Build
    mesaArm64Build
    mesaDemosArm64Build
    migcomDarwinArm64Build
    netsurfArm64Build
    openglFrameworkArm64Build
    opensshArm64Build
    pangoArm64Build
    pythonArm64Build
    securityArm64Build
    systemConfigurationArm64Build
    startupNotificationArm64Build
    xcalcArm64Build
    xcbArm64Build
    xcbCursorArm64Build
    xcbImageArm64Build
    xcbKeysymsArm64Build
    xcbRenderUtilArm64Build
    xcbUtilArm64Build
    xcbWmArm64Build
    xcbXrmArm64Build
    xclockArm64Build
    xeyesArm64Build
    xkbcommonArm64Build
    xkbcompArm64Build
    xlibArm64Build
    xmessageArm64Build
    xorgArm64Build
    xtermArm64Build
    xvfbArm64Build
    xvfbLibICEArm64Build
    xvfbLibSMArm64Build
    xvfbLibXauArm64Build
    xvfbLibXawArm64Build
    xvfbLibXcursorArm64Build
    xvfbLibXdmcpArm64Build
    xvfbLibXextArm64Build
    xvfbLibXfixesArm64Build
    xvfbLibXfont2Arm64Build
    xvfbLibXiArm64Build
    xvfbLibXkbfileArm64Build
    xvfbLibXmuArm64Build
    xvfbLibXpmArm64Build
    xvfbLibXrandrArm64Build
    xvfbLibXrenderArm64Build
    xvfbLibXtArm64Build
    xvfbLibxcvtArm64Build
    xvfbPixmanArm64Build
    pdVirglShimArm64Build
    hostOtoolArm64Build
    xkeyboardConfigArm64Build
    xlibLocaleArm64Build
    xvfbFontsArm64Build
    i3statusShimArm64Build
    libSystemArm64Build
    icuCoreArm64Build
    libcxxabiDylibArm64Build
    libcxxDylibArm64Build
    libobjcArm64Build
    coreFoundationArm64Build
    iokitArm64Build
    launchdArm64Build
    launchctlArm64Build
    mkArm64Build
    xvfbZlibArm64Build
    toyboxArm64Build
    pdadbdArm64Build
    xzArm64Build
    fileArm64Build
    opensslArm64Build
    sqliteArm64Build
    libjpegArm64Build
    libwebpArm64Build
    libgpgErrorArm64Build
    libgcryptArm64Build
    libtasn1Arm64Build
    nghttp2Arm64Build
    libpslArm64Build
    gettextArm64Build
    libxshmfenceSharedArm64Build
    nettleSharedArm64Build
    xvfbLibXineramaArm64Build
    xvfbLibXresArm64Build
    xvfbLibXcompositeArm64Build
    xvfbLibXdamageArm64Build
    xvfbLibXpresentArm64Build
    iceauthArm64Build
    xrandrArm64Build
    xrdbArm64Build
    xinitArm64Build
    coreServicesArm64Build
    iomediacheckArm64Build
    pdsurfaceArm64Build
    diskArbitrationArm64Build
    libcrocoArm64Build
    librsvgArm64Build
    gnutlsSharedArm64Build
    libsoupArm64Build
    vteArm64Build
    libwnckArm64Build
    llvmCrossArm64Build
    airjitdArm64Build
    compilerRtArm64Build
    wineArm64Build
    asmjitTestArm64Build
    gtkLayerShellArm64Build
    libgbmArm64Build
    jsoncArm64Build
    libX11SharedArm64Build
    libxcbSharedArm64Build
    libXauSharedArm64Build
    libXdmcpSharedArm64Build
    libXextSharedArm64Build
    libXrenderSharedArm64Build
    libXfixesSharedArm64Build
    libXiSharedArm64Build
    libXcursorSharedArm64Build
    libXrandrSharedArm64Build
    mesonArm64Build
    cmakeArm64Build
    ninjaArm64Build
    clangCrossArm64Build
    wlrootsArm64Build
    swayArm64Build
    libdrmArm64Build
    libdrmPdpvrArm64Build
    libdrmDriArm64Build
    mesaMsmArm64Build
    mesaNouveauArm64Build
    tx1GlRootArm64Bundle
    pdgltriNouveauArm64Build
    neuwldMsmArm64Build
    neuswcMsmArm64Build
    argonMsmArm64Bundle
    tx1DesktopArm64Bundle
    footArm64Build
    iosurfaceArm64Build
    gluMsmArm64Build
    onyx2dArm64Build
    coregraphicsArm64Build
    coretextArm64Build
    corevideoMsmArm64Build
    quartzcoreMsmArm64Build
    windowserverArm64Build
    appkitMsmArm64Build
    coredataArm64Build
    cocoaMsmArm64Build
    carbonArm64Build
    pddecorMsmArm64Build
    dockMsmArm64Build
    menubarMsmArm64Build
    terminalMsmArm64Build
    finderMsmArm64Build
    calculatorMsmArm64Build
    argonDesktopMsmArm64Bundle
    pdgltriMsmArm64Build
    mesaPvrNativeToolsBuild
    mesaPowervrArm64Build
    vulkanLoaderArm64Build
    pdvktriArm64Build
    pdvkspinArm64Build
    pvrVulkanArm64
    pvrVulkanIpdArm64
    libdrmVirtgpuArm64Build
    mesaVenusArm64Build
    venusVulkanIpdArm64
    mesaPowervrX11Arm64Build
    vulkanLoaderX11Arm64Build
    vulkanToolsArm64Build
    x11PvrArm64
    xorgNoGlxArm64Build
    puredarwingopArm64Build
    xwaylandArm64Build
    libdisplayInfoArm64Build
    xvfbLibXxf86vmArm64Build
    waylandArm64Build
    bmakeArm64Build
    gnumakeArm64Build
    gnum4Arm64Build
    pkgconfArm64Build
    bisonArm64Build
    flexArm64Build
    xxdArm64Build
    nanoArm64Build
    libffiArm64Build
    expatArm64Build
    pcre2Arm64Build
    libevArm64Build
    libpngArm64Build
    freetype2Arm64Build
    fontconfigArm64Build
    libxml2Arm64Build
    yajlArm64Build
    glibArm64Build
    libiconvArm64Build
    ncursesArm64Build
    zshArm64Build
    userlandArm64Build
    kernelArm64Build
    kernelArm64VirtBuild
    kernelArm64VirtDebugBuild
    kernelArm64VirtAmpBuild
    kernelArm64T8010Build
    kernelArm64T8010DebugBuild
    kernelArm64S8000Build
    kernelArm64S8000DebugBuild
    kernelArm64T8103Build
    kernelArm64T8103DebugBuild
    kernelArm64Bcm2837Build
    kernelArm64Bcm2837DebugBuild
    kernelArm64Virt4kDebugBuild
    kernelArm64Sun50iBuild
    kernelArm64Sun50iDebugBuild
    kernelArm64Sc8280xpBuild
    kernelArm64Sc8280xpDebugBuild
    kernelArm64Sdm670Build
    kernelArm32Bcm2835Build
    kernelArm32Bcm2835DebugBuild
    kernelArm32Bcm2835DevBuild
    kextsArm32Bcm2835Build
    compilerRtArmv6Build
    libSystemArmv6Build
    libcxxabiDylibArmv6Build
    libcxxDylibArmv6Build
    libobjcArmv6Build
    icuCoreArmv6Build
    coreFoundationArmv6Build
    iokitCFStaticArmv6Build
    iokitArmv6Build
    launchdArmv6Build
    userlandArm32Bcm2835Build
    kextsArm64Build
    splitBaseSystemArm64VirtMinimal
    splitBaseSystemArm64VirtMinimalRelease
    splitBaseSystemArm64VirtWayland
    imageExtraPackageSetArm64
    imageExtraPackagesArm64
    imageExtraPackageSetArm64Nox
    imageExtraPackagesArm64Nox
    imageExtraPackagesArm64Sg2002
    splitBaseSystemArm64Sg2002
    cairoArm64NoxBuild
    cairoGobjectArm64NoxBuild
    dbusArm64NoxBuild
    fastfetchArm64NoxBuild
    gtk3Arm64NoxBuild
    gtkLayerShellArm64NoxBuild
    harfbuzzArm64NoxBuild
    libepoxyArm64NoxBuild
    librsvgArm64NoxBuild
    mesaArm64NoxBuild
    mesaDemosArm64NoxBuild
    netsurfArm64NoxBuild
    openglFrameworkArm64NoxBuild
    pangoArm64NoxBuild
    swayArm64NoxBuild
    wlrootsArm64NoxBuild
    xkbcommonArm64NoxBuild
    ;
}
