# arm64 cross-compiled package set.
#
# Every binding here targets arm64-apple-darwin20.4 via mkArm64Build, which
# injects the arm64 toolchain, target triple and arm64 libSystem into each
# package's own argument set. Packages whose derivation only differs by
# toolchain use .override on the x86 build rather than a second call.
{ lib
, pkgs
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
  autoconfArm64Build = mkArm64Build ./pkgs/base/autoconf.nix {
    autoconf = pkgs.autoconf;
  };
  automakeArm64Build = mkArm64Build ./pkgs/base/automake.nix {
    automake = pkgs.automake;
    # Host autoconf, not autoconfArm64Build: this only drives
    # automake's own build/test-generation on the Linux builder
    autoconf = pkgs.autoconf;
  };
  cairoArm64Build = mkArm64Build ./pkgs/gtk/cairo.nix {
    nativeMesonTools = nativeMesonToolsDir;
    inherit (pkgs) cairo xorgproto;
    pixman = xvfbPixmanArm64Build;
    zlib = xvfbZlibArm64Build;
    libX11 = xlibArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    freetype = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    expat = expatArm64Build;
    libpng = libpngArm64Build;
  };
  cairoGobjectArm64Build = mkArm64Build ./pkgs/gtk/cairo-gobject.nix {
    cairo = cairoArm64Build;
    cairoReal = pkgs.cairo;
    glib = glibArm64Build;
  };
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
  dilloArm64Build = mkArm64Build ./pkgs/apps/dillo.nix {
    inherit (pkgs) dillo util-macros;
    # Without this it falls back to nixpkgs' libiconv, which on Linux is
    # glibc-iconv - a host library with no arm64-darwin lib dir at all.
    libiconv = libiconvArm64Build;
    fltk = fltkArm64Build;
    openssl = opensslArm64Build;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXft = libXftArm64Build;
    libxcbcursor = xcbCursorArm64Build;
    libICE = xvfbLibICEArm64Build;
    libSM = xvfbLibSMArm64Build;
    fontconfig = fontconfigArm64Build;
    freetype2 = freetype2Arm64Build;
    expat = expatArm64Build;
    inherit (pkgs) xorgproto;
  };
  dmenuArm64Build = mkArm64Build ./pkgs/x11/dmenu.nix {
    inherit (pkgs) dmenu;
    inherit (pkgs) xorgproto;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXft = libXftArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    freetype2 = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    expat = expatArm64Build;
  };
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
  fltkArm64Build = mkArm64Build ./pkgs/apps/fltk.nix {
    inherit (pkgs) fltk_1_3 util-macros;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXft = libXftArm64Build;
    libxcbcursor = xcbCursorArm64Build;
    libICE = xvfbLibICEArm64Build;
    libSM = xvfbLibSMArm64Build;
    fontconfig = fontconfigArm64Build;
    freetype2 = freetype2Arm64Build;
    expat = expatArm64Build;
    inherit (pkgs) xorgproto;
  };
  foundationArm64Build = mkArm64Build ./pkgs/apple/foundation.nix {
    libobjc = libobjcArm64Build;
    corefoundation = coreFoundationArm64Build;
    src = "${foundationSource}/src/Libraries/Foundation";
  };
  fribidiArm64Build = mkArm64Build ./pkgs/gtk/fribidi.nix {
    inherit (pkgs) fribidi;
  };
  gdkPixbufArm64Build = mkArm64Build ./pkgs/gtk/gdk-pixbuf.nix {
    nativeMesonTools = nativeMesonToolsDir;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
    libpng = libpngArm64Build;
    inherit (pkgs) gdk-pixbuf meson ninja python3;
  };
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
  harfbuzzArm64Build = mkArm64Build ./pkgs/gtk/harfbuzz.nix {
    inherit (pkgs) harfbuzz;
    freetype = freetype2Arm64Build;
    icu = icuCoreArm64Build;
    glib = glibArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    libiconv = libiconvArm64Build;
    cairo = cairoArm64Build;
    pixman = xvfbPixmanArm64Build;
    libpng = libpngArm64Build;
    zlib = xvfbZlibArm64Build;
    expat = expatArm64Build;
    fontconfig = fontconfigArm64Build;
  };
  i3Arm64Build = mkArm64Build ./pkgs/x11/i3.nix {
    inherit (pkgs) i3;
    inherit (pkgs) xorgproto;
    startup-notification = startupNotificationArm64Build;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libxcb-util = xcbUtilArm64Build;
    libxcb-keysyms = xcbKeysymsArm64Build;
    libxcb-wm = xcbWmArm64Build;
    libxcb-render-util = xcbRenderUtilArm64Build;
    libxcb-image = xcbImageArm64Build;
    libxcb-cursor = xcbCursorArm64Build;
    xcb-util-xrm = xcbXrmArm64Build;
    xkbcommon = xkbcommonArm64Build;
    yajl = yajlArm64Build;
    pcre2 = pcre2Arm64Build;
    cairo = cairoArm64Build;
    pango = pangoArm64Build;
    glib = glibArm64Build;
    fribidi = fribidiArm64Build;
    harfbuzz = harfbuzzArm64Build;
    libev = libevArm64Build;
    libiconv = libiconvArm64Build;
    zlib = xvfbZlibArm64Build;
    libffi = libffiArm64Build;
    pixman = xvfbPixmanArm64Build;
    fontconfig = fontconfigArm64Build;
    freetype = freetype2Arm64Build;
    expat = expatArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libpng = libpngArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
  };
  ioregArm64Build = mkArm64Build ./pkgs/apple/ioreg.nix {
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
  };
  libXftArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXft";
    version = pkgs.libXft.version;
    src = pkgs.libXft.src;
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xvfbLibXrenderArm64Build
      freetype2Arm64Build
      fontconfigArm64Build
      expatArm64Build
    ];
    nativeDeps = [ pkgs.util-macros ];
  };
  libcssArm64Build = mkArm64Build ./pkgs/apps/libcss.nix {
    libwapcaplet = libwapcapletArm64Build;
    libparserutils = libparserutilsArm64Build;
    inherit (pkgs) libcss perl python3;
  };
  libcurlDylibArm64Build = mkArm64Build ./pkgs/base/libcurl-dylib.nix {
    zlib = xvfbZlibArm64Build;
    openssl = opensslArm64Build;
    corefoundation = coreFoundationArm64Build;
    systemConfiguration = systemConfigurationArm64Build;
    inherit (pkgs) curl;
  };
  libcxxTestArm64Build = mkArm64Build ./pkgs/apple/libcxx-test.nix {
    libcxxabiDylib = libcxxabiDylibArm64Build;
    libcxxDylib = libcxxDylibArm64Build;
  };
  libdomArm64Build = mkArm64Build ./pkgs/apps/libdom.nix {
    libwapcaplet = libwapcapletArm64Build;
    libparserutils = libparserutilsArm64Build;
    libhubbub = libhubbubArm64Build;
    expat = expatArm64Build;
    inherit (pkgs) libdom;
  };
  libepoxyArm64Build = mkArm64Build ./pkgs/gtk/libepoxy.nix {
    nativeMesonTools = nativeMesonToolsDir;
    libX11 = xlibArm64Build;
    # epoxy's generated egl headers include <EGL/eglplatform.h>, which comes
    # from Mesa; without it the EGL half of the library will not compile.
    mesa = mesaArm64Build;
    inherit (pkgs) libepoxy xorgproto meson ninja python3;
  };
  libfontencArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libfontenc";
    version = pkgs.libfontenc.version;
    src = pkgs.libfontenc.src;
    deps = [ pkgs.xorgproto xvfbZlibArm64Build ];
  };
  libhubbubArm64Build = mkArm64Build ./pkgs/apps/libhubbub.nix {
    libwapcaplet = libwapcapletArm64Build;
    libparserutils = libparserutilsArm64Build;
    inherit (pkgs) libhubbub perl gperf gnused;
  };
  libnsbmpArm64Build = mkArm64Build ./pkgs/apps/libnsbmp.nix {
    inherit (pkgs) libnsbmp;
  };
  libnsgifArm64Build = mkArm64Build ./pkgs/apps/libnsgif.nix {
    inherit (pkgs) libnsgif;
  };
  libnsutilsArm64Build = mkArm64Build ./pkgs/apps/libnsutils.nix {
    inherit (pkgs) libnsutils;
  };
  libparserutilsArm64Build = mkArm64Build ./pkgs/apps/libparserutils.nix {
    libiconv = libiconvArm64Build;
    inherit (pkgs) libparserutils perl;
  };
  libutf8procArm64Build = mkArm64Build ./pkgs/base/libutf8proc.nix {
    inherit (pkgs) libutf8proc;
  };
  libwapcapletArm64Build = mkArm64Build ./pkgs/apps/libwapcaplet.nix {
    inherit (pkgs) libwapcaplet;
  };
  libzDylibArm64Build = mkArm64Build ./pkgs/base/libz-dylib.nix {
    inherit (pkgs) zlib;
  };
  libgbmArm64Build = mkArm64Build ./pkgs/x11/libgbm.nix {
    pdsurface = pdsurfaceArm64Build;
  };
  libdrmArm64Build = mkArm64Build ./pkgs/x11/libdrm.nix {
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
    puredarwinSource = ../src/Libraries/libdrm;
    src = pkgs.libdrm.src;
    inherit (pkgs) meson ninja pkg-config python3;
  };
  xwaylandArm64Build = mkArm64Build ./pkgs/x11/xwayland.nix {
    xwayland = pkgs.xwayland;
    pixman = xvfbPixmanArm64Build;
    xorgproto = pkgs.xorgproto;
    xtrans = pkgs.xtrans;
    xlib = xlibArm64Build;
    xcb = xcbArm64Build;
    libXfont2 = xvfbLibXfont2Arm64Build;
    libxkbfile = xvfbLibXkbfileArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXrandr = xvfbLibXrandrArm64Build;
    libXres = xvfbLibXresArm64Build;
    libXcomposite = xvfbLibXcompositeArm64Build;
    libXdamage = xvfbLibXdamageArm64Build;
    libxshmfence = libxshmfenceSharedArm64Build;
    zlib = xvfbZlibArm64Build;
    freetype2 = freetype2Arm64Build;
    libfontenc = libfontencArm64Build;
    xvfbZlib = xvfbZlibArm64Build;
    libxcvt = xvfbLibxcvtArm64Build;
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    xkbcommon = xkbcommonArm64Build;
    xkbcomp = xkbcompArm64Build;
    xkeyboardConfig = xkeyboardConfigArm64Build;
    openssl = opensslArm64Build;
    mesaGlHeaders = pkgs.mesa-gl-headers;
    mesa = mesaArm64Build;
    libepoxy = libepoxyArm64Build;
    libdrm = libdrmArm64Build;
  };
  mesonArm64Build = mkArm64Build ./pkgs/base/meson.nix {
    python = pythonArm64Build;
    inherit (pkgs) meson;
  };
  cmakeArm64Build = mkArm64Build ./pkgs/base/cmake.nix {
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    corefoundation = coreFoundationArm64Build;
    libcurlDylib = libcurlDylibArm64Build;
    inherit (pkgs) cmake ninja;
  };
  ninjaArm64Build = mkArm64Build ./pkgs/base/ninja.nix {
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    inherit (pkgs) ninja;
  };
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
    # Only version and src are taken from it. nixpkgs' top-level `wine` is
    # winePackages.full, which pulls in pkgsi686Linux and cannot be evaluated
    # on a non-x86 host; wine64 has the same version and src derivation.
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
  jsoncArm64Build = mkArm64Build ./pkgs/apps/json-c.nix {
    src = pkgs.json_c.src;
    inherit (pkgs) cmake ninja pkg-config;
  };
  wlrootsArm64Build = mkArm64Build ./pkgs/wayland/wlroots.nix {
    nativeMesonTools = nativeMesonToolsDir;
    iokit = iokitArm64Build;
    iokitHeaders = iokitCFStaticArm64Build;
    pdgopSource = ../src/Libraries/PDGOP;
    pdVirglShim = pdVirglShimArm64Build;
    libdrm = libdrmArm64Build;
    pixman = xvfbPixmanArm64Build;
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    xkbcommon = xkbcommonArm64Build;
    xcb = xcbArm64Build;
    xcbWm = xcbWmArm64Build;
    xwayland = xwaylandArm64Build;
    pdsurface = pdsurfaceArm64Build;
    src = ../src/ThirdParty/wlroots;
  };
  swayArm64Build = mkArm64Build ./pkgs/wayland/sway.nix {
    nativeMesonTools = nativeMesonToolsDir;
    cairo = cairoArm64Build;
    fribidi = fribidiArm64Build;
    freetype = freetype2Arm64Build;
    glib = glibArm64Build;
    harfbuzz = harfbuzzArm64Build;
    jsonc = jsoncArm64Build;
    libdrm = libdrmArm64Build;
    pango = pangoArm64Build;
    pcre2 = pcre2Arm64Build;
    pixman = xvfbPixmanArm64Build;
    wayland = waylandArm64Build;
    waylandProtocols = waylandProtocolsBuild;
    waylandScanner = waylandScannerBuild;
    wlroots = wlrootsArm64Build;
    xkbcommon = xkbcommonArm64Build;
    xcb = xcbArm64Build;
    xcbWm = xcbWmArm64Build;
    src = ../src/ThirdParty/sway;
  };
  libdisplayInfoArm64Build = mkArm64Build ./pkgs/mesa/libdisplay-info.nix {
    nativeMesonTools = nativeMesonToolsDir;
    inherit (pkgs) libdisplay-info hwdata;
  };
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
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    nativeMesonTools = nativeMesonToolsDir;
    llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
    llvmVersion = pkgs.llvmPackages_21.llvm.version;
    nativeTblgen = "${pkgs.llvmPackages_21.llvm}/bin/llvm-tblgen";
    nativeLlvmConfig = "${pkgs.llvmPackages_21.llvm.dev}/bin/llvm-config";
  };
  xvfbLibXxf86vmArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXxf86vm";
    inherit (pkgs.libXxf86vm) version src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXextArm64Build ];
  };
  waylandArm64Build = mkArm64Build ./pkgs/wayland/wayland.nix {
    libffi = libffiArm64Build;
    # wayland-scanner and wayland-protocols are build-host artifacts (a code
    # generator and a set of XML protocol files), so both architectures share
    # the one instance rather than cross-building a second copy.
    waylandScanner = waylandScannerBuild;
    src = ../src/ThirdParty/wayland;
  };
  gluArm64Build = mkArm64Build ./pkgs/mesa/glu.nix {
    nativeMesonTools = nativeMesonToolsDir;
    libcxxDylib = libcxxDylibArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
    mesa = mesaArm64Build;
    inherit (pkgs) meson ninja pkg-config;
  };
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
    inherit (pkgs) meson ninja pkg-config python3 bison flex xorgproto xtrans;
  };
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
    src = ../src/Libraries/OpenGL;
  };
  opensshArm64Build = mkArm64Build ./pkgs/base/openssh.nix {
    openssh = pkgs.openssh;
    openssl = opensslArm64Build;
    zlib = xvfbZlibArm64Build;
  };
  pangoArm64Build = mkArm64Build ./pkgs/gtk/pango.nix {
    nativeMesonTools = nativeMesonToolsDir;
    inherit (pkgs) pango;
    glib = glibArm64Build;
    fribidi = fribidiArm64Build;
    harfbuzz = harfbuzzArm64Build;
    cairo = cairoArm64Build;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
    pixman = xvfbPixmanArm64Build;
    libxcb = xcbArm64Build;
    fontconfig = fontconfigArm64Build;
    freetype = freetype2Arm64Build;
    expat = expatArm64Build;
    libX11 = xlibArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    inherit (pkgs) xorgproto;
    libpng = libpngArm64Build;
  };
  pythonArm64Build = mkArm64Build ./pkgs/base/python.nix {
    python3 = pkgs.python3;
    zlib = xvfbZlibArm64Build;
    openssl = opensslArm64Build;
    libffi = libffiArm64Build;
  };
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
  coreServicesArm64Build = mkArm64Build ./pkgs/apple/coreservices.nix {
    src = coreServicesSource;
  };
  iomediacheckArm64Build = mkArm64Build ./pkgs/apple/iomediacheck.nix {
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
    iokitHeaders = iokitCFStaticArm64Build;
  };
  pdsurfaceArm64Build = mkArm64Build ./pkgs/x11/pdsurface.nix {
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
  };
  diskArbitrationArm64Build =
    if isDarwin then null else (mkPureDarwinBuild {
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
  securityArm64Build = mkArm64Build ./pkgs/apple/security.nix {
    pname = "puredarwin-security-arm64";
    puredarwinArch = "arm64";
    inherit mkPureDarwinBuild arm64CrossToolchain;
    corefoundation = coreFoundationArm64Build;
    libobjc = libobjcArm64Build;
    foundation = foundationArm64Build;
    sqlite = sqliteArm64Build;
    src = securitySource;
  };
  # IPConfiguration's report_symptoms.c needs this; the x86 build cannot be
  # reused, it is a dylib of the wrong architecture.
  symptomReporterArm64Build =
    if isDarwin || symptomReporterBuild == null then null
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
  startupNotificationArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-startup-notification";
    version = pkgs.libstartup_notification.version;
    src = pkgs.libstartup_notification.src;
    deps = [ pkgs.xorgproto xlibArm64Build xcbArm64Build xcbUtilArm64Build ];
    configureFlags = [
      "--x-includes=${lib.getDev xlibArm64Build}/include"
      "--x-libraries=${xlibArm64Build}/lib"
    ];
    preConfigureExtra = ''
      export lf_cv_sane_realloc=yes
    '';
    postPatchExtra = ''
      sed -i 's/^SUBDIRS=libsn test doc/SUBDIRS=libsn/' Makefile.in
    '';
  };
  xcalcArm64Build = mkArm64Build ./pkgs/x11/xcalc.nix {
    xcalc = pkgs.xcalc;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXmu = xvfbLibXmuArm64Build;
    libXt = xvfbLibXtArm64Build;
    libXaw = xvfbLibXawArm64Build;
    libICE = xvfbLibICEArm64Build;
    libSM = xvfbLibSMArm64Build;
    inherit (pkgs) xorgproto;
  };
  xcbArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb";
    version = pkgs.libxcb.version;
    src = pkgs.libxcb.src;
    deps = [
      pkgs.xorgproto
      xvfbLibXauArm64Build
      xvfbLibXdmcpArm64Build
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
  };
  xcbCursorArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-cursor";
    version = pkgs.libxcb-cursor.version;
    src = pkgs.libxcb-cursor.src;
    deps = [
      pkgs.xorgproto
      xcbArm64Build
      xcbUtilArm64Build
      xcbKeysymsArm64Build
      xcbImageArm64Build
      xcbRenderUtilArm64Build
    ];
    nativeDeps = [ pkgs.m4 ];
  };
  xcbImageArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-image";
    version = pkgs.libxcb-image.version;
    src = pkgs.libxcb-image.src;
    deps = [ pkgs.xorgproto xcbArm64Build xcbUtilArm64Build xcbRenderUtilArm64Build ];
    postPatchExtra = ''
      sed -i 's/^SUBDIRS = image test/SUBDIRS = image/' Makefile.in
    '';
  };
  xcbKeysymsArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-keysyms";
    version = pkgs.libxcb-keysyms.version;
    src = pkgs.libxcb-keysyms.src;
    deps = [ pkgs.xorgproto xcbArm64Build xcbUtilArm64Build ];
  };
  xcbRenderUtilArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-render-util";
    version = pkgs.libxcb-render-util.version;
    src = pkgs.libxcb-render-util.src;
    deps = [ pkgs.xorgproto xcbArm64Build xcbUtilArm64Build ];
  };
  xcbUtilArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-util";
    version = pkgs.libxcb-util.version;
    src = pkgs.libxcb-util.src;
    deps = [ pkgs.xorgproto xcbArm64Build ];
  };
  xcbWmArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxcb-wm";
    version = pkgs.libxcb-wm.version;
    src = pkgs.libxcb-wm.src;
    deps = [ pkgs.xorgproto xcbArm64Build xcbUtilArm64Build ];
    nativeDeps = [ pkgs.m4 ];
  };
  xcbXrmArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-xcb-util-xrm";
    version = pkgs.xcbutilxrm.version;
    src = pkgs.xcbutilxrm.src;
    deps = [ pkgs.xorgproto xlibArm64Build xcbArm64Build xcbUtilArm64Build ];
    nativeDeps = [ pkgs.m4 pkgs.util-macros ];
    configureFlags = [
      "--disable-devel-docs"
    ];
  };
  xclockArm64Build = mkArm64Build ./pkgs/x11/xclock.nix {
    xclock = pkgs.xclock;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXmu = xvfbLibXmuArm64Build;
    libXt = xvfbLibXtArm64Build;
    libXaw = xvfbLibXawArm64Build;
    libXft = libXftArm64Build;
    libxkbfile = xvfbLibXkbfileArm64Build;
    freetype2 = freetype2Arm64Build;
    fontconfig = fontconfigArm64Build;
    expat = expatArm64Build;
    libICE = xvfbLibICEArm64Build;
    libSM = xvfbLibSMArm64Build;
    inherit (pkgs) xorgproto;
  };
  xeyesArm64Build = mkArm64Build ./pkgs/x11/xeyes.nix {
    xeyes = pkgs.xeyes;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXi = xvfbLibXiArm64Build;
    libXrender = xvfbLibXrenderArm64Build;
    libXfixes = xvfbLibXfixesArm64Build;
    libXmu = xvfbLibXmuArm64Build;
    libXt = xvfbLibXtArm64Build;
    libICE = xvfbLibICEArm64Build;
    libSM = xvfbLibSMArm64Build;
    inherit (pkgs) xorgproto;
  };
  xkbcommonArm64Build = mkArm64Build ./pkgs/x11/xkbcommon.nix {
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    # Without this, callPackage resolves libxml2 from nixpkgs and the link
    # picks up the build host's native x86 archive.
    libxml2 = libxml2Arm64Build;
    xkeyboard-config = xkeyboardConfigArm64Build;
  };
  xkbcompArm64Build = mkArm64Build ./pkgs/x11/xvfb-xkbcomp.nix {
    inherit (pkgs) xkbcomp xorgproto;
    libX11 = xlibArm64Build;
    libxkbfile = xvfbLibXkbfileArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libxcb = xcbArm64Build;
  };
  xlibArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libX11";
    version = pkgs.libX11.version;
    src = pkgs.libX11.src;
    deps = [
      pkgs.xorgproto
      pkgs.xtrans
      xcbArm64Build
      xvfbLibXauArm64Build
      xvfbLibXdmcpArm64Build
    ];
    configureFlags = [
      "--disable-specs"
      "--enable-xlocaledir"
    ];
  };
  xmessageArm64Build = mkArm64Build ./pkgs/x11/xmessage.nix {
    xmessage = pkgs.xmessage;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXmu = xvfbLibXmuArm64Build;
    libXt = xvfbLibXtArm64Build;
    libXaw = xvfbLibXawArm64Build;
    libICE = xvfbLibICEArm64Build;
    libSM = xvfbLibSMArm64Build;
    inherit (pkgs) xorgproto;
  };
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
  xtermArm64Build = mkArm64Build ./pkgs/x11/xterm.nix {
    xterm = pkgs.xterm;
    libX11 = xlibArm64Build;
    libxcb = xcbArm64Build;
    libXau = xvfbLibXauArm64Build;
    libXdmcp = xvfbLibXdmcpArm64Build;
    libICE = xvfbLibICEArm64Build;
    libSM = xvfbLibSMArm64Build;
    libXt = xvfbLibXtArm64Build;
    libXext = xvfbLibXextArm64Build;
    libXmu = xvfbLibXmuArm64Build;
    libXpm = xvfbLibXpmArm64Build;
    libXaw = xvfbLibXawArm64Build;
    inherit (pkgs) xorgproto;
    ncurses = ncursesArm64Build;
  };
  xvfbArm64Build = mkArm64Build ./pkgs/x11/xvfb.nix {
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
  };
  xvfbLibICEArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libICE";
    version = pkgs.libICE.version;
    src = pkgs.libICE.src;
    deps = [ pkgs.xorgproto pkgs.xtrans ];
    preConfigureExtra = ''
      export ac_cv_func_arc4random_buf=yes
    '';
  };
  xvfbLibSMArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libSM";
    version = pkgs.libSM.version;
    src = pkgs.libSM.src;
    deps = [ pkgs.xorgproto pkgs.xtrans xvfbLibICEArm64Build ];
    configureFlags = [
      "--without-libuuid"
    ];
  };
  xvfbLibXauArm64Build = mkArm64Build ./pkgs/x11/xvfb-stub-lib.nix {
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
  xvfbLibXawArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXaw";
    version = pkgs.libXaw.version;
    src = pkgs.libXaw.src;
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xvfbLibXextArm64Build
      xvfbLibXmuArm64Build
      xvfbLibXpmArm64Build
      xvfbLibXtArm64Build
      xvfbLibSMArm64Build
      xvfbLibICEArm64Build
    ];
    preConfigureExtra = ''
      export CFLAGS="$CFLAGS -include limits.h"
    '';
    postInstallExtra = ''
      ln -sf libXaw7.a $out/lib/libXaw.a
    '';
  };
  xvfbLibXcursorArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXcursor";
    version = pkgs.libXcursor.version;
    src = pkgs.libXcursor.src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXfixesArm64Build xvfbLibXrenderArm64Build ];
    postInstallExtra = ''
      mkdir -p .libXcursor-dylib
      (
        cd .libXcursor-dylib
        ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar x "$out/lib/libXcursor.a"
        ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
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
  xvfbLibXextArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXext";
    version = pkgs.libXext.version;
    src = pkgs.libXext.src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXauArm64Build ];
  };
  xvfbLibXfixesArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXfixes";
    version = pkgs.libXfixes.version;
    src = pkgs.libXfixes.src;
    deps = [ pkgs.xorgproto xlibArm64Build ];
  };
  xvfbLibXfont2Arm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXfont2";
    version = pkgs.libxfont_2.version;
    src = pkgs.libxfont_2.src;
    deps = [
      pkgs.xorgproto
      pkgs.xtrans
      xvfbZlibArm64Build
      freetype2Arm64Build
      libfontencArm64Build
    ];
    configureFlags = [
      "--disable-devel-docs"
    ];
  };
  xvfbLibXiArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXi";
    version = pkgs.libXi.version;
    src = pkgs.libXi.src;
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xvfbLibXextArm64Build
      xvfbLibXfixesArm64Build
    ];
    configureFlags = [
      "--disable-malloc0returnsnull"
    ];
  };
  xvfbLibXkbfileArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libxkbfile";
    version = pkgs.libxkbfile.version;
    src = pkgs.libxkbfile.src;
    deps = [ pkgs.xorgproto xlibArm64Build ];
  };
  xvfbLibXmuArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXmu";
    version = pkgs.libXmu.version;
    src = pkgs.libXmu.src;
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xvfbLibXextArm64Build
      xvfbLibXtArm64Build
      xvfbLibSMArm64Build
      xvfbLibICEArm64Build
    ];
  };
  xvfbLibXpmArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXpm";
    version = pkgs.libXpm.version;
    src = pkgs.libXpm.src;
    deps = [ pkgs.xorgproto xlibArm64Build ];
  };
  xvfbLibXrandrArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXrandr";
    version = pkgs.libXrandr.version;
    src = pkgs.libXrandr.src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXrenderArm64Build xvfbLibXextArm64Build ];
  };
  xvfbLibXrenderArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXrender";
    version = pkgs.libXrender.version;
    src = pkgs.libXrender.src;
    deps = [ pkgs.xorgproto xlibArm64Build ];
  };
  xvfbLibXtArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXt";
    version = pkgs.libXt.version;
    src = pkgs.libXt.src;
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xvfbLibICEArm64Build
      xvfbLibSMArm64Build
    ];
  };
  xvfbLibxcvtArm64Build = mkArm64Build ./pkgs/x11/xvfb-libxcvt.nix {
    inherit (pkgs) libxcvt;
  };
  xvfbPixmanArm64Build = mkArm64Build ./pkgs/x11/xvfb-pixman.nix {
    inherit (pkgs) pixman;
  };
  pdVirglShimArm64Build =
  if isDarwin then null else (mkPureDarwinBuild {
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
  icuCoreArm64Build = icuCoreBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
  };
  libcxxabiDylibArm64Build = libcxxabiDylibBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
  };
  libcxxDylibArm64Build = libcxxDylibBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
  };
  libobjcArm64Build = libobjcBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
    libcxxabiDylib = libcxxabiDylibArm64Build;
  };
  coreFoundationArm64Build = coreFoundationBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
    icu = icuCoreArm64Build;
    libobjc = libobjcArm64Build;
  };
  # The static archive this force-loads has to be the arm64 one too. Overriding
  # only the toolchain leaves it pulling the x86_64 libIOKitCF.a, whose objects
  # the linker skips as wrong-architecture - producing a valid, and completely
  # empty, arm64 dylib rather than any error.
  iokitCFStaticArm64Build = iokitCFStaticBuild.override {
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
  };
  iokitArm64Build = iokitBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
    corefoundation = coreFoundationArm64Build;
    iokitCFStatic = iokitCFStaticArm64Build;
    # IOKitCF's hid.subproj has ObjC (HIDDeviceBase.m et al) subclassing
    # NSObject; without these the override keeps the x86 builds and the arm64
    # link cannot find _OBJC_CLASS_$_NSObject.
    libobjc = libobjcArm64Build;
    foundation = foundationArm64Build;
  };
  # ARMv6 (Pi Zero). launchd links CoreFoundation and IOKitCF, which in turn
  # need ICU, libobjc and libc++abi, so the whole chain gets an armv6 build.
  libcxxabiDylibArmv6Build = libcxxabiDylibBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
  };
  libobjcArmv6Build = libobjcBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
    libcxxabiDylib = libcxxabiDylibArmv6Build;
    compilerRt = compilerRtArmv6Build;
  };
  icuCoreArmv6Build = icuCoreBuild.override {
    darwinCrossToolchain = armv6CrossToolchain;
    targetTriple = "armv6-apple-darwin20.4";
    libSystem = libSystemArmv6Build;
    compilerRt = compilerRtArmv6Build;
    libcxxabiDylib = libcxxabiDylibArmv6Build;
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
  launchdArm64Build = launchdBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
  };
  launchctlArm64Build = launchctlBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
  };
  mkArm64Build = file: deps:
  if isDarwin then null else
  let
  f = import file;
  common = {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
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

  xvfbZlibArm64Build = mkArm64Build ./pkgs/x11/xvfb-zlib.nix { inherit (pkgs) zlib; };
  toyboxArm64Build = mkArm64Build ./pkgs/base/toybox.nix { zlib = xvfbZlibArm64Build; };
  xzArm64Build = mkArm64Build ./pkgs/base/xz.nix { };
  fileArm64Build = mkArm64Build ./pkgs/base/file.nix { zlib = xvfbZlibArm64Build; };
  opensslArm64Build = mkArm64Build ./pkgs/base/openssl.nix { };

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
  libjpegArm64Build = mkArm64Build ./pkgs/base/libjpeg-turbo.nix {
    inherit (pkgs) libjpeg_turbo cmake ninja;
  };
  libwebpArm64Build = mkArm64Build ./pkgs/base/libwebp.nix {
    inherit (pkgs) libwebp cmake ninja;
  };
  libgpgErrorArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libgpg-error";
    inherit (pkgs.libgpg-error) version src;
    # mkheader looks for syscfg/lock-obj-pub.<host_os>.h, i.e. darwin20.4, while
    # the tree ships the generic x86_64-apple-darwin one. Same contents, and the
    # lock object does not differ by CPU, so arm64 reuses it the same way.
    postPatchExtra = ''
      cp src/syscfg/lock-obj-pub.x86_64-apple-darwin.h \
         src/syscfg/lock-obj-pub.darwin20.4.h
    '';
    # sysutils.c calls mkdir() without including <sys/stat.h>, which newer clang
    # makes a hard error rather than an implicit decl.
    preConfigureExtra = ''
      export CFLAGS="$CFLAGS -include sys/stat.h"
    '';
    configureFlags = [
      "--disable-nls"
      "--disable-doc"
      "--disable-tests"
      "--disable-languages"
    ];
  };
  libgcryptArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libgcrypt";
    inherit (pkgs.libgcrypt) version src;
    deps = [ libgpgErrorArm64Build ];
    configureFlags = [
      "--disable-doc"
      "--disable-tests"
      "--disable-asm"
      "--with-libgpg-error-prefix=${libgpgErrorArm64Build}"
    ];
  };
  libtasn1Arm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libtasn1";
    inherit (pkgs.libtasn1) version src;
    configureFlags = [ "--disable-doc" "--disable-gtk-doc" ];
  };
  nghttp2Arm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-nghttp2";
    inherit (pkgs.nghttp2) version src;
    # Only libnghttp2 is wanted; the apps are C++ and pull in
    # libev/openssl/jansson that nothing here needs.
    configureFlags = [ "--enable-lib-only" "--disable-python-bindings" ];
  };
  libpslArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libpsl";
    inherit (pkgs.libpsl) version;
    # Upstream ships a .tar.lz, which the default unpacker cannot read.
    src = pkgs.runCommand "libpsl-src-${pkgs.libpsl.version}" { nativeBuildInputs = [ pkgs.lzip ]; } ''
      lzip -dc ${pkgs.libpsl.src} | tar -x
      mkdir -p $out
      cp -R libpsl-*/. $out/
      chmod -R u+w $out
    '';
    # No libidn2/libunistring here, so IDNA is the builtin variant.
    # python is a build-time tool: it generates the suffix tables.
    nativeDeps = [ pkgs.python3 ];
    configureFlags = [ "--disable-runtime" "--disable-builtin" "--disable-man" ];
  };
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
  xvfbLibXineramaArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXinerama";
    inherit (pkgs.libXinerama) version src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXextArm64Build ];
  };
  xvfbLibXresArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXres";
    inherit (pkgs.libXres) version src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXextArm64Build ];
  };
  xvfbLibXcompositeArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXcomposite";
    inherit (pkgs.libXcomposite) version src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXfixesArm64Build ];
  };
  xvfbLibXdamageArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXdamage";
    inherit (pkgs.libXdamage) version src;
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXfixesArm64Build ];
  };
  xvfbLibXpresentArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libXpresent";
    inherit (pkgs.libXpresent) version src;
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xvfbLibXextArm64Build
      xvfbLibXfixesArm64Build
      xvfbLibXrandrArm64Build
      xvfbLibXrenderArm64Build
    ];
  };
  iceauthArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-iceauth";
    version = "1.0.11";
    src = pkgs.fetchurl {
      url = "https://www.x.org/releases/individual/app/iceauth-1.0.11.tar.xz";
      sha256 = "sha256-nWM88NTR2Y4+8C0YZgNylYtgpnAW6Kcs0ECTqNj41Ok=";
    };
    deps = [ pkgs.xorgproto xlibArm64Build xvfbLibICEArm64Build ];
  };
  xrandrArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-xrandr";
    version = "1.5.4";
    src = pkgs.fetchurl {
      url = "https://www.x.org/releases/individual/app/xrandr-1.5.4.tar.xz";
      hash = "sha256-LK/MsqrySRpAaGdhF6DU+QqzB3JLlv/8VM0dqVN3lAA=";
    };
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xvfbLibXrandrArm64Build
      xvfbLibXrenderArm64Build
      xvfbLibXextArm64Build
      xcbArm64Build
      xvfbLibXauArm64Build
      xvfbLibXdmcpArm64Build
    ];
    preConfigureExtra = ''
      export LIBS="-lXrandr -lXrender -lXext -lX11 -lxcb -lXau -lXdmcp $LIBS"
    '';
  };
  xrdbArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-xrdb";
    version = "1.2.3";
    # release tarball: nixpkgs' src is a git checkout with no configure
    src = pkgs.fetchurl {
      url = "https://www.x.org/releases/individual/app/xrdb-1.2.3.tar.xz";
      sha256 = "sha256-yI9WAkMnjIls5PySrlpForUFoxb/pCf+VbAuXVkUxOQ=";
    };
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xvfbLibXmuArm64Build
      xvfbLibXtArm64Build
      xvfbLibXextArm64Build
      xvfbLibSMArm64Build
      xvfbLibICEArm64Build
      xcbArm64Build
      xvfbLibXauArm64Build
      xvfbLibXdmcpArm64Build
    ];
    preConfigureExtra = ''
      export LIBS="-lXmu -lXt -lXext -lX11 -lxcb -lXau -lXdmcp -lSM -lICE $LIBS"
    '';
    configureFlags = [
      "--with-cpp=/usr/bin/cpp,/bin/cpp"
    ];
  };
  xinitArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    guestPrefix = true;
    pname = "puredarwin-xinit";
    version = "1.4.4";
    src = pkgs.fetchurl {
      url = "https://www.x.org/releases/individual/app/xinit-1.4.4.tar.xz";
      sha256 = "sha256-QKR8ehZMf5gc43h7Szf35BH7QyMdzeVD1wCUB12s/vk=";
    };
    deps = [
      pkgs.xorgproto
      xlibArm64Build
      xcbArm64Build
      xvfbLibXauArm64Build
      xvfbLibXdmcpArm64Build
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
  };
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
  bmakeArm64Build = mkArm64Build ./pkgs/base/bmake.nix { };
  gnumakeArm64Build = mkArm64Build ./pkgs/base/gnumake.nix { };
  gnum4Arm64Build = mkArm64Build ./pkgs/base/gnum4.nix { };
  pkgconfArm64Build = mkArm64Build ./pkgs/base/pkgconf.nix {
    pkgconf = pkgs.pkgconf-unwrapped;
  };
  bisonArm64Build = mkArm64Build ./pkgs/base/bison.nix { };
  flexArm64Build = mkArm64Build ./pkgs/base/flex.nix { };
  xxdArm64Build = mkArm64Build ./pkgs/toolchain/xxd.nix { };
  nanoArm64Build = mkArm64Build ./pkgs/base/nano.nix {
    ncurses = ncursesArm64Build;
  };

  # Core libraries.
  libffiArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libffi";
    version = pkgs.libffi.version;
    src = pkgs.libffi.src;
    configureFlags = [
      "--disable-docs"
      "--disable-multi-os-directory"
    ];
  };
  expatArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-expat";
    version = pkgs.expat.version;
    src = pkgs.expat.src;
    configureFlags = [
      "--without-docbook"
      "--without-examples"
      "--without-tests"
    ];
  };
  pcre2Arm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
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
  };
  libevArm64Build = mkArm64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libev";
    version = pkgs.libev.version;
    src = pkgs.libev.src;
    preConfigureExtra = ''
      export ac_cv_func_poll=yes
      export ac_cv_func_select=yes
      export ac_cv_header_poll_h=yes
    '';
  };
  libpngArm64Build = mkArm64Build ./pkgs/gtk/libpng.nix {
    zlib = xvfbZlibArm64Build;
    inherit (pkgs) libpng;
  };
  freetype2Arm64Build = mkArm64Build ./pkgs/x11/xvfb-freetype.nix {
    nativeMesonTools = nativeMesonToolsDir;
    inherit (pkgs) zlib freetype;
  };
  fontconfigArm64Build = mkArm64Build ./pkgs/x11/fontconfig.nix {
    nativeMesonTools = nativeMesonToolsDir;
    inherit (pkgs) fontconfig;
    freetype = freetype2Arm64Build;
    expat = expatArm64Build;
  };
  libxml2Arm64Build = mkArm64Build ./pkgs/base/libxml2.nix {
    inherit (pkgs) libxml2 meson ninja python3 git;
  };
  yajlArm64Build = mkArm64Build ./pkgs/base/yajl.nix {
    inherit (pkgs) yajl;
  };
  glibArm64Build = mkArm64Build ./pkgs/gtk/glib.nix {
    nativeMesonTools = nativeMesonToolsDir;
    inherit (pkgs) glib;
    pcre2 = pcre2Arm64Build;
    libffi = libffiArm64Build;
    zlib = xvfbZlibArm64Build;
    libiconv = libiconvArm64Build;
  };

  libiconvArm64Build = libiconvBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
  };
  ncursesArm64Build = ncursesBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
  };
  zshArm64Build = zshBuild.override {
    darwinCrossToolchain = arm64CrossToolchain;
    targetTriple = "arm64-apple-darwin20.4";
    libSystem = libSystemArm64Build;
    ncurses = ncursesArm64Build;
  };
  userlandArm64Build = userlandBuild.override {
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    prebuiltLibSystem = libSystemArm64Build;
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
  kernelArm64VirtBuild = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm64-virt";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "RELEASE";
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=VIRT" ];
  };
  kernelArm64VirtDebugBuild = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm64-virt-debug";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "DEBUG";
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=VIRT" ];
  };
  kernelArm64Bcm2837Build = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm64-bcm2837";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "RELEASE";
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=BCM2837" ];
  };
  kernelArm64Bcm2837DebugBuild = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm64-bcm2837-debug";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "DEBUG";
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=BCM2837" ];
  };
  # Apple A10 (T8010) - iPad 6th gen / iPhone 7, booted by pongoOS over
  # checkm8 rather than by xnu-loader. The board config, AIC and the s5l
  # and dockchannel UARTs are Apple's own in-tree code; only the machine
  # config selection differs from the QEMU virt kernel.
  kernelArm64T8010Build = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm64-t8010";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "RELEASE";
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=T8010" ];
  };
  kernelArm64T8010DebugBuild = mkPureDarwinBuild {
    pname = "puredarwin-kernel-arm64-t8010-debug";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "DEBUG";
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
    extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=T8010" ];
  };
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
  kextsArm64Build = mkPureDarwinBuild {
    pname = "puredarwin-kexts-arm64";
    src = kextsSource;
    buildTargets = [
      "IOPCIFamily" "IOStorageFamily" "IOCDStorageFamily.kext"
      "IODVDStorageFamily.kext" "IOBDStorageFamily.kext" "IOVirtIOFamily.kext"
      "IOVirtIONet.kext" "IONetworkingFamily.kext" "IOHIDFamily.kext"
      "RavynAHCIPort.kext" "ext4.kext" "Ext4FileSystemDriver.kext"
      "AppleFileSystemDriver.kext" "corecrypto.kext" "pthread.kext" "amfi.kext"
      "PDArmPlatformExpert" "PDArmPCI"
      # Arch-neutral drivers, matching what x86 builds: filesystems,
      # USB, the rest of VirtIO, and the remaining storage families.
      "msdosfs.kext" "apfs.kext" "hfs.kext" "HFSEncodings.kext"
      "IOUSBFamily" "AppleUSBEHCI.kext" "AppleUSBOHCI.kext"
      "IOUSBCompositeDriver.kext"
      "IOUSBHIDDriver.kext" "AppleUSBMergeNub.kext"
      "RavynXHCIPort.kext" "IOVirtIOGPU.kext" "IOVirtIOBlock.kext"
      "IOGraphicsFamily.kext" "IOGOPFramebuffer.kext"
      "IONVMEFamily.kext"
      "RavynHDAudio.kext" "PDE1000.kext" "PDRealtek8111.kext"
      "PDBcm2835SD.kext"
    ];
    enableUserspace = false;
    installUserland = false;
    installKernel = false;
    installKexts = true;
    installKextNames = [
      "PDBcm2835SD.kext"
      "IOPCIFamily.kext" "IOStorageFamily.kext" "IOCDStorageFamily.kext"
      "IODVDStorageFamily.kext" "IOBDStorageFamily.kext" "IOVirtIOFamily.kext"
      "IOVirtIONet.kext" "IONetworkingFamily.kext" "IOHIDFamily.kext"
      "RavynAHCIPort.kext" "ext4.kext" "Ext4FileSystemDriver.kext"
      "AppleFileSystemDriver.kext" "corecrypto.kext" "pthread.kext" "amfi.kext"
      "PDArmPlatformExpert.kext" "PDArmPCI.kext"
      "msdosfs.kext" "apfs.kext" "hfs.kext" "HFSEncodings.kext"
      "IOUSBFamily.kext" "AppleUSBEHCI.kext" "AppleUSBOHCI.kext"
      "IOUSBCompositeDriver.kext"
      "IOUSBHIDDriver.kext" "AppleUSBMergeNub.kext"
      "RavynXHCIPort.kext" "IOVirtIOGPU.kext" "IOVirtIOBlock.kext"
      "IOGraphicsFamily.kext" "IOGOPFramebuffer.kext"
      "IONVMEFamily.kext"
      "RavynHDAudio.kext" "PDE1000.kext" "PDRealtek8111.kext"
    ];
    enableIOGraphicsFamily = true;
    puredarwinArch = "arm64";
    inherit arm64CrossToolchain;
  };
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
  imageExtraPackageSetArm64 = lib.optionalAttrs (!isDarwin) {
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
    kernelArm64T8010Build
    kernelArm64T8010DebugBuild
    kernelArm64Bcm2837Build
    kernelArm64Bcm2837DebugBuild
    kernelArm32Bcm2835Build
    kernelArm32Bcm2835DebugBuild
    kernelArm32Bcm2835DevBuild
    kextsArm32Bcm2835Build
    compilerRtArmv6Build
    libSystemArmv6Build
    libcxxabiDylibArmv6Build
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
