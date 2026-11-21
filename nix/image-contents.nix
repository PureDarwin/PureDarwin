# What actually goes into an image: the base system splits, the extra guest
# package set layered on top, and the host-side (Linux) tooling used to build
# and run them.
{
# the x86_64 PureDarwin package set (nix/pd/scope.nix)
  pd
, darwinCrossToolchain
, exoBuild
, fastfetchNoGLArm64Build
, vmprobeArm64Build
, foundationArm64Build
, fbdoomExternalSrc
, utilityAppBuilds ? [ ]
, iosurfaceTestsBuild ? null
, garconBuild
, imageExtraPackagesArm64
, imageExtraPackagesArm64Nox
, imageExtraPackagesArm64Sg2002
, splitBaseSystemArm64Sg2002
, iographicsBuild
, llvmCrossBuild
, mesaVenusWaylandBuild
, isDarwin
, kc-tools
, kernelArm64Build
, kernelArm64VirtBuild
, kernelArm64VirtDebugBuild
, kernelArm64VirtAmpBuild
, kernelArm64T8010Build
, kernelArm64T8010DebugBuild
, kernelArm64S8000Build
, kernelArm64S8000DebugBuild
, kernelArm64T8103Build
, kernelArm64T8103DebugBuild
, kernelArm64Bcm2837Build
, kernelArm64Bcm2837DebugBuild
, kernelArm64Sun50iBuild
, kernelArm64Sun50iDebugBuild
, kernelArm64Sc8280xpBuild
, kernelArm64Sc8280xpDebugBuild
, kernelArm64Sdm670Build
, kernelArm64Virt4kDebugBuild
, kernelArm32Bcm2835Build
, kernelArm32Bcm2835DebugBuild
, kernelArm32Bcm2835DevBuild
, kextsArm32Bcm2835Build
, compilerRtArmv6Build
, kernelBuild
, kernelDebugBuild
, kernelSource
, kextsArm64Build
, drmMsmArm64Build ? null
, drmNouveauArm64Build ? null
, nouveauFirmwareGm20b ? null
, tegra210XusbFirmware ? null
, tegra210Dtb ? null
, tx1GlRootArm64Bundle ? null
, tx1DesktopArm64Bundle ? null
, kextsBuild
, lib
, nixPortPackages
, libiconvArm64Build
, libxfce4uiBuild
, libxfce4utilBuild
, libxfce4windowingBuild
, migcomDarwinBuild
, mkPureDarwinBuild
, clangCrossBuild
, dsymutilBuild
, kcToolsGuestBuild
, nativeLd
, pkgs
, splitBaseSystemArm64VirtMinimal
, splitBaseSystemArm64VirtMinimalRelease
, splitBaseSystemArm64VirtWayland
, system
, toyboxArm64Build
, thunarBuild
, xfce4AppfinderBuild
, xfce4PanelBuild
, xfce4SessionBuild
, xfce4SettingsBuild
, xfce4TerminalBuild
, xfconfBuild
, xfdesktopBuild
, xfwm4Build
, xnu-loader
, xnuHeadersBuild
, xnuHeadersArm64Build
, xxdBuild
, asmjitTestArm64Build
, zshArm64Build
, pdadbdArm64Build ? null
# the drm drivers (amdgpu, i915) in the aux KC by default with their firmware, the GOP
# framebuffer stays the fallback when none attaches
, kcGpuBuild
, gpuDefaultPackages
, mesaDefaultBuild
}:

