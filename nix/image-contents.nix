# What actually goes into an image: the base system splits, the extra guest
# package set layered on top, and the host-side (Linux) tooling used to build
# and run them.
{ atspi2CoreBuild
, autoconfBuild
, automakeBuild
, bisonBuild
, bmakeBuild
, cairoBuild
, cairoGobjectBuild
, cctoolsBuild
, coreFoundationBuild
, curlBuild
, darwinCrossToolchain
, dbusBuild
, dilloBuild
, diskArbitrationBuild
, dmenuBuild
, exoBuild
, expatBuild
, fastfetchBuild
, fbdoomBuild
, fbdoomExternalSrc
, fileBuild
, flexBuild
, fontconfigBuild
, foundationBuild
, freetype2Build
, fribidiBuild
, garconBuild
, gdkPixbufBuild
, gitBuild
, glibBuild
, gnum4Build
, gnumakeBuild
, gtk3Build
, gtkLayerShellBuild
, harfbuzzBuild
, i3Build
, i3statusShimBuild
, iceauthBuild
, cursorThemeBuild
, iconThemesBuild
, icuCoreBuild
, imageExtraPackagesArm64
, iographicsBuild
, iokitBuild
, coreServicesBuild
, jsoncBuild
, wineBuild
, libX11SharedBuild
, libxcbSharedBuild
, libXauSharedBuild
, libXdmcpSharedBuild
, libXextSharedBuild
, libXrenderSharedBuild
, libXfixesSharedBuild
, libXiSharedBuild
, libXcursorSharedBuild
, libXrandrSharedBuild
, nettleSharedBuild
, gnutlsSharedBuild
, glibNetworkingBuild
, llvmCrossBuild
, vulkanLoaderBuild
, vulkanToolsBuild
, libxshmfenceSharedBuild
, iomediacheckBuild
, ioregBuild
, isDarwin
, kc-tools
, kernelArm64Build
, kernelArm64VirtBuild
, kernelArm64VirtDebugBuild
, kernelBuild
, kernelDebugBuild
, kextsArm64Build
, kextsBuild
, launchctlBuild
, launchdBuild
, lib
, libSystemBuild
, libdrmBuild
, pdsurfaceBuild
, libgbmBuild
, waylandBuild
, waylandProtocolsBuild
, wlrootsBuild
, swayBuild
, xwaylandBuild
, libXftBuild
, libapfsrwBuild
, libcrocoBuild
, libcssBuild
, libcurlDylibBuild
, libcxxDylibBuild
, libcxxTestBuild
, libcxxabiDylibBuild
, libdisplayInfoBuild
, libdomBuild
, libepoxyBuild
, libevBuild
, libffiBuild
, libhubbubBuild
, libiconvArm64Build
, libiconvBuild
, libnsbmpBuild
, libnsgifBuild
, libnsutilsBuild
, libobjcBuild
, libparserutilsBuild
, libpngBuild
, librsvgBuild
, gettextBuild
, webkitgtkBuild
, libsoupBuild
, sqliteBuild
, libpslBuild
, nghttp2Build
, libgcryptBuild
, libgpgErrorBuild
, libtasn1Build
, libjpegBuild
, libwebpBuild
, libutf8procBuild
, libwapcapletBuild
, libwnckBuild
, libxfce4uiBuild
, libxfce4utilBuild
, libxfce4windowingBuild
, libxml2Build
, libzDylibBuild
, mesaBuild
, mesaDemosBuild
, migcomDarwinBuild
, mkPureDarwinBuild
, nanoBuild
, ninjaBuild
, cmakeBuild
, clangCrossBuild
, kcToolsGuestBuild
, mesonBuild
, nativeLd
, ncursesBuild
, netsurfBuild
, objcTestBuild
, openglFrameworkBuild
, opensshBuild
, opensslBuild
, pangoBuild
, pcre2Build
, pdVirglShimBuild
, pkgconfBuild
, pkgs
, pythonBuild
, securityBuild
, splitBaseSystemArm64VirtMinimal
, splitBaseSystemArm64VirtMinimalRelease
, startupNotificationBuild
, system
, systemConfigurationBuild
, systemStarterBuild
, tccBuild
, toyboxArm64Build
, toyboxBuild
, userlandBuild
, vteBuild
, xcalcBuild
, xcbBuild
, xcbCursorBuild
, xcbImageBuild
, xcbKeysymsBuild
, xcbRenderUtilBuild
, xcbUtilBuild
, xcbWmBuild
, xcbXrmBuild
, xclockBuild
, xeyesBuild
, thunarBuild
, xfce4AppfinderBuild
, xfce4PanelBuild
, xfce4SessionBuild
, xfce4SettingsBuild
, xfce4TerminalBuild
, xfconfBuild
, xfdesktopBuild
, xfwm4Build
, xinitBuild
, xkbcommonBuild
, xvfbPixmanBuild
, xkbcompBuild
, xkeyboardConfigBuild
, xlibBuild
, xlibLocaleBuild
, xmessageBuild
, xnu-loader
, xnuHeadersBuild
, xorgBuild
, xrdbBuild
, xrandrBuild
, xtermBuild
, xvfbBuild
, xvfbFontsBuild
, xvfbLibICEBuild
, xvfbLibSMBuild
, xvfbLibXauBuild
, xvfbLibXcompositeBuild
, xvfbLibXcursorBuild
, xvfbLibXdamageBuild
, xvfbLibXdmcpBuild
, xvfbLibXextBuild
, xvfbLibXfixesBuild
, xvfbLibXineramaBuild
, xvfbLibXkbfileBuild
, xvfbLibXpresentBuild
, xvfbLibXrandrBuild
, xvfbLibXrenderBuild
, xvfbLibXresBuild
, xvfbLibxcvtBuild
, xvfbZlibBuild
, xxdBuild
, xzBuild
, yajlBuild
, asmjitTestArm64Build
, jitTestArm64Build
, zshArm64Build
, zshBuild
}:

let
  fullBuild = mkPureDarwinBuild {
    pname = "puredarwin";
    src = ../.;
    buildTargets = [ "xnu" "kexts" "libsystem_kernel" "pcmplay" ];
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
    cp -a ${libSystemBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${userlandBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${tccBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${cctoolsBuild}/. "$out/"
  '' + lib.optionalString (!isDarwin && launchdBuild != null) ''
    chmod -R u+w "$out"
    cp -a ${launchdBuild}/. "$out/"
    chmod -R u+w "$out"
  '' + lib.optionalString (!isDarwin && launchctlBuild != null) ''
    cp -a ${launchctlBuild}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
  ''
  + lib.optionalString (!isDarwin) ''
    chmod -R u+w "$out"
    cp -a ${bmakeBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${xvfbBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${xeyesBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${xkbcompBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${xkeyboardConfigBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${xvfbFontsBuild}/. "$out/"
  '');
  splitBaseSystemStripped = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } (''
    mkdir -p "$out"
    cp -a ${kernelBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${kextsBuild}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libSystemBuild}/. "$out/"
  '' + lib.optionalString (!isDarwin && launchdBuild != null) ''
    chmod -R u+w "$out"
    cp -a ${launchdBuild}/. "$out/"
    chmod -R u+w "$out"
  '' + lib.optionalString (!isDarwin && launchctlBuild != null) ''
    cp -a ${launchctlBuild}/. "$out/"
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
    cp -a ${libSystemBuild}/. "$out/"
  '' + lib.optionalString (!isDarwin && launchdBuild != null) ''
    chmod -R u+w "$out"
    cp -a ${launchdBuild}/. "$out/"
    chmod -R u+w "$out"
  '' + lib.optionalString (!isDarwin && launchctlBuild != null) ''
    cp -a ${launchctlBuild}/. "$out/"
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
    cp -a ${userlandBuild}/. "$out/"
  '';


  splitBaseSystemMinimalDebug = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } ''
    mkdir -p "$out"
    cp -a ${splitBaseSystemStrippedDebug}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${userlandBuild}/. "$out/"
  '';

  imageExtraPackageSet = lib.optionalAttrs (!isDarwin) {
    wine = wineBuild;
    libX11-shared = libX11SharedBuild;
    libxcb-shared = libxcbSharedBuild;
    libXau-shared = libXauSharedBuild;
    libXdmcp-shared = libXdmcpSharedBuild;
    libXext-shared = libXextSharedBuild;
    libXrender-shared = libXrenderSharedBuild;
    libXfixes-shared = libXfixesSharedBuild;
    libXi-shared = libXiSharedBuild;
    libXcursor-shared = libXcursorSharedBuild;
    libXrandr-shared = libXrandrSharedBuild;
    nettle = nettleSharedBuild;
    gnutls = gnutlsSharedBuild;
    glib-networking = glibNetworkingBuild;
    # llvmpipe and lavapipe JIT through libLLVM at runtime.
    llvm = llvmCrossBuild;
    vulkan-loader = vulkanLoaderBuild;
    vulkan-tools = vulkanToolsBuild;
    libxshmfence = libxshmfenceSharedBuild;
    xvfb = xvfbBuild;
    xorg = xorgBuild;
    xwayland = xwaylandBuild;
    wayland = waylandBuild;
    wlroots = wlrootsBuild;
    sway = swayBuild;
    # sway links libjson-c as a dylib, so it has to be on the image and not
    # only in the build closure.
    json-c = jsoncBuild;
    libdrm = libdrmBuild;
    pdsurface = pdsurfaceBuild;
    libgbm = libgbmBuild;
    libxcvt = xvfbLibxcvtBuild;
    xeyes = xeyesBuild;
    xclock = xclockBuild;
    xcalc = xcalcBuild;
    xmessage = xmessageBuild;
    dillo = dilloBuild;
    libz-dylib = libzDylibBuild;
    libcurl-dylib = libcurlDylibBuild;
    dbus = dbusBuild;
    libxml2 = libxml2Build;
    at-spi2-core = atspi2CoreBuild;
    libepoxy = libepoxyBuild;
    gdk-pixbuf = gdkPixbufBuild;
    cairo-gobject = cairoGobjectBuild;
    libXrandr = xvfbLibXrandrBuild;
    gtk3 = gtk3Build;
    gtk-layer-shell = gtkLayerShellBuild;
    libpng = libpngBuild;
    libcroco = libcrocoBuild;
    librsvg = librsvgBuild;
    gettext = gettextBuild;
    #webkitgtk = webkitgtkBuild;
    libsoup = libsoupBuild;
    sqlite = sqliteBuild;
    libpsl = libpslBuild;
    nghttp2 = nghttp2Build;
    libgcrypt = libgcryptBuild;
    libgpg-error = libgpgErrorBuild;
    libtasn1 = libtasn1Build;
    libjpeg = libjpegBuild;
    libwebp = libwebpBuild;
    libwapcaplet = libwapcapletBuild;
    libparserutils = libparserutilsBuild;
    libnsutils = libnsutilsBuild;
    libnsgif = libnsgifBuild;
    libnsbmp = libnsbmpBuild;
    libutf8proc = libutf8procBuild;
    libhubbub = libhubbubBuild;
    libcss = libcssBuild;
    libdom = libdomBuild;
    netsurf = netsurfBuild;
    xxd = xxdBuild;
    xz = xzBuild;
    xterm = xtermBuild;
    xkbcomp = xkbcompBuild;
    xkeyboard-config = xkeyboardConfigBuild;
    libx11-locale = xlibLocaleBuild;
    fonts = xvfbFontsBuild;
    libiconv = libiconvBuild;
    nano = nanoBuild;
    bmake = bmakeBuild;
    gnumake = gnumakeBuild;
    ninja = ninjaBuild;
    cmake = cmakeBuild;
    clang = clangCrossBuild;
    kc-tools-guest = kcToolsGuestBuild;
    meson = mesonBuild;
    pkgconf = pkgconfBuild;
    gnum4 = gnum4Build;
    autoconf = autoconfBuild;
    automake = automakeBuild;
    bison = bisonBuild;
    flex = flexBuild;
    python = pythonBuild;
    #perl = perlBuild;
    zsh = zshBuild;
    toybox = toyboxBuild;
    file = fileBuild;
    openssl = opensslBuild;
    curl = curlBuild;
    openssh = opensshBuild;
    git = gitBuild;
    migcomDarwin = migcomDarwinBuild;
    ioreg = ioregBuild;
    xkbcommon = xkbcommonBuild;
    fastfetch = fastfetchBuild;
    corefoundation = coreFoundationBuild;
    icucore = icuCoreBuild;
    libcxxabi-dylib = libcxxabiDylibBuild;
    libcxx-dylib = libcxxDylibBuild;
    libcxx-test = libcxxTestBuild;
    mesa = mesaBuild;
    mesa-demos = mesaDemosBuild;
    pd-virgl-shim = pdVirglShimBuild;
    libobjc = libobjcBuild;
    foundation = foundationBuild;
    iokit = iokitBuild;
    coreServices = coreServicesBuild;
    security = securityBuild;
    systemConfiguration = systemConfigurationBuild;
    diskArbitration = diskArbitrationBuild;
    iomediacheck = iomediacheckBuild;
    opengl-framework = openglFrameworkBuild;
    i3 = i3Build;
    i3status = i3statusShimBuild;
    startup-notification = startupNotificationBuild;
    libX11 = xlibBuild;
    libxcb = xcbBuild;
    libxcb-util = xcbUtilBuild;
    libxcb-keysyms = xcbKeysymsBuild;
    libxcb-wm = xcbWmBuild;
    libxcb-render-util = xcbRenderUtilBuild;
    libxcb-image = xcbImageBuild;
    libxcb-cursor = xcbCursorBuild;
    xcb-util-xrm = xcbXrmBuild;
    libev = libevBuild;
    pcre2 = pcre2Build;
    yajl = yajlBuild;
    cairo = cairoBuild;
    libffi = libffiBuild;
    glib = glibBuild;
    fribidi = fribidiBuild;
    harfbuzz = harfbuzzBuild;
    expat = expatBuild;
    fontconfig = fontconfigBuild;
    freetype2 = freetype2Build;
    pango = pangoBuild;
    libXft = libXftBuild;
    dmenu = dmenuBuild;
    zlib = xvfbZlibBuild;
    libXau = xvfbLibXauBuild;
    libXdmcp = xvfbLibXdmcpBuild;
    libXext = xvfbLibXextBuild;
    libXrender = xvfbLibXrenderBuild;
    libXfixes = xvfbLibXfixesBuild;
    libXcursor = xvfbLibXcursorBuild;
    libICE = xvfbLibICEBuild;
    libSM = xvfbLibSMBuild;
    libXinerama = xvfbLibXineramaBuild;
    libXres = xvfbLibXresBuild;
    libXcomposite = xvfbLibXcompositeBuild;
    libXdamage = xvfbLibXdamageBuild;
    libXpresent = xvfbLibXpresentBuild;
    libxfce4util = libxfce4utilBuild;
    xfconf = xfconfBuild;
    libwnck = libwnckBuild;
    libxfce4ui = libxfce4uiBuild;
    xfwm4 = xfwm4Build;
    libdisplay-info = libdisplayInfoBuild;
    libxfce4windowing = libxfce4windowingBuild;
    garcon = garconBuild;
    exo = exoBuild;
    xfce4-session = xfce4SessionBuild;
    xfce4-panel = xfce4PanelBuild;
    xfdesktop = xfdesktopBuild;
    xfce4-appfinder = xfce4AppfinderBuild;
    thunar = thunarBuild;
    cursor-theme = cursorThemeBuild;
    icon-themes = iconThemesBuild;
    xfce4-settings = xfce4SettingsBuild;
    vte = vteBuild;
    xfce4-terminal = xfce4TerminalBuild;
    iceauth = iceauthBuild;
    xrdb = xrdbBuild;
    xrandr = xrandrBuild;
    xinit = xinitBuild;
  };

  commonPackages = {
    darwin-toolchain = darwinCrossToolchain;
    launchd = launchdBuild;
    launchctl = launchctlBuild;
    corefoundation = coreFoundationBuild;
    icucore = icuCoreBuild;
    iokit = iokitBuild;
    libobjc = libobjcBuild;
    libcxx-dylib = libcxxDylibBuild;
    libcxxabi-dylib = libcxxabiDylibBuild;
    userland = userlandBuild;
    tcc = tccBuild;
    cctools = cctoolsBuild;
    libsystem = libSystemBuild;
    libSystem = libSystemBuild;
    libapfsrw = libapfsrwBuild;
    xnu-headers = xnuHeadersBuild;
    xnu = kernelBuild;
    xnu-debug = kernelDebugBuild;
    kernel = kernelBuild;
    kernel-debug = kernelDebugBuild;
    kernel-arm64 = kernelArm64Build;
    kernel-arm64-virt = kernelArm64VirtBuild;
    kernel-arm64-virt-debug = kernelArm64VirtDebugBuild;
    kexts = kextsBuild;
    kexts-arm64 = kextsArm64Build;
    iographics = iographicsBuild;
    basesystem = fullBuild;
    basesystem-split = splitBaseSystem;
    default = fullBuild;
    fbdoom = fbdoomBuild;
    wayland = waylandBuild;
    wayland-protocols = waylandProtocolsBuild;
    wlroots = wlrootsBuild;
    sway = swayBuild;
    xwayland = xwaylandBuild;
  } // imageExtraPackageSet // lib.optionalAttrs (!isDarwin) {
    libX11 = xlibBuild;
    libxcb = xcbBuild;
    freetype2 = freetype2Build;
    ncurses = ncursesBuild;
    libiconv = libiconvBuild;
    libxkbfile = xvfbLibXkbfileBuild;
    libxcb-util = xcbUtilBuild;
    libxcb-keysyms = xcbKeysymsBuild;
    libxcb-wm = xcbWmBuild;
    libxcb-render-util = xcbRenderUtilBuild;
    libxcb-image = xcbImageBuild;
    libxcb-cursor = xcbCursorBuild;
    xcb-util-xrm = xcbXrmBuild;
    libev = libevBuild;
    pcre2 = pcre2Build;
    yajl = yajlBuild;
    startup-notification = startupNotificationBuild;
    xfconf = xfconfBuild;
    libxfce4util = libxfce4utilBuild;
    libxfce4ui = libxfce4uiBuild;
    libxfce4windowing = libxfce4windowingBuild;
    libwnck = libwnckBuild;
    garcon = garconBuild;
    exo = exoBuild;
    xfwm4 = xfwm4Build;
    xfce4-session = xfce4SessionBuild;
    xfce4-panel = xfce4PanelBuild;
    xfdesktop = xfdesktopBuild;
    xfce4-appfinder = xfce4AppfinderBuild;
    thunar = thunarBuild;
    xfce4-settings = xfce4SettingsBuild;
    vte = vteBuild;
    xfce4-terminal = xfce4TerminalBuild;
    cairo = cairoBuild;
    libffi = libffiBuild;
    glib = glibBuild;
    fribidi = fribidiBuild;
    harfbuzz = harfbuzzBuild;
    expat = expatBuild;
    fontconfig = fontconfigBuild;
    pango = pangoBuild;
    i3 = i3Build;
    i3status = i3statusShimBuild;
    openssh = opensshBuild;
    gnumake = gnumakeBuild;
    ninja = ninjaBuild;
    cmake = cmakeBuild;
    clang = clangCrossBuild;
    kc-tools-guest = kcToolsGuestBuild;
    meson = mesonBuild;
    pkgconf = pkgconfBuild;
    gnum4 = gnum4Build;
    autoconf = autoconfBuild;
    automake = automakeBuild;
    bison = bisonBuild;
    flex = flexBuild;
    python = pythonBuild;
    #perl = perlBuild;
  };

  linuxPackages =
    let
      kcBuild = pkgs.callPackage ./pkgs/toolchain/kc.nix {
        kernel = kernelBuild;
        kexts = kextsBuild;
        kcTools = kc-tools.packages.${system}.default;
      };
      kcDebugBuild = pkgs.callPackage ./pkgs/toolchain/kc.nix {
        kernel = kernelDebugBuild;
        kexts = kextsBuild;
        kcTools = kc-tools.packages.${system}.default;
      };
      kcArm64DebugBuild = pkgs.callPackage ./pkgs/toolchain/kc-arm64.nix {
        kernel = kernelArm64VirtDebugBuild;
        kexts = kextsArm64Build;
        kcTools = kc-tools.packages.${system}.default;
      };
      kcArm64ReleaseBuild = pkgs.callPackage ./pkgs/toolchain/kc-arm64.nix {
        kernel = kernelArm64VirtBuild;
        kexts = kextsArm64Build;
        kcTools = kc-tools.packages.${system}.default;
      };
      imageExtraPackages = lib.attrValues imageExtraPackageSet
        ++ lib.optional (fbdoomExternalSrc != null) fbdoomBuild;
      imageBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystem;
        extraPackages = imageExtraPackages;
        kc = kcBuild;
        xnuLoader = xnu-loader.packages.${system}.default;
        apfsprogs = pkgs.apfsprogs;
        #testAudioFile = /home/vali/development/darwin/stillalive.pcm;
      };
      imageHfsBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystem;
        extraPackages = imageExtraPackages;
        kc = kcBuild;
        xnuLoader = xnu-loader.packages.${system}.default;
        apfsprogs = pkgs.apfsprogs;
        hfsprogs = pkgs.hfsprogs;
        libdmg-hfsplus = pkgs.callPackage ./pkgs/toolchain/libdmg-hfsplus.nix { };
        rootFsType = "hfs";
        #testAudioFile = /home/vali/development/darwin/stillalive.pcm;
      };
      imageDebugBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystem;
        extraPackages = lib.attrValues imageExtraPackageSet;
        kc = kcDebugBuild;
        xnuLoader = xnu-loader.packages.${system}.default;
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-debug.img";
      };
      imageArm64VirtBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystem;
        extraPackages = imageExtraPackages;
        kc = kcArm64DebugBuild;
        xnuLoader = xnu-loader.packages.${system}.arm64-virt;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        imageFileName = "puredarwin-arm64-virt.img";
      };
      imageArm64VirtMinimalBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimal;
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build
                          jitTestArm64Build asmjitTestArm64Build ];
        kc = kcArm64DebugBuild;
        xnuLoader = xnu-loader.packages.${system}.arm64-virt;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        espMB = 768;
        rootMB = 512;
        imageFileName = "puredarwin-arm64-virt-minimal.img";
        ramdiskMB = 512;
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 pdtrace=1 serial_video_mirror=1 no_interrupt_masked_debug=1 rd=md0";
      };
      netbootArm64VirtMinimalBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimal;
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build
                          jitTestArm64Build asmjitTestArm64Build ];
        kc = kcArm64DebugBuild;
        xnuLoader = xnu-loader.packages.${system}.arm64-virt;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        netbootOnly = true;
        useRamdisk = true;
        ramdiskMB = 512;
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 pdtrace=1 serial_video_mirror=1 no_interrupt_masked_debug=1 rd=md0";
      };
      imageArm64VirtFullBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimalRelease;
        extraPackages = imageExtraPackagesArm64;
        kc = kcArm64ReleaseBuild;
        xnuLoader = xnu-loader.packages.${system}.arm64-virt;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        espMB = 64;
        rootMB = 3072;
        imageFileName = "puredarwin-arm64-virt.img";
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 pdtrace=1 serial_video_mirror=1 no_interrupt_masked_debug=1 vgpu_debug=1";
      };
      imageArm64VirtMinimalReleaseBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemArm64VirtMinimalRelease;
        extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build ];
        kc = kcArm64ReleaseBuild;
        xnuLoader = xnu-loader.packages.${system}.arm64-virt;
        apfsprogs = pkgs.apfsprogs;
        efiBinary = "BOOTAA64.EFI";
        espMB = 64;
        rootMB = 512;
        imageFileName = "puredarwin-arm64-virt-minimal-release.img";
        bootArgs = "-v serial=3 ahci_debug=1 kext=0xffff io=0xffff";
      };
      strippedExtraPackages = [ zshBuild toyboxBuild libiconvBuild coreFoundationBuild icuCoreBuild iokitBuild coreServicesBuild libcxxabiDylibBuild libcxxDylibBuild libcxxTestBuild libobjcBuild objcTestBuild foundationBuild securityBuild systemConfigurationBuild diskArbitrationBuild ioregBuild ];
      imageStrippedBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemStripped;
        extraPackages = strippedExtraPackages;
        kc = kcBuild;
        xnuLoader = xnu-loader.packages.${system}.default;
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-stripped.img";
      };
      imageMinimalBuild = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemMinimal;
        extraPackages = strippedExtraPackages;
        kc = kcBuild;
        xnuLoader = xnu-loader.packages.${system}.default;
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-minimal.img";
        espMB = 64;
        rootMB = 384;
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 gen9_debug=1";
      };
      imageMinimalBuildDebug = pkgs.callPackage ../image.nix {
        baseSystem = splitBaseSystemMinimalDebug;
        extraPackages = strippedExtraPackages;
        kc = kcDebugBuild;
        xnuLoader = xnu-loader.packages.${system}.default;
        apfsprogs = pkgs.apfsprogs;
        imageFileName = "puredarwin-minimal-debug.img";
        espMB = 64;
        rootMB = 384;
        bootArgs = "-v debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 gen9_debug=1 serial_video_mirror=1 pdtrace=1";
      };
      runVm = pkgs.writeShellApplication {
        name = "puredarwin-vm";
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          state_dir="''${PUREDARWIN_VM_STATE_DIR:-$PWD/.puredarwin-vm}"
          image="''${PUREDARWIN_IMAGE:-}"
          ovmf_code="''${PUREDARWIN_OVMF_CODE:-${pkgs.OVMF.fd}/FV/OVMF_CODE.fd}"
          ovmf_vars_template="''${PUREDARWIN_OVMF_VARS_TEMPLATE:-${pkgs.OVMF.fd}/FV/OVMF_VARS.fd}"
          ovmf_vars="''${PUREDARWIN_OVMF_VARS:-$state_dir/OVMF_VARS.fd}"

          if [ -z "$image" ]; then
            if [ -e "$PWD/puredarwin.img" ]; then
              image="$PWD/puredarwin.img"
            elif [ -e "$PWD/result/puredarwin.img" ]; then
              image="$PWD/result/puredarwin.img"
            else
              echo "puredarwin-vm: no image found; set PUREDARWIN_IMAGE or run nix build .#image" >&2
              exit 1
            fi
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

          exec qemu-system-x86_64 \
            -M q35 \
            -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
            -smp "''${PUREDARWIN_VM_SMP:-4}" \
            -vga "''${PUREDARWIN_VM_VGA:-std}" \
            -cpu IvyBridge,vendor=GenuineIntel \
            -fw_cfg name=opt/ovmf/X-PciMmio64Mb,string=2048 \
            -drive if=pflash,format=raw,unit=0,readonly=on,file="$ovmf_code" \
            -drive if=pflash,format=raw,unit=1,file="$ovmf_vars" \
            -drive id=root,format=raw,file="$image"$image_readonly_opt \
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
      runKvm = pkgs.writeShellApplication {
        name = "puredarwin-kvm";
        runtimeInputs = [ pkgs.qemu ];
        text = ''
          set -euo pipefail

          state_dir="''${PUREDARWIN_VM_STATE_DIR:-$PWD/.puredarwin-kvm}"
          image="''${PUREDARWIN_IMAGE:-}"
          ovmf_code="''${PUREDARWIN_OVMF_CODE:-${pkgs.OVMF.fd}/FV/OVMF_CODE.fd}"
          ovmf_vars_template="''${PUREDARWIN_OVMF_VARS_TEMPLATE:-${pkgs.OVMF.fd}/FV/OVMF_VARS.fd}"
          ovmf_vars="''${PUREDARWIN_OVMF_VARS:-$state_dir/OVMF_VARS.fd}"

          if [ -z "$image" ]; then
            if [ -e "$PWD/puredarwin.img" ]; then
              image="$PWD/puredarwin.img"
            elif [ -e "$PWD/result/puredarwin.img" ]; then
              image="$PWD/result/puredarwin.img"
            else
              echo "puredarwin-kvm: no image found; set PUREDARWIN_IMAGE or run nix build .#image" >&2
              exit 1
            fi
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

          case "''${PUREDARWIN_VM_DISK:-ahci}" in
            virtio|virtio-blk)
              disk_args=(
                -drive "if=none,id=system,file=$image,format=raw,cache=writeback$image_readonly_opt"
                -device "virtio-blk-pci,drive=system"
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

          exec qemu-system-x86_64 \
            -machine q35,accel=kvm \
            -cpu "''${PUREDARWIN_KVM_CPU:-host}" \
            -smp "''${PUREDARWIN_VM_SMP:-4}" \
            -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
            -vga "''${PUREDARWIN_VM_VGA:-std}" \
            -fw_cfg name=opt/ovmf/X-PciMmio64Mb,string=2048 \
            -drive if=pflash,format=raw,unit=0,readonly=on,file="$ovmf_code" \
            -drive if=pflash,format=raw,unit=1,file="$ovmf_vars" \
            "''${disk_args[@]}" \
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
            "$@"
        '';
      };
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

          if [ -z "$image" ]; then
            if [ -e "$PWD/puredarwin-arm64-virt.img" ]; then
              image="$PWD/puredarwin-arm64-virt.img"
            elif [ -e "$PWD/result/puredarwin-arm64-virt.img" ]; then
              image="$PWD/result/puredarwin-arm64-virt.img"
            else
              echo "puredarwin-arm64-virt: no image found; run nix build .#image-arm64-virt" >&2
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
            -device virtio-blk-pci,drive=system,bootindex=1 \
            -device ramfb \
            -device virtio-net-pci,netdev=net0 \
            -netdev user,id=net0,hostfwd=tcp::2223-:22 \
            -device qemu-xhci,id=xhci \
            -device usb-kbd,bus=xhci.0 \
            -device usb-mouse,bus=xhci.0 \
            -serial mon:stdio \
            -display none \
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
      corefoundation = coreFoundationBuild;
      icucore = icuCoreBuild;
      libcxxabi-dylib = libcxxabiDylibBuild;
      libcxx-dylib = libcxxDylibBuild;
      libcxx-test = libcxxTestBuild;
      mesa = mesaBuild;
      libobjc = libobjcBuild;
      objc-test = objcTestBuild;
      foundation = foundationBuild;
      autoconf = autoconfBuild;
      automake = automakeBuild;
      iokit = iokitBuild;
      coreServices = coreServicesBuild;
      security = securityBuild;
      systemstarter = systemStarterBuild;
      launchd = launchdBuild;
      launchctl = launchctlBuild;
      image = imageBuild;
      image-arm64-virt = imageArm64VirtBuild;
      image-arm64-virt-minimal = imageArm64VirtMinimalBuild;
      netboot-arm64-virt-minimal = netbootArm64VirtMinimalBuild;
      image-arm64-virt-minimal-release = imageArm64VirtMinimalReleaseBuild;
      image-arm64-virt-full = imageArm64VirtFullBuild;
      image-hfs = imageHfsBuild;
      image-debug = imageDebugBuild;
      image-stripped = imageStrippedBuild;
      image-minimal = imageMinimalBuild;
      image-minimal-debug = imageMinimalBuildDebug;
      xorg = xorgBuild;
      libxcvt = xvfbLibxcvtBuild;
      userland = userlandBuild;
      vm-runner = runVm;
      kvm-runner = runKvm;
      arm64-virt-runner = runArm64Virt;
      arm64-uefi-runner = runArm64Uefi;
      arm64-uboot-runner = runArm64Uboot;
    };

  linuxApps =
    let
      runVm = linuxPackages.vm-runner;
      runKvm = linuxPackages.kvm-runner;
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