let
  fullBuild = mkPureDarwinBuild {
    pname = "puredarwin";
    # the whole tree for the one-shot CMake build, without the Nix files it never reads, so editing them doesn't
    # change this build
    src = lib.cleanSourceWith {
      src = ../.;
      filter = path: type:
        let rel = lib.removePrefix "${toString ../.}/" (toString path); in
        !(rel == "flake.nix" || rel == "flake.lock" || rel == "nix" || lib.hasPrefix "nix/" rel);
    };
    buildTargets = [ "xnu" "kexts" "libsystem_kernel" ];
    installUserland = false;
    installKernel = false;
    installBaseSystem = true;
    enableIOGraphicsFamily = true;
  };
  splitBaseSystem = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } (''
    mkdir -p "$out"
    cp -a ${kernelBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kextsBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.libSystem}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.userland}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.tcc}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.cctools}/. "$out/"
  '' + lib.optionalString (pd.launchd != null) ''
    chmod -R u+w "$out"
    cp -a ${pd.launchd}/. "$out/"
    chmod -R u+w "$out"
  '' + lib.optionalString (pd.launchctl != null) ''
    cp -a ${pd.launchctl}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
  ''
  + ''
    chmod -R u+w "$out"
    cp -a ${pd.bmake}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.xvfb}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.xeyes}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.xkbcomp}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.xkeyboardConfig}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.xvfbFonts}/. "$out/"
  '');
  # splitBaseSystem bakes Xvfb/xeyes/xkbcomp straight into the base, so the
  # Wayland-only image needs its own split. xkeyboard-config stays: libxkbcommon
  # reads that data directly and never shells out to xkbcomp.
  splitBaseSystemWayland = pkgs.runCommand "puredarwin-basesystem-split-wayland-0.1" { } (''
    mkdir -p "$out"
    cp -a ${kernelBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kextsBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.libSystem}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.userlandNox}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.tcc}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.cctools}/. "$out/"
  '' + lib.optionalString (pd.launchd != null) ''
    chmod -R u+w "$out"
    cp -a ${pd.launchd}/. "$out/"
    chmod -R u+w "$out"
  '' + lib.optionalString (pd.launchctl != null) ''
    cp -a ${pd.launchctl}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
  '' + ''
    chmod -R u+w "$out"
    cp -a ${pd.bmake}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.xkeyboardConfig}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.xvfbFonts}/. "$out/"
    chmod -R u+w "$out"
    # startx builds as part of the userland, but there is no X server here.
    rm -f "$out/bin/startx" "$out/usr/bin/startx"
  '');
  splitBaseSystemStripped = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } (''
    mkdir -p "$out"
    cp -a ${kernelBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kextsBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.libSystem}/. "$out/"
  '' + lib.optionalString (pd.launchd != null) ''
    chmod -R u+w "$out"
    cp -a ${pd.launchd}/. "$out/"
    chmod -R u+w "$out"
  '' + lib.optionalString (pd.launchctl != null) ''
    cp -a ${pd.launchctl}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
  '');
  splitBaseSystemStrippedDebug = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } (''
    mkdir -p "$out"
    cp -a ${kernelDebugBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kextsBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.libSystem}/. "$out/"
  '' + lib.optionalString (pd.launchd != null) ''
    chmod -R u+w "$out"
    cp -a ${pd.launchd}/. "$out/"
    chmod -R u+w "$out"
  '' + lib.optionalString (pd.launchctl != null) ''
    cp -a ${pd.launchctl}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
  '');

  # Stripped base plus the CLI userland (sw_vers, mount, virgl-smoke,
  # etc.) - a lean image that still has usable tools, without the heavy
  # tcc/cctools/X of the full base.
  # Stripped base plus the CLI userland (sw_vers, mount, virgl-smoke,
  # etc.) - a lean image that still has usable tools, without the heavy
  # tcc/cctools/X of the full base.
  splitBaseSystemMinimal = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } ''
    mkdir -p "$out"
    cp -a ${splitBaseSystemStripped}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.userland}/. "$out/"
  '';


  splitBaseSystemMinimalDebug = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } ''
    mkdir -p "$out"
    cp -a ${splitBaseSystemStrippedDebug}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${pd.userland}/. "$out/"
  '';

  imageExtraPackageSet = {
    # Multi-user Nix; image.nix adds its build users when this is present.
    nix = nixPortPackages.nix;
    wine = pd.wine;
    # winetricks and the cabextract it unpacks Microsoft's redistributables with (corefonts and the like)
    cabextract = pd.cabextract;
    winetricks = pd.winetricks;
    # Direct3D over Vulkan for Wine prefixes, set up with pd-dxvk
    dxvk = pd.dxvk;
    libX11-shared = pd.libX11SharedBuild;
    libxcb-shared = pd.libxcbSharedBuild;
    libXau-shared = pd.libXauSharedBuild;
    libXdmcp-shared = pd.libXdmcpSharedBuild;
    libXext-shared = pd.libXextSharedBuild;
    libXrender-shared = pd.libXrenderSharedBuild;
    libXfixes-shared = pd.libXfixesSharedBuild;
    libXi-shared = pd.libXiSharedBuild;
    libXcursor-shared = pd.libXcursorSharedBuild;
    libXrandr-shared = pd.libXrandrSharedBuild;
    libXxf86vm-shared = pd.libXxf86vm;
    nettle = pd."nettle-shared";
    gnutls = pd.gnutls;
    glib-networking = pd."glib-networking";
    # llvmpipe and lavapipe JIT through libLLVM at runtime.
    llvm = llvmCrossBuild;
    vulkan-loader = pd.vulkanLoader;
    vulkan-tools = pd."vulkan-tools";
    libxshmfence = pd.libxshmfence;
    xvfb = pd.xvfb;
    xorg = pd.xorg;
    xwayland = pd.xwayland;
    wayland = pd.wayland;
    wlroots = pd.wlroots;
    sway = pd.sway;
    # sway links libjson-c as a dylib, so it has to be on the image and not
    # only in the build closure.
    json-c = pd.jsonc;
    libdrm = pd.libdrm;
    pdsurface = pd.pdsurface;
    libgbm = pd.libgbm;
    libxcvt = pd.libxcvt;
    xeyes = pd.xeyes;
    xclock = pd.xclock;
    xcalc = pd.xcalc;
    xmessage = pd.xmessage;
    dillo = pd.dillo;
    libz-dylib = pd.libzDylib;
    libcurl-dylib = pd.libcurl;
    dbus = pd.dbus;
    libxml2 = pd.libxml2;
    at-spi2-core = pd.atspi2Core;
    libepoxy = pd.libepoxy;
    gdk-pixbuf = pd.gdkPixbuf;
    cairo-gobject = pd.cairoGobject;
    libXrandr = pd.libXrandr;
    gtk3 = pd.gtk3;
    gtk-layer-shell = pd.gtkLayerShell;
    libpng = pd.libpng;
    libcroco = pd.libcroco;
    librsvg = pd.librsvg;
    gettext = pd.gettext;
    #webkitgtk = webkitgtkBuild;
    libsoup = pd.libsoup;
    sqlite = pd.sqlite;
    libpsl = pd.libpsl;
    nghttp2 = pd.nghttp2;
    libgcrypt = pd.libgcrypt;
    libgpg-error = pd."libgpg-error";
    libtasn1 = pd.libtasn1;
    libjpeg = pd.libjpeg;
    libwebp = pd.libwebp;
    libwapcaplet = pd.libwapcaplet;
    libparserutils = pd.libparserutils;
    libnsutils = pd.libnsutils;
    libnsgif = pd.libnsgif;
    libnsbmp = pd.libnsbmp;
    libutf8proc = pd.libutf8proc;
    libhubbub = pd.libhubbub;
    libcss = pd.libcss;
    libdom = pd.libdom;
    netsurf = pd.netsurf;
    xxd = xxdBuild;
    xz = pd.xz;
    unzip = pd.unzip;
    bzip2 = pd.bzip2;
    xterm = pd.xterm;
    xkbcomp = pd.xkbcomp;
    xkeyboard-config = pd.xkeyboardConfig;
    libx11-locale = pd.xlibLocale;
    fonts = pd.xvfbFonts;
    libiconv = pd.libiconv;
    nano = pd.nano;
    bmake = pd.bmake;
    gnumake = pd.gnumake;
    ninja = pd.ninja;
    cmake = pd.cmake;
    clang = clangCrossBuild;
    dsymutil = dsymutilBuild;
    kc-tools-guest = kcToolsGuestBuild;
    meson = pd.meson;
    pkgconf = pd.pkgconf;
    gnum4 = pd.gnum4;
    autoconf = pd.autoconf;
    automake = pd.automake;
    bison = pd.bison;
    flex = pd.flex;
    python = pd.python;
    #perl = perlBuild;
    zsh = pd.zsh;
    # /bin/bash: Darwin software and Nix's sandbox profiles expect it.
    bash = pd.bash;
    toybox = pd.toybox;
    file = pd.file;
    openssl = pd.openssl;
    curl = pd.curl;
    openssh = pd.openssh;
    git = pd.git;
    migcomDarwin = migcomDarwinBuild;
    ioreg = pd.ioreg;
    dtrace = pd.dtrace;
    xkbcommon = pd.xkbcommon;
    fastfetch = pd.fastfetch;
    corefoundation = pd.corefoundation;
    onyx2d = pd.onyx2d;
    coregraphics = pd.coregraphics;
    coretext = pd.coretext;
    # AppKit links both, without them no AppKit app loads
    quartzcore = pd.quartzcore;
    corevideo = pd.corevideo;
    coredata = pd.coredata;
    cg-screen-demo = pd."cg-screen-demo";
    pdres = pd.pdres;
    system-fonts = pd.systemFonts;
    shared-mime-info = pd."shared-mime-info";
    icucore = pd.icu;
    libcxxabi-dylib = pd.libcxxabiDylib;
    libcxx-dylib = pd.libcxxDylib;
    libcxx-test = pd.libcxxTest;
    mesa = pd.mesaBuild;
    glu = pd.gluBuild;
    mesa-demos = pd.mesaDemos;
    pd-virgl-shim = pd.pdVirglShim;
    libobjc = pd.libobjc;
    foundation = pd.foundation;
    framework-compat = pd."framework-compat";
    swift-runtime = pd.swiftRuntime;
    cocoa = pd.cocoa;
    app-frameworks = pd.appFrameworks;
    audiotoolbox = pd.audiotoolbox;
    pcmplay = pd.pcmplay;
    coreaudiod = pd.coreaudiod;
    carbon = pd.carbon;
    iosurface = pd.iosurface;
    iokit = pd.iokit;
    coreServices = pd.coreservices;
    security = pd.security;
    certificates-bundle = pd."certificates-bundle";
    symptomReporter = pd.symptomReporter;
    systemConfiguration = pd.systemConfiguration;
    diskArbitration = pd.diskArbitration;
    iomediacheck = pd.iomediacheck;
    opengl-framework = pd.openglFrameworkBuild;
    i3 = pd.i3;
    i3status = pd.i3statusShim;
    startup-notification = pd."startup-notification";
    libX11 = pd.libX11;
    libxcb = pd.libxcb;
    libxcb-util = pd."libxcb-util";
    libxcb-keysyms = pd."libxcb-keysyms";
    libxcb-wm = pd.xcbWm;
    libxcb-render-util = pd."libxcb-render-util";
    libxcb-image = pd."libxcb-image";
    libxcb-cursor = pd.libxcbcursor;
    xcb-util-xrm = pd."xcb-util-xrm";
    libev = pd.libev;
    pcre2 = pd.pcre2;
    yajl = pd.yajl;
    cairo = pd.cairo;
    libffi = pd.libffi;
    glib = pd.glib;
    fribidi = pd.fribidi;
    harfbuzz = pd.harfbuzz;
    expat = pd.expat;
    fontconfig = pd.fontconfig;
    freetype2 = pd.freetype2;
    pango = pd.pango;
    libXft = pd.libXft;
    dmenu = pd.dmenu;
    zlib = pd.zlib;
    libXau = pd.libXau;
    libXdmcp = pd.libXdmcp;
    libXext = pd.libXext;
    libXrender = pd.libXrender;
    libXfixes = pd.libXfixes;
    libXcursor = pd.libXcursor;
    libICE = pd.libICE;
    libSM = pd.libSM;
    libXinerama = pd.xvfbLibXinerama;
    libXres = pd.libXres;
    libXcomposite = pd.libXcomposite;
    libXdamage = pd.libXdamage;
    libXpresent = pd.xvfbLibXpresent;
    libxfce4util = libxfce4utilBuild;
    xfconf = xfconfBuild;
    libwnck = pd.libwnck;
    libxfce4ui = libxfce4uiBuild;
    xfwm4 = xfwm4Build;
    libdisplay-info = pd.libdisplayInfo;
    libxfce4windowing = libxfce4windowingBuild;
    garcon = garconBuild;
    exo = exoBuild;
    xfce4-session = xfce4SessionBuild;
    xfce4-panel = xfce4PanelBuild;
    xfdesktop = xfdesktopBuild;
    xfce4-appfinder = xfce4AppfinderBuild;
    thunar = thunarBuild;
    cursor-theme = pd.cursorTheme;
    icon-themes = pd.iconThemes;
    xfce4-settings = xfce4SettingsBuild;
    vte = pd.vte;
    xfce4-terminal = xfce4TerminalBuild;
    iceauth = pd.iceauth;
    xrdb = pd.xrdb;
    xrandr = pd.xrandr;
    xinit = pd.xinit;
  };

  commonPackages = {
    darwin-toolchain = darwinCrossToolchain;
    launchd = pd.launchd;
    launchctl = pd.launchctl;
    corefoundation = pd.corefoundation;
    icucore = pd.icu;
    iokit = pd.iokit;
    libobjc = pd.libobjc;
    libcxx-dylib = pd.libcxxDylib;
    libcxxabi-dylib = pd.libcxxabiDylib;
    userland = pd.userland;
    userland-nox = pd.userlandNox;
    tcc = pd.tcc;
    cctools = pd.cctools;
    libsystem = pd.libSystem;
    libSystem = pd.libSystem;
    libapfsrw = pd.libapfsrw;
    xnu-headers = xnuHeadersBuild;
    xnu-headers-arm64 = xnuHeadersArm64Build;
    xnu = kernelBuild;
    xnu-debug = kernelDebugBuild;
    kernel = kernelBuild;
    kernel-debug = kernelDebugBuild;
    kernel-arm64 = kernelArm64Build;
    kernel-arm64-virt = kernelArm64VirtBuild;
    kernel-arm64-virt-debug = kernelArm64VirtDebugBuild;
    kernel-arm64-t8010 = kernelArm64T8010Build;
    kernel-arm64-s8000 = kernelArm64S8000Build;
    kernel-arm64-s8000-debug = kernelArm64S8000DebugBuild;
    kernel-arm64-t8103 = kernelArm64T8103Build;
    kernel-arm64-t8103-debug = kernelArm64T8103DebugBuild;
    kernel-arm64-t8010-debug = kernelArm64T8010DebugBuild;
    kernel-arm64-bcm2837 = kernelArm64Bcm2837Build;
    kernel-arm64-bcm2837-debug = kernelArm64Bcm2837DebugBuild;
    kernel-arm64-sun50i = kernelArm64Sun50iBuild;
    kernel-arm64-sun50i-debug = kernelArm64Sun50iDebugBuild;
    kernel-arm64-sc8280xp = kernelArm64Sc8280xpBuild;
    kernel-arm64-sc8280xp-debug = kernelArm64Sc8280xpDebugBuild;
    kernel-arm64-virt4k-debug = kernelArm64Virt4kDebugBuild;
    kernel-arm64-virtamp = kernelArm64VirtAmpBuild;
    kernel-arm32-bcm2835 = kernelArm32Bcm2835Build;
    kernel-arm32-bcm2835-debug = kernelArm32Bcm2835DebugBuild;
    kernel-arm32-bcm2835-dev = kernelArm32Bcm2835DevBuild;
    kexts-arm32-bcm2835 = kextsArm32Bcm2835Build;
    compiler-rt-armv6 = compilerRtArmv6Build;
    kexts = kextsBuild;
    kexts-arm64 = kextsArm64Build;
    iographics = iographicsBuild;
    basesystem = fullBuild;
    basesystem-split = splitBaseSystem;
    default = fullBuild;
    # fbdoom (GPL) builds from an external checkout supplied via
    # PUREDARWIN_FBDOOM_SOURCE_ENV. Without it the source is null, so only
    # publish the attribute when it is set; otherwise the whole package set
    # fails to evaluate for everyone who has not opted in.
  } // lib.optionalAttrs (fbdoomExternalSrc != null) {
    fbdoom = pd.fbdoom;
  } // imageExtraPackageSet // {
    # The Wayland stack is built only on Linux hosts; on Darwin these
    # evaluate to something that is not a derivation.
    wayland = pd.wayland;
    wayland-protocols = pd.waylandProtocols;
    neuwld = pd.neuwld;
    neuswc = pd.neuswc;
    wlroots = pd.wlroots;
    sway = pd.sway;
    xwayland = pd.xwayland;
    libX11 = pd.libX11;
    libxcb = pd.libxcb;
    freetype2 = pd.freetype2;
    ncurses = pd.ncurses;
    libiconv = pd.libiconv;
    libxkbfile = pd.libxkbfile;
    libxcb-util = pd."libxcb-util";
    libxcb-keysyms = pd."libxcb-keysyms";
    libxcb-wm = pd.xcbWm;
    libxcb-render-util = pd."libxcb-render-util";
    libxcb-image = pd."libxcb-image";
    libxcb-cursor = pd.libxcbcursor;
    xcb-util-xrm = pd."xcb-util-xrm";
    libev = pd.libev;
    pcre2 = pd.pcre2;
    yajl = pd.yajl;
    startup-notification = pd."startup-notification";
    xfconf = xfconfBuild;
    libxfce4util = libxfce4utilBuild;
    libxfce4ui = libxfce4uiBuild;
    libxfce4windowing = libxfce4windowingBuild;
    libwnck = pd.libwnck;
    garcon = garconBuild;
    exo = exoBuild;
    xfwm4 = xfwm4Build;
    xfce4-session = xfce4SessionBuild;
    xfce4-panel = xfce4PanelBuild;
    xfdesktop = xfdesktopBuild;
    xfce4-appfinder = xfce4AppfinderBuild;
    thunar = thunarBuild;
    xfce4-settings = xfce4SettingsBuild;
    vte = pd.vte;
    xfce4-terminal = xfce4TerminalBuild;
    cairo = pd.cairo;
    libffi = pd.libffi;
    glib = pd.glib;
    fribidi = pd.fribidi;
    harfbuzz = pd.harfbuzz;
    expat = pd.expat;
    fontconfig = pd.fontconfig;
    pango = pd.pango;
    i3 = pd.i3;
    i3status = pd.i3statusShim;
    openssh = pd.openssh;
    gnumake = pd.gnumake;
    ninja = pd.ninja;
    cmake = pd.cmake;
    clang = clangCrossBuild;
    kc-tools-guest = kcToolsGuestBuild;
    meson = pd.meson;
    pkgconf = pd.pkgconf;
    gnum4 = pd.gnum4;
    autoconf = pd.autoconf;
    automake = pd.automake;
    bison = pd.bison;
    flex = pd.flex;
    python = pd.python;
    #perl = perlBuild;
  };

  linuxPackages =
    let
      # the x86-only boot programs (BIOS stage, 32-bit UEFI) come from the x86_64-linux set on any other
      # host: nixpkgs has i686 and BIOS packages only on x86, and they are small enough for any x86 builder
      x86System = if pkgs.stdenv.buildPlatform.isx86 then system else "x86_64-linux";
      x86Pkgs = if pkgs.stdenv.buildPlatform.isx86 then pkgs else import pkgs.path { system = x86System; };
      makeXnuLoaderHostBuild = makeXnuLoaderBuildOn pkgs;
      makeXnuLoaderBuildOn = pkgs: loader: crossPkgs:
        let
          efiInputTarget =
            if crossPkgs.stdenv.hostPlatform.isx86_64
            then "elf64-x86-64"
            else if crossPkgs.stdenv.hostPlatform.isi686
            then "elf32-i386"
            else "elf64-littleaarch64";
        in
        (loader.override {
          stdenv = crossPkgs.stdenv;
          gnu-efi = crossPkgs.gnu-efi;
        }).overrideAttrs (old: {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.coreutils
            pkgs.dosfstools
            pkgs.mtools
          ];
          # The unwrapped binutils installs only target-prefixed tools in bin/
          # for a cross set, so bin/ld and bin/objcopy do not exist there and
          # the link fails with "No such file or directory". The wrapper does
          # carry the prefixed names.
          cmakeFlags = (old.cmakeFlags or [ ]) ++ [
            "-DCMAKE_LINKER=${crossPkgs.stdenv.cc.bintools}/bin/${crossPkgs.stdenv.cc.targetPrefix}ld"
            "-DCMAKE_OBJCOPY=${crossPkgs.stdenv.cc.bintools}/bin/${crossPkgs.stdenv.cc.targetPrefix}objcopy"
          ];
          postPatch = (old.postPatch or "") + ''
            if grep -q -- '--target=efi-app-''${ARCH}' CMakeLists.txt; then
              substituteInPlace CMakeLists.txt \
                --replace-fail 'COMMAND ''${CMAKE_OBJCOPY}' \
                  'COMMAND ''${CMAKE_OBJCOPY} --input-target=${efiInputTarget}' \
                --replace-fail '--target=efi-app-''${ARCH}' \
                  '--output-target=efi-app-''${ARCH}'
            fi
          '';
        });
      xnuLoaderDefault = makeXnuLoaderHostBuild xnu-loader.packages.${system}.default pkgs.pkgsCross.gnu64;
      xnuLoaderArm64 = makeXnuLoaderHostBuild xnu-loader.packages.${system}.arm64-virt pkgs.pkgsCross.aarch64-multiplatform;
      xnuLoaderArm64Sc8280xp = makeXnuLoaderHostBuild xnu-loader.packages.${system}.arm64-sc8280xp pkgs.pkgsCross.aarch64-multiplatform;
      # 32-bit UEFI on a 64-bit CPU: the firmware loads only IA32 PE images,
      # while the kernel it boots stays x86_64. Built from the i686 set so
      # libgcc, gnu-efi and binutils are all 32-bit.
      xnuLoaderIa32 = makeXnuLoaderBuildOn x86Pkgs xnu-loader.packages.${x86System}.ia32 x86Pkgs.pkgsi686Linux;
      kcBuild = pkgs.callPackage ./pkgs/toolchain/kc.nix {
        kernel = kernelBuild;
        inherit kernelSource;
        kexts = kextsBuild;
        kcTools = kc-tools.packages.${system}.default;
        # each fbsdkpi nic links its own copy of the layer, together they pushed this kc past the
        # loader's 63 MiB span, PDE1000 and PDRealtek8111 still cover the common nics here
        excludeKexts = [ "PDFBSDem.kext" "PDFBSDigc.kext" "PDFBSDrge.kext" ];
      };
      kcDebugBuild = pkgs.callPackage ./pkgs/toolchain/kc.nix {
        kernel = kernelDebugBuild;
        inherit kernelSource;
        kexts = kextsBuild;
        kcTools = kc-tools.packages.${system}.default;
      };
      # an arm64 collection from the board table: the board's recipe and kcBase, extra on top
      boardKc = board: kernel: extra: let b = (import ./boards.nix).boards.${board}; in
        pkgs.callPackage ./pkgs/toolchain/${b.kc}.nix ({
          inherit kernel;
          kexts = kextsArm64Build;
          kcTools = kc-tools.packages.${system}.default;
        } // lib.optionalAttrs (b.kc != "kc-arm64-bcm2837") { inherit kernelSource; }
          // lib.optionalAttrs (b ? kcBase) { inherit (b) kcBase; } // extra);
      kcArm64DebugBuild = boardKc "virt" kernelArm64VirtDebugBuild { };
      kcArm64ReleaseBuild = boardKc "virt" kernelArm64VirtBuild { };
      # the A733 kernel: VIRT with an E and a P cluster for the AMP (Edge) scheduler
      kcArm64A733ReleaseBuild = boardKc "virtamp" kernelArm64VirtAmpBuild { };
      # M1 for darwin-vm: the virt collection's kexts over the T8103 kernel, same large-memory base
      kcArm64T8103DebugBuild = boardKc "t8103" kernelArm64T8103DebugBuild { };
      kcArm64T8010DebugBuild = boardKc "t8010" kernelArm64T8010DebugBuild { };
      # the A9 (S8000) collection for Inferno's s8000 machine, the same kext set as T8010
      kcArm64S8000DebugBuild = (boardKc "s8000" kernelArm64S8000DebugBuild {
        # Inferno patches the apfs kext and needs it present
        extraKexts = [ "pthread" "Sandbox" "amfi" "apfs" "PDStubAppleMobileFileIntegrity" "PDStubAppleImage4" "PDStubImage4"
          "PDStubAppleFirmwareUpdateKext" "PDStubAppleS5L8960XWatchDogTimer" ];
      }).overrideAttrs { pname = "puredarwin-kc-arm64-s8000"; };
      kcArm64T8010ReleaseBuild = boardKc "t8010" kernelArm64T8010Build { };
      kcArm64Bcm2837ReleaseBuild = boardKc "bcm2837" kernelArm64Bcm2837Build { };
      kcArm64Bcm2837DebugBuild = boardKc "bcm2837" kernelArm64Bcm2837DebugBuild { };
      # 4KB-page VIRT: the collection header sits at the classic ARM64_KC_BASE
      kcArm64Virt4kDebugBuild = boardKc "virt4k" kernelArm64Virt4kDebugBuild { };
      # Same collection as VIRT4K (classic geometry, kcBase = ARM64_KC_BASE); the
      # board differs from QEMU only in the platform expert's peripherals
      kcArm64Sun50iReleaseBuild = boardKc "sun50i" kernelArm64Sun50iBuild { };
      # the sun50i collection plus the drm layer and the adreno driver, for the msm8916 stick
      kcArm64Msm8916Build = boardKc "sun50i" kernelArm64Sun50iBuild {
        extraKexts = [
          "${drmMsmArm64Build}/System/Library/Extensions/PDDRMDri.kext"
          "${drmMsmArm64Build}/System/Library/Extensions/PDDRMmsm.kext"
          # no display: neuswc draws into ram, pdfbd streams it out over usb
          "${kextsArm64Build}/System/Library/Extensions/PDVirtualFramebuffer.kext"
        ];
      };
      # the sun50i collection plus the drm layer and nouveau, for the jetson tx1's gm20b
      kcArm64Tegra210Build = boardKc "sun50i" kernelArm64Sun50iBuild {
        extraKexts = [
          "${drmNouveauArm64Build}/System/Library/Extensions/PDDRMDri.kext"
          "${drmNouveauArm64Build}/System/Library/Extensions/PDDRMnouveau.kext"
        ];
        # nouveau needs about 6.7 MB, so drivers for other boards and unused filesystems make room
        omitKexts = [
          "hfs" "HFSEncodings" "apfs" "ApfsFileSystemDriver" "msdosfs" "IONVMEFamily" "RavynAHCIPort"
          "PDPowerVR" "PDSun60iDisplay" "PDSg2002SD" "PDSg2002Eth"
          "IOVirtIOFamily" "IOVirtIOGPU" "IOVirtIOBlock" "IOVirtIONet"
          "PDQcomTLMM" "PDQcomGeniI2C" "PDI2CHID" "AppleUSBOHCI"
          # the usb host stays: PDTegraXUSB hands the xusb xhci to RavynXHCIPort for hid input
          "AppleUSBEHCI" "PDDiskImages2" "IOGOPFramebuffer" "PDEcamPCI" "PDI2CFamily"
          # the e1000 nic and FileVault have nothing on this board
          "PDFBSDem" "PDKeyStore"
        ];
      };
      kcArm64Sun50iDebugBuild = boardKc "sun50i" kernelArm64Sun50iDebugBuild { };
      kcArm64Sc8280xpReleaseBuild = boardKc "sc8280xp" kernelArm64Sc8280xpBuild { };
      kcArm64Sc8280xpDebugBuild = boardKc "sc8280xp" kernelArm64Sc8280xpDebugBuild { };
      kcArm64Sdm670Build = boardKc "sdm670" kernelArm64Sdm670Build { };
      prelinkedArm32Bcm2835Build =
        pkgs.callPackage ./pkgs/toolchain/prelinked-arm32-bcm2835.nix {
          kernel = kernelArm32Bcm2835Build;
          kexts = kextsArm32Bcm2835Build;
          kcTools = kc-tools.packages.${system}.default;
        };
      prelinkedArm32Bcm2835DevBuild =
        pkgs.callPackage ./pkgs/toolchain/prelinked-arm32-bcm2835.nix {
          kernel = kernelArm32Bcm2835DevBuild;
          kexts = kextsArm32Bcm2835Build;
          kcTools = kc-tools.packages.${system}.default;
        };
      prelinkedArm32Bcm2835DebugBuild =
        pkgs.callPackage ./pkgs/toolchain/prelinked-arm32-bcm2835.nix {
          kernel = kernelArm32Bcm2835DebugBuild;
          kexts = kextsArm32Bcm2835Build;
          kcTools = kc-tools.packages.${system}.default;
        };
      imageExtraPackages = lib.attrValues imageExtraPackageSet
        ++ lib.optional (fbdoomExternalSrc != null) pd.fbdoom;
      # image-store-minimal plus Nix and a toolchain, a PD guest that builds natively
      builderPackageSet = {
        inherit (imageExtraPackageSet)
          nix llvm bash zsh toybox file openssl curl libcurl-dylib libz-dylib openssh git xz unzip bzip2
          gnumake bmake ninja cmake clang meson pkgconf gnum4 autoconf automake bison flex
          python migcomDarwin dsymutil libcxx-dylib libcxxabi-dylib libiconv nano xxd kc-tools-guest
          corefoundation systemConfiguration icucore libobjc;
        cctools = pd.cctools;
      };
      imageBuilderBuild = imageStoreMinimalBuild.override {
        extraPackages = strippedExtraPackages ++ lib.attrValues builderPackageSet;
        imageFileName = "puredarwin-builder.img";
        rootMB = 16384;
      };
      legacyBoot = xnu-loader.packages.${x86System}.legacy-boot.override {
        stage2Lba = 34;
      };
      imageHfsBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystem;
        extraPackages = imageExtraPackages;
        kc = kcBuild;
        xnuLoader = xnuLoaderDefault;
        apfsprogs = pkgs.apfsprogs;
        hfsprogs = pkgs.hfsprogs;
        libdmg-hfsplus = pkgs.callPackage ./pkgs/toolchain/libdmg-hfsplus.nix { };
        rootFsType = "hfs";
        imageFileName = "puredarwin-hfs.img";
        #testAudioFile = /home/vali/development/darwin/stillalive.pcm;
      };
      imageApfsBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystem;
        extraPackages = imageExtraPackages;
        kc = kcBuild;
        xnuLoader = xnuLoaderDefault;
        apfsprogs = pkgs.apfsprogs;
        libapfsrw = pd.libapfsrw;
        rootFsType = "apfs";
        imageFileName = "puredarwin-apfs.img";
      };
      imageDebugBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystem;
        extraPackages = lib.attrValues imageExtraPackageSet;
        kc = kcDebugBuild;
        xnuLoader = xnuLoaderDefault;
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-debug.img";
      };
      imageArm64VirtBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimalRelease;
        extraPackages = imageExtraPackagesArm64;
        kc = kcArm64ReleaseBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        espMB = 64;
        rootMB = 3072;
        imageFileName = "puredarwin-arm64-virt.img";
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 pdtrace=1 serial_video_mirror=1 no_interrupt_masked_debug=1 vgpu_debug=1";
      };
      imageArm64VirtDebugBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimalRelease;
        extraPackages = imageExtraPackagesArm64;
        kc = kcArm64ReleaseBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        espMB = 64;
        rootMB = 3072;
        imageFileName = "puredarwin-debug-arm64-virt.img";
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 pdtrace=1 serial_video_mirror=1 no_interrupt_masked_debug=1 vgpu_debug=1";
      };
      imageArm64VirtWaylandBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtWayland;
        extraPackages = imageExtraPackagesArm64Nox;
        kc = kcArm64ReleaseBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        imageFileName = "puredarwin-wayland-arm64-virt.img";
      };
      # Apple M1 (t8103): the KC goes to the boot loader on its own, the disk is for Apple's NVMe and its 4 KiB blocks
      imageArm64T8103Build = imageArm64VirtWaylandBuild.override {
        kc = kcArm64T8103DebugBuild;
        gptBlockSize = 4096;
        python3 = pkgs.python3;
        imageFileName = "puredarwin-arm64-t8103.img";
      };
      imageArm64VirtWaylandDebugBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtWayland;
        extraPackages = imageExtraPackagesArm64Nox;
        kc = kcArm64DebugBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        imageFileName = "puredarwin-wayland-debug-arm64-virt.img";
      };
      imageArm64VirtMinimalBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimal;
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build asmjitTestArm64Build ];
        kc = kcArm64DebugBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        espMB = 768;
        rootMB = 512;
        imageFileName = "puredarwin-arm64-virt-minimal.img";
        ramdiskMB = 512;
        # No rd=md0 here: this target has no useRamdisk, so nothing loads an
        # md0 and IOFindBSDRoot panics on it. The loader publishes the ext4
        # root it finds on the boot disk instead. The netboot target below is
        # the one that really boots from a ramdisk.
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 pdtrace=1 serial_video_mirror=1 no_interrupt_masked_debug=1";
      };
      # licheerv nano root: the arm64 minimal system plus configd for en0 and dhcp
      imageArm64Sg2002Build = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64Sg2002;
        extraPackages = imageExtraPackagesArm64Sg2002;
        kc = kcArm64Sun50iDebugBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        espMB = 64;
        rootMB = 1024;
        imageFileName = "puredarwin-arm64-sg2002.img";
        bootArgs = "-v debug=0x8 serial=3 keepsyms=1 -nogzalloc_mode";
      };
      netbootArm64VirtMinimalBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimal;
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build asmjitTestArm64Build ];
        kc = kcArm64DebugBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        netbootOnly = true;
        useRamdisk = true;
        ramdiskMB = 512;
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 pdtrace=1 serial_video_mirror=1 no_interrupt_masked_debug=1 rd=md0";
      };
      # T8010 (iPad 6). No EFI and no disk: pongoOS chainloads the kernel
      # collection and hands the same staging tree over as an ext4 ramdisk,
      # which XNU mounts as /dev/md0. netbootOnly keeps the builder from
      # assembling a partitioned disk image around it - only kernel and
      # ramdisk are ever used.
      ramdiskArm64T8010Build = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimal;
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build ];
        kc = kcArm64T8010DebugBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        netbootOnly = true;
        useRamdisk = true;
        # the pruned tree is ~98MB, mostly libicudata for CoreFoundation, leaving ~55MB to write into
        # pongoOS's 128MB upload cap and 64MB static-region cap were raised to carry it
        ramdiskMB = 160;
        # pongoOS chainloads the kernel, so the kernels and link archives go, with the headers and static
        # libraries nothing on a first boot needs
        ramdiskPrune = [
          "System/Library/Kernels"
          "include"
          "usr/include"
          "usr/local/include"
          "pd-guest-headers"
          "pd-xpc-dev"
          "lib/pkgconfig"
          "usr/lib/pkgconfig"
          "applets.txt"
        ];
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 serial_video_mirror=1 no_interrupt_masked_debug=1 rd=md0";
      };
      # the same minimal root for Inferno's s8000, md0 from the initrd
      ramdiskArm64S8000Build = ramdiskArm64T8010Build.override {
        kc = kcArm64S8000DebugBuild;
      };
      # same pruned ext4 root as T8010, for the Orange Pi Zero 3: xnu-loader carries it as ramdisk.img
      ramdiskArm64Sun50iBuild = ramdiskArm64T8010Build.override {
        kc = kcArm64Sun50iReleaseBuild;
      };
      # the sun50i collection carries the tegra210 drivers too, plus adb over the jetson tx1's usb port
      ramdiskArm64Tegra210Build = ramdiskArm64T8010Build.override {
        kc = kcArm64Tegra210Build;
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build ]
          ++ lib.optional (pdadbdArm64Build != null) pdadbdArm64Build
          ++ lib.optional (nouveauFirmwareGm20b != null) nouveauFirmwareGm20b
          ++ lib.optional (tegra210XusbFirmware != null) tegra210XusbFirmware;
      };
      # the same root on the sd card, with mesa (nouveau and llvmpipe) and pdgltri, too big for the blob
      ramdiskArm64Tegra210SdBuild = ramdiskArm64Tegra210Build.override {
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build ]
          ++ lib.optional (pdadbdArm64Build != null) pdadbdArm64Build
          ++ lib.optional (nouveauFirmwareGm20b != null) nouveauFirmwareGm20b
          ++ lib.optional (tegra210XusbFirmware != null) tegra210XusbFirmware
          ++ lib.optional (tx1GlRootArm64Bundle != null) tx1GlRootArm64Bundle;
        ramdiskMB = 512;
      };
      # the sd root plus the argon desktop, started by launchd on the PDTegraDisplay framebuffer
      ramdiskArm64Tegra210SdDesktopBuild = ramdiskArm64Tegra210SdBuild.override (old: {
        # fastfetch for the desktop terminal, with the foundation it links and no second mesa
        extraPackages = old.extraPackages ++ lib.optional (tx1DesktopArm64Bundle != null) tx1DesktopArm64Bundle
          ++ [ fastfetchNoGLArm64Build foundationArm64Build ];
        ramdiskMB = 1536;
      });
      # mainline device trees in place of cboot's l4t tree, off by default since cboot's memory and carveout fixups are lost
      ramdiskArm64Tegra210MainlineDtbBuild = ramdiskArm64Tegra210Build.override { boardDtb = tegra210Dtb; };
      ramdiskArm64Tegra210SdMainlineDtbBuild = ramdiskArm64Tegra210SdBuild.override { boardDtb = tegra210Dtb; };
      ramdiskArm64Sc8280xpBuild = ramdiskArm64T8010Build.override {
        kc = kcArm64Sc8280xpReleaseBuild;
        xnuLoader = xnuLoaderArm64Sc8280xp;
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=0 rd=md0 hid-legacy-shim=1";
      };
      # the pixel 3a (sdm670): the sc8280xp kernel and collection, whose kernel hardcodes nothing past the arm
      # architecture's own timer interrupt, on the screen the bootloader leaves lit
      ramdiskArm64Sdm670Build = ramdiskArm64Sc8280xpBuild.override {
        kc = kcArm64Sdm670Build;
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 rd=md0 hid-legacy-shim=1";
      };
      # the upstream pixel 3a device trees (sargo, and bonito-tianma for the 3a xl), built here from the devicetree-rebasing mirror and never kept in the tree
      sdm670Dtb = board: pkgs.runCommand "puredarwin-dtb-sdm670-google-${board}" {
        src = pkgs.fetchgit {
          url = "https://git.kernel.org/pub/scm/linux/kernel/git/devicetree/devicetree-rebasing.git";
          rev = "765d590c63386d13351a62d919326b99841cdc7d";
          sparseCheckout = [ "src/arm64/qcom" "include" ];
          hash = "sha256-4DuPhmB5bN408cvCin9mHAyjQbwF9GUDt+Ih6qJkgaI=";
        };
        nativeBuildInputs = [ pkgs.dtc pkgs.stdenv.cc ];
      } ''
        cd $src
        cpp -nostdinc -undef -x assembler-with-cpp -D__DTS__ -I include -I src/arm64/qcom -I src/arm64 \
          src/arm64/qcom/sdm670-google-${board}.dts > $TMPDIR/board.dts
        mkdir -p $out
        dtc -q -I dts -O dtb -o $out/board.dtb $TMPDIR/board.dts
      '';
      # an android boot image for fastboot boot: the loader's linux Image with the dtb appended, and a cpio of the
      # collection, the boot-args and the root image, the header as pmaports' sargo deviceinfo gives it. ram only
      bootimgArm64Sdm670 = board: pkgs.runCommand "puredarwin-bootimg-arm64-sdm670-${board}" {
        nativeBuildInputs = [ pkgs.android-tools pkgs.cpio pkgs.gzip ];
      } ''
        mkdir -p root/EFI/BOOT $out
        cp ${kcArm64Sdm670Build}/kernel root/EFI/BOOT/kernel
        printf '%s' ${lib.escapeShellArg "-v debug=0x218 -nogzalloc_mode keepsyms=1 rd=md0 hid-legacy-shim=1"} > root/EFI/BOOT/boot-args.txt
        gzip -9n < ${ramdiskArm64Sdm670Build}/ramdisk.img > root/ramdisk.img.gz
        (cd root && find EFI ramdisk.img.gz | sort | cpio -o -H newc --reproducible --quiet) > initrd.cpio
        gzip -9n < ${xnu-loader.packages.${system}.kernel-arm64-generic}/boot/xnu-loader.Image > Image.gz
        cat Image.gz ${sdm670Dtb board}/board.dtb > Image.gz-dtb
        mkbootimg --kernel Image.gz-dtb --ramdisk initrd.cpio --pagesize 4096 --base 0x00000000 \
          --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --second_offset 0x00000000 \
          --tags_offset 0x00000100 --cmdline "" -o $out/puredarwin-sdm670-${board}.img
      '';
      imageArm64Bcm2837Build = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimal;
        # fastfetch needs libFoundation, which the minimal base system does not
        # carry; the no-GL build is the one that does not also want Mesa.
        extraPackages = [
          zshArm64Build libiconvArm64Build toyboxArm64Build
          fastfetchNoGLArm64Build foundationArm64Build vmprobeArm64Build
        ];
        kc = kcArm64Bcm2837ReleaseBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        imageFileName = "puredarwin-rpi3.img";
        espMB = 64;
        rootMB = 1024;
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=1 no_interrupt_masked_debug=1";
      };
      imageArm64VirtMinimalReleaseBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimalRelease;
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build ];
        kc = kcArm64ReleaseBuild;
        xnuLoader = xnuLoaderArm64;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        espMB = 64;
        rootMB = 512;
        imageFileName = "puredarwin-arm64-virt-minimal-release.img";
        bootArgs = "-v serial=3 ahci_debug=1 kext=0xffff io=0xffff";
      };
      # Wayland-only image: no X server, no X client, no X library anywhere on
      # the image. Anything that links libX11/libxcb either gets dropped here
      # or is rebuilt from a *NoxBuild variant below.
      waylandOnlyDroppedPackages = [
        # X server and Xwayland
        "xorg" "xvfb" "xwayland" "xkbcomp" "libxcvt"
        # X clients and X-only WM/session bits
        "xeyes" "xclock" "xcalc" "xmessage" "xterm" "dmenu"
        "i3" "i3status" "startup-notification"
        "iceauth" "xrdb" "xrandr" "xinit"
        # dillo is FLTK/X11. Wine stays, its default driver is winemac.drv, AppKit windows in Argon
        "dillo"
        # X client libraries (both the static and -shared flavours)
        "libX11" "libxcb" "libxcb-util" "libxcb-keysyms" "libxcb-wm"
        "libxcb-render-util" "libxcb-image" "libxcb-cursor" "xcb-util-xrm"
        "libXau" "libXdmcp" "libXext" "libXrender" "libXfixes" "libXcursor"
        "libXrandr" "libXft" "libICE" "libSM" "libXinerama" "libXres"
        "libXcomposite" "libXdamage" "libXpresent" "libxshmfence"
        "libx11-locale"
        "libX11-shared" "libxcb-shared" "libXau-shared" "libXdmcp-shared"
        "libXext-shared" "libXrender-shared" "libXfixes-shared" "libXi-shared"
        "libXcursor-shared" "libXrandr-shared" "libXxf86vm-shared"
        # Xfce is an X11 desktop end to end
        "libxfce4util" "xfconf" "libwnck" "libxfce4ui" "xfwm4"
        "libxfce4windowing" "garcon" "exo" "xfce4-session" "xfce4-panel"
        "xfdesktop" "xfce4-appfinder" "thunar" "xfce4-settings" "vte"
        "xfce4-terminal"
      ];
      waylandOnlyExtraPackageSet =
        (removeAttrs imageExtraPackageSet waylandOnlyDroppedPackages)
        // {
          wlroots = pd.wlrootsNox;
          iosurface-tests = pd."iosurface-tests";
          sway = pd.swayNox;
          gtk3 = pd.gtk3Nox;
          gtk-layer-shell = pd.gtkLayerShellNox;
          cairo = pd.cairoNox;
          pango = pd.pangoNox;
          netsurf = pd.netsurfNox;
          libepoxy = pd.libepoxyNox;
          fastfetch = pd.fastfetchNox;
          harfbuzz = pd.harfbuzzNox;
          at-spi2-core = pd.atspi2CoreNox;
          cairo-gobject = pd.cairoGobjectNox;
          xkbcommon = pd.xkbcommonNox;
          foot = pd.foot;
          fcft = pd.fcft;
          tllist = pd.tllist;
          pd-epoll-shim = pd.epollShim;
          dbus = pd.dbusNox;
          # the one mesa with every gpu driver, the loader picks by render node, else virgl or llvmpipe
          mesa = mesaDefaultBuild;
          libdrm-gpu = pd.libdrmAmdgpu;
          libelf = pd.libelf;
    glu = pd.gluNox;
          opengl-framework = pd.openglFrameworkNox;
          mesa-demos = pd.mesaDemosNox;
          librsvg = pd.librsvgNox;
        };
      # the desktop session: neuswc with pdwm, the AppKit apps and org.puredarwin.argon to start them
      waylandDesktopPackages = lib.filter lib.isDerivation ([
          pd.localeData pd.foundation pd.freetype2 pd.fontconfig pd.libpng pd.libjpeg pd.zlib
          pd.xvfbFonts pd.pdsurface pd.onyx2d pd.coregraphics pd.coretext
          pd.wayland pd.xkbcommonNox pd.xkeyboardConfig pd.neuswc
          pd.quartzcore pd.corevideo pd.applicationservices pd.windowserver pd.appkit pd.coredata pd.cocoa pd.appFrameworks pd.carbon pd.mpsNox
          pd.openglFrameworkNox mesaDefaultBuild pd.libdrmAmdgpu pd.libelf pd.gluNox llvmCrossBuild pd.pdVirglShim
          pd.terminal pd.menubar pd.pddecor pd.loginwindow pd.dock pd.finder pd."system-information" pd.calculator pd.settings pd."argon-session" psOnlyBuild diagToolsBuild
          # the built-in utilities: text editor, activity, logs, image viewer, screenshot and disks
        ] ++ utilityAppBuilds ++ [
          # Inter and the other system faces the window chrome and menu bar are drawn in
          pd.systemFonts
          # the mime database glib reads, so gtk and webkit can tell what a file or response is
          pd."shared-mime-info"
          # something full-screen to run in it
          pd.ncurses pd.nano pd.fastfetchNox
          # vulkan on the host gpu with PUREDARWIN_VM_VENUS=1 (venus virtio-gpu)
          pd.vulkanLoaderNox mesaVenusWaylandBuild pd."pd-metal-surface" pd.libdrmVirtgpu pd.vulkanToolsNox pd.venusTestsNox
          # the Metal demos: pdmtl and Indium tests (metal-pd), Metal.framework (metal-objc), Apple's samples
          pd.glfw pd.metalPdNox pd.metalObjcPdNox pd.metalAppleSamplesNox pd.spirvCross
          # mslc compiles newLibraryWithSource: on a cache miss, through the image clang
          pd."mslc-pd"
          # eglinfo and peglgears, to tell a gpu GL apart from the compositor on testers machines
          pd.mesaDemosNox
      ]);
      imageWaylandBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemWayland;
        # the same desktop session as image-wayland-minimal, over the full userland
        extraPackages = lib.unique (lib.attrValues waylandOnlyExtraPackageSet ++ waylandDesktopPackages
          ++ gpuDefaultPackages ++ [ pdkextFiles ]);
        kc = kcGpuBuild;
        xnuLoader = xnuLoaderDefault;
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-wayland.img";
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 gen9_debug=1 ahci_debug=1 csr-active-config=0x23 dtrace_kernel_symbol_mode=1 pdnouveau=1";
      };
      # wifi firmware the drivers load from /Library/Firmware/<driver>, kept out
      # of the kernel collection and the kext bundles
      rtw89FirmwareBuild = pkgs.runCommand "puredarwin-rtw89-firmware" { } ''
        fw=$out/Library/Firmware/rtw89
        mkdir -p $fw
        for f in rtw8852b_fw-1 rtw8852c_fw-2; do
          src=${pkgs.linux-firmware}/lib/firmware/rtw89/$f.bin
          if [ -e "$src.zst" ]; then ${pkgs.zstd}/bin/zstd -dqf "$src.zst" -o $fw/$f.bin
          elif [ -e "$src.xz" ]; then ${pkgs.xz}/bin/xz -dc "$src.xz" > $fw/$f.bin
          else cp -L "$src" $fw/$f.bin
          fi
        done
        cp ${pkgs.linux-firmware.src}/LICENSES/LICENCE.rtlwifi_firmware.txt $fw/LICENCE.rtlwifi_firmware.txt
      '';
      # realtek usb wifi firmware for PDFBSDrtwn from linux-firmware, under the names rtwn asks for
      rtwnFirmwareBuild = pkgs.runCommand "puredarwin-rtwn-firmware" { } ''
        fw=$out/Library/Firmware
        mkdir -p $fw
        while read name src; do
          from=${pkgs.linux-firmware}/lib/firmware/$src
          if [ -e "$from.zst" ]; then ${pkgs.zstd}/bin/zstd -dqf "$from.zst" -o $fw/$name
          elif [ -e "$from.xz" ]; then ${pkgs.xz}/bin/xz -dc "$from.xz" > $fw/$name
          elif [ -e "$from" ]; then cp -L "$from" $fw/$name
          else echo "rtwn firmware: no $src in linux-firmware" >&2; exit 1
          fi
        done <<EOF
        rtwn-rtl8188eufw rtlwifi/rtl8188eufw.bin
        rtwn-rtl8192cfwT rtlwifi/rtl8192cufw_TMSC.bin
        rtwn-rtl8192cfwU rtlwifi/rtl8192cufw_A.bin
        rtwn-rtl8192eufw rtlwifi/rtl8192eu_nic.bin
        rtwn-rtl8723bufw rtlwifi/rtl8723bu_nic.bin
        rtwn-rtl8812aufw rtw88/rtw8812a_fw.bin
        rtwn-rtl8821aufw rtw88/rtw8821a_fw.bin
        EOF
        cp ${pkgs.linux-firmware.src}/LICENSES/LICENCE.rtlwifi_firmware.txt $fw/LICENCE.rtwn_firmware.txt
      '';
      # rt2870.bin for PDFBSDrun, as the runfw image firmware(9) asks for: rt2870 in its first 4 KB, rt3071 in the rest
      # intel 1000 to 6000 series firmware for PDFBSDiwn from linux-firmware, under the names iwn asks for
      iwnFirmwareBuild = pkgs.runCommand "puredarwin-iwn-firmware" { } ''
        fw=$out/Library/Firmware
        mkdir -p $fw
        while read name src; do
          from=${pkgs.linux-firmware}/lib/firmware/$src
          if [ -e "$from.zst" ]; then ${pkgs.zstd}/bin/zstd -dqf "$from.zst" -o $fw/$name
          elif [ -e "$from.xz" ]; then ${pkgs.xz}/bin/xz -dc "$from.xz" > $fw/$name
          elif [ -e "$from" ]; then cp -L "$from" $fw/$name
          else echo "iwn firmware: no $src in linux-firmware" >&2; exit 1
          fi
        done <<EOF
        iwn100fw iwlwifi-100-5.ucode
        iwn105fw iwlwifi-105-6.ucode
        iwn135fw iwlwifi-135-6.ucode
        iwn1000fw iwlwifi-1000-5.ucode
        iwn2000fw iwlwifi-2000-6.ucode
        iwn2030fw iwlwifi-2030-6.ucode
        iwn5000fw iwlwifi-5000-5.ucode
        iwn5150fw iwlwifi-5150-2.ucode
        iwn6000fw iwlwifi-6000-4.ucode
        iwn6000g2afw iwlwifi-6000g2a-6.ucode
        iwn6000g2bfw iwlwifi-6000g2b-6.ucode
        iwn6050fw iwlwifi-6050-5.ucode
        EOF
        cp ${pkgs.linux-firmware.src}/LICENSES/LICENCE.iwlwifi_firmware $fw/LICENCE.iwn_firmware
      '';
      runFirmwareBuild = pkgs.runCommand "puredarwin-run-firmware" { } ''
        fw=$out/Library/Firmware
        mkdir -p $fw
        from=${pkgs.linux-firmware}/lib/firmware/rt2870.bin
        if [ -e "$from.zst" ]; then ${pkgs.zstd}/bin/zstd -dqf "$from.zst" -o $fw/runfw
        elif [ -e "$from.xz" ]; then ${pkgs.xz}/bin/xz -dc "$from.xz" > $fw/runfw
        else cp -L "$from" $fw/runfw
        fi
        [ "$(stat -c %s $fw/runfw)" = 8192 ] || { echo "runfw is not 8 KB" >&2; exit 1; }
        cp ${pkgs.linux-firmware.src}/LICENSES/LICENCE.ralink-firmware.txt $fw/LICENCE.runfw.txt
      '';
      # WCN6855 firmware for PDQwx from linux-firmware, under the names qwx(4) asks for
      qwxFirmwareBuild = pkgs.runCommand "puredarwin-qwx-firmware" { } ''
        fw=$out/Library/Firmware/qwx
        mkdir -p $fw
        dir=${pkgs.linux-firmware}/lib/firmware/ath11k/WCN6855
        for rev in hw2.0 hw2.1; do
          for f in amss m3 board-2 regdb; do
            src=$dir/$rev/$f.bin
            dst=$fw/qwx-wcn6855-$rev-$f
            # hw2.1 mostly shares hw2.0's files, a link keeps one copy in the image
            if [ $rev = hw2.1 ] && [ "$(readlink -f "$src")" = "$(readlink -f "$dir/hw2.0/$f.bin")" ]; then
              ln -s qwx-wcn6855-hw2.0-$f "$dst"
            elif [ -e "$src.zst" ]; then ${pkgs.zstd}/bin/zstd -dqf "$src.zst" -o "$dst"
            elif [ -e "$src.xz" ]; then ${pkgs.xz}/bin/xz -dc "$src.xz" > "$dst"
            elif [ -e "$src" ]; then cp -L "$src" "$dst"
            fi
          done
        done
        cp -L $dir/hw2.0/Notice.txt $fw/Notice.WCN6855.txt
        cp ${pkgs.linux-firmware.src}/LICENSES/LICENSE.QualcommAtheros_ath10k $fw/ 2>/dev/null || \
          cp ${pkgs.linux-firmware.src}/LICENSE.QualcommAtheros_ath10k $fw/
      '';
      strippedExtraPackages = lib.filter lib.isDerivation (
        [
          rtw89FirmwareBuild
          rtwnFirmwareBuild
          iwnFirmwareBuild
          runFirmwareBuild
          qwxFirmwareBuild
          pd.localeData
          pd.corefoundation
          pd.icu
          pd.libcxxabiDylib
          pd.libcxxDylib
          pd.libobjc
        ]
        ++ [
          pd.zsh
          pd.toybox
          pd.libiconv
          pd.iokit
          pd.coreservices
          pd.libcxxTest
          pd.objcTest
          pd.foundation
          # x86 CoreFoundation.framework binary (Foundation + CF re-export) and Versions/C paths
          pd."framework-compat"
          # /usr/lib/swift, which framework-compat's Foundation re-exports from
          pd.swiftRuntime
          pd.audiotoolbox
          pd.pcmplay
          pd.coreaudiod
          pd.carbon
          pd.iosurface
          pd.security
          pd.symptomReporter
          pd.systemConfiguration
          pd.diskArbitration
          pd.ioreg
          pd.dtrace
          pd.libzDylib
        ]
      );
      minimalBootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 gen9_debug=1 pdnouveau=1";
      imageMinimalBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemMinimal;
        extraPackages = strippedExtraPackages;
        kc = kcBuild;
        xnuLoader = xnuLoaderDefault;
        apfsprogs = pkgs.apfsprogs;
        # BIOS through the MBR stages, 64-bit and 32-bit UEFI from the ESP, as image does
        inherit legacyBoot;
        extraLoaders = [ { loader = xnuLoaderIa32; efiBinary = "BOOTIA32.EFI"; } ];
        imageFileName = "puredarwin-minimal.img";
        espMB = 60;
        rootMB = 320;
        bootArgs = minimalBootArgs;
      };
      # image-minimal with CoreGraphics and what it loads, for the CoreGraphics test run at boot
      imageCgTestBuild = imageMinimalBuild.override {
        extraPackages = lib.unique (strippedExtraPackages ++ [
          pd.freetype2 pd.fontconfig pd.libpng pd.libjpeg pd.zlib pd.onyx2d pd.pdsurface
          pd.coregraphics pd."cg-ff-tests"
        ]);
        imageFileName = "puredarwin-cg-test.img";
        rootMB = 512;
      };
      # image-minimal with the installer's disk tools and ssh, for testing them against a second disk
      imageToolsTestBuild = imageMinimalBuild.override {
        extraPackages = strippedExtraPackages ++ [ pd.e2fsprogs pd.msdosCmds pd.pdbless pd.openssh ];
        imageFileName = "puredarwin-tools-test.img";
        rootMB = 512;
      };
      # image-tools-test with the media, audio and app frameworks and what they load, for the framework tests
      imageFrameworksTestBuild = imageMinimalBuild.override {
        extraPackages = lib.unique (strippedExtraPackages ++ [
          pd.openssh pd.freetype2 pd.fontconfig pd.libpng pd.libjpeg pd.zlib pd.onyx2d pd.pdsurface
          pd.coregraphics pd.coretext pd.quartzcore pd.corevideo pd.applicationservices pd.openglFrameworkNox
          pd.mesaNox pd.gluNox llvmCrossBuild pd.pdVirglShim pd.wayland pd.appFrameworks pd."frameworks-ff-tests"
        ]);
        imageFileName = "puredarwin-frameworks-test.img";
        rootMB = 2048;
      };
      # image-minimal with the example out-of-tree kext linked into an auxiliary KC
      imageAuxTestBuild = imageMinimalBuild.override {
        kc = kcBuild.override { auxKexts = [ "PDAuxHello.kext" ]; };
        auxLoad = [ "org.puredarwin.example.AuxHello" ];
        imageFileName = "puredarwin-aux-test.img";
      };
      # image-minimal with the system tree in /nix/store, the Apple paths link through /run/current-system
      imageStoreMinimalBuild = imageMinimalBuild.override {
        nixStore = true;
        extraPackages = strippedExtraPackages ++ [ imageExtraPackageSet.nix ];
        imageFileName = "puredarwin-store-minimal.img";
        rootMB = 1024;
      };
      # BIOS boot through GRUB and xnu-loader's Multiboot2 ELF. UEFI still works
      grubMultibootBoot = xnu-loader.packages.${system}.kernel-grub-bios;
      # GRUB (BIOS and UEFI) loading xnu-loader through the Linux boot protocol,
      # with the kernel collection and boot-args in a cpio initrd
      imageGrubLinuxMinimalBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemMinimal;
        extraPackages = strippedExtraPackages;
        kc = kcBuild;
        xnuLoader = xnuLoaderDefault;
        grubMultiboot = grubMultibootBoot;
        grubEfi = xnu-loader.packages.${system}.kernel-grub-efi;
        grubProtocol = "linux";
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-grub-linux-minimal.img";
        espMB = 60;
        rootMB = 260;
        bootArgs = minimalBootArgs;
      };
      # ps from PureDarwin's own userland, toybox's ps reads Linux /proc
      psOnlyBuild = pkgs.runCommand "puredarwin-ps" { } ''
        mkdir -p $out/bin
        cp "$(find ${pd.userlandNox} -path '*/bin/ps' | head -n 1)" $out/bin/ps
      '';
      # the kernel log, the wifi tool with its autojoin job and the power daemon, for bringing drivers up in the minimal image
      diagToolsBuild = pkgs.runCommand "puredarwin-diag-tools" { } ''
        mkdir -p $out/bin $out/usr/sbin $out/System/Library/LaunchDaemons
        cp "$(find ${pd.userlandNox} -path '*/bin/dmesg' | head -n 1)" $out/bin/dmesg
        cp "$(find ${pd.userlandNox} -path '*/usr/sbin/wifiutil' | head -n 1)" $out/usr/sbin/wifiutil
        cp "$(find ${pd.userlandNox} -path '*/usr/sbin/pdpowerd' | head -n 1)" $out/usr/sbin/pdpowerd
        mkdir -p $out/usr/libexec
        cp "$(find ${pd.userlandNox} -path '*/usr/libexec/pd-dmesg-save' | head -n 1)" $out/usr/libexec/pd-dmesg-save
        cp "$(find ${pd.userlandNox} -name org.puredarwin.pdpowerd.plist | head -n 1)" $out/System/Library/LaunchDaemons/
        # the AppleDouble parser the kernel asks for xattrs on filesystems without native ones
        cp "$(find ${pd.userlandNox} -path '*/usr/libexec/doubleagentd' | head -n 1)" $out/usr/libexec/doubleagentd
        cp "$(find ${pd.userlandNox} -name com.apple.doubleagentd.plist | head -n 1)" $out/System/Library/LaunchDaemons/
        # joins a remembered network at boot
        cp "$(find ${pd.userlandNox} -name org.puredarwin.wifi-autojoin.plist | head -n 1)" $out/System/Library/LaunchDaemons/
      '';
      # the stripped base has zsh but no /bin/sh, which the session script and launchd jobs run under
      binShBuild = pkgs.runCommand "puredarwin-bin-sh" { } ''
        mkdir -p $out/bin
        ln -s zsh $out/bin/sh
      '';
      # the Wayland counterpart of a minimal X11 session: neuswc with pdwm, and the
      # AppKit Terminal started by org.puredarwin.argon.
      # pdkext alone from the userland, it loads the aux KC at boot for PDFBSDix and the drm kexts
      pdkextFiles = pkgs.runCommand "puredarwin-pdkext" { } ''
        mkdir -p $out/usr/sbin $out/System/Library/LaunchDaemons
        cp ${pd.userland}/usr/sbin/pdkext $out/usr/sbin/pdkext
        cp ${pd.userland}/System/Library/LaunchDaemons/org.puredarwin.pdkext.plist $out/System/Library/LaunchDaemons/
      '';
      imageWaylandMinimalBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemStripped;
        extraPackages = lib.unique (strippedExtraPackages ++ waylandDesktopPackages ++ [ binShBuild ]
          ++ gpuDefaultPackages ++ [ pdkextFiles ]);
        kc = kcGpuBuild;
        xnuLoader = xnuLoaderDefault;
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-wayland-minimal.img";
        espMB = 60;
        rootMB = 1200;
        # a graphical boot: the progress bar instead of the verbose log, which -v brings back
        # csr-active-config=0x23 lets the unprotected aux KC load (0x3) and opens DTrace up to fbt/sdt (0x20),
        # dtrace_kernel_symbol_mode=1 has the kernel give fbt its own symbols, no CoreSymbolication needed
        bootArgs = "debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopprogress=1 csr-active-config=0x23 dtrace_kernel_symbol_mode=1 pdnouveau=1";
      };
      # the install and rescue drive: the desktop with the disk tools, pdinstall and Install PureDarwin
      imageInstallerBuild = imageWaylandMinimalBuild.override (old: {
        extraPackages = old.extraPackages ++ [ pd.e2fsprogs pd.msdosCmds pd.pdbless pd.installer
          # the stripped base system carries no userland, so the commands pdinstall runs come from it here
          (pkgs.runCommand "puredarwin-installer-commands" { } ''
            mkdir -p $out/sbin $out/usr/sbin
            cp "$(find ${pd.userlandNox} -path '*/sbin/pdinstall' | head -n 1)" $out/sbin/pdinstall
            cp "$(find ${pd.userlandNox} -path '*/sbin/gpt' | head -n 1)" $out/usr/sbin/gpt
            mkdir -p $out/bin
            for c in mount umount; do cp "$(find ${pd.userlandNox} -path "*/sbin/$c" | head -n 1)" $out/sbin/$c; done
            cp "$(find ${pd.userlandNox} -path '*/bin/sync' | head -n 1)" $out/bin/sync
          '')
          # the session opens Install PureDarwin on the drive that has this file, pdinstall leaves it behind
          (pkgs.runCommand "puredarwin-install-media-marker" { } "mkdir -p $out; echo 1 > $out/.pd-install-media")
        ];
        imageFileName = "puredarwin-installer.img";
        rootMB = 1600;
        bootFilesInTree = true;
      });
      # the same system as the install drive, run from a ramdisk so it works from a CD
      installerRamdiskBuild = imageInstallerBuild.override (old: {
        netbootOnly = true;
        useRamdisk = true;
        # the tree is about 650 MB, the rest is what the live system can write
        ramdiskMB = 1024;
        bootArgs = old.bootArgs + " rd=md0";
      });
      # a hybrid ISO: the ESP (loader, kernel, ramdisk) is a file inside the ISO that El Torito boots from a CD,
      # and a GPT entry points at the same file for a USB stick written with dd. inside the ISO volume, because
      # firmware sizes an El Torito image too big for its 16 bit count by where the volume ends
      isoInstallerBuild = pkgs.runCommand "puredarwin-installer-iso" {
        nativeBuildInputs = [ pkgs.dosfstools pkgs.mtools pkgs.xorriso ];
      } ''
        n=${installerRamdiskBuild}
        mkdir -p iso/boot
        # the ESP holds the ramdisk whole, sized to its files plus room for the FAT itself
        kb=$(( ( $(stat -c %s $n/ramdisk.img) + $(stat -c %s $n/kernel) + $(stat -c %s $n/BOOTX64.EFI) ) / 1024 + 16384 ))
        mkfs.vfat -F 32 -n PUREDARWIN -C iso/boot/efi.img $kb >/dev/null
        mmd -i iso/boot/efi.img ::/EFI ::/EFI/BOOT
        mcopy -i iso/boot/efi.img $n/BOOTX64.EFI $n/kernel $n/boot-args.txt ::/EFI/BOOT/
        mcopy -i iso/boot/efi.img $n/ramdisk.img ::/ramdisk.img
        echo "PureDarwin installer, boot it in UEFI mode" > iso/README.TXT
        mkdir -p $out
        xorriso -as mkisofs -o $out/puredarwin-installer.iso -iso-level 3 -V PUREDARWIN -R -J \
          -e boot/efi.img -no-emul-boot -isohybrid-gpt-basdat \
          iso
      '';
      # One body for every UEFI runner. The differences between them are real
      # but small - firmware, accelerator, CPU, how the disk is attached - so
      # they are parameters rather than four near-copies that drift apart.
      # One body for every UEFI runner. The differences between them are real
      # but small - firmware, accelerator, CPU, how the disk is attached - so
      # they are parameters rather than several near-copies that drift apart.
      #
      # The qemu invocation is assembled as an array rather than a
      # backslash-continued command: an option that contributes no arguments
      # would otherwise leave a blank line mid-continuation, which ends the
      # command early and makes the next flag look like a command name.
      mkRunner =
        { name
        , imageNames            # candidates looked for in $PWD and result/
        , imageHint             # the flake target named in the "no image" error
        , firmware ? pkgs.OVMF.fd   # null: must be supplied via the environment
        , machine
        , cpuVar ? "PUREDARWIN_VM_CPU"
        , cpuDefault
        , smpDefault ? "4"
        , diskStyle ? "simple"  # "simple" | "switch" (ahci/virtio via env)
        , network ? false
        , audio ? true
        , pciMmio64 ? true      # OVMF's 64-bit MMIO window; X64 firmware only
        , noReboot ? true
        }:
        let
          # One shell token per element, each emitted already quoted: an
          # unquoted element containing a comma reads as an attempt to separate
          # array elements, and a flag and its value must stay separate argv
          # entries. Values needing expansion carry their own "${...}".
          q = tok: ''"${tok}"'';
          qemuArgs =
            [
              (q "-machine") (q machine)
              (q "-cpu") ''"''${${cpuVar}:-${cpuDefault}}"''
              (q "-smp") ''"''${PUREDARWIN_VM_SMP:-${smpDefault}}"''
              (q "-m") ''"''${PUREDARWIN_VM_MEMORY:-4096}"''
              (q "-vga") ''"''${PUREDARWIN_VM_VGA:-std}"''
            ]
            ++ lib.optionals pciMmio64 [
              (q "-fw_cfg") (q "name=opt/ovmf/X-PciMmio64Mb,string=2048")
            ]
            ++ [
              (q "-drive") ''"if=pflash,format=raw,unit=0,readonly=on,file=$ovmf_code"''
              (q "-drive") ''"if=pflash,format=raw,unit=1,file=$ovmf_vars"''
            ]
            ++ lib.optionals network [
              (q "-device") (q "virtio-net,netdev=net0")
              # the guest's ssh on the host, PUREDARWIN_VM_SSH_PORT picks another port for a second VM
              (q "-netdev") ''"user,id=net0,hostfwd=tcp::''${PUREDARWIN_VM_SSH_PORT:-2222}-:22"''
            ]
            ++ [
              (q "-device") (q "qemu-xhci,id=xhci")
              (q "-device") (q "usb-kbd,bus=xhci.0")
              (q "-device") (q "usb-mouse,bus=xhci.0")
            ]
            ++ lib.optionals audio [
              (q "-device") (q "intel-hda,id=hda")
              (q "-device") (q "hda-duplex,audiodev=snd0")
              (q "-audiodev") ''"''${PUREDARWIN_VM_AUDIODEV:-none},id=snd0"''
            ]
            ++ [ (q "-serial") (q "mon:stdio") ]
            ++ lib.optionals noReboot [ (q "-no-reboot") (q "-no-shutdown") ];
        in
        pkgs.writeShellApplication {
          inherit name;
          runtimeInputs = [ pkgs.qemu ];
          text = ''
            set -euo pipefail

            state_dir="''${PUREDARWIN_VM_STATE_DIR:-$PWD/.${name}}"
            image="''${PUREDARWIN_IMAGE:-}"
            ${if firmware == null then ''
            # No firmware is built for this configuration; see the comment on
            # the runner that sets firmware = null.
            ovmf_code="''${PUREDARWIN_OVMF_CODE:-}"
            ovmf_vars_template="''${PUREDARWIN_OVMF_VARS_TEMPLATE:-}"
            if [ -z "$ovmf_code" ] || [ -z "$ovmf_vars_template" ]; then
              echo "${name}: set PUREDARWIN_OVMF_CODE and PUREDARWIN_OVMF_VARS_TEMPLATE to a 32-bit (IA32) UEFI firmware." >&2
              echo "  There is no IA32 OVMF to default to: edk2 202602 ships only OvmfPkgX64.dsc," >&2
              echo "  OvmfPkgIa32X64.dsc (32-bit PEI but 64-bit DXE, so it loads BOOTX64.EFI, not" >&2
              echo "  BOOTIA32.EFI) and OvmfXen.dsc - upstream dropped the pure-IA32 platform." >&2
              echo "  Use an IA32 build from an older edk2 (OvmfPkgIa32.dsc existed through ~2024)," >&2
              echo "  a distro package, or a dump of the target machine's own firmware." >&2
              exit 1
            fi
            '' else ''
            ovmf_code="''${PUREDARWIN_OVMF_CODE:-${firmware}/FV/OVMF_CODE.fd}"
            ovmf_vars_template="''${PUREDARWIN_OVMF_VARS_TEMPLATE:-${firmware}/FV/OVMF_VARS.fd}"
            ''}
            ovmf_vars="''${PUREDARWIN_OVMF_VARS:-$state_dir/OVMF_VARS.fd}"

            if [ -z "$image" ]; then
              for candidate in ${lib.concatMapStringsSep " " (n: ''"$PWD/${n}" "$PWD/result/${n}"'') imageNames}; do
                if [ -e "$candidate" ]; then
                  image="$candidate"
                  break
                fi
              done
            fi
            if [ -z "$image" ]; then
              echo "${name}: no image found, set PUREDARWIN_IMAGE or run nix build ${imageHint}" >&2
              exit 1
            fi

            image_readonly_opt=""
            if [ ! -w "$image" ]; then
              image_readonly_opt=",snapshot=on"
            fi

            mkdir -p "$state_dir"
            if [ ! -e "$ovmf_vars" ]; then
              cp "$ovmf_vars_template" "$ovmf_vars"
              chmod u+w "$ovmf_vars"
            fi

            # PUREDARWIN_VM_VENUS=1 renders a venus/blob virtio-gpu on the host GPU, and virtio disks move to an iothread
            # so host shader compiles in the render worker don't stall guest I/O into IOStorageFamily timeouts.
            venus_args=()
            disk_iothread=""
            if [ -n "''${PUREDARWIN_VM_VENUS:-}" ]; then
              # virglrenderer dlopens libvulkan.so, the ICDs come from the host driver
              export LD_LIBRARY_PATH="${pkgs.vulkan-loader}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
              venus_args=(
                -vga none
                -device "virtio-gpu-gl-pci,venus=on,blob=on,hostmem=''${PUREDARWIN_VM_HOSTMEM:-1G}"
                -display "''${PUREDARWIN_VM_DISPLAY:-egl-headless,rendernode=''${PUREDARWIN_VM_RENDERNODE:-/dev/dri/renderD128}}"
                -object "iothread,id=io0"
              )
              disk_iothread=",iothread=io0"
            fi
            # PUREDARWIN_VM_DISPLAY outside venus too, e.g. none for a headless run
            display_args=()
            if [ -z "''${PUREDARWIN_VM_VENUS:-}" ] && [ -n "''${PUREDARWIN_VM_DISPLAY:-}" ]; then
              display_args=(-display "$PUREDARWIN_VM_DISPLAY")
            fi

            ${if diskStyle == "switch" then ''
            case "''${PUREDARWIN_VM_DISK:-ahci}" in
              virtio|virtio-blk)
                disk_args=(
                  -drive "if=none,id=system,file=$image,format=raw,cache=writeback$image_readonly_opt"
                  -device "virtio-blk-pci,drive=system$disk_iothread"
                )
                ;;
              ahci)
                disk_args=(
                  -device "ich9-ahci,id=sata"
                  -drive "if=none,id=system,file=$image,format=raw,cache=writeback$image_readonly_opt"
                  -device "ide-hd,bus=sata.0,drive=system"
                )
                ;;
              *)
                echo "PUREDARWIN_VM_DISK must be 'ahci' or 'virtio'" >&2
                exit 1
                ;;
            esac
            '' else ''
            disk_args=(-drive "id=root,format=raw,file=$image$image_readonly_opt")
            ''}

            qemu_args=(
              ${lib.concatStringsSep "\n              " qemuArgs}
            )
            qemu_args+=("''${disk_args[@]}")
            qemu_args+=("''${venus_args[@]}")
            qemu_args+=("''${display_args[@]}")

            # Zero-or-one argument, so it cannot be an array element: an unset
            # variable would contribute an empty argv entry.
            if [ -n "''${PUREDARWIN_VM_NETDUMP:-}" ]; then
              qemu_args+=(-object "filter-dump,id=netdump,netdev=net0,file=$PUREDARWIN_VM_NETDUMP")
            fi

            # GL and Vulkan drivers from this flake's mesa, the host's /run/opengl-driver may need a newer glibc
            # than this qemu has. PUREDARWIN_VM_SYSTEMGL=1 keeps the host's drivers
            if [ -z "''${PUREDARWIN_VM_SYSTEMGL:-}" ]; then
              export __EGL_VENDOR_LIBRARY_DIRS="${pkgs.mesa}/share/glvnd/egl_vendor.d"
              export LIBGL_DRIVERS_PATH="${pkgs.mesa}/lib/dri"
              export GBM_BACKENDS_PATH="${pkgs.mesa}/lib/gbm"
              export __GLX_VENDOR_LIBRARY_NAME=mesa
              export VK_DRIVER_FILES="${pkgs.mesa}/share/vulkan/icd.d/radeon_icd.x86_64.json:${pkgs.mesa}/share/vulkan/icd.d/intel_icd.x86_64.json:${pkgs.mesa}/share/vulkan/icd.d/nouveau_icd.x86_64.json"
              export LD_LIBRARY_PATH="${pkgs.mesa}/lib:${pkgs.libglvnd}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            fi

            exec "''${PUREDARWIN_VM_QEMU:-qemu-system-x86_64}" "''${qemu_args[@]}" "$@"
          '';
        };

      mkUefiRunner = args: mkRunner ({
        imageNames = [ "puredarwin.img" ];
        imageHint = ".#image";
        diskStyle = "switch";
        network = true;
        # As this runner has always behaved: a reboot inside the guest
        # restarts it rather than stopping the machine.
        noReboot = false;
      } // args);

      runVm = mkUefiRunner {
        name = "puredarwin-vm";
        machine = "q35";
        cpuDefault = "IvyBridge,vendor=GenuineIntel";
      };

      runKvm = mkUefiRunner {
        name = "puredarwin-kvm";
        machine = "q35,accel=kvm";
        cpuVar = "PUREDARWIN_KVM_CPU";
        cpuDefault = "host";
      };

      # 32-bit UEFI firmware. edk2 removed OvmfPkgIa32.dsc in stable202511, so
      # the sources are pinned to stable202508 - the last release that has it.
      # OvmfPkgIa32X64.dsc is not a substitute: its DXE phase is 64-bit, so it
      # loads BOOTX64.EFI rather than the BOOTIA32.EFI an ia32 image ships.
      edk2Ia32 = pkgs.pkgsi686Linux.edk2.overrideAttrs (old: {
        version = "202508";
        src = pkgs.fetchFromGitHub {
          owner = "tianocore";
          repo = "edk2";
          rev = "edk2-stable202508";
          fetchSubmodules = true;
          hash = "sha256-YZcjPGPkUQ9CeJS9JxdHBmpdHsAj7T0ifSZWZKyNPMk=";
        };
      });

      ovmfIa32 = (pkgs.pkgsi686Linux.OVMF.override {
        edk2 = edk2Ia32;
        projectDscPath = "OvmfPkg/OvmfPkgIa32.dsc";
        fwPrefix = "OVMF";
        metaPlatforms = [ "i686-linux" "x86_64-linux" ];
      });

      # ia32 differs in exactly two things: the firmware is 32-bit, and OVMF's
      # 64-bit MMIO window does not exist there.
      mkIa32Runner = args: mkUefiRunner ({
        imageNames = [ "puredarwin-ia32-minimal.img" "puredarwin-ia32.img" ];
        imageHint = ".#image-ia32-minimal";
        firmware = ovmfIa32.fd;
        pciMmio64 = false;
      } // args);

      runVmIa32 = mkIa32Runner {
        name = "puredarwin-vm-ia32";
        machine = "q35";
        cpuDefault = "Penryn";
      };

      runKvmIa32 = mkIa32Runner {
        name = "puredarwin-kvm-ia32";
        machine = "q35,accel=kvm";
        cpuVar = "PUREDARWIN_KVM_CPU";
        cpuDefault = "host";
      };

      runVmLegacy = pkgs.writeShellApplication {
        name = "puredarwin-vm-legacy";
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          image="''${PUREDARWIN_IMAGE:-}"
          if [ -z "$image" ]; then
            if [ -e "$PWD/puredarwin-legacy.img" ]; then
              image="$PWD/puredarwin-legacy.img"
            elif [ -e "$PWD/result/puredarwin-legacy.img" ]; then
              image="$PWD/result/puredarwin-legacy.img"
            else
              echo "puredarwin-vm-legacy: no image found, set PUREDARWIN_IMAGE or run nix build .#image-legacy" >&2
              exit 1
            fi
          fi

          image_readonly_opt=""
          if [ ! -w "$image" ]; then
            image_readonly_opt=",snapshot=on"
          fi

          exec qemu-system-x86_64 \
            -machine q35 \
            -cpu "''${PUREDARWIN_VM_CPU:-Penryn}" \
            -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
            -smp "''${PUREDARWIN_VM_SMP:-1}" \
            -vga "''${PUREDARWIN_VM_VGA:-std}" \
            -device piix3-ide,id=legacy-ide \
            -drive "if=none,id=system,file=$image,format=raw$image_readonly_opt" \
            -device ide-hd,bus=legacy-ide.0,drive=system \
            -device qemu-xhci,id=xhci \
            -device usb-kbd,bus=xhci.0 \
            -device usb-mouse,bus=xhci.0 \
            -device intel-hda,id=hda \
            -device hda-duplex,audiodev=snd0 \
            -audiodev "''${PUREDARWIN_VM_AUDIODEV:-none},id=snd0" \
            -serial mon:stdio \
            -no-reboot \
            -no-shutdown \
            "$@"
        '';
      };
      runKvmLegacy = pkgs.writeShellApplication {
        name = "puredarwin-kvm-legacy";
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          image="''${PUREDARWIN_IMAGE:-}"
          if [ -z "$image" ]; then
            if [ -e "$PWD/puredarwin-legacy.img" ]; then
              image="$PWD/puredarwin-legacy.img"
            elif [ -e "$PWD/result/puredarwin-legacy.img" ]; then
              image="$PWD/result/puredarwin-legacy.img"
            else
              echo "puredarwin-kvm-legacy: no image found, set PUREDARWIN_IMAGE or run nix build .#image-legacy" >&2
              exit 1
            fi
          fi

          image_readonly_opt=""
          if [ ! -w "$image" ]; then
            image_readonly_opt=",snapshot=on"
          fi

          exec qemu-system-x86_64 \
            -machine q35,accel=kvm \
            -cpu "''${PUREDARWIN_KVM_CPU:-host}" \
            -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
            -smp "''${PUREDARWIN_VM_SMP:-1}" \
            -vga "''${PUREDARWIN_VM_VGA:-std}" \
            -device piix3-ide,id=legacy-ide \
            -drive "if=none,id=system,file=$image,format=raw,cache=writeback$image_readonly_opt" \
            -device ide-hd,bus=legacy-ide.0,drive=system \
            -device virtio-net,netdev=net0 \
            -netdev user,id=net0,hostfwd=tcp::2222-:22 \
            ''${PUREDARWIN_VM_NETDUMP:+-object filter-dump,id=netdump,netdev=net0,file="$PUREDARWIN_VM_NETDUMP"} \
            -device qemu-xhci,id=xhci \
            -device usb-kbd,bus=xhci.0 \
            -device usb-mouse,bus=xhci.0 \
            -device intel-hda,id=hda \
            -device hda-duplex,audiodev=snd0 \
            -audiodev "''${PUREDARWIN_VM_AUDIODEV:-none},id=snd0" \
            -serial mon:stdio \
            -no-reboot \
            -no-shutdown \
            "$@"
        '';
      };
      # xnu-loader as a Linux bzImage: QEMU -kernel loads it, and the initrd is a
      # cpio of the minimal image's kernel collection and boot-args
      kernelBootMinimal = pkgs.runCommand "puredarwin-kernel-boot-minimal" {
        nativeBuildInputs = [ pkgs.cpio ];
      } ''
        mkdir -p root/EFI/BOOT $out
        cp ${kcBuild}/kernel root/EFI/BOOT/kernel
        printf '%s' ${lib.escapeShellArg minimalBootArgs} > root/EFI/BOOT/boot-args.txt
        (cd root && find EFI | sort | cpio -o -H newc --reproducible --quiet) > $out/initrd.cpio
        cp ${xnu-loader.packages.${system}.kernel-multiboot2}/boot/xnu-loader.bzImage $out/
      '';
      wslBootArgs = "keepsyms=1 rd=md0 pd_pivot_root=1";
      wslDebugBootArgs = "-v keepsyms=1 serial=3 rd=md0 pd_pivot_root=1";
      wslImage = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemMinimal;
        extraPackages = strippedExtraPackages;
        kc = kcBuild;
        xnuLoader = xnuLoaderDefault;
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-wsl-root.img";
        espMB = 60;
        rootMB = 260;
        bootArgs = minimalBootArgs;
        rootfsTarball = true;
      };
      mkWslStub = bootArgs: pkgs.runCommand "puredarwin-wsl-stub" {
        nativeBuildInputs = [ pkgs.e2fsprogs pkgs.fakeroot pkgs.cpio ];
      } ''
        stub=stub-root
        mkdir -p $stub/sbin $stub/dev $stub/mnt/root $stub/private/tmp
        # Take the stub's files from the rootfs itself so both always match
        tar -xzf ${wslImage}/rootfs.tar.gz -C $stub \
          ./usr/libexec/wslinit ./usr/lib/dyld ./usr/lib/libSystem.B.dylib \
          ./usr/lib/system/libdyld.dylib ./usr/lib/libobjc.A.dylib \
          ./usr/lib/libc++abi.dylib ./usr/lib/libc++.1.dylib
        mv $stub/usr/libexec/wslinit $stub/sbin/launchd
        chmod -R u+w $stub
        # mke2fs -d skips empty directories, so give the mount points a placeholder
        for d in dev mnt/root private/tmp; do touch $stub/$d/.keep; done
        fakeroot sh -c "chown -R 0:0 $stub && mke2fs -q -F -t ext4 -b 4096 -O ^orphan_file -L wsl-stub -d $stub ramdisk.img 16M"
        mkdir -p root/EFI/BOOT $out
        cp ramdisk.img root/ramdisk.img
        cp ${kcBuild}/kernel root/EFI/BOOT/kernel
        printf '%s' ${lib.escapeShellArg bootArgs} > root/EFI/BOOT/boot-args.txt
        (cd root && find . -type f | sed 's|^\./||' | sort | cpio -o -H newc --reproducible --quiet) > $out/initrd.cpio
      '';
      mkWslBundle = name: bootArgs: let
        loader = xnu-loader.packages.${system}.kernel-multiboot2.override {
          embeddedInitrd = "${mkWslStub bootArgs}/initrd.cpio";
        };
      in pkgs.runCommand name { } ''
        mkdir -p $out
        cp ${loader}/boot/xnu-loader.bzImage $out/puredarwin.bzImage
        cp ${wslImage}/rootfs.tar.gz $out/rootfs.tar.gz
        cat > $out/wslconfig.example <<'EOF'
[wsl2]
kernel=C:\\WSL\\PureDarwin\\puredarwin.bzImage
processors=1
networkingMode=none
dnsTunneling=false
guiApplications=false
telemetry=false
kernelBootTimeout=1800000
EOF
        cat > $out/install.ps1 <<'EOF'
# Installs PureDarwin as a WSL2 distribution. The kernel setting applies to every
# WSL2 distribution on this machine. An existing .wslconfig is kept as .wslconfig.bak
param([string]$Target = "C:\WSL\PureDarwin")
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $Target | Out-Null
Copy-Item "$PSScriptRoot\puredarwin.bzImage" $Target -Force
$config = Join-Path $env:USERPROFILE ".wslconfig"
wsl --shutdown
# WSL unpacks the tarball inside its utility VM, so import with the stock kernel first
if (Test-Path $config) { Move-Item $config "$config.bak" -Force }
wsl --import PureDarwin (Join-Path $Target "disk") "$PSScriptRoot\rootfs.tar.gz" --version 2
if ($LASTEXITCODE -ne 0) { throw "wsl --import failed" }
wsl --shutdown
# .wslconfig paths need doubled backslashes
$escaped = $Target.Replace('\', '\\')
(Get-Content "$PSScriptRoot\wslconfig.example").Replace('C:\\WSL\\PureDarwin', $escaped) | Set-Content $config
Write-Host "Done: wsl -d PureDarwin"
EOF
      '';
      runKernelMinimal = pkgs.writeShellApplication {
        name = "puredarwin-kernel-minimal";
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          image="''${PUREDARWIN_IMAGE:-${imageMinimalBuild}/puredarwin-minimal.img}"
          image_readonly_opt=""
          if [ ! -w "$image" ]; then
            image_readonly_opt=",snapshot=on"
          fi
          accel=tcg
          cpu="''${PUREDARWIN_VM_CPU:-Penryn}"
          if [ -w /dev/kvm ]; then
            accel=kvm
            cpu="''${PUREDARWIN_KVM_CPU:-host}"
          fi

          # Legacy IDE at 0x1f0: the loader's disk probe finds the ext4 root there
          exec qemu-system-x86_64 \
            -machine "q35,accel=$accel" \
            -cpu "$cpu" \
            -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
            -smp "''${PUREDARWIN_VM_SMP:-2}" \
            -vga "''${PUREDARWIN_VM_VGA:-std}" \
            -kernel ${kernelBootMinimal}/xnu-loader.bzImage \
            -initrd ${kernelBootMinimal}/initrd.cpio \
            -device piix3-ide,id=legacy-ide \
            -drive "if=none,id=system,file=$image,format=raw$image_readonly_opt" \
            -device ide-hd,bus=legacy-ide.0,drive=system \
            -device virtio-net,netdev=net0 \
            -netdev user,id=net0 \
            -device qemu-xhci,id=xhci \
            -device usb-kbd,bus=xhci.0 \
            -device usb-mouse,bus=xhci.0 \
            -serial mon:stdio \
            -no-reboot \
            -no-shutdown \
            "$@"
        '';
      };
      # GRUB image runners. Legacy IDE so the loader's disk probe sees the root
      mkGrubLinuxRunner = { name, uefi }: pkgs.writeShellApplication {
        inherit name;
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          image="''${PUREDARWIN_IMAGE:-${imageGrubLinuxMinimalBuild}/puredarwin-grub-linux-minimal.img}"
          image_readonly_opt=""
          if [ ! -w "$image" ]; then
            image_readonly_opt=",snapshot=on"
          fi
          accel=tcg
          cpu="''${PUREDARWIN_VM_CPU:-Penryn}"
          if [ -w /dev/kvm ]; then
            accel=kvm
            cpu="''${PUREDARWIN_KVM_CPU:-host}"
          fi
          firmware_args=()
          ${lib.optionalString uefi ''
          state_dir="''${PUREDARWIN_VM_STATE_DIR:-$PWD/.${name}}"
          mkdir -p "$state_dir"
          if [ ! -e "$state_dir/OVMF_VARS.fd" ]; then
            cp ${pkgs.OVMF.fd}/FV/OVMF_VARS.fd "$state_dir/OVMF_VARS.fd"
            chmod u+w "$state_dir/OVMF_VARS.fd"
          fi
          # XNU cannot map OVMF's default 64-bit BAR window, so keep it low
          firmware_args=(
            -fw_cfg "name=opt/ovmf/X-PciMmio64Mb,string=2048"
            -drive "if=pflash,format=raw,readonly=on,file=${pkgs.OVMF.fd}/FV/OVMF_CODE.fd"
            -drive "if=pflash,format=raw,file=$state_dir/OVMF_VARS.fd"
          )
          ''}

          exec qemu-system-x86_64 \
            -machine "q35,accel=$accel" \
            -cpu "$cpu" \
            -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
            -smp "''${PUREDARWIN_VM_SMP:-2}" \
            -vga "''${PUREDARWIN_VM_VGA:-std}" \
            "''${firmware_args[@]}" \
            -device piix3-ide,id=legacy-ide \
            -drive "if=none,id=system,file=$image,format=raw$image_readonly_opt" \
            -device ide-hd,bus=legacy-ide.0,drive=system,bootindex=0 \
            -device virtio-net,netdev=net0 \
            -netdev user,id=net0 \
            -device qemu-xhci,id=xhci \
            -device usb-kbd,bus=xhci.0 \
            -device usb-mouse,bus=xhci.0 \
            -serial mon:stdio \
            -no-reboot \
            -no-shutdown \
            "$@"
        '';
      };
      runGrubLinuxMinimal = mkGrubLinuxRunner { name = "puredarwin-grub-linux-minimal"; uefi = false; };
      runGrubLinuxMinimalUefi = mkGrubLinuxRunner { name = "puredarwin-grub-linux-minimal-uefi"; uefi = true; };
      runArm64Uefi = pkgs.writeShellApplication {
        name = "puredarwin-arm64-uefi";
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          state_dir="''${PUREDARWIN_ARM64_UEFI_STATE_DIR:-$PWD/.puredarwin-arm64-uefi}"
          aavmf_code="''${PUREDARWIN_AAVMF_CODE:-${pkgs.pkgsCross.aarch64-multiplatform.OVMF.fd}/FV/AAVMF_CODE.fd}"
          aavmf_vars_template="''${PUREDARWIN_AAVMF_VARS_TEMPLATE:-${pkgs.pkgsCross.aarch64-multiplatform.OVMF.fd}/FV/AAVMF_VARS.fd}"
          aavmf_vars="''${PUREDARWIN_AAVMF_VARS:-$state_dir/AAVMF_VARS.fd}"

          mkdir -p "$state_dir"
          if [ "''${PUREDARWIN_ARM64_RESET_VARS:-0}" = 1 ]; then
            rm -f "$aavmf_vars"
          fi
          if [ ! -e "$aavmf_vars" ]; then
            cp "$aavmf_vars_template" "$aavmf_vars"
            chmod u+w "$aavmf_vars"
          fi

          exec qemu-system-aarch64 \
            -machine virt,gic-version=3 \
            -cpu "''${PUREDARWIN_ARM64_VM_CPU:-max}" \
            -m "''${PUREDARWIN_VM_MEMORY:-1024}" \
            -drive if=pflash,format=raw,unit=0,readonly=on,file="$aavmf_code" \
            -drive if=pflash,format=raw,unit=1,file="$aavmf_vars" \
            -device virtio-gpu-pci \
            -serial mon:stdio \
            -display "''${PUREDARWIN_ARM64_UEFI_DISPLAY:-gtk}" \
            -no-reboot \
            -no-shutdown \
            "$@"
        '';
      };
      runArm64Uboot = pkgs.writeShellApplication {
        name = "puredarwin-arm64-uboot";
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          image="''${PUREDARWIN_IMAGE:-}"
          if [ -z "$image" ]; then
            if [ -e "$PWD/puredarwin-arm64-virt.img" ]; then
              image="$PWD/puredarwin-arm64-virt.img"
            elif [ -e "$PWD/result/puredarwin-arm64-virt.img" ]; then
              image="$PWD/result/puredarwin-arm64-virt.img"
            else
              echo "puredarwin-arm64-uboot: no image found" >&2
              exit 1
            fi
          fi

          image_readonly_opt=""
          if [ ! -w "$image" ]; then
            image_readonly_opt=",snapshot=on"
          fi

          exec qemu-system-aarch64 \
            -machine virt,gic-version=3 \
            -cpu "''${PUREDARWIN_ARM64_VM_CPU:-max}" \
            -smp "''${PUREDARWIN_VM_SMP:-4}" \
            -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
            -bios "${pkgs.pkgsCross.aarch64-multiplatform.ubootQemuAarch64}/u-boot.bin" \
            -drive if=none,id=system,file="$image",format=raw$image_readonly_opt \
            -device ich9-ahci,id=ahci0 \
            -device ide-hd,drive=system,bus=ahci0.0 \
            -serial mon:stdio \
            -display none \
            -no-reboot \
            -no-shutdown \
            "$@"
        '';
      };
      runArm64Virt = pkgs.writeShellApplication {
        name = "puredarwin-arm64-virt";
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          state_dir="''${PUREDARWIN_ARM64_VM_STATE_DIR:-$PWD/.puredarwin-arm64-virt}"
          image="''${PUREDARWIN_IMAGE:-}"
          aavmf_code="''${PUREDARWIN_AAVMF_CODE:-${pkgs.pkgsCross.aarch64-multiplatform.OVMF.fd}/FV/AAVMF_CODE.fd}"
          aavmf_vars_template="''${PUREDARWIN_AAVMF_VARS_TEMPLATE:-${pkgs.pkgsCross.aarch64-multiplatform.OVMF.fd}/FV/AAVMF_VARS.fd}"
          aavmf_vars="''${PUREDARWIN_AAVMF_VARS:-$state_dir/AAVMF_VARS.fd}"

          vga_args=()
          if [ "''${PUREDARWIN_VM_VGA:-ramfb}" != none ]; then
            vga_args=(-device "''${PUREDARWIN_VM_VGA:-ramfb}")
          fi

          # PUREDARWIN_VM_VENUS=1 as in the x86 runners. PUREDARWIN_VM_HOSTGL names a host library dir (the Pi's v3dv setup) put first on the path,
          # and its v3dv-rdwr.so is preloaded when present.
          venus_args=()
          disk_iothread=""
          if [ -n "''${PUREDARWIN_VM_VENUS:-}" ]; then
            export LD_LIBRARY_PATH="''${PUREDARWIN_VM_HOSTGL:+$PUREDARWIN_VM_HOSTGL:}${pkgs.vulkan-loader}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            if [ -e "''${PUREDARWIN_VM_HOSTGL:-/nonexistent}/v3dv-rdwr.so" ]; then
              export LD_PRELOAD="$PUREDARWIN_VM_HOSTGL/v3dv-rdwr.so"
            fi
            venus_args=(
              -device "virtio-gpu-gl-pci,venus=on,blob=on,hostmem=''${PUREDARWIN_VM_HOSTMEM:-1G}"
              -display "''${PUREDARWIN_VM_DISPLAY:-egl-headless,rendernode=''${PUREDARWIN_VM_RENDERNODE:-/dev/dri/renderD128}}"
              -object "iothread,id=io0"
            )
            disk_iothread=",iothread=io0"
          fi
          # PUREDARWIN_VM_DISPLAY outside venus too, e.g. none for a headless run
          display_args=()
          if [ -z "''${PUREDARWIN_VM_VENUS:-}" ] && [ -n "''${PUREDARWIN_VM_DISPLAY:-}" ]; then
            display_args=(-display "$PUREDARWIN_VM_DISPLAY")
          fi

          if [ -z "$image" ]; then
            if [ -e "$PWD/puredarwin-arm64-virt.img" ]; then
              image="$PWD/puredarwin-arm64-virt.img"
            elif [ -e "$PWD/result/puredarwin-arm64-virt.img" ]; then
              image="$PWD/result/puredarwin-arm64-virt.img"
            else
              echo "puredarwin-arm64-virt: no image found, run nix build .#image-arm64-virt" >&2
              exit 1
            fi
          fi

          image_readonly_opt=""
          if [ ! -w "$image" ]; then
            image_readonly_opt=",snapshot=on"
          fi

          mkdir -p "$state_dir"
          if [ "''${PUREDARWIN_ARM64_RESET_VARS:-0}" = 1 ]; then
            rm -f "$aavmf_vars"
          fi
          if [ ! -e "$aavmf_vars" ]; then
            cp "$aavmf_vars_template" "$aavmf_vars"
            chmod u+w "$aavmf_vars"
          fi

          exec qemu-system-aarch64 \
            -machine virt,gic-version=3 \
            -boot order=c,strict=on \
            -cpu "''${PUREDARWIN_ARM64_VM_CPU:-neoverse-n2}" \
            -smp "''${PUREDARWIN_VM_SMP:-4}" \
            -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
            -drive if=pflash,format=raw,unit=0,readonly=on,file="$aavmf_code" \
            -drive if=pflash,format=raw,unit=1,file="$aavmf_vars" \
            -drive if=none,id=system,file="$image",format=raw$image_readonly_opt \
            -device "virtio-blk-pci,drive=system,bootindex=1$disk_iothread" \
            "''${vga_args[@]}" \
            "''${venus_args[@]}" \
            "''${display_args[@]}" \
            -device virtio-net-pci,netdev=net0 \
            -netdev user,id=net0,hostfwd=tcp::2223-:22 \
            -device qemu-xhci,id=xhci \
            -device usb-kbd,bus=xhci.0 \
            -device usb-mouse,bus=xhci.0 \
            -serial mon:stdio \
            -no-reboot \
            -no-shutdown \
            "$@"
        '';
      };
    in {
      darwin-cross-toolchain = darwinCrossToolchain;
      native-ld = nativeLd;
      kc = kcBuild;
      kc-debug = kcDebugBuild;
      kc-arm64-debug = kcArm64DebugBuild;
      kc-arm64 = kcArm64ReleaseBuild;
      kc-arm64-a733 = kcArm64A733ReleaseBuild;
      kc-arm64-t8010-debug = kcArm64T8010DebugBuild;
      kc-arm64-s8000-debug = kcArm64S8000DebugBuild;
      kc-arm64-t8103-debug = kcArm64T8103DebugBuild;
      image-arm64-t8103 = imageArm64T8103Build;
      ramdisk-arm64-t8010 = ramdiskArm64T8010Build;
      ramdisk-arm64-s8000 = ramdiskArm64S8000Build;
      ramdisk-arm64-sun50i = ramdiskArm64Sun50iBuild;
      ramdisk-arm64-tegra210 = ramdiskArm64Tegra210Build;
      ramdisk-arm64-tegra210-sd = ramdiskArm64Tegra210SdBuild;
      ramdisk-arm64-tegra210-sd-desktop = ramdiskArm64Tegra210SdDesktopBuild;
      ramdisk-arm64-tegra210-mainline-dtb = ramdiskArm64Tegra210MainlineDtbBuild;
      ramdisk-arm64-tegra210-sd-mainline-dtb = ramdiskArm64Tegra210SdMainlineDtbBuild;
      ramdisk-arm64-sc8280xp = ramdiskArm64Sc8280xpBuild;
      ramdisk-arm64-sdm670 = ramdiskArm64Sdm670Build;
      bootimg-arm64-sdm670-sargo = bootimgArm64Sdm670 "sargo";
      bootimg-arm64-sdm670-bonito = bootimgArm64Sdm670 "bonito-tianma";
      kc-arm64-t8010 = kcArm64T8010ReleaseBuild;
      kc-arm64-virt4k-debug = kcArm64Virt4kDebugBuild;
      kc-arm64-sun50i = kcArm64Sun50iReleaseBuild;
      kc-arm64-tegra210 = kcArm64Tegra210Build;
      kc-arm64-msm8916 = kcArm64Msm8916Build;
      kc-arm64-sun50i-debug = kcArm64Sun50iDebugBuild;
      kc-arm64-sc8280xp = kcArm64Sc8280xpReleaseBuild;
      kc-arm64-sdm670 = kcArm64Sdm670Build;
      kc-arm64-sc8280xp-debug = kcArm64Sc8280xpDebugBuild;
      kc-arm64-bcm2837 = kcArm64Bcm2837ReleaseBuild;
      kc-arm64-bcm2837-debug = kcArm64Bcm2837DebugBuild;
      prelinked-arm32-bcm2835 = prelinkedArm32Bcm2835Build;
      prelinked-arm32-bcm2835-debug = prelinkedArm32Bcm2835DebugBuild;
      prelinked-arm32-bcm2835-dev = prelinkedArm32Bcm2835DevBuild;
      corefoundation = pd.corefoundation;
      icucore = pd.icu;
      libcxxabi-dylib = pd.libcxxabiDylib;
      libcxx-dylib = pd.libcxxDylib;
      libcxx-test = pd.libcxxTest;
      mesa = pd.mesaBuild;
      libobjc = pd.libobjc;
      objc-test = pd.objcTest;
      foundation = pd.foundation;
      autoconf = pd.autoconf;
      automake = pd.automake;
      iokit = pd.iokit;
      coreServices = pd.coreservices;
      security = pd.security;
      symptomReporter = pd.symptomReporter;
      systemstarter = pd.systemStarter;
      launchd = pd.launchd;
      launchctl = pd.launchctl;
      image-arm64-virt = imageArm64VirtBuild;
      image-arm64-virt-debug = imageArm64VirtDebugBuild;
      image-arm64-bcm2837 = imageArm64Bcm2837Build;
      image-arm64-virt-minimal = imageArm64VirtMinimalBuild;
      image-arm64-sg2002 = imageArm64Sg2002Build;
      netboot-arm64-virt-minimal = netbootArm64VirtMinimalBuild;
      image-arm64-virt-minimal-release = imageArm64VirtMinimalReleaseBuild;
      image-arm64-virt-wayland = imageArm64VirtWaylandBuild;
      image-arm64-virt-wayland-debug = imageArm64VirtWaylandDebugBuild;
      image-hfs = imageHfsBuild;
      image-apfs = imageApfsBuild;
      image-debug = imageDebugBuild;
      onyx2d = pd.onyx2d;
      coregraphics = pd.coregraphics;
      coretext = pd.coretext;
      coredata = pd.coredata;
      pd-epoll-shim = pd.epollShim;
      tllist = pd.tllist;
      fcft = pd.fcft;
      foot = pd.foot;
      cairo-nox = pd.cairoNox;
      dbus-nox = pd.dbusNox;
      mesa-nox = pd.mesaNox;
      opengl-framework-nox = pd.openglFrameworkNox;
      mesa-demos-nox = pd.mesaDemosNox;
      image-wayland = imageWaylandBuild;
      # the same image under the desktop's name
      image-argon = imageWaylandBuild;
      # the Argon image with one local account, so it boots to the login screen
      image-login-test = imageWaylandBuild.override {
        defaultUser = { name = "tester"; realName = "Test Person"; password = "argon"; };
      };
      image-wayland-minimal = imageWaylandMinimalBuild;
      image-installer = imageInstallerBuild;
      iso-installer = isoInstallerBuild;
      wlroots-nox = pd.wlrootsNox;
      sway-nox = pd.swayNox;
      gtk3-nox = pd.gtk3Nox;
      image-minimal = imageMinimalBuild;
      e2fsprogs = pd.e2fsprogs;
      image-tools-test = imageToolsTestBuild;
      image-frameworks-test = imageFrameworksTestBuild;
      image-cg-test = imageCgTestBuild;
      msdos-cmds = pd.msdosCmds;
      pdbless = pd.pdbless;
      image-aux-test = imageAuxTestBuild;
      image-builder = imageBuilderBuild;
      # the 32-bit uefi loader, so other images can carry it next to BOOTX64.EFI
      xnu-loader-ia32 = xnuLoaderIa32;
      inherit x86System;
      xorg = pd.xorg;
      libxcvt = pd.libxcvt;
      userland = pd.userland;
      vm-runner = runVm;
      kvm-runner = runKvm;
      vm-ia32-runner = runVmIa32;
      kvm-ia32-runner = runKvmIa32;
      vm-legacy-runner = runVmLegacy;
      kvm-legacy-runner = runKvmLegacy;
      kernel-minimal-runner = runKernelMinimal;
      grub-linux-minimal-runner = runGrubLinuxMinimal;
      grub-linux-minimal-uefi-runner = runGrubLinuxMinimalUefi;
      kernel-boot-minimal = kernelBootMinimal;
      wsl = mkWslBundle "puredarwin-wsl" wslBootArgs;
      wsl-debug = mkWslBundle "puredarwin-wsl-debug" wslDebugBootArgs;
      arm64-virt-runner = runArm64Virt;
      arm64-uefi-runner = runArm64Uefi;
      arm64-uboot-runner = runArm64Uboot;
    };

  linuxApps =
    let
      runVm = linuxPackages.vm-runner;
      runKvm = linuxPackages.kvm-runner;
      runVmLegacy = linuxPackages.vm-legacy-runner;
      runKvmLegacy = linuxPackages.kvm-legacy-runner;
      runVmIa32 = linuxPackages.vm-ia32-runner;
      runKvmIa32 = linuxPackages.kvm-ia32-runner;
      runVirt = linuxPackages.arm64-virt-runner;
    in {
      default = {
        type = "app";
        program = "${runVm}/bin/puredarwin-vm";
      };
      vm = {
        type = "app";
        program = "${runVm}/bin/puredarwin-vm";
      };
      grub-linux-minimal = {
        type = "app";
        program = "${linuxPackages.grub-linux-minimal-runner}/bin/puredarwin-grub-linux-minimal";
      };
      grub-linux-minimal-uefi = {
        type = "app";
        program = "${linuxPackages.grub-linux-minimal-uefi-runner}/bin/puredarwin-grub-linux-minimal-uefi";
      };
      kernel-minimal = {
        type = "app";
        program = "${linuxPackages.kernel-minimal-runner}/bin/puredarwin-kernel-minimal";
      };
      vm-legacy = {
        type = "app";
        program = "${runVmLegacy}/bin/puredarwin-vm-legacy";
      };
      arm64-virt = {
        type = "app";
        program = "${runVirt}/bin/puredarwin-arm64-virt";
      };
      arm64-uefi = {
        type = "app";
        program = "${linuxPackages.arm64-uefi-runner}/bin/puredarwin-arm64-uefi";
      };
      arm64-uboot = {
        type = "app";
        program = "${linuxPackages.arm64-uboot-runner}/bin/puredarwin-arm64-uboot";
      };
      kvm = {
        type = "app";
        program = "${runKvm}/bin/puredarwin-kvm";
      };
      kvm-legacy = {
        type = "app";
        program = "${runKvmLegacy}/bin/puredarwin-kvm-legacy";
      };
      vm-ia32 = {
        type = "app";
        program = "${runVmIa32}/bin/puredarwin-vm-ia32";
      };
      kvm-ia32 = {
        type = "app";
        program = "${runKvmIa32}/bin/puredarwin-kvm-ia32";
      };
    };
in {
  inherit
    fullBuild
    splitBaseSystem
    splitBaseSystemStripped
    splitBaseSystemMinimal
    imageExtraPackageSet
    commonPackages
    linuxPackages
    linuxApps
    ;
}
