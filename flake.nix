{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    iig-tools.url = "github:PureDarwin/iig-tools";
    kc-tools.url = "github:PureDarwin/kc-tools/xnu-12377";
    xnu-loader.url = "github:PureDarwin/xnu-loader/xnu-12377";
    # the interim corecrypto fork, the submodule at external/corecrypto
    self.submodules = true;
    corecrypto-src = {
      url = "path:./external/corecrypto";
      flake = false;
    };
    # the MOK key pair that signs Secure Boot images, a placeholder until a build overrides it with
    # --override-input pd-sb-key path:/path/to/keydir, see nix/secureboot/no-key/README
    pd-sb-key = { url = "path:./nix/secureboot/no-key"; flake = false; };
  };

  outputs = { self, nixpkgs, iig-tools, kc-tools, xnu-loader, corecrypto-src, pd-sb-key }:
    let
      lib = nixpkgs.lib;
      systems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = lib.genAttrs systems;
      # a path: input inside this repository is a subpath of the whole flake source, so any edit anywhere would
      # change its hash and rebuild everything built on it; a copy of the tree alone is hashed by its own files
      corecryptoSrc = builtins.path { path = corecrypto-src; name = "corecrypto-src"; };

      mkSystem = system:
        let
          basePkgs = import nixpkgs { inherit system; };

          # Two stages so the MIG-generated <mach/*.h> can be produced at build
          # time instead of being carried pre-generated: the base SDK has the
          # hand-written headers migcom itself needs, and the final SDK adds
          # what migcom emits from xnu's .defs.
          appleSdkBase = basePkgs.callPackage ./nix/pkgs/toolchain/apple-sdk-pinned.nix { };
          sdkMigcom = basePkgs.callPackage ./nix/pkgs/toolchain/migcom.nix {
            appleSdk = appleSdkBase;
          };
          appleSdk = basePkgs.callPackage ./nix/pkgs/toolchain/apple-sdk-pinned.nix {
            migcom = sdkMigcom;
          };
          pkgs = basePkgs.extend (_: _: { inherit appleSdk; });
          isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
          # fbDOOM (GPL, opt-in - see src/Userspace/fbdoom/CMakeLists.txt) is
          # an external checkout, not a flake input: point PUREDARWIN_FBDOOM_SOURCE_ENV
          # at it (requires --impure). Mirrors the SDK tarball's requireFile
          # pattern - never hardcode a personal machine path into this file.
          fbdoomExternalSrcEnv = builtins.getEnv "PUREDARWIN_FBDOOM_SOURCE_ENV";
          # Component source trees (see nix/sources.nix).
          sources = import ./nix/sources.nix {
            inherit pkgs sourceWith sourceWithExtraFilter frameworkHeadersOnly libSystemSourcePaths fbdoomExternalSrcEnv;
          };
          inherit (sources)
            fbdoomExternalSrc
            chocolateDoomPatchedSrc
            kernelSource
            libSystemSource
            kextsSource
            xnuHeadersSource
            kextsOnlyFiles
            kextsOnlySource
            userlandSource
            fbdoomSource
            cctoolsSource
            coreFoundationSource
            coreServicesSource
            securitySource
            systemConfigurationSource
            iokitCFSource
            diskArbitrationSource
            symptomReporterSource
            protocolBufferSource
            wirelessDiagnosticsSource
            objcSource
            libcxxDylibSource
            foundationSource
            xfconfSrc
            libxfce4uiSrc
            xfwm4Src
            garconSrc
            exoSrc
            xfce4SessionSrc
            xfce4PanelSrc
            xfdesktopSrc
            asmjitSrc
            vteSrc
            xfce4TerminalSrc
            xfce4SettingsSrc
            xfce4AppfinderSrc
            thunarSrc
            ;
          iig = iig-tools.packages.${system}.default or (
            (pkgs.callPackage iig-tools { }).overrideAttrs (old: {
              meta = (old.meta or { }) // {
                platforms = pkgs.lib.platforms.unix;
              };
            })
          );
          # Bootstrap toolchain: ld64.lld, used only to build the real cctools
          # ld64 below. Everything else uses darwinCrossToolchain, which has the
          # real linker - lld silently ignores -dylib_file/-image_base/-segaddr.
          bootstrapCrossToolchain =
            pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix { };
          libtapi = pkgs.callPackage ./nix/pkgs/toolchain/libtapi.nix { };
          # Every host uses the in-tree cctools ld64, so the Mach-O the kexts
          # and kernel get is the same everywhere; it is also what lets a Mac
          # link kexts as MH_BUNDLE (see cmake/kext.cmake), which is the layout
          # kc-builder expects.
          llvmRiscvMachO = pkgs.callPackage ./nix/pkgs/toolchain/llvm-riscv-macho { };
          nativeLd =
            pkgs.callPackage ./nix/pkgs/toolchain/native-ld.nix {
              darwinCrossToolchain = bootstrapCrossToolchain;
              inherit libtapi iig;
              inherit corecryptoSrc;
            };
          # Not actually a *cross* toolchain on Darwin - the wrappers pin the
          # target triple, the pinned SDK and the linker, which is what every
          # consumer wants on either host. toolchain.nix is built from nixpkgs'
          # llvmPackages_21, which exists on both.
          darwinCrossToolchain = pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix {
            inherit nativeLd;
          };
          arm64CrossToolchain = pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix {
            inherit nativeLd;
            target = "arm64-apple-darwin20.4";
            clangTarget = "arm64-apple-macosx26.5";
          };
          # riscv64 mach-o through the patched llvm and the in-tree ld64
          riscv64CrossToolchain = pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix {
            inherit nativeLd;
            llvmPackages_21 = llvmRiscvMachO;
            target = "riscv64-apple-darwin20.4";
            clangTarget = "riscv64-apple-macosx26.5";
          };
          # userland builtins, hard float rv64gc/lp64d like everything outside the kernel
          compilerRtRiscv64Build = pkgs.callPackage ./nix/pkgs/toolchain/compiler-rt.nix {
            darwinCrossToolchain = riscv64CrossToolchain;
            nativeMesonTools = nativeMesonToolsDir;
            inherit nativeLd;
            llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
            llvmVersion = pkgs.llvmPackages_21.llvm.version;
            targetTriple = "riscv64-apple-darwin20.4";
            targetArch = "riscv64";
          };
          compilerRtRiscv64Kernel = pkgs.callPackage ./nix/pkgs/toolchain/compiler-rt-riscv64-kernel.nix {
            inherit riscv64CrossToolchain;
            llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
            llvmVersion = pkgs.llvmPackages_21.llvm.version;
          };
          # The Pi Zero's ARM1176 is ARMv6; there is no macosx deployment
          # target for 32-bit ARM, so the triple stays a plain darwin one.
          armv6CrossToolchain = pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix {
            inherit nativeLd;
            target = "armv6-apple-darwin20.4";
            clangTarget = "armv6-apple-darwin20.4";
          };
          nativeUnifdef = pkgs.callPackage ./nix/pkgs/toolchain/unifdef.nix { };
          nativeMigcom = pkgs.callPackage ./nix/pkgs/toolchain/migcom.nix { };
          libapfsrwBuild = pd.x86_64.libapfsrw;

          sourceWithExtraFilter = name: prefixes: extraFilter:
            lib.cleanSourceWith {
              src = ./.;
              filter = path: type:
                let
                  rel = lib.removePrefix "${toString ./.}/" (toString path);
                  isParentOfPrefix = prefix:
                    lib.hasPrefix "${rel}/" prefix;
                in
                  extraFilter rel type
                  && (rel == "CMakeLists.txt"
                  || rel == "src/CMakeLists.txt"
                  || rel == "cmake"
                  || lib.hasPrefix "cmake/" rel
                  || lib.any (prefix:
                    rel == prefix
                    || lib.hasPrefix "${prefix}/" rel
                    || (type == "directory" && isParentOfPrefix prefix)
                  ) prefixes);
            };
          sourceWith = name: prefixes:
            sourceWithExtraFilter name prefixes (rel: type: true);
          # The top-level CMakeLists.txt globs *.h out of the CoreFoundation and
          # Foundation trees for a header search path, and nothing built from
          # libSystemSource compiles their sources. Dropping the implementation
          # files keeps a .m edit in either framework from invalidating
          # libSystem - and so from rebuilding LLVM, which depends on it.
          frameworkHeadersOnly = rel: type:
            !(type == "regular"
              && (lib.hasPrefix "src/Frameworks/CoreFoundation/" rel
                  || lib.hasPrefix "src/Frameworks/Foundation/" rel)
              && !(lib.hasSuffix ".h" rel));
          libSystemSourcePaths = [
            "src/Kernel/xnu/EXTERNAL_HEADERS"
            "src/Kernel/xnu/osfmk"
            "src/Kernel/xnu/libkern/libkern"
            "src/Kernel/xnu/libkern/libkern/arm"
            "src/Kernel/xnu/libkern/os"
            # os/log_private.h includes <firehose/tracepoint_private.h>
            "src/Kernel/xnu/libkern/firehose"
            "src/Kernel/xnu/bsd/arm"
            # machine/_types.h and friends pick bsd/riscv on a riscv64 build
            "src/Kernel/xnu/bsd/riscv"
            "src/Kernel/xnu/bsd/i386"
            "src/Kernel/xnu/bsd/bsm"
            "src/Kernel/xnu/bsd/machine"
            "src/Kernel/xnu/bsd/net"
            "src/Kernel/xnu/bsd/netinet"
            "src/Kernel/xnu/bsd/netinet6"
            "src/Kernel/xnu/bsd/pthread"
            "src/Kernel/xnu/bsd/sys"
            "src/Kernel/xnu/bsd/sys_private"
            "src/Kernel/xnu/bsd/uuid"
            "src/Kernel/xnu/bsd/kern/makesyscalls.sh"
            "src/Kernel/xnu/bsd/kern/syscalls.master"
            "src/Frameworks/CoreFoundation"
            "src/Frameworks/Foundation"
            "src/Libraries"
            "src/Libraries/libSystem/libmalloc/compat-include"
            "tools/mig"
            # libobjc needs the mach-o getsection helpers compiled into libSystem.
            "tools/cctools/libmacho/getsecbyname.c"
            # NXGetArchInfo* is part of libSystem on Darwin, same libmacho source.
            "tools/cctools/libmacho/arch.c"
            # mach-o/utils.h (macOS 13) arch-name helpers, over that same table.
            "tools/cctools/libmacho/utils.c"
            "tools/cctools/include/mach-o/utils.h"
            "tools/cctools/include/mach-o/arch.h"
            "tools/cctools/include/stuff/openstep_mach.h"
            "tools/cctools/include/mach/machine.h"
          ];
          mkPureDarwinBuild = args: pkgs.callPackage ./build.nix ({
            inherit appleSdk darwinCrossToolchain nativeLd nativeUnifdef nativeMigcom iig;
            # the kernel proper never builds corecrypto, keep its hash independent of it
            corecryptoSrc = if lib.hasPrefix "puredarwin-kernel" (args.pname or "") || lib.hasPrefix "puredarwin-xnu-headers" (args.pname or "")
              then null else corecryptoSrc;
            compilerRt = compilerRtBuild;
            compilerRtArm64 = arm64.compilerRtArm64Build or null;
            compilerRtRiscv64 = compilerRtRiscv64Build;
          } // args);

          userlandBuild = pd.x86_64.userland;
          # Wayland-only image: the same userland without the Xorg DDX drivers,
          # which are what drag the whole Xorg tree into the build graph.
          # startx stays in the target list - it is plain libSystem and only
          # execs Xorg at runtime, so it costs no dependency, and its install
          # rule is unconditional in CMake. It is removed from the image in
          # splitBaseSystemWayland instead.
          userlandNoxBuild = pd.x86_64.userlandNox;
          tccBuild = pd.x86_64.tcc;
          cctoolsBuild = pd.x86_64.cctools;
          xvfbPixmanBuild = pd.x86_64.pixman;
          xvfbLibXauBuild = pd.x86_64.libXau;
          xvfbLibXdmcpBuild = pd.x86_64.libXdmcp;
          xvfbZlibBuild = pd.x86_64.zlib;
          # Always shared: fontconfig is a dylib and records a dependency on
          # /lib/libfreetype.6.N.dylib, so a static freetype cannot satisfy it.
          freetype2Build = pd.x86_64.freetype2;
          libfontencBuild = pd.x86_64.libfontenc;
          xvfbLibXfont2Build = pd.x86_64.libXfont2;
          xlibBuild = pd.x86_64.libX11;
          xcbBuild = pd.x86_64.libxcb;
          xcbUtilBuild = pd.x86_64.libxcb-util;
          xcbKeysymsBuild = pd.x86_64.libxcb-keysyms;
          xcbWmBuild = pd.x86_64.xcbWm;
          xcbRenderUtilBuild = pd.x86_64.libxcb-render-util;
          xcbImageBuild = pd.x86_64.libxcb-image;
          xcbCursorBuild = pd.x86_64.libxcbcursor;
          xcbXrmBuild = pd.x86_64.xcb-util-xrm;
          libevBuild = pd.x86_64.libev;
          pcre2Build = pd.x86_64.pcre2;
          yajlBuild = pd.x86_64.yajl;
          startupNotificationBuild = pd.x86_64.startup-notification;
          cairoBuild = pd.x86_64.cairo;
          libffiBuild = pd.x86_64.libffi;
          glibBuild = pd.x86_64.glib;
          expatBuild = pd.x86_64.expat;
          fontconfigBuild = pd.x86_64.fontconfig;
          fribidiBuild = pd.x86_64.fribidi;
          harfbuzzBuild = pd.x86_64.harfbuzz;
          pangoBuild = pd.x86_64.pango;
          i3Build = pd.x86_64.i3;
          i3statusShimBuild = pd.x86_64.i3statusShim;
          xvfbLibICEBuild = pd.x86_64.libICE;
          xvfbLibSMBuild = pd.x86_64.libSM;
          xvfbLibXtBuild = pd.x86_64.libXt;
          xvfbLibXextBuild = pd.x86_64.libXext;
          xvfbLibXmuBuild = pd.x86_64.libXmu;
          xvfbLibXpmBuild = pd.x86_64.libXpm;
          xvfbLibXawBuild = pd.x86_64.libXaw;
          xvfbLibXkbfileBuild = pd.x86_64.libxkbfile;
          xkbcompBuild = pd.x86_64.xkbcomp;
          xvfbFontsBuild = pd.x86_64.xvfbFonts;
          xkeyboardConfigBuild = pd.x86_64.xkeyboardConfig;
          xlibLocaleBuild = pd.x86_64.xlibLocale;
          libzDylibBuild = pd.x86_64.libzDylib;
          libcurlDylibBuild = pd.x86_64.libcurl;
          dbusBuild = pd.x86_64.dbus;
          libxml2Build = pd.x86_64.libxml2;
          atspi2CoreBuild = pd.x86_64.atspi2Core;
          libwapcapletBuild = pd.x86_64.libwapcaplet;
          libparserutilsBuild = pd.x86_64.libparserutils;
          libnsutilsBuild = pd.x86_64.libnsutils;
          libnsgifBuild = pd.x86_64.libnsgif;
          libnsbmpBuild = pd.x86_64.libnsbmp;
          libutf8procBuild = pd.x86_64.libutf8proc;
          libhubbubBuild = pd.x86_64.libhubbub;
          libcssBuild = pd.x86_64.libcss;
          libdomBuild = pd.x86_64.libdom;
          netsurfBuild = pd.x86_64.netsurf;
          libepoxyBuild = pd.x86_64.libepoxy;
          pdVirglShimBuild = pd.x86_64.pdVirglShim;
          mesaBuild = pd.x86_64.mesaBuild;
          gluBuild = pd.x86_64.gluBuild;
          gluNoxBuild = pd.x86_64.gluNox;
          mesaDemosBuild = pd.x86_64.mesaDemos;
          # A Mac already has cctools' otool/install_name_tool in nixpkgs.
          hostOtoolBuild =
            if isDarwin then pkgs.cctools
            else pkgs.callPackage ./nix/pkgs/toolchain/host-otool.nix { };
          # aarch64 Windows PE toolchain (llvm-mingw equivalent) for Wine's
          # new WoW64 on arm64. See the file for the two nixpkgs bugs it works around.
          fexWow64Build = pd.x86_64.fex-wow64;

          mingwAarch64 =
            import ./nix/pkgs/toolchain/mingw-aarch64.nix {
              inherit pkgs system;
              # 21 hits the AArch64 SEH unwind backend bug on Wine's concrt140.
              llvmVersion = "22";
            };

          # Cross-built for the target triple, so the host does not matter; on a
          # Darwin host leaving it out left libSystems export list demanding
          # 128-bit builtins that nothing defined.
          compilerRtBuild =
            pkgs.callPackage ./nix/pkgs/toolchain/compiler-rt.nix {
              inherit darwinCrossToolchain nativeLd;
              nativeMesonTools = nativeMesonToolsDir;
              llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
              llvmVersion = pkgs.llvmPackages_21.llvm.version;
            };
          # dsymutil for the guest toolchain: clang runs it after every -g link on Darwin, so
          # configure link tests fail without it. Same configuration as llvm-cross, only this tool
          dsymutilBuild = llvmCrossBuild.overrideAttrs (old: {
            pname = "puredarwin-dsymutil";
            buildPhase = "ninja -C build dsymutil";
            # libLLVM is found at /usr/lib in the image, as llvm-cross stamps it, never through an rpath
            installPhase = ''
              install -Dm755 build/bin/dsymutil $out/usr/bin/dsymutil
              ${nativeMesonToolsDir}/bin/install_name_tool -change @rpath/libLLVM.dylib /usr/lib/libLLVM.dylib $out/usr/bin/dsymutil
            '';
          });
          llvmCrossBuild =
            pkgs.callPackage ./nix/pkgs/toolchain/llvm-cross.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              nativeMesonTools = nativeMesonToolsDir;
              llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
              llvmVersion = pkgs.llvmPackages_21.llvm.version;
              nativeTblgen = "${pkgs.llvmPackages_21.llvm}/bin/llvm-tblgen";
              nativeLlvmConfig = "${pkgs.llvmPackages_21.llvm.dev}/bin/llvm-config";
            };
          # WebKitGTK's mandatory dependencies (Source/cmake/OptionsGTK.cmake).
          # Independently useful: sqlite3, libjpeg and libsoup have no other
          # provider in this tree.
          libgpgErrorBuild = pd.x86_64.libgpg-error;
          libtasn1Build = pd.x86_64.libtasn1;
          sqliteBuild = pd.x86_64.sqlite;
          webkitgtkBuild = pd.x86_64.webkitgtk;
          libsoupBuild = pd.x86_64.libsoup;
          nghttp2Build = pd.x86_64.nghttp2;
          libpslBuild = pd.x86_64.libpsl;
          libwebpBuild = pd.x86_64.libwebp;
          libjpegBuild = pd.x86_64.libjpeg;
          libgcryptBuild = pd.x86_64.libgcrypt;
          kcToolsGuestBuild =
            pkgs.callPackage ./nix/pkgs/toolchain/kc-tools-guest.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              kcToolsSrc = kc-tools;
              inherit (pkgs) cmake ninja;
            };
          clangCrossBuild =
            pkgs.callPackage ./nix/pkgs/toolchain/clang-cross.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              nativeMesonTools = nativeMesonToolsDir;
              llvm = llvmCrossBuild;
              llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
              llvmVersion = pkgs.llvmPackages_21.llvm.version;
              nativeTblgen = "${pkgs.llvmPackages_21.llvm}/bin/llvm-tblgen";
            };
          # On a Darwin host cctools already provides both tools, so the
          # cross-built host otool is only needed elsewhere.
          nativeMesonToolsDir =
            pkgs.runCommand "puredarwin-native-meson-tools" { } (
              let toolsdir = "${hostOtoolBuild}"; in ''
              mkdir -p $out/bin
              ln -s ${toolsdir}/bin/otool $out/bin/otool
              ln -s ${toolsdir}/bin/install_name_tool $out/bin/install_name_tool
            '');
          libpngBuild = pd.x86_64.libpng;
          libcrocoBuild = pd.x86_64.libcroco;

          # Only gettext-runtime is built: it is what produces libintl, and the
          # gettext-tools half is a build-host toolchain (msgfmt/xgettext) that
          # nothing on the guest needs.
          gettextBuild = pd.x86_64.gettext;

          librsvgBuild = pd.x86_64.librsvg;

          cairoGobjectBuild = pd.x86_64.cairoGobject;
          gdkPixbufBuild = pd.x86_64.gdkPixbuf;
          gtk3Build = pd.x86_64.gtk3;
          onyx2dBuild = pd.x86_64.onyx2d;
          # the CoreGraphics calls the official Firefox build imports, run at boot in image-cg-test
          cgFfTestsBuild = pd.x86_64.cg-ff-tests;
          coregraphicsBuild = pd.x86_64.coregraphics;
          coretextBuild = pd.x86_64.coretext;
          coredataBuild = pd.x86_64.coredata;
          # pdres: display modes listed and switched from a shell, through CoreGraphics as Settings does
          pdresBuild = pd.x86_64.pdres;
          cgScreenDemoBuild = pd.x86_64.cg-screen-demo;
          terminalBuild = pd.x86_64.terminal;
          # the system menu bar, the server AppKit apps publish their menus to
          menubarBuild = pd.x86_64.menubar;
          # /usr/share/locale: UTF-8, en_US.UTF-8 and C.UTF-8, built with adv_cmds on the host
          localeDataBuild = pd.x86_64.localeData;
          # title bars for Wayland windows that are not AppKit's, drawn with AppKit's frame code
          pddecorBuild = pd.x86_64.pddecor;
          # the desktop picture and the dock bar
          dockBuild = pd.x86_64.dock;
          # the AppKit control gallery that is compared against macOS
          uigalleryBuild = pd.x86_64.uigallery;
          # the file browser the dock's Files tile opens
          finderBuild = pd.x86_64.finder;
          # the built-in utility apps, all built the same way with UtilityKit
          utilityApp = args:
            pkgs.callPackage ./nix/pkgs/apps/utility-app.nix ({
              librsvg = pkgs.librsvg;
              iokit = iokitBuild;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              appkit = appkitBuild;
              onyx2d = onyx2dBuild;
              coretext = coretextBuild;
              quartzcore = quartzcoreBuild;
              coregraphics = coregraphicsBuild;
              applicationservices = applicationservicesBuild;
              coreservices = coreServicesBuild;
              windowserver = windowserverBuild;
              openglFramework = openglFrameworkNoxBuild;
              mesa = mesaNoxBuild;
              src = ./src/Userspace;
            } // args);
          textEditorBuild = pd.x86_64.text-editor;
          activityBuild = pd.x86_64.activity;
          logsBuild = pd.x86_64.logs;
          imageViewerBuild = utilityApp { appName = "Image Viewer"; appDir = "ImageViewer"; iconSvg = "imageviewer.svg"; description = "PureDarwin Image Viewer: pictures and their folder"; };
          screenshotBuild = pd.x86_64.screenshot;
          disksBuild = pd.x86_64.disks;
          utilityAppBuilds = [ textEditorBuild activityBuild logsBuild imageViewerBuild screenshotBuild disksBuild ];
          # PureDarwin's own system settings
          settingsBuild = pd.x86_64.settings;
          # the hardware report behind About PureDarwin's More Info
          systemInformationBuild = pd.x86_64.system-information;
          # the basic calculator the Dock and Lookout open
          calculatorBuild = pd.x86_64.calculator;
          # Install PureDarwin, the app over pdinstall
          installerBuild = pd.x86_64.installer;
          # the login screen and dsuser, local accounts kept in dslocal
          loginwindowBuild = pd.x86_64.loginwindow;
          # Argon, the desktop session: neuswc, the Dock, the menu bar and Terminal from launchd
          waylandSessionBuild = pd.x86_64.argon-session;
          systemFontsBuild = pd.x86_64.systemFonts;
          sharedMimeInfoBuild = pd.x86_64.shared-mime-info;
          minibrowserAppBuild = pd.x86_64.minibrowser-app;
          # Wayland-only image: cairo/dbus/mesa each bake an libX11 path into
          # their output unless their X11 backends are configured out.
          cairoNoxBuild = pd.x86_64.cairoNox;
          dbusNoxBuild = pd.x86_64.dbusNox;
          mesaNoxBuild = pd.x86_64.mesaNox;
          mesaAmdgpuBuild = pd.x86_64.mesaAmdgpu;
          # the system mesa: radeonsi, crocus, iris and nouveau by the drm device's pci id, else virgl or llvmpipe
          mesaGpuBuild = pd.x86_64.mesaGpu;
          openglFrameworkNoxBuild = pd.x86_64.openglFrameworkNox;
          mesaDemosNoxBuild = pd.x86_64.mesaDemosNox;
          # librsvg takes an explicit deps list rather than a cairo argument, so
          # the nox cairo has to be substituted into it by hand.
          librsvgNoxBuild = pd.x86_64.librsvgNox;
          netsurfNoxBuild = pd.x86_64.netsurfNox;
          libepoxyNoxBuild = pd.x86_64.libepoxyNox;
          fastfetchNoxBuild = pd.x86_64.fastfetchNox;
          harfbuzzNoxBuild = pd.x86_64.harfbuzzNox;
          atspi2CoreNoxBuild = pd.x86_64.atspi2CoreNox;
          cairoGobjectNoxBuild = pd.x86_64.cairoGobjectNox;
          xkbcommonNoxBuild = pd.x86_64.xkbcommonNox;
          pangoNoxBuild = pd.x86_64.pangoNox;
          gtk3NoxBuild = pd.x86_64.gtk3Nox;
          gtkLayerShellNoxBuild = pd.x86_64.gtkLayerShellNox;
          libwnckBuild = pd.x86_64.libwnck;
          gtkLayerShellBuild = pd.x86_64.gtkLayerShell;
          xvfbBuild = pd.x86_64.xvfb;
          xvfbLibxcvtBuild = pd.x86_64.libxcvt;
          xorgBuild = pd.x86_64.xorg;
          pdsurfaceBuild = pd.x86_64.pdsurface;
          libgbmBuild = pd.x86_64.libgbm;
          libdrmBuild = pd.x86_64.libdrm;
          libdrmAmdgpuBuild = pd.x86_64.libdrmAmdgpu;
          libelfBuild = pd.x86_64.libelf;
          jsoncBuild = pd.x86_64.jsonc;
          xwaylandBuild = pd.x86_64.xwayland;
          xvfbLibXrenderBuild = pd.x86_64.libXrender;
          # libGL's direct-rendering path calls into XF86VidMode for refresh
          # rate reporting, so glx-direct=true needs this.
          xvfbLibXxf86vmBuild = pd.x86_64.xvfbLibXxf86vm;
          xvfbLibXfixesBuild = pd.x86_64.libXfixes;
          xvfbLibXcursorBuild = pd.x86_64.libXcursor;
          xvfbLibXrandrBuild = pd.x86_64.libXrandr;
          # XFCE desktop (see nix/xfce.nix).
          xfceDesktop = import ./nix/xfce.nix {
            inherit lib;
            inherit atspi2CoreBuild;
            inherit cairoBuild;
            inherit cairoGobjectBuild;
            inherit darwinCrossToolchain;
            inherit dbusBuild;
            inherit exoSrc;
            inherit expatBuild;
            inherit fontconfigBuild;
            inherit freetype2Build;
            inherit fribidiBuild;
            inherit garconSrc;
            inherit gdkPixbufBuild;
            inherit glibBuild;
            inherit gtk3Build;
            inherit gtkLayerShellBuild;
            inherit mesaBuild;
            inherit waylandBuild;
            inherit waylandProtocolsBuild;
            inherit waylandScannerBuild;
            inherit xkbcommonBuild;
            inherit harfbuzzBuild;
            inherit isDarwin;
            inherit libSystemBuild;
            inherit libdisplayInfoBuild;
            inherit libepoxyBuild;
            inherit libffiBuild;
            inherit libiconvBuild;
            inherit libpngBuild;
            inherit libwnckBuild;
            inherit libxfce4uiSrc;
            inherit nativeLd;
            inherit nativeMesonToolsDir;
            inherit pangoBuild;
            inherit pcre2Build;
            inherit pkgs;
            inherit startupNotificationBuild;
            inherit thunarSrc;
            inherit vteBuild;
            inherit xcbBuild;
            inherit xfce4AppfinderSrc;
            inherit xfce4PanelSrc;
            inherit xfce4SessionSrc;
            inherit xfce4SettingsSrc;
            inherit xfce4TerminalSrc;
            inherit xfconfSrc;
            inherit xfdesktopSrc;
            inherit xfwm4Src;
            inherit xlibBuild;
            inherit xvfbLibICEBuild;
            inherit xvfbLibSMBuild;
            inherit xvfbLibXauBuild;
            inherit xvfbLibXcompositeBuild;
            inherit xvfbLibXcursorBuild;
            inherit xvfbLibXdamageBuild;
            inherit xvfbLibXdmcpBuild;
            inherit xvfbLibXextBuild;
            inherit xvfbLibXfixesBuild;
            inherit xvfbLibXiBuild;
            inherit xvfbLibXineramaBuild;
            inherit xvfbLibXpresentBuild;
            inherit xvfbLibXrandrBuild;
            inherit xvfbLibXrenderBuild;
            inherit xvfbLibXresBuild;
            inherit xvfbPixmanBuild;
            inherit xvfbZlibBuild;
          };
          xfceDesktopArm64 = import ./nix/xfce.nix {
            inherit lib;
            targetTriple = "arm64-apple-darwin20.4";
            atspi2CoreBuild = atspi2CoreArm64Build;
            cairoBuild = cairoArm64Build;
            cairoGobjectBuild = cairoGobjectArm64Build;
            darwinCrossToolchain = arm64CrossToolchain;
            dbusBuild = dbusArm64Build;
            inherit exoSrc;
            expatBuild = expatArm64Build;
            fontconfigBuild = fontconfigArm64Build;
            freetype2Build = freetype2Arm64Build;
            fribidiBuild = fribidiArm64Build;
            inherit garconSrc;
            gdkPixbufBuild = gdkPixbufArm64Build;
            glibBuild = glibArm64Build;
            gtk3Build = gtk3Arm64Build;
            gtkLayerShellBuild = gtkLayerShellArm64Build;
            mesaBuild = mesaArm64Build;
            waylandBuild = waylandArm64Build;
            inherit waylandProtocolsBuild;
            inherit waylandScannerBuild;
            xkbcommonBuild = xkbcommonArm64Build;
            harfbuzzBuild = harfbuzzArm64Build;
            inherit isDarwin;
            libSystemBuild = libSystemArm64Build;
            libdisplayInfoBuild = libdisplayInfoArm64Build;
            libepoxyBuild = libepoxyArm64Build;
            libffiBuild = libffiArm64Build;
            libiconvBuild = libiconvArm64Build;
            libpngBuild = libpngArm64Build;
            libwnckBuild = libwnckArm64Build;
            inherit libxfce4uiSrc;
            inherit nativeLd;
            inherit nativeMesonToolsDir;
            pangoBuild = pangoArm64Build;
            pcre2Build = pcre2Arm64Build;
            inherit pkgs;
            startupNotificationBuild = startupNotificationArm64Build;
            inherit thunarSrc;
            vteBuild = vteArm64Build;
            xcbBuild = xcbArm64Build;
            inherit xfce4AppfinderSrc;
            inherit xfce4PanelSrc;
            inherit xfce4SessionSrc;
            inherit xfce4SettingsSrc;
            inherit xfce4TerminalSrc;
            inherit xfconfSrc;
            inherit xfdesktopSrc;
            inherit xfwm4Src;
            xlibBuild = xlibArm64Build;
            xvfbLibICEBuild = xvfbLibICEArm64Build;
            xvfbLibSMBuild = xvfbLibSMArm64Build;
            xvfbLibXauBuild = xvfbLibXauArm64Build;
            xvfbLibXcompositeBuild = xvfbLibXcompositeArm64Build;
            xvfbLibXcursorBuild = xvfbLibXcursorArm64Build;
            xvfbLibXdamageBuild = xvfbLibXdamageArm64Build;
            xvfbLibXdmcpBuild = xvfbLibXdmcpArm64Build;
            xvfbLibXextBuild = xvfbLibXextArm64Build;
            xvfbLibXfixesBuild = xvfbLibXfixesArm64Build;
            xvfbLibXiBuild = xvfbLibXiArm64Build;
            xvfbLibXineramaBuild = xvfbLibXineramaArm64Build;
            xvfbLibXpresentBuild = xvfbLibXpresentArm64Build;
            xvfbLibXrandrBuild = xvfbLibXrandrArm64Build;
            xvfbLibXrenderBuild = xvfbLibXrenderArm64Build;
            xvfbLibXresBuild = xvfbLibXresArm64Build;
            xvfbPixmanBuild = xvfbPixmanArm64Build;
            xvfbZlibBuild = xvfbZlibArm64Build;
          };
          xfconfArm64 = xfceDesktopArm64.xfconfBuild;
          libxfce4utilArm64 = xfceDesktopArm64.libxfce4utilBuild;
          libxfce4uiArm64 = xfceDesktopArm64.libxfce4uiBuild;
          xfwm4Arm64 = xfceDesktopArm64.xfwm4Build;
          libxfce4windowingArm64 = xfceDesktopArm64.libxfce4windowingBuild;
          garconArm64 = xfceDesktopArm64.garconBuild;
          exoArm64 = xfceDesktopArm64.exoBuild;
          xfce4SessionArm64 = xfceDesktopArm64.xfce4SessionBuild;
          xfce4PanelArm64 = xfceDesktopArm64.xfce4PanelBuild;
          xfdesktopArm64 = xfceDesktopArm64.xfdesktopBuild;
          xfce4TerminalArm64 = xfceDesktopArm64.xfce4TerminalBuild;
          xfce4SettingsArm64 = xfceDesktopArm64.xfce4SettingsBuild;
          xfce4AppfinderArm64 = xfceDesktopArm64.xfce4AppfinderBuild;
          thunarArm64 = xfceDesktopArm64.thunarBuild;
          inherit (xfceDesktop)
            xfconfBuild
            libxfce4utilBuild
            libxfce4uiBuild
            xfwm4Build
            libxfce4windowingBuild
            garconBuild
            exoBuild
            xfce4SessionBuild
            xfce4PanelBuild
            xfdesktopBuild
            xfce4TerminalBuild
            xfce4SettingsBuild
            xfce4AppfinderBuild
            thunarBuild
            ;
          cursorThemeBuild = pd.x86_64.cursorTheme;
          iconThemesBuild = pd.x86_64.iconThemes;
          libdisplayInfoBuild = pd.x86_64.libdisplayInfo;
          glibNetworkingBuild = pd.x86_64.glib-networking;

          vteBuild = pd.x86_64.vte;
          xrandrBuild = pd.x86_64.xrandr;
          xrdbBuild = pd.x86_64.xrdb;
          xinitBuild = pd.x86_64.xinit;
          iceauthBuild = pd.x86_64.iceauth;
          xvfbLibXineramaBuild = pd.x86_64.xvfbLibXinerama;
          xvfbLibXresBuild = pd.x86_64.libXres;
          xvfbLibXcompositeBuild = pd.x86_64.libXcomposite;
          xvfbLibXdamageBuild = pd.x86_64.libXdamage;
          xvfbLibXpresentBuild = pd.x86_64.xvfbLibXpresent;
          libXftBuild = pd.x86_64.libXft;
          dmenuBuild = pd.x86_64.dmenu;
          xvfbLibXiBuild = pd.x86_64.libXi;
          xeyesBuild = pd.x86_64.xeyes;
          xclockBuild = pd.x86_64.xclock;
          xcalcBuild = pd.x86_64.xcalc;
          xmessageBuild = pd.x86_64.xmessage;
          fltkBuild = pd.x86_64.fltk;
          mkSharedXorgLib = args: pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix ({
            inherit darwinCrossToolchain nativeLd;
            libSystem = libSystemBuild;
            nativeMesonTools = nativeMesonToolsDir;
            guestPrefix = true;
            shared = true;
          } // args);

          libXauSharedBuild = pd.x86_64.libXauSharedBuild;
          libXdmcpSharedBuild = pd.x86_64.libXdmcpSharedBuild;
          libxcbSharedBuild = pd.x86_64.libxcbSharedBuild;
          libX11SharedBuild = pd.x86_64.libX11SharedBuild;
          libXextSharedBuild = pd.x86_64.libXextSharedBuild;
          libXrenderSharedBuild = pd.x86_64.libXrenderSharedBuild;
          libXfixesSharedBuild = pd.x86_64.libXfixesSharedBuild;
          libXiSharedBuild = pd.x86_64.libXiSharedBuild;
          libXrandrSharedBuild = pd.x86_64.libXrandrSharedBuild;
          libXxf86vmSharedBuild = pd.x86_64.libXxf86vm;
          libXcursorSharedBuild = pd.x86_64.libXcursorSharedBuild;

          # Wine's schannel/bcrypt TLS backend is GnuTLS-only
          vulkanToolsBuild = pd.x86_64.vulkan-tools;
          libdrmVirtgpuBuild = pd.x86_64.libdrmVirtgpu;
          mesaVenusBuild =
            pkgs.callPackage ./nix/pkgs/mesa/mesa-venus.nix rec {
              version = "26.0.3";
              src = pkgs.fetchurl {
                url = "https://archive.mesa3d.org/mesa-${version}.tar.xz";
                sha256 = "0d71d6vslcmyr94bma7cvacz0dzrgc3hzf5nni2sm2cf68yl9dyx";
              };
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd appleSdk;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              zlib = xvfbZlibBuild;
              libdrmVirtgpu = libdrmVirtgpuBuild;
              inherit (pkgs) meson ninja pkg-config python3 bison flex glslang;
            };
          venusTestsBuild = pd.x86_64.venus-tests;
          # venus with x11 and wayland wsi, and the Metal demos (pdmtl window, Indium tests) windowed on PD
          mesaVenusWsiBuild =
            mesaVenusBuild.override {
              withX11 = true;
              withWayland = true;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              libX11 = libX11SharedBuild;
              libxcb = libxcbSharedBuild;
              libXau = libXauSharedBuild;
              libXdmcp = libXdmcpSharedBuild;
              libxshmfence = libxshmfenceSharedBuild;
              inherit (pkgs) xorgproto;
            };
          # the wayland-only vulkan stack for image-wayland-minimal, so it carries no x11 client libraries
          vulkanLoaderNoxBuild = pd.x86_64.vulkanLoaderNox;
          vulkanToolsNoxBuild = pd.x86_64.vulkanToolsNox;
          mesaVenusWaylandBuild =
            mesaVenusBuild.override {
              withWayland = true;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
            };
          venusTestsNoxBuild = pd.x86_64.venusTestsNox;
          # the Metal demos on the same loader, GLFW dlopens its window system, so Wayland needs no x11 libraries
          metalPdNoxBuild = pd.x86_64.metalPdNox;
          metalObjcPdNoxBuild = pd.x86_64.metalObjcPdNox;
          metalAppleSamplesNoxBuild = pd.x86_64.metalAppleSamplesNox;
          glfwPdBuild = pd.x86_64.glfw;
          spirvCrossPdBuild = pd.x86_64.spirvCross;
          metalPdBuild = pd.x86_64.metal-pd;
          # mslc for Metal.framework's newLibraryWithSource:, loaded into PD's clang
          mslcPdBuild = pd.x86_64.mslc-pd;
          metalObjcPdBuild = pd.x86_64.metalObjc;
          # MetalPerformanceShaders' image kernels on PD's Metal
          mpsBuild = pd.x86_64.mps;
          mpsNoxBuild = pd.x86_64.mpsNox;
          iosurfaceBuild = pd.x86_64.iosurface;
          akffTestBuild = pd.x86_64.akff-test;
          argonGfxTestBuild = pd.x86_64.argon-gfx-test;
          carbonBuild = pd.x86_64.carbon;
          # sharing, plane and GL tests for IOSurface, for test images only
          iosurfaceTestsBuild = pd.x86_64.iosurface-tests;
          audiotoolboxBuild = pd.x86_64.audiotoolbox;
          # VK_EXT_metal_surface as an implicit layer over CAMetalLayer's Wayland subsurface
          pdMetalSurfaceBuild = pd.x86_64.pd-metal-surface;
          pcmplayBuild = pd.x86_64.pcmplay;
          coreaudiodBuild = pd.x86_64.coreaudiod;
          # apple's framework install paths (CoreFoundation re-export shim, Versions/C) for Xcode-built apps
          frameworkCompatBuild = pd.x86_64.framework-compat;
          # the Cocoa.framework umbrella over Foundation, AppKit and CoreData that Xcode-built apps link
          cocoaBuild = pd.x86_64.cocoa;
          # CoreMedia, UniformTypeIdentifiers, ImageIO, UserNotifications, ScreenCaptureKit, GameKit
          appFrameworksBuild = pd.x86_64.appFrameworks;
          # tests for the frameworks the official Firefox build links, run at boot in image-frameworks-test
          frameworksFfTestsBuild = pd.x86_64.frameworks-ff-tests;
          # apple's Metal samples (MIT) on PD's Metal, MetalKit and AppKit storyboards
          # mslc for the build host, compiling the samples' shaders at build time
          mslcHostBuild = pd.x86_64.mslc;
          metalAppleSamplesBuild = pd.x86_64.metal-apple-samples;
          vulkanLoaderBuild = pd.x86_64.vulkanLoader;
          libxshmfenceSharedBuild = pd.x86_64.libxshmfence;
          nettleSharedBuild = pd.x86_64.nettle-shared;
          gnutlsSharedBuild = pd.x86_64.gnutls;

          wineToolsBuild = pd.x86_64.wineTools;

          wineBuild = pd.x86_64.wine;

          dilloBuild = pd.x86_64.dillo;
          # the PureDarwin package sets, one per target (see nix/pd/scope.nix)
          pdScope = import ./nix/pd/scope.nix {
            inherit lib pkgs;
            shared = {
              appleSdk = appleSdk;
              asmjitSrc = asmjitSrc;
              coreFoundationSource = coreFoundationSource;
              coreServicesSource = coreServicesSource;
              foundationSource = foundationSource;
              libSystemSource = libSystemSource;
              libcxxDylibSource = libcxxDylibSource;
              mingwAarch64 = mingwAarch64;
              mkPureDarwin = mkPureDarwinBuild;
              nativeMesonToolsDir = nativeMesonToolsDir;
              nativeMigcom = nativeMigcom;
              objcSource = objcSource;
              protocolBufferSource = protocolBufferSource;
              swiftToolchain = swiftToolchain;
              wirelessDiagnosticsSource = wirelessDiagnosticsSource;
              cctoolsSource = cctoolsSource;
              chocolateDoomPatchedSrc = chocolateDoomPatchedSrc;
              diskArbitrationSource = diskArbitrationSource;
              fbdoomExternalSrc = fbdoomExternalSrc;
              fbdoomSource = fbdoomSource;
              iokitCFSource = iokitCFSource;
              linux614Src = linux614Src;
              mkSharedXorgLib = mkSharedXorgLib;
              mkSystemConfiguration = mkSystemConfigurationBuild;
              securitySource = securitySource;
              symptomReporterSource = symptomReporterSource;
              userlandSource = userlandSource;
              vteSrc = vteSrc;
              iig-tools = iig-tools;
            };
          };
          pd.x86_64 = pdScope {
            inherit darwinCrossToolchain nativeLd;
            # the packages not yet in nix/pd/packages.nix, by their scope names
            bridge = {
              atspi2CoreNoxBuild = atspi2CoreNoxBuild;
              cairoGobjectNoxBuild = cairoGobjectNoxBuild;
              clang = clangCrossBuild;
              compilerRt = compilerRtBuild;
              darwinCrossToolchain = darwinCrossToolchain;
              glu = gluNoxBuild;
              gtk3NoxBuild = gtk3NoxBuild;
              harfbuzzNoxBuild = harfbuzzNoxBuild;
              hostOtool = hostOtoolBuild;
              libepoxyNoxBuild = libepoxyNoxBuild;
              libz = libzDylibBuild;
              llvm = llvmCrossBuild;
              mesa = mesaNoxBuild;
              mesaDefaultBuild = mesaGpuBuild;
              metalObjcPdNoxBuild = metalObjcPdNoxBuild;
              metalPd = metalPdBuild;
              nativeLd = nativeLd;
              openglFramework = openglFrameworkNoxBuild;
              opensslThreadsBuild = opensslThreadsBuild;
              pangoNoxBuild = pangoNoxBuild;
              utilityApp = utilityApp;
              xkbcommonNoxBuild = xkbcommonNoxBuild;
            };
          };
          ncursesBuild = pd.x86_64.ncurses;
          libiconvBuild = pd.x86_64.libiconv;
          toyboxBuild = pd.x86_64.toybox;
          nanoBuild = pd.x86_64.nano;
          # the DLLs are the same on any host, nixpkgs only offers dxvk on x86 so other hosts take it from gnu64
          dxvkBuild = pd.x86_64.dxvk;
          winetricksBuild = pd.x86_64.winetricks;
          cabextractBuild = pd.x86_64.cabextract;
          xxdBuild =
            pkgs.callPackage ./nix/pkgs/toolchain/xxd.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              tinyxxd = pkgs.tinyxxd;
            };
          xzBuild = pd.x86_64.xz;
          e2fsprogsBuild = pd.x86_64.e2fsprogs;
          bzip2Build = pd.x86_64.bzip2;
          unzipBuild = pd.x86_64.unzip;
          bmakeBuild = pd.x86_64.bmake;
          gnumakeBuild = pd.x86_64.gnumake;
          pkgconfBuild = pd.x86_64.pkgconf;
          mesonBuild = pd.x86_64.meson;
          cmakeBuild = pd.x86_64.cmake;
          # Nix and its static dependencies (see nix/pkgs/nix).
          nixPortPackages =
            import ./nix/pkgs/nix {
              inherit lib pkgs darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              openssl = opensslBuild;
              zlib = xvfbZlibBuild;
              xz = xzBuild;
              curl = curlBuild;
              corefoundation = coreFoundationBuild;
              systemConfiguration = systemConfigurationBuild;
            };
          ninjaBuild = pd.x86_64.ninja;
          gnum4Build = pd.x86_64.gnum4;
          autoconfBuild = pd.x86_64.autoconf;
          automakeBuild = pd.x86_64.automake;
          bisonBuild = pd.x86_64.bison;
          flexBuild = pd.x86_64.flex;
          pythonBuild = pd.x86_64.python;
          perlBuild = pd.x86_64.perl;
          zshBuild = pd.x86_64.zsh;
          bashBuild = pd.x86_64.bash;
          fileBuild = pd.x86_64.file;
          opensslBuild = pd.x86_64.openssl;
          curlBuild = pd.x86_64.curl;
          opensshBuild = pd.x86_64.openssh;
          gitBuild = pd.x86_64.git;
          migcomDarwinBuild =
            pkgs.callPackage ./nix/pkgs/toolchain/migcom-darwin.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
            };
          pdblessBuild = pd.x86_64.pdbless;
          msdosCmdsBuild = pd.x86_64.msdosCmds;
          ioregBuild = pd.x86_64.ioreg;
          xkbcommonBuild = pd.x86_64.xkbcommon;
          waylandScannerBuild = pd.x86_64.waylandScanner;
          waylandProtocolsBuild = pd.x86_64.waylandProtocols;
          neuwldBuild = pd.x86_64.neuwld;
          neuswcBuild = pd.x86_64.neuswc;
          wlrootsBuild = pd.x86_64.wlroots;
          swayBuild = pd.x86_64.sway;
          # Wayland-only image: wlroots/sway rebuilt without the Xwayland
          # backend so nothing on the image links libxcb.
          wlrootsNoxBuild = pd.x86_64.wlrootsNox;
          swayNoxBuild = pd.x86_64.swayNox;
          tllistBuild = pd.x86_64.tllist;
          fcftBuild = pd.x86_64.fcft;
          footBuild = pd.x86_64.foot;
          pdEpollShimBuild = pd.x86_64.epollShim;
          waylandBuild = pd.x86_64.wayland;
          fastfetchBuild = pd.x86_64.fastfetch;
          xtermBuild = pd.x86_64.xterm;
          icuCoreBuild = pd.x86_64.icu;
          coreFoundationBuild = pd.x86_64.corefoundation;
          libcxxabiDylibBuild = pd.x86_64.libcxxabiDylib;

          libcxxDylibBuild = pd.x86_64.libcxxDylib;
          libcxxTestBuild = pd.x86_64.libcxxTest;
          libobjcBuild = pd.x86_64.libobjc;
          asmjitTestBuild = pd.x86_64.asmjit-test;
          elfaotBuild = pd.x86_64.elfaot;
          elfaotDarwinBuild = pd.x86_64.elfaotDarwin;
          elftransStageBuild = pd.x86_64.elftrans-stage;
          elftransBuild = pd.x86_64.elftrans;
          objcTestBuild = pd.x86_64.objcTest;
          foundationBuild = pd.x86_64.foundation;
          swiftToolchain = pkgs.callPackage ./nix/pkgs/toolchain/swift-toolchain.nix { };
          swiftRuntimeBuild = pd.x86_64.swiftRuntime;
          swiftRuntimeArm64Build = swiftRuntimeBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = arm64.libSystemArm64Build;
            libobjc = libobjcArm64Build;
            libcxxDylib = libcxxDylibArm64Build;
            libcxxabiDylib = libcxxabiDylibArm64Build;
            corefoundation = coreFoundationArm64Build;
            foundation = foundationArm64Build;
          };
          frameworkCompatArm64Build = frameworkCompatBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = arm64.libSystemArm64Build;
            corefoundation = coreFoundationArm64Build;
            foundation = foundationArm64Build;
            swiftRuntime = swiftRuntimeArm64Build;
          };
          protocolBufferBuild = pd.x86_64.protocolBuffer;
          wirelessDiagnosticsBuild = pd.x86_64.wirelessDiagnostics;
          iokitBuild = pd.x86_64.iokit;
          coreServicesBuild = pd.x86_64.coreservices;
          openglFrameworkBuild = pd.x86_64.openglFrameworkBuild;
          appkitBuild = pd.x86_64.appkit;
          windowserverBuild = pd.x86_64.windowserver;
          applicationservicesBuild = pd.x86_64.applicationservices;
          corevideoBuild = pd.x86_64.corevideo;
          quartzcoreBuild = pd.x86_64.quartzcore;
          # arm64 twins of the remaining image packages and their dependency
          # closure, generated from the x86 wiring: toolchain/triple/libSystem
          # come from mkArm64Build, and each package's own PureDarwin deps are
          # pointed at their arm64 builds.

          # arm64 cross-compiled package set. Split out to keep this file
          # focused on the x86 build and the shared plumbing both arches use.
          arm64 = import ./nix/arm64.nix {
            inherit lib pkgs isDarwin pdScope;
            inherit kextsOnlySource xnuHeadersArm64Build kextsOnlyArgs;
            inherit arm64CrossToolchain;
            inherit armv6CrossToolchain;
            inherit coreFoundationBuild;
            inherit darwinCrossToolchain;
            inherit fbdoomSource;
            inherit foundationSource;
            inherit hostOtoolBuild;
            inherit i3statusShimBuild;
            inherit tllistBuild;
            inherit localeDataBuild;
            inherit applicationservicesBuild systemFontsBuild waylandSessionBuild;
            inherit icuCoreBuild;
            inherit iokitBuild;
            inherit iokitCFStaticBuild;
            inherit kernelSource;
            inherit kextsSource;
            inherit launchctlBuild;
            inherit launchdBuild;
            inherit libSystemBuild;
            inherit libcxxDylibBuild;
            inherit libcxxabiDylibBuild;
            inherit libiconvBuild;
            inherit libobjcBuild;
            inherit mkPureDarwinBuild;
            inherit mkSystemConfigurationBuild;
            inherit symptomReporterBuild;
            inherit nativeLd;
            inherit nativeMesonToolsDir;
            inherit ncursesBuild;
            inherit coreServicesSource;
            inherit vteSrc;
            inherit asmjitSrc;
            inherit wineToolsBuild;
            inherit fexWow64Build;
            mingwAarch64Cc = mingwAarch64.cc;
            mingwArm64ecCc = mingwAarch64.arm64ecCc;
            inherit waylandScannerBuild;
            inherit waylandProtocolsBuild;
            inherit diskArbitrationSource;
            inherit securitySource;
            inherit userlandBuild;
            inherit userlandSource;
            inherit xkeyboardConfigBuild;
            inherit xlibLocaleBuild;
            inherit xvfbFontsBuild;
            inherit zshBuild;
            inherit xfconfArm64 libxfce4utilArm64 libxfce4uiArm64
              libxfce4windowingArm64 libwnckArm64Build garconArm64 exoArm64
              xfwm4Arm64 xfce4SessionArm64 xfce4PanelArm64 xfdesktopArm64
              xfce4AppfinderArm64 thunarArm64 xfce4SettingsArm64
              xfce4TerminalArm64;
          };
          inherit (arm64)
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
            mesaNouveauArm64Build
            pdgltriNouveauArm64Build
            tx1GlRootArm64Bundle
            tx1DesktopArm64Bundle
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
            neuwldMsmArm64Build
            neuswcMsmArm64Build
            argonMsmArm64Bundle
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
            kernelArm64Sun50iBuild
            kernelArm64Sun50iDebugBuild
            kernelArm64Sc8280xpBuild
            kernelArm64Sc8280xpDebugBuild
            kernelArm64Sdm670Build
            kernelArm64Virt4kDebugBuild
            kernelArm32Bcm2835Build
            kernelArm32Bcm2835DebugBuild
            kernelArm32Bcm2835DevBuild
            kextsArm32Bcm2835Build
            compilerRtArmv6Build
            libSystemArmv6Build
            userlandArm32Bcm2835Build
            kextsArm64Build
            splitBaseSystemArm64VirtMinimal
            splitBaseSystemArm64VirtMinimalRelease
            splitBaseSystemArm64VirtWayland
            imageExtraPackageSetArm64
            imageExtraPackagesArm64Nox
            imageExtraPackagesArm64Sg2002
            splitBaseSystemArm64Sg2002
            ;
          # The XFCE desktop is instantiated in this file (it needs both the
          # arm64 builds and the shared sources), so it is appended here rather
          # than inside arm64.nix where the rest of the list lives.
          imageExtraPackagesArm64 = arm64.imageExtraPackagesArm64 ++ [
            xfconfArm64 libxfce4utilArm64 libxfce4uiArm64 libxfce4windowingArm64
            garconArm64 exoArm64 xfwm4Arm64 xfdesktopArm64 thunarArm64
            # /usr/lib/swift and the Foundation path Xcode-built apps link
            swiftRuntimeArm64Build frameworkCompatArm64Build
          ];
          certificatesBundleBuild = pd.x86_64.certificates-bundle;
          opensslThreadsBuild = pd.x86_64.opensslThreads;
          securityBuild = pd.x86_64.security;
          mkSystemConfigurationBuild = { corefoundation, libobjc, security, iokit, symptomReporter }:
            (mkPureDarwinBuild {
              pname = "puredarwin-systemconfiguration";
              src = systemConfigurationSource;
              buildTargets = [ "SystemConfiguration" "configd" "bootplib_static" "ipconfiguration_static" "ipconfig" ];
              enableProjects = false;
              enableKernel = false;
              enableUserspace = false;
              installUserland = false;
              installKernel = false;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_SYSTEMCONFIGURATION=ON"
                "-DPUREDARWIN_COREFOUNDATION_PREFIX=${corefoundation}"
                "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjc}"
                "-DPUREDARWIN_SECURITY_PREFIX=${security}"
                # InterfaceNamer drives the interface-naming user client through
                # IOKitUser's CF-based IOKitLib.
                "-DPUREDARWIN_IOKIT_PREFIX=${iokit}"
                "-DPUREDARWIN_SYMPTOMREPORTER_PREFIX=${symptomReporter}"
              ];
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                fw="$out/System/Library/Frameworks/SystemConfiguration.framework"
                mkdir -p "$fw/Versions/A/Headers"
                cp build-nix/src/Libraries/SystemConfiguration/libSystemConfiguration.dylib \
                  "$fw/Versions/A/SystemConfiguration"
                cp -a src/Libraries/SystemConfiguration/include/SystemConfiguration/. \
                  "$fw/Versions/A/Headers/"
                # Resources the framework looks up at runtime: the localized
                # interface-name strings and NetworkConfiguration.plist, which
                # InterfaceNamer consults while naming. Without them every
                # lookup logs "failed to get resource url" and falls back to an
                # empty strings table.
                mkdir -p "$fw/Versions/A/Resources"
                cp src/Libraries/SystemConfiguration/SystemConfiguration.fproj/NetworkConfiguration.plist \
                  "$fw/Versions/A/Resources/"
                # CFBundleGetBundleWithIdentifier needs the identifier from Info.plist
                cp src/Libraries/SystemConfiguration/SystemConfiguration.fproj/Info.plist \
                  "$fw/Versions/A/Resources/"
                cp -a src/Libraries/SystemConfiguration/SystemConfiguration.fproj/en.lproj \
                  "$fw/Versions/A/Resources/"
                ln -s A "$fw/Versions/Current"
                ln -s Versions/Current/SystemConfiguration "$fw/SystemConfiguration"
                ln -s Versions/Current/Headers "$fw/Headers"
                ln -s Versions/Current/Resources "$fw/Resources"
                # Flat dylib alias, as Security.framework and OpenGL have.
                mkdir -p "$out/usr/lib"
                ln -s "../../System/Library/Frameworks/SystemConfiguration.framework/Versions/A/SystemConfiguration" \
                  "$out/usr/lib/libSystemConfiguration.dylib"
                mkdir -p "$out/include"
                cp -a src/Libraries/SystemConfiguration/include/SystemConfiguration "$out/include/"
                # configd, the SCDynamicStore server, plus its launchd job.
                mkdir -p "$out/usr/libexec" "$out/System/Library/LaunchDaemons"
                cp build-nix/src/Libraries/SystemConfiguration/configd.tproj/configd \
                  "$out/usr/libexec/configd"
                cp src/Libraries/SystemConfiguration/configd.tproj/com.apple.configd.plist \
                  "$out/System/Library/LaunchDaemons/"
                # Builtin plugins live inside configd, but plugin_support.c still
                # discovers them by walking /System/Library/SystemConfiguration
                # for bundles, so each one needs its Info.plist installed.
                # IPConfiguration is builtin too, but its sources live in the
                # bootp project rather than under Plugins/, and its plist is
                # generated by src/Libraries/bootp/CMakeLists.txt.
                for p in PreferencesMonitor LinkConfiguration KernelEventMonitor IPMonitor InterfaceNamer; do
                  d="$out/System/Library/SystemConfiguration/$p.bundle/Contents"
                  mkdir -p "$d"
                  # The build-dir copy, not the source one: these plists set
                  # CFBundleIdentifier to Xcode's $(PRODUCT_BUNDLE_IDENTIFIER),
                  # and configd skips any bundle whose identifier does not
                  # resolve. The build substitutes it; the source cannot.
                  cp "build-nix/src/Libraries/SystemConfiguration/configd.tproj/plugin-plists/$p/Info.plist" "$d/"
                done
                d="$out/System/Library/SystemConfiguration/IPConfiguration.bundle/Contents"
                mkdir -p "$d"
                cp "build-nix/src/Libraries/bootp/plugin-plists/IPConfiguration/Info.plist" "$d/"
                # ipconfig(8): SystemStarter runs `ipconfig waitall` to block
                # until every interface has finished configuring.
                mkdir -p "$out/usr/sbin"
                cp build-nix/src/Libraries/bootp/ipconfig "$out/usr/sbin/ipconfig"
                runHook postInstall
              '';
            });
          # IOKitUser's CoreFoundation-based IOKitLib. Built apart from libSystem
          # because it links CoreFoundation, which itself links libSystem.
          iokitCFStaticBuild = pd.x86_64.iokitCFStatic;
          # Diagnostic: replays diskarbitrationd's DADiskCreateFromIOMedia checks
          # against one IOMedia and names the failing one.
          iomediacheckBuild = pd.x86_64.iomediacheck;
          # Apple's SymptomReporter is a private framework with no source and
          # no SDK header; this is PureDarwin's own, recording symptoms through
          # os_log. bootp's IPConfiguration plugin is the consumer.
          symptomReporterBuild = pd.x86_64.symptomReporter;
          diskArbitrationBuild = pd.x86_64.diskArbitration;
          systemConfigurationBuild = pd.x86_64.systemConfiguration;
          systemStarterBuild = pd.x86_64.systemStarter;
          launchctlBuild = pd.x86_64.launchctl;
          launchdBuild = pd.x86_64.launchd;
          libSystemBuild = pd.x86_64.libSystem;
          fbdoomBuild = pd.x86_64.fbdoom;
          kernelBuild = mkPureDarwinBuild {
            pname = "puredarwin-kernel";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "RELEASE";
          };
          kernelDebugBuild = mkPureDarwinBuild {
            pname = "puredarwin-kernel-debug";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "DEBUG";
          };
          # the kasan and kcov denylists name .c files the headers source leaves out
          xnuHeadersPostPatch = old: {
            postPatch = (old.postPatch or "") + ''
              printf '#!/bin/sh\nexit 0\n' > src/Kernel/xnu/san/tools/validate_denylist.sh
            '';
          };
          xnuHeadersBuild = (mkPureDarwinBuild {
            pname = "puredarwin-xnu-headers";
            src = xnuHeadersSource;
            buildTargets = [ "xnu_headers.extproj" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installXnuHeaders = true;
            xnuKernelConfig = "RELEASE";
          }).overrideAttrs xnuHeadersPostPatch;
          xnuHeadersArm64Build = (mkPureDarwinBuild {
            pname = "puredarwin-xnu-headers-arm64";
            src = xnuHeadersSource;
            buildTargets = [ "xnu_headers.extproj" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installXnuHeaders = true;
            xnuKernelConfig = "RELEASE";
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
          }).overrideAttrs xnuHeadersPostPatch;
          # kext builds configure only the kexts and take the xnu headers prebuilt
          kextsOnlyArgs = xnuHeaders: {
            enableTools = false;
            extraCmakeFlags = [
              "-DPUREDARWIN_KEXTS_ONLY=ON"
              "-DPUREDARWIN_PREBUILT_XNU_HEADERS=${xnuHeaders}"
            ];
          };
          # the kexts nix/lib/kexts.nix gives an arch, one derivation per kext directory,
          # each from that directory, the header dirs it includes and the kext-only root project
          kextList = import ./nix/lib/kexts.nix;
          kextsFor = arch: builtins.filter (k: builtins.elem arch k.arches) kextList;
          kextDirSource = arch: dir:
            let
              fs = lib.fileset;
              ext = d: ./src/Kernel/Extensions + "/${d}";
              own = builtins.filter (k: k.dir == dir) (kextsFor arch);
              # kext dirs nested in this one belong to their own derivations
              nested = lib.unique (builtins.filter (d: lib.hasPrefix "${dir}/" d) (map (k: k.dir) kextList));
              depsOf = d: lib.concatMap (k: k.headers or [ ]) (builtins.filter (k: k.dir == d) kextList);
              headerDirs = map (x: x.key) (builtins.genericClosure {
                startSet = map (d: { key = d; }) (lib.concatMap (k: k.headers or [ ]) own);
                operator = x: map (d: { key = d; }) (depsOf x.key);
              });
              # a family with header targets brings its CMakeLists.txt, which stops after them
              headerFiles = d: fs.unions ([
                (fs.fileFilter (f: f.hasExt "h" || f.hasExt "hpp" || f.hasExt "inc" || f.hasExt "def") (ext d))
              ] ++ lib.optional (builtins.any (k: k.dir == d && (k.headerTargets or false)) kextList)
                (ext "${d}/CMakeLists.txt"));
            in
            fs.toSource {
              root = ./.;
              fileset = fs.unions ([
                kextsOnlyFiles
                ./src/Kernel/Extensions/CMakeLists.txt
                (if nested == [ ] then ext dir else fs.difference (ext dir) (fs.unions (map ext nested)))
              ] ++ map headerFiles (lib.subtractLists [ dir ] headerDirs)
                ++ map (x: ./. + "/${x}") (lib.unique (lib.concatMap (k: k.extra or [ ]) own)));
            };
          # the slice configures only the dirs it has, and dirs that are not its own only define headers
          kextPostPatch = dir: ''
            cat - src/Kernel/Extensions/CMakeLists.txt > CMakeLists.kext <<'EOF'
            set(_pd_kext_dir "${dir}")
            macro(add_subdirectory _pd_dir)
                set(_pd_subdir "''${_pd_dir}")
                if(NOT IS_ABSOLUTE "''${_pd_subdir}")
                    set(_pd_subdir "''${CMAKE_CURRENT_SOURCE_DIR}/''${_pd_subdir}")
                endif()
                if(EXISTS "''${_pd_subdir}/CMakeLists.txt")
                    file(RELATIVE_PATH _pd_rel "''${CMAKE_SOURCE_DIR}/src/Kernel/Extensions" "''${_pd_subdir}")
                    set(_pd_headers_only "''${PUREDARWIN_KEXT_HEADERS_ONLY}")
                    if(_pd_rel MATCHES "^[.][.]/" OR _pd_rel STREQUAL _pd_kext_dir)
                        set(PUREDARWIN_KEXT_HEADERS_ONLY OFF)
                    else()
                        set(PUREDARWIN_KEXT_HEADERS_ONLY ON)
                    endif()
                    _add_subdirectory(''${ARGV})
                    set(PUREDARWIN_KEXT_HEADERS_ONLY "''${_pd_headers_only}")
                endif()
            endmacro()
            # drop ordering on kexts outside the slice
            macro(add_dependencies _pd_target)
                if(NOT "''${_pd_target}" STREQUAL "kexts")
                    set(_pd_deps)
                    foreach(_pd_dep ''${ARGN})
                        if(TARGET "''${_pd_dep}" OR NOT CMAKE_CURRENT_SOURCE_DIR MATCHES "/src/Kernel/Extensions(/|$)")
                            list(APPEND _pd_deps "''${_pd_dep}")
                        endif()
                    endforeach()
                    if(_pd_deps)
                        _add_dependencies("''${_pd_target}" ''${_pd_deps})
                    endif()
                endif()
            endmacro()
            EOF
            mv CMakeLists.kext src/Kernel/Extensions/CMakeLists.txt
          '';
          mkKext = arch: xnuHeaders: dir:
            let
              own = builtins.filter (k: k.dir == dir) (kextsFor arch);
              built = builtins.filter (k: !(builtins.elem arch (k.plistOnly or [ ]))) own;
            in
            (mkPureDarwinBuild ({
              pname = "puredarwin-kext-${lib.toLower (lib.removeSuffix ".kext" (builtins.head own).name)}";
              src = kextDirSource arch dir;
              # a plist-only kext is just configured, build.ninja is already up to date
              buildTargets = if built == [ ] then [ "build.ninja" ] else map (k: k.target or k.name) built;
              enableUserspace = false;
              installUserland = false;
              installKernel = false;
              installKexts = true;
              installKextNames = map (k: k.name) own;
              enableIOGraphicsFamily = true;
            } // lib.optionalAttrs (arch != "x86_64") {
              puredarwinArch = arch;
              inherit arm64CrossToolchain;
            } // kextsOnlyArgs xnuHeaders // {
              # the fbsdkpi kexts build linuxkpi the drm way, against the one drm-kmod tree every driver shares
              extraCmakeFlags = (kextsOnlyArgs xnuHeaders).extraCmakeFlags
                ++ lib.optional (arch == "x86_64" && builtins.any (k: builtins.elem "src/Kernel/fbsdkpi/fbsdkpi.cmake" (k.extra or [ ])) own)
                  "-DPD_DRMKMOD_DIR=${drmKmodAllSrc}";
            })).overrideAttrs (old: {
              postPatch = (old.postPatch or "") + kextPostPatch dir;
            });
          kextDirs = arch: lib.unique (map (k: k.dir) (kextsFor arch));
          kextBuilds = lib.genAttrs (kextDirs "x86_64") (mkKext "x86_64" xnuHeadersBuild);
          # one tree as before, with the links replaced by copies, since the base system splits cp -a it into images
          kextsBuild = pkgs.symlinkJoin {
            name = "puredarwin-kexts-0.1";
            paths = builtins.attrValues kextBuilds;
            postBuild = ''
              find $out -type l | while read -r f; do
                cp --remove-destination "$(readlink -f "$f")" "$f"
              done
            '';
          };
          # FreeBSD's drm-kmod (Linux 6.14 DRM and amdgpu on LinuxKPI), with the
          # files PureDarwin leaves out removed and its small patch applied
          drmKmodSrc = pkgs.runCommand "puredarwin-drm-kmod-src" {
            src = pkgs.fetchFromGitHub {
              owner = "freebsd";
              repo = "drm-kmod";
              rev = "c16de0f9fa84f70b9551f69fde837a7f3a73c0e9";
              sha256 = "00rbflry435i0h54pgca5fzp0qsvbgzcxzv0c832znksr174k6pa";
            };
          } ''
            cp -r $src $out
            chmod -R u+w $out
            cd $out
            rm -rf drivers/gpu/drm/i915 drivers/gpu/drm/radeon
            grep -v '^#' ${./src/Kernel/fbsdkpi/drm/drm-kmod-remove.txt} | while read -r f; do
              rm -rf "$f"
            done
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/drm-kmod-pd.patch}
          '';
          # the same drm-kmod with i915 kept, its PureDarwin patch applied and the
          # unlicensed or GPL i915 files PD replaces left out
          drmKmodI915Src = pkgs.runCommand "puredarwin-drm-kmod-i915-src" {
            src = drmKmodSrc.src;
          } ''
            cp -r $src $out
            chmod -R u+w $out
            cd $out
            rm -rf drivers/gpu/drm/radeon
            grep -v '^#' ${./src/Kernel/fbsdkpi/drm/drm-kmod-remove.txt} | while read -r f; do
              rm -rf "$f"
            done
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/drm-kmod-pd.patch}
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/drm-kmod-i915-pd.patch}
          '';
          # the same drm-kmod with radeon kept and its PureDarwin patch applied. radeon_freebsd.c
          # and the trace and register table headers carry no licence notice, PD brings its own
          # trace header and regenerates the tables from reg_srcs with the MIT mkregtable
          drmKmodRadeonSrc = pkgs.runCommand "puredarwin-drm-kmod-radeon-src" {
            src = drmKmodSrc.src;
            nativeBuildInputs = [ pkgs.stdenv.cc ];
          } ''
            cp -r $src $out
            chmod -R u+w $out
            cd $out
            rm -rf drivers/gpu/drm/i915
            grep -v '^#' ${./src/Kernel/fbsdkpi/drm/drm-kmod-remove.txt} | while read -r f; do
              rm -rf "$f"
            done
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/drm-kmod-pd.patch}
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/drm-kmod-radeon-pd.patch}
            r=drivers/gpu/drm/radeon
            rm -f $r/radeon_freebsd.c $r/radeon_trace_freebsd.h $r/*_reg_safe.h
            cc -o mkregtable $r/mkregtable.c
            for t in r100 r200 r300 r420 rn50 rs600 rv515 r600 evergreen cayman; do
              ./mkregtable $r/reg_srcs/$t > $r/''${t}_reg_safe.h
            done
            rm mkregtable
          '';
          # nouveau from linux 6.14, the release drm-kmod's drm core follows, in a copy of the
          # drm-kmod tree with the ch7006 and sil164 headers nouveau's nv04 tv-out includes
          linux614Src = pkgs.fetchurl {
            url = "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.14.tar.xz";
            hash = "sha256-opS2g+exYbsFF7sy7H7R0up2A9+6utE1Fw7RLQDEdnA=";
          };
          drmKmodNouveauSrc = pkgs.runCommand "puredarwin-drm-kmod-nouveau-src" {
            nativeBuildInputs = [ pkgs.xz ];
          } ''
            cp -r ${drmKmodSrc} $out
            chmod -R u+w $out
            tar -xJf ${linux614Src} --strip-components=1 -C $out \
              linux-6.14/drivers/gpu/drm/nouveau \
              linux-6.14/include/drm/i2c/ch7006.h linux-6.14/include/drm/i2c/sil164.h
            cd $out
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/linux-nouveau-pd.patch}
            # PD's clean-room headers take the place of three whose history could not be settled
            cp -r ${./src/Kernel/fbsdkpi/drm/nouveau}/. drivers/gpu/drm/nouveau/
            chmod -R u+w drivers/gpu/drm/nouveau
          '';
          # one tree for every drm driver and the shared linuxkpi: the drm core is the same in all of them, the
          # per-driver patches only touch their own driver
          drmKmodAllSrc = pkgs.runCommand "puredarwin-drm-kmod-all-src" {
            nativeBuildInputs = [ pkgs.xz pkgs.stdenv.cc ];
          } ''
            cp -r ${drmKmodSrc.src} $out
            chmod -R u+w $out
            cd $out
            grep -v '^#' ${./src/Kernel/fbsdkpi/drm/drm-kmod-remove.txt} | while read -r f; do
              rm -rf "$f"
            done
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/drm-kmod-pd.patch}
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/drm-kmod-i915-pd.patch}
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/drm-kmod-radeon-pd.patch}
            r=drivers/gpu/drm/radeon
            rm -f $r/radeon_freebsd.c $r/radeon_trace_freebsd.h $r/*_reg_safe.h
            cc -o mkregtable $r/mkregtable.c
            for t in r100 r200 r300 r420 rn50 rs600 rv515 r600 evergreen cayman; do
              ./mkregtable $r/reg_srcs/$t > $r/''${t}_reg_safe.h
            done
            rm mkregtable
            tar -xJf ${linux614Src} --strip-components=1 -C $out \
              linux-6.14/drivers/gpu/drm/nouveau \
              linux-6.14/include/drm/i2c/ch7006.h linux-6.14/include/drm/i2c/sil164.h
            patch -p1 < ${./src/Kernel/fbsdkpi/drm/linux-nouveau-pd.patch}
            cp -r ${./src/Kernel/fbsdkpi/drm/nouveau}/. drivers/gpu/drm/nouveau/
            chmod -R u+w drivers/gpu/drm/nouveau
          '';
          # a drm kext's own slice of the tree, so edits to other kexts leave it alone
          drmKextSource = kexts:
            let
              fs = lib.fileset;
              headers = dir: fs.fileFilter (f: f.hasExt "h" || f.hasExt "hpp") (./. + "/${dir}");
            in
            fs.toSource {
              root = ./.;
              fileset = fs.unions ([
                kextsOnlyFiles
                ./src/Kernel/fbsdkpi
                ./src/Kernel/Extensions/CMakeLists.txt
                (headers "src/Kernel/Extensions/IOACPIFamily/include")
                (headers "src/Kernel/Extensions/IOGraphicsFamily")
                (headers "src/Kernel/Extensions/IOHIDFamily/IOHIDSystem")
                (headers "src/Kernel/Extensions/IOPCIFamily/include")
                # fbsdkpi links its IOKit/network shim to these
                (headers "src/Kernel/Extensions/IONetworkingFamily")
              ] ++ map (k: ./. + "/src/Kernel/Extensions/${k}") kexts);
            };
          # the root project adds every kext, so skip the directories this slice leaves out
          drmKextPostPatch = ''
            cat - src/Kernel/Extensions/CMakeLists.txt > CMakeLists.drm <<'EOF'
            macro(add_subdirectory _pd_dir)
                set(_pd_subdir "''${_pd_dir}")
                if(NOT IS_ABSOLUTE "''${_pd_subdir}")
                    set(_pd_subdir "''${CMAKE_CURRENT_SOURCE_DIR}/''${_pd_subdir}")
                endif()
                if(EXISTS "''${_pd_subdir}/CMakeLists.txt")
                    _add_subdirectory(''${ARGV})
                endif()
            endmacro()
            # kexts outside the slice are already defined or absent, so drop the absent ones
            macro(add_dependencies _pd_target)
                if(NOT "''${_pd_target}" STREQUAL "kexts")
                    set(_pd_deps)
                    foreach(_pd_dep ''${ARGN})
                        if(TARGET "''${_pd_dep}" OR NOT _pd_dep MATCHES "[.]kext$")
                            list(APPEND _pd_deps "''${_pd_dep}")
                        endif()
                    endforeach()
                    if(_pd_deps)
                        _add_dependencies("''${_pd_target}" ''${_pd_deps})
                    endif()
                endif()
            endmacro()
            EOF
            mv CMakeLists.drm src/Kernel/Extensions/CMakeLists.txt
          '';
          mkDrmKext = kexts: args: (mkPureDarwinBuild ({ src = drmKextSource kexts; } // args // {
            extraCmakeFlags = (kextsOnlyArgs (if (args.puredarwinArch or "x86_64") == "arm64"
              then xnuHeadersArm64Build else xnuHeadersBuild)).extraCmakeFlags ++ (args.extraCmakeFlags or [ ]);
            enableTools = false;
          })).overrideAttrs (old: {
            postPatch = (old.postPatch or "") + drmKextPostPatch;
          });
          # nvidia graphics from nv50 on, opt-in with the pdnouveau=1 boot-arg
          drmNouveauBuild = mkDrmKext [ "PDDRMnouveau" ] {
            pname = "puredarwin-drm-nouveau";
            buildTargets = [ "PDDRMnouveau.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "PDDRMnouveau.kext" ];
            extraCmakeFlags = [ "-DPD_DRMKMOD_DIR=${drmKmodAllSrc}" ];
            ninjaFlags = [ "-k" "0" ];
          };
          # nvidia-drm from NVIDIA's 580 package over PDNVLoader's modeset core, for the cards the blob drives
          drmNvidiaBuild = mkDrmKext [ "PDDRMnvidia" ] {
            pname = "puredarwin-drm-nvidia";
            buildTargets = [ "PDDRMnvidia.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "PDDRMnvidia.kext" ];
            extraCmakeFlags = [ "-DPD_DRMKMOD_DIR=${drmKmodAllSrc}" "-DPD_NVIDIA_DRM_DIR=${pd.x86_64.nvidiaDrmSrc}" ];
            ninjaFlags = [ "-k" "0" ];
          };
          # nvidia-uvm from NVIDIA's 580 package (its MIT files) over linuxkpi and PDNVLoader, what CUDA opens
          nvUvmBuild = mkDrmKext [ "PDNVUVM" "PDDRMnvidia" "PDNVLoader" "IOPCIFamily" ] {
            pname = "puredarwin-nvidia-uvm";
            # PDNVLoader too: PDNVUVM links against it, so its side of the link is checked with it
            buildTargets = [ "PDNVUVM.kext" "PDNVLoader.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "PDNVUVM.kext" ];
            extraCmakeFlags = [ "-DPD_DRMKMOD_DIR=${drmKmodAllSrc}" "-DPD_NVIDIA_UVM_DIR=${pd.x86_64.nvidiaUvmSrc}" ];
            ninjaFlags = [ "-k" "0" ];
          };
          # gsp firmware for turing (tu1xx), ampere ga10x (ga107 is the rtx 2050) and ada ad10x, only
          # the 535.113.01 release linux 6.14's nouveau asks for. fermi needs none.
          nouveauFirmware = pkgs.runCommand "puredarwin-nouveau-firmware" { } ''
            fw=$out/Library/Firmware/nvidia
            mkdir -p $fw
            src=${pkgs.linux-firmware}/lib/firmware/nvidia
            for chip in tu102 tu104 tu106 tu10x tu116 tu117 ga102 ga103 ga104 ga106 ga107 \
              ad102 ad103 ad104 ad106 ad107; do
              [ -e "$src/$chip" ] || continue
              # resolved, turing's gr/fecs_bl.bin links into gm200, which isn't shipped
              cp -rL "$src/$chip" "$fw/"
            done
            # gm20b (tegra x1) loads acr, pmu and gr images, some of them links to other chips
            cp -rL "$src/gm20b" "$fw/"
            # pascal's signed acr, gr, sec2 and nvdec images link into gp102 and gm200, so copy them resolved
            for chip in gp100 gp102 gp104 gp106 gp107 gp108; do
              [ -e "$src/$chip" ] || continue
              cp -rL "$src/$chip" "$fw/"
            done
            chmod -R u+w $fw
            find $fw -name '*-570.*' -delete
            find $fw -name '*.zst' -exec ${pkgs.zstd}/bin/zstd -dq --rm {} \;
            find $fw -name '*.xz' -exec ${pkgs.xz}/bin/xz -dq {} \;
            cp ${pkgs.linux-firmware.src}/LICENCE.nvidia $fw/LICENCE.nvidia 2>/dev/null || \
              cp ${pkgs.linux-firmware.src}/LICENSES/LICENCE.nvidia $fw/LICENCE.nvidia
          '';
          # board device trees, built out of tree from the linux 6.14 tarball
          dtbTegra210P2371_2180 = pd.x86_64.dtbTegra210P2371_2180;
          # the gm20b slice of it for the jetson tx1's ramdisk, which has no room for the gsp images
          nouveauFirmwareGm20b = pkgs.runCommand "puredarwin-nouveau-firmware-gm20b" { } ''
            mkdir -p $out/Library/Firmware/nvidia
            cp -r ${nouveauFirmware}/Library/Firmware/nvidia/gm20b $out/Library/Firmware/nvidia/
            cp ${nouveauFirmware}/Library/Firmware/nvidia/LICENCE.nvidia $out/Library/Firmware/nvidia/
          '';
          # the tegra210 xusb falcon image PDTegraXUSB loads, under the same nvidia licence as gm20b
          tegra210XusbFirmware = pkgs.runCommand "puredarwin-tegra210-xusb-firmware" { } ''
            mkdir -p $out/Library/Firmware/nvidia/tegra210
            cp ${pkgs.linux-firmware.src}/nvidia/tegra210/xusb.bin $out/Library/Firmware/nvidia/tegra210/
            cp ${pkgs.linux-firmware.src}/LICENCE.nvidia $out/Library/Firmware/nvidia/LICENCE.nvidia 2>/dev/null || \
              cp ${pkgs.linux-firmware.src}/LICENSES/LICENCE.nvidia $out/Library/Firmware/nvidia/LICENCE.nvidia
          '';
          # nouveau on arm64 for tegra210's gm20b, a device tree platform device, opt-in with pdnouveau=1
          drmNouveauArm64Build = mkDrmKext [ "PDDRMnouveau" "PDDRMDri" ] {
            pname = "puredarwin-drm-nouveau-arm64";
            buildTargets = [ "PDDRMnouveau.kext" "PDDRMDri.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "PDDRMnouveau.kext" "PDDRMDri.kext" ];
            enableIOGraphicsFamily = true;
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
            extraCmakeFlags = [ "-DPD_DRMKMOD_DIR=${drmKmodNouveauSrc}" ];
            ninjaFlags = [ "-k" "0" ];
          };
          # i915 for gen7, gen9 and gen12+ intel graphics, attaches unless pdi915=0
          drmI915Build = mkPureDarwinBuild {
            pname = "puredarwin-drm-i915";
            src = kextsSource;
            buildTargets = [ "PDDRMi915.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "PDDRMi915.kext" ];
            extraCmakeFlags = [ "-DPD_DRMKMOD_DIR=${drmKmodAllSrc}" ];
            ninjaFlags = [ "-k" "0" ];
          };
          # the amdgpu kext, built apart from the boot kexts and loaded from the
          # auxiliary kernel collection
          drmAmdgpuBuild = mkDrmKext [ "PDDRMamdgpu" ] {
            pname = "puredarwin-drm-amdgpu";
            buildTargets = [ "PDDRMamdgpu.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "PDDRMamdgpu.kext" ];
            extraCmakeFlags = [ "-DPD_DRMKMOD_DIR=${drmKmodAllSrc}" ];
            # report every compile error of a bring-up build at once
            ninjaFlags = [ "-k" "0" ];
          };
          # radeonkms for evergreen and northern islands (terascale 2), loaded from the aux KC
          drmRadeonBuild = mkDrmKext [ "PDDRMradeon" ] {
            pname = "puredarwin-drm-radeon";
            buildTargets = [ "PDDRMradeon.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "PDDRMradeon.kext" ];
            extraCmakeFlags = [ "-DPD_DRMKMOD_DIR=${drmKmodAllSrc}" ];
            ninjaFlags = [ "-k" "0" ];
          };
          # the adreno drm driver for arm64, the fbsdkpi layer's first arm64 user
          drmMsmArm64Build = mkDrmKext [ "PDDRMmsm" "PDDRMDri" ] {
            pname = "puredarwin-drm-msm-arm64";
            buildTargets = [ "PDDRMmsm.kext" "PDDRMDri.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "PDDRMmsm.kext" "PDDRMDri.kext" ];
            enableIOGraphicsFamily = true;
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
            extraCmakeFlags = [ "-DPD_DRMKMOD_DIR=${drmKmodSrc}" ];
            ninjaFlags = [ "-k" "0" ];
          };
          iographicsBuild = mkPureDarwinBuild {
            pname = "puredarwin-iographics";
            src = kextsSource;
            buildTargets = [ "IOGraphicsFamily.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "IOGraphicsFamily.kext" ];
            enableIOGraphicsFamily = true;
          };
          # riscv64 kernel, kexts and kernel collection (see nix/riscv64.nix)
          riscv64 = import ./nix/riscv64.nix {
            inherit lib pkgs mkPureDarwinBuild kernelSource kextsSource;
            inherit riscv64CrossToolchain compilerRtRiscv64Kernel compilerRtRiscv64Build libSystemBuild;
            inherit icuCoreBuild libcxxabiDylibBuild libcxxDylibBuild libobjcBuild;
            inherit nativeLd foundationSource coreFoundationBuild iokitCFStaticBuild iokitBuild;
            inherit launchdBuild launchctlBuild userlandBuild libiconvBuild ncursesBuild zshBuild;
            xnuLoaderRiscv64 = xnu-loader.packages.${system}.kernel-riscv64 or null;
            kcTools = kc-tools.packages.${system}.default;
          };
          # Image contents (see nix/image-contents.nix).
          imageContents = import ./nix/image-contents.nix {
            pd = pd.x86_64;
            inherit
              kcGpuBuild gpuDefaultPackages;
            tegra210Dtb = dtbTegra210P2371_2180;
            mesaDefaultBuild = mesaGpuBuild;
            inherit
              utilityAppBuilds nixPortPackages darwinCrossToolchain exoBuild llvmCrossBuild mesaVenusWaylandBuild fbdoomExternalSrc garconBuild imageExtraPackagesArm64 imageExtraPackagesArm64Nox imageExtraPackagesArm64Sg2002 splitBaseSystemArm64Sg2002 iographicsBuild asmjitTestArm64Build isDarwin kc-tools kernelArm64Build kernelArm64VirtBuild kernelArm64VirtDebugBuild kernelArm64VirtAmpBuild kernelArm64T8010Build kernelArm64T8010DebugBuild kernelArm64S8000Build kernelArm64S8000DebugBuild kernelArm64T8103Build kernelArm64T8103DebugBuild kernelArm64Bcm2837Build kernelArm64Bcm2837DebugBuild kernelArm64Sun50iBuild kernelArm64Sun50iDebugBuild kernelArm64Sc8280xpBuild kernelArm64Sc8280xpDebugBuild kernelArm64Sdm670Build kernelArm64Virt4kDebugBuild kernelArm32Bcm2835Build kernelArm32Bcm2835DebugBuild kernelArm32Bcm2835DevBuild kextsArm32Bcm2835Build compilerRtArmv6Build kernelBuild kernelDebugBuild kernelSource kextsArm64Build drmMsmArm64Build kextsBuild drmNouveauArm64Build nouveauFirmwareGm20b tegra210XusbFirmware tx1GlRootArm64Bundle tx1DesktopArm64Bundle pdadbdArm64Build lib libiconvArm64Build libxfce4uiBuild libxfce4utilBuild libxfce4windowingBuild migcomDarwinBuild mkPureDarwinBuild clangCrossBuild dsymutilBuild kcToolsGuestBuild nativeLd pkgs splitBaseSystemArm64VirtMinimal splitBaseSystemArm64VirtMinimalRelease splitBaseSystemArm64VirtWayland system fastfetchNoGLArm64Build foundationArm64Build vmprobeArm64Build toyboxArm64Build thunarBuild xfce4AppfinderBuild xfce4PanelBuild xfce4SessionBuild xfce4SettingsBuild xfce4TerminalBuild xfconfBuild xfdesktopBuild xfwm4Build xnu-loader xnuHeadersBuild xnuHeadersArm64Build xxdBuild zshArm64Build;
          };
          inherit (imageContents)
            fullBuild
            splitBaseSystem
            splitBaseSystemStripped
            splitBaseSystemMinimal
            imageExtraPackageSet
            commonPackages
            linuxPackages
            linuxApps
            ;
          # Investigative builds, not image contents.
          probePackages = {
            # Cross toolchain, exposed so out-of-tree flakes (e.g. checkm8-tools'
            # PongoOS build) can link Mach-O with the real cctools ld64.
            arm64-cross-toolchain = arm64CrossToolchain;
            # riscv mach-o: clang with the mach-o writer, and a bare-metal image linked by ld64
            llvm-riscv-macho = llvmRiscvMachO.clang-unwrapped;
            riscv64-cross-toolchain = riscv64CrossToolchain;
            compiler-rt-riscv64-kernel = compilerRtRiscv64Kernel;
            compiler-rt-riscv64 = compilerRtRiscv64Build;
            kernel-riscv64-virt-debug = riscv64.kernelRiscv64VirtDebugBuild;
            kexts-riscv64 = riscv64.kextsRiscv64Build;
            kc-riscv64-virt-debug = riscv64.kcRiscv64VirtDebugBuild;
            libsystem-riscv64 = riscv64.libSystemRiscv64Build;
            icucore-riscv64 = riscv64.icuCoreRiscv64Build;
            libcxxabi-dylib-riscv64 = riscv64.libcxxabiDylibRiscv64Build;
            libcxx-dylib-riscv64 = riscv64.libcxxDylibRiscv64Build;
            libobjc-riscv64 = riscv64.libobjcRiscv64Build;
            corefoundation-riscv64 = riscv64.coreFoundationRiscv64Build;
            foundation-riscv64 = riscv64.foundationRiscv64Build;
            iokit-riscv64 = riscv64.iokitRiscv64Build;
            launchd-riscv64 = riscv64.launchdRiscv64Build;
            launchctl-riscv64 = riscv64.launchctlRiscv64Build;
            userland-riscv64 = riscv64.userlandRiscv64Build;
            zsh-riscv64 = riscv64.zshRiscv64Build;
            toybox-riscv64 = riscv64.toyboxRiscv64Build;
            fastfetch-riscv64 = riscv64.fastfetchRiscv64Build;
            basesystem-riscv64-virt-minimal = riscv64.splitBaseSystemRiscv64VirtMinimal;
            image-riscv64-virt-minimal = riscv64.imageRiscv64VirtMinimalBuild;
            initrd-riscv64-virt = riscv64.initrdRiscv64VirtBuild;
            riscv64-virt-runner = riscv64.runRiscv64Virt;
            riscv64-macho-hello = pkgs.callPackage ./nix/pkgs/toolchain/riscv64-macho-hello.nix {
              inherit llvmRiscvMachO nativeLd;
            };
            riscv64-macho-ctz-test = pkgs.callPackage ./nix/pkgs/toolchain/riscv64-macho-ctz-test.nix {
              inherit llvmRiscvMachO nativeLd;
            };
            riscv64-macho-userland-test = pkgs.callPackage ./nix/pkgs/toolchain/riscv64-macho-userland-test.nix {
              inherit llvmRiscvMachO nativeLd;
            };
            riscv64-macho-pcrel-test = pkgs.callPackage ./nix/pkgs/toolchain/riscv64-macho-pcrel-test.nix {
              inherit llvmRiscvMachO nativeLd;
            };
            riscv64-macho-libcalls-test = pkgs.callPackage ./nix/pkgs/toolchain/riscv64-macho-libcalls-test.nix {
              inherit llvmRiscvMachO nativeLd;
              libSystem = riscv64.libSystemRiscv64Build;
            };
            darwin-cross-toolchain = darwinCrossToolchain;
            native-ld = nativeLd;
            libsystem = libSystemBuild;
            libcxxabi-dylib = libcxxabiDylibBuild;
            libcxx-dylib = libcxxDylibBuild;
            libobjc = libobjcBuild;
            icucore = icuCoreBuild;
            coreservices = coreServicesBuild;
            wine-tools = wineToolsBuild;
            wine-macos = wineBuild.override { stockMac = true; };
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
            libXxf86vm-shared = libXxf86vmSharedBuild;
            nettle-shared = nettleSharedBuild;
            gnutls-shared = gnutlsSharedBuild;
            libxshmfence-shared = libxshmfenceSharedBuild;
            xrandr = xrandrBuild;
            vulkan-loader = vulkanLoaderBuild;
            vulkan-tools = vulkanToolsBuild;
            llvm-cross = llvmCrossBuild;
            compiler-rt = compilerRtBuild;
            debugserver = pkgs.callPackage ./nix/pkgs/apps/debugserver.nix {
              inherit darwinCrossToolchain nativeLd nativeMigcom;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              corefoundation = coreFoundationBuild;
              clang = pkgs.llvmPackages_21.clang-unwrapped;
              llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
              llvmVersion = pkgs.llvmPackages_21.llvm.version;
            };
            mingw-aarch64-cc = mingwAarch64.cc;
            mingw-arm64ec-cc = mingwAarch64.arm64ecCc;
            fex-wow64 = fexWow64Build;
            mingw-aarch64-crt = mingwAarch64.mingw;
            asmjit-test = asmjitTestBuild;
            elftrans = elftransBuild;
            elftrans-stage = elftransStageBuild;
            elfaot = elfaotBuild;
            elfaot-darwin = elfaotDarwinBuild;
            clang = clangCrossBuild;
            kc-tools-guest = kcToolsGuestBuild;
            libgpg-error = libgpgErrorBuild;
            libgcrypt = libgcryptBuild;
            libjpeg = libjpegBuild;
            libwebp = libwebpBuild;
            nghttp2 = nghttp2Build;
            libpsl = libpslBuild;
            libsoup = libsoupBuild;
            webkitgtk = webkitgtkBuild;
            gettext = gettextBuild;
            libtasn1 = libtasn1Build;
            sqlite = sqliteBuild;
            glib-networking = glibNetworkingBuild;
            freetype-shared = freetype2Build;
            neuwld = neuwldBuild;
            neuswc = neuswcBuild;
            quartzcore = quartzcoreBuild;
            corevideo = corevideoBuild;
            applicationservices = applicationservicesBuild;
            windowserver = windowserverBuild;
            appkit = appkitBuild;
            terminal = terminalBuild;
            menubar = menubarBuild;
            dock = dockBuild;
            finder = finderBuild;
            system-information = systemInformationBuild;
            calculator = calculatorBuild;
            text-editor = textEditorBuild;
            activity = activityBuild;
            logs = logsBuild;
            image-viewer = imageViewerBuild;
            screenshot = screenshotBuild;
            disks = disksBuild;
            installer = installerBuild;
            pddecor = pddecorBuild;
            settings = settingsBuild;
            loginwindow = loginwindowBuild;
            argon-session = waylandSessionBuild;
          };
          arm64Packages = {
            libSystem-armv6 = arm64.libSystemArmv6Build;
            libsystem-arm64 = arm64.libSystemArm64Build;
            userland-arm32-bcm2835 = arm64.userlandArm32Bcm2835Build;
            libsystem-armv6 = arm64.libSystemArmv6Build;
            libobjc-armv6 = arm64.libobjcArmv6Build;
            libcxxabi-armv6 = arm64.libcxxabiDylibArmv6Build;
            libcxx-armv6 = arm64.libcxxDylibArmv6Build;
            icu-armv6 = arm64.icuCoreArmv6Build;
            corefoundation-armv6 = arm64.coreFoundationArmv6Build;
            iokit-armv6 = arm64.iokitArmv6Build;
            launchd-armv6 = arm64.launchdArmv6Build;
            compiler-rt-armv6 = arm64.compilerRtArmv6Build;
            kernel-arm32-bcm2835 = arm64.kernelArm32Bcm2835Build;
            kernel-arm32-bcm2835-debug = arm64.kernelArm32Bcm2835DebugBuild;
            kernel-arm32-bcm2835-dev = arm64.kernelArm32Bcm2835DevBuild;
            kexts-arm32-bcm2835 = arm64.kextsArm32Bcm2835Build;
            zlib-arm64 = xvfbZlibArm64Build;
            toybox-arm64 = toyboxArm64Build;
            pdadbd-arm64 = pdadbdArm64Build;
            xz-arm64 = xzArm64Build;
            file-arm64 = fileArm64Build;
            openssl-arm64 = opensslArm64Build;
            sqlite-arm64 = sqliteArm64Build;
            libjpeg-arm64 = libjpegArm64Build;
            libwebp-arm64 = libwebpArm64Build;
            libgpg-error-arm64 = libgpgErrorArm64Build;
            libgcrypt-arm64 = libgcryptArm64Build;
            libtasn1-arm64 = libtasn1Arm64Build;
            nghttp2-arm64 = nghttp2Arm64Build;
            libpsl-arm64 = libpslArm64Build;
            gettext-arm64 = gettextArm64Build;
            libxshmfence-arm64 = libxshmfenceSharedArm64Build;
            nettle-arm64 = nettleSharedArm64Build;
            libXinerama-arm64 = xvfbLibXineramaArm64Build;
            libXres-arm64 = xvfbLibXresArm64Build;
            libXcomposite-arm64 = xvfbLibXcompositeArm64Build;
            libXdamage-arm64 = xvfbLibXdamageArm64Build;
            libXpresent-arm64 = xvfbLibXpresentArm64Build;
            iceauth-arm64 = iceauthArm64Build;
            xrandr-arm64 = xrandrArm64Build;
            xrdb-arm64 = xrdbArm64Build;
            xinit-arm64 = xinitArm64Build;
            coreServices-arm64 = coreServicesArm64Build;
            iomediacheck-arm64 = iomediacheckArm64Build;
            pdsurface-arm64 = pdsurfaceArm64Build;
            diskArbitration-arm64 = diskArbitrationArm64Build;
            libcroco-arm64 = libcrocoArm64Build;
            librsvg-arm64 = librsvgArm64Build;
            gnutls-arm64 = gnutlsSharedArm64Build;
            libsoup-arm64 = libsoupArm64Build;
            vte-arm64 = vteArm64Build;
            libwnck-arm64 = libwnckArm64Build;
            llvm-arm64 = llvmCrossArm64Build;
            airjitd = airjitdArm64Build;
            air2spv = pkgs.callPackage ./nix/pkgs/gpu/air2spv.nix { };
            metal-window = pkgs.callPackage ./nix/pkgs/gpu/metal-window.nix { };
            indium-pd = pkgs.callPackage ./nix/pkgs/gpu/indium-pd.nix { };
            compiler-rt-arm64 = compilerRtArm64Build;
            wine-arm64 = wineArm64Build;
            asmjit-test-arm64 = asmjitTestArm64Build;
            gtkLayerShell-arm64 = gtkLayerShellArm64Build;
            libgbm-arm64 = libgbmArm64Build;
            jsonc-arm64 = jsoncArm64Build;
            meson-arm64 = mesonArm64Build;
            cmake-arm64 = cmakeArm64Build;
            ninja-arm64 = ninjaArm64Build;
            clang-arm64 = clangCrossArm64Build;
            wlroots-arm64 = wlrootsArm64Build;
            sway-arm64 = swayArm64Build;
            libdrm-arm64 = libdrmArm64Build;
            libdrm-dri-arm64 = libdrmDriArm64Build;
            libdrm-pdpvr-arm64 = libdrmPdpvrArm64Build;
            libdrm-virtgpu = libdrmVirtgpuBuild;
            mesa-venus = mesaVenusBuild;
            venus-tests = venusTestsBuild;
            mesa-venus-wsi = mesaVenusWsiBuild;
            mesa-venus-wayland = mesaVenusWaylandBuild;
            glfw-pd = glfwPdBuild;
            metal-pd = metalPdBuild;
            metal-objc = metalObjcPdBuild;
            # the Wayland images' Metal.framework, so it can be built and injected on its own
            metal-objc-nox = metalObjcPdNoxBuild;
            mslc-pd = mslcPdBuild;
            mslc = mslcHostBuild;
            uigallery = uigalleryBuild;
            opengl-framework-nox = openglFrameworkNoxBuild;
            mps = mpsBuild;
            metal-apple-samples = metalAppleSamplesBuild;
            metal-apple-samples-nox = metalAppleSamplesNoxBuild;
            framework-compat = frameworkCompatBuild;
            cocoa = cocoaBuild;
            app-frameworks = appFrameworksBuild;
            certificates-bundle = certificatesBundleBuild;
            frameworks-ff-tests = frameworksFfTestsBuild;
            cg-ff-tests = cgFfTestsBuild;
            audiotoolbox = audiotoolboxBuild;
            pcmplay = pcmplayBuild;
            pd-metal-surface = pdMetalSurfaceBuild;
            cabextract = cabextractBuild;
            winetricks = winetricksBuild;
            dxvk = dxvkBuild;
            coreaudiod = coreaudiodBuild;
            carbon = carbonBuild;
            akff-test = akffTestBuild;
            argon-gfx-test = argonGfxTestBuild;
            iosurface = iosurfaceBuild;
            iosurface-tests = iosurfaceTestsBuild;
            mesa-pvr-native-tools = mesaPvrNativeToolsBuild;
            mesa-powervr-arm64 = mesaPowervrArm64Build;
            mesa-msm-arm64 = mesaMsmArm64Build;
            neuswc-msm-arm64 = neuswcMsmArm64Build;
            argon-msm-arm64 = argonMsmArm64Bundle;
            foot-arm64 = footArm64Build;
            onyx2d-arm64 = onyx2dArm64Build;
            coregraphics-arm64 = coregraphicsArm64Build;
            coretext-arm64 = coretextArm64Build;
            quartzcore-msm-arm64 = quartzcoreMsmArm64Build;
            windowserver-arm64 = windowserverArm64Build;
            appkit-msm-arm64 = appkitMsmArm64Build;
            cocoa-msm-arm64 = cocoaMsmArm64Build;
            terminal-msm-arm64 = terminalMsmArm64Build;
            dock-msm-arm64 = dockMsmArm64Build;
            menubar-msm-arm64 = menubarMsmArm64Build;
            pddecor-msm-arm64 = pddecorMsmArm64Build;
            finder-msm-arm64 = finderMsmArm64Build;
            calculator-msm-arm64 = calculatorMsmArm64Build;
            argon-desktop-msm-arm64 = argonDesktopMsmArm64Bundle;
            pdgltri-msm-arm64 = pdgltriMsmArm64Build;
            mesa-nouveau-arm64 = mesaNouveauArm64Build;
            pdgltri-nouveau-arm64 = pdgltriNouveauArm64Build;
            vulkan-loader-arm64 = vulkanLoaderArm64Build;
            pdvktri-arm64 = pdvktriArm64Build;
            pdvkspin-arm64 = pdvkspinArm64Build;
            pvr-vulkan-arm64 = pvrVulkanArm64;
            pvr-vulkan-ipd-arm64 = pvrVulkanIpdArm64;
            libdrm-virtgpu-arm64 = libdrmVirtgpuArm64Build;
            mesa-venus-arm64 = mesaVenusArm64Build;
            venus-vulkan-ipd-arm64 = venusVulkanIpdArm64;
            mesa-powervr-x11-arm64 = mesaPowervrX11Arm64Build;
            vulkan-loader-x11-arm64 = vulkanLoaderX11Arm64Build;
            vulkan-tools-arm64 = vulkanToolsArm64Build;
            x11-pvr-arm64 = x11PvrArm64;
            xorg-noglx-arm64 = xorgNoGlxArm64Build;
            puredarwingop-arm64 = puredarwingopArm64Build;
            xwayland-arm64 = xwaylandArm64Build;
            libdisplayInfo-arm64 = libdisplayInfoArm64Build;
            libXxf86vm-arm64 = xvfbLibXxf86vmArm64Build;
            wayland-arm64 = waylandArm64Build;
            xfconf-arm64 = xfconfArm64;
            libxfce4util-arm64 = libxfce4utilArm64;
            libxfce4ui-arm64 = libxfce4uiArm64;
            xfwm4-arm64 = xfwm4Arm64;
            libxfce4windowing-arm64 = libxfce4windowingArm64;
            garcon-arm64 = garconArm64;
            exo-arm64 = exoArm64;
            xfce4Session-arm64 = xfce4SessionArm64;
            xfce4Panel-arm64 = xfce4PanelArm64;
            xfdesktop-arm64 = xfdesktopArm64;
            xfce4Terminal-arm64 = xfce4TerminalArm64;
            xfce4Settings-arm64 = xfce4SettingsArm64;
            xfce4Appfinder-arm64 = xfce4AppfinderArm64;
            thunar-arm64 = thunarArm64;
            ncurses-arm64 = ncursesArm64Build;
            libiconv-arm64 = libiconvArm64Build;
            zsh-arm64 = zshArm64Build;
            bmake-arm64 = bmakeArm64Build;
            gnumake-arm64 = gnumakeArm64Build;
            gnum4-arm64 = gnum4Arm64Build;
            pkgconf-arm64 = pkgconfArm64Build;
            bison-arm64 = bisonArm64Build;
            flex-arm64 = flexArm64Build;
            xxd-arm64 = xxdArm64Build;
            nano-arm64 = nanoArm64Build;
            libffi-arm64 = libffiArm64Build;
            expat-arm64 = expatArm64Build;
            pcre2-arm64 = pcre2Arm64Build;
            libev-arm64 = libevArm64Build;
            libpng-arm64 = libpngArm64Build;
            freetype2-arm64 = freetype2Arm64Build;
            fontconfig-arm64 = fontconfigArm64Build;
            libxml2-arm64 = libxml2Arm64Build;
            yajl-arm64 = yajlArm64Build;
            glib-arm64 = glibArm64Build;
            userland = userlandBuild;
            userland-arm64 = userlandArm64Build;
            coreFoundation-arm64 = coreFoundationArm64Build;
            i3statusShim-arm64 = i3statusShimArm64Build;
            icuCore-arm64 = icuCoreArm64Build;
            iokit = iokitBuild;
            iokit-arm64 = iokitArm64Build;
            libcxxDylib-arm64 = libcxxDylibArm64Build;
            libcxxabiDylib-arm64 = libcxxabiDylibArm64Build;
            libobjc-arm64 = libobjcArm64Build;
            pdVirglShim-arm64 = pdVirglShimArm64Build;
            xkeyboardConfig-arm64 = xkeyboardConfigArm64Build;
            xlibLocale-arm64 = xlibLocaleArm64Build;
            xvfbFonts-arm64 = xvfbFontsArm64Build;
            atspi2Core-arm64 = atspi2CoreArm64Build;
            autoconf-arm64 = autoconfArm64Build;
            automake-arm64 = automakeArm64Build;
            cairo-arm64 = cairoArm64Build;
            cairoGobject-arm64 = cairoGobjectArm64Build;
            curl-arm64 = curlArm64Build;
            dbus-arm64 = dbusArm64Build;
            dillo-arm64 = dilloArm64Build;
            dmenu-arm64 = dmenuArm64Build;
            fastfetch-arm64 = fastfetchArm64Build;
            fastfetch-arm64-nogl = fastfetchNoGLArm64Build;
            vmprobe-arm64 = vmprobeArm64Build;
            fltk-arm64 = fltkArm64Build;
            foundation-arm64 = foundationArm64Build;
            swift-runtime = swiftRuntimeBuild;
            swift-toolchain = swiftToolchain;
            swift-runtime-arm64 = swiftRuntimeArm64Build;
            framework-compat-arm64 = frameworkCompatArm64Build;
            fribidi-arm64 = fribidiArm64Build;
            gdkPixbuf-arm64 = gdkPixbufArm64Build;
            git-arm64 = gitArm64Build;
            gtk3-arm64 = gtk3Arm64Build;
            gtk3-nox = gtk3NoxBuild;
            shared-mime-info = sharedMimeInfoBuild;
            minibrowser-app = minibrowserAppBuild;
            harfbuzz-arm64 = harfbuzzArm64Build;
            i3-arm64 = i3Arm64Build;
            ioreg-arm64 = ioregArm64Build;
            libXft-arm64 = libXftArm64Build;
            libcss-arm64 = libcssArm64Build;
            libcurlDylib-arm64 = libcurlDylibArm64Build;
            libcxxTest-arm64 = libcxxTestArm64Build;
            libdom-arm64 = libdomArm64Build;
            libepoxy-arm64 = libepoxyArm64Build;
            libfontenc-arm64 = libfontencArm64Build;
            libhubbub-arm64 = libhubbubArm64Build;
            libnsbmp-arm64 = libnsbmpArm64Build;
            libnsgif-arm64 = libnsgifArm64Build;
            libnsutils-arm64 = libnsutilsArm64Build;
            libparserutils-arm64 = libparserutilsArm64Build;
            libutf8proc-arm64 = libutf8procArm64Build;
            libwapcaplet-arm64 = libwapcapletArm64Build;
            libzDylib-arm64 = libzDylibArm64Build;
            mesa-arm64 = mesaArm64Build;
            mesaDemos-arm64 = mesaDemosArm64Build;
            migcomDarwin-arm64 = migcomDarwinArm64Build;
            netsurf-arm64 = netsurfArm64Build;
            openglFramework-arm64 = openglFrameworkArm64Build;
            openssh-arm64 = opensshArm64Build;
            pango-arm64 = pangoArm64Build;
            python-arm64 = pythonArm64Build;
            security-arm64 = securityArm64Build;
            systemConfiguration-arm64 = systemConfigurationArm64Build;
            diskArbitration = diskArbitrationBuild;
            symptomReporter = symptomReporterBuild;
            protocolBuffer = protocolBufferBuild;
            wirelessDiagnostics = wirelessDiagnosticsBuild;
            iokitcf-static = iokitCFStaticBuild;
            iomediacheck = iomediacheckBuild;
            startupNotification-arm64 = startupNotificationArm64Build;
            xcalc-arm64 = xcalcArm64Build;
            xcb-arm64 = xcbArm64Build;
            xcbCursor-arm64 = xcbCursorArm64Build;
            xcbImage-arm64 = xcbImageArm64Build;
            xcbKeysyms-arm64 = xcbKeysymsArm64Build;
            xcbRenderUtil-arm64 = xcbRenderUtilArm64Build;
            xcbUtil-arm64 = xcbUtilArm64Build;
            xcbWm-arm64 = xcbWmArm64Build;
            xcbXrm-arm64 = xcbXrmArm64Build;
            xclock-arm64 = xclockArm64Build;
            xeyes-arm64 = xeyesArm64Build;
            xkbcommon-arm64 = xkbcommonArm64Build;
            xkbcomp-arm64 = xkbcompArm64Build;
            xlib-arm64 = xlibArm64Build;
            xmessage-arm64 = xmessageArm64Build;
            xorg-arm64 = xorgArm64Build;
            xterm-arm64 = xtermArm64Build;
            xvfb-arm64 = xvfbArm64Build;
            xvfbLibICE-arm64 = xvfbLibICEArm64Build;
            xvfbLibSM-arm64 = xvfbLibSMArm64Build;
            xvfbLibXau-arm64 = xvfbLibXauArm64Build;
            xvfbLibXaw-arm64 = xvfbLibXawArm64Build;
            xvfbLibXcursor-arm64 = xvfbLibXcursorArm64Build;
            xvfbLibXdmcp-arm64 = xvfbLibXdmcpArm64Build;
            xvfbLibXext-arm64 = xvfbLibXextArm64Build;
            xvfbLibXfixes-arm64 = xvfbLibXfixesArm64Build;
            xvfbLibXfont2-arm64 = xvfbLibXfont2Arm64Build;
            xvfbLibXi-arm64 = xvfbLibXiArm64Build;
            xvfbLibXkbfile-arm64 = xvfbLibXkbfileArm64Build;
            xvfbLibXmu-arm64 = xvfbLibXmuArm64Build;
            xvfbLibXpm-arm64 = xvfbLibXpmArm64Build;
            xvfbLibXrandr-arm64 = xvfbLibXrandrArm64Build;
            xvfbLibXrender-arm64 = xvfbLibXrenderArm64Build;
            xvfbLibXt-arm64 = xvfbLibXtArm64Build;
            xvfbLibxcvt-arm64 = xvfbLibxcvtArm64Build;
            xvfbPixman-arm64 = xvfbPixmanArm64Build;
          };

          devShell = pkgs.mkShell ({
            packages = [
              iig
              pkgs.cmake
              pkgs.ninja
              pkgs.bison
              pkgs.flex
              pkgs.perl
              pkgs.bash
              pkgs.ed
              pkgs.unifdef
              pkgs.tcsh
              pkgs.pax
              pkgs.coreutils
              pkgs.findutils
              pkgs.gawk
              pkgs.gnused
              pkgs.clang
              pkgs.ruby
            ] ++ [
              darwinCrossToolchain
              nativeMigcom
              nativeUnifdef
              pkgs.libuuid
            ];
          } // {
            NIX_DARWIN_TOOLCHAIN_DIR = "${darwinCrossToolchain}/bin";
            NIX_NATIVE_LD_PATH = "${nativeLd}/bin/ld";
            NIX_HOST_CC_PATH = "${pkgs.clang}/bin/clang";
            NIX_MIGCOM_PATH = "${nativeMigcom}/bin/migcom";
            NIX_UNIFDEF_PATH = "${nativeUnifdef}/bin/unifdef";
            PUREDARWIN_TCC_SOURCE = "${pkgs.tinycc.src}";
            shellHook = ''
              export CMAKE_TOOLCHAIN_FILE="$PWD/cmake/nix-toolchain.cmake"
              echo "PureDarwin Nix kernel shell: cmake/nix-toolchain.cmake and cached native ld/migcom/unifdef are active."
            '';
          });
          # amdgpu firmware from linux-firmware, per gpu family (the file prefixes
          # of its gc, psp, sdma, dcn and vcn ip versions), into
          # /Library/Firmware/amdgpu where the kext loads it by name. AMD's licence
          # allows redistribution, LICENSE.amdgpu goes along.
          amdgpuFirmwareFamilies = {
            granite-ridge = [ "gc_10_3_6" "psp_13_0_5" "sdma_5_2_6" "dcn_3_1_5" "vcn_3_1_2" ];
            mendocino = [ "gc_10_3_7" "psp_13_0_8" "sdma_5_2_7" "dcn_3_1_6" "vcn_3_1_2" ];
            # navi 2x and renoir predate ip discovery, their firmware files carry the codename
            navi21 = [ "sienna_cichlid" ];
            navi22 = [ "navy_flounder" ];
            navi23 = [ "dimgrey_cavefish" ];
            navi24 = [ "beige_goby" ];
            renoir = [ "renoir" "green_sardine" ];
            # rembrandt (radeon 680m and 660m) also predates ip discovery in its firmware names
            rembrandt = [ "yellow_carp" ];
            # navi 31 (radeon rx 7900 xt and xtx), its gc, psp, smu, sdma, dcn and vcn ip versions
            navi31 = [ "gc_11_0_0" "psp_13_0_0" "smu_13_0_0" "sdma_6_0_0" "dcn_3_2_0" "vcn_4_0_0" ];
            # navi 32 (radeon rx 7800 xt), the files the driver asked for on it
            navi32 = [ "gc_11_0_3" "psp_13_0_10" "smu_13_0_10" "sdma_6_0_3" "dcn_3_2_0" "vcn_4_0_0" ];
            # navi 33 (radeon rx 7600 and 7700s), its gc, psp, smu, sdma, dcn and vcn ip versions
            navi33 = [ "gc_11_0_2" "psp_13_0_7" "smu_13_0_7" "sdma_6_0_2" "dcn_3_2_1" "vcn_4_0_4" ];
            # raven ridge and picasso apus (vega 3, 8 and 11) predate ip discovery, codename files
            raven = [ "raven_" "raven2_" "picasso_" ];
            # gcn2 (sea islands) also predates ip discovery: the kaveri, kabini and mullins apus, bonaire and hawaii dgpus
            kaveri = [ "kaveri_" ];
            kabini = [ "kabini_" ];
            mullins = [ "mullins_" ];
            bonaire = [ "bonaire_" ];
            hawaii = [ "hawaii_" ];
            # polaris (gcn4, rx 400 and 500) and vega m predate ip discovery, codename files
            polaris10 = [ "polaris10_" ];
            polaris11 = [ "polaris11_" ];
            polaris12 = [ "polaris12_" ];
            vegam = [ "vegam_" ];
          };
          # families: a list of amdgpuFirmwareFamilies names, or "all"
          mkAmdgpuFirmware = families: let
            prefixes = if families == "all" then [ "" ]
              else lib.unique (lib.concatMap (f: amdgpuFirmwareFamilies.${f}) families);
            tag = if families == "all" then "all" else lib.concatStringsSep "-" families;
          in pkgs.runCommand "puredarwin-amdgpu-firmware-${tag}" { } ''
            fw=$out/Library/Firmware/amdgpu
            mkdir -p $fw
            for p in ${lib.escapeShellArgs prefixes}; do
              for src in ${pkgs.linux-firmware}/lib/firmware/amdgpu/"$p"*; do
                [ -e "$src" ] || continue
                name=$(basename "$src")
                case "$src" in
                  *.zst) ${pkgs.zstd}/bin/zstd -dqf "$src" -o "$fw/''${name%.zst}" ;;
                  *.xz) ${pkgs.xz}/bin/xz -dc "$src" > "$fw/''${name%.xz}" ;;
                  *) cp -L "$src" "$fw/" ;;
                esac
              done
            done
            cp ${pkgs.linux-firmware.src}/LICENSES/LICENSE.amdgpu $fw/LICENSE.amdgpu
          '';
          # the test images, by their name in the tests set and their package name in image-contents.nix
          testImageNames = {
            cg = "image-cg-test";
            frameworks = "image-frameworks-test";
            tools = "image-tools-test";
            aux = "image-aux-test";
            login = "image-login-test";
          };
          # the images built on image-wayland-minimal bring their own aux KC instead of the default one
          dropGpuDefaults = builtins.filter (p: !(builtins.elem p gpuDefaultPackages));
          # the radeon microcode northern islands asks for (pfp, me, mc, smc, rlc and uvd), into
          # /Library/Firmware/radeon. AMD's licence allows redistribution, LICENSE.radeon goes along.
          radeonFirmware = pkgs.runCommand "puredarwin-radeon-firmware" { } ''
            fw=$out/Library/Firmware/radeon
            mkdir -p $fw
            for n in BARTS_pfp BARTS_me BARTS_mc BARTS_smc TURKS_pfp TURKS_me TURKS_mc TURKS_smc \
              CAICOS_pfp CAICOS_me CAICOS_mc CAICOS_smc BTC_rlc SUMO_uvd; do
              for src in ${pkgs.linux-firmware}/lib/firmware/radeon/$n.bin*; do
                [ -e "$src" ] || continue
                case "$src" in
                  *.zst) ${pkgs.zstd}/bin/zstd -dqf "$src" -o "$fw/$n.bin" ;;
                  *.xz) ${pkgs.xz}/bin/xz -dc "$src" > "$fw/$n.bin" ;;
                  *) cp -L "$src" "$fw/$n.bin" ;;
                esac
              done
              [ -s "$fw/$n.bin" ] || { echo "missing radeon/$n.bin"; exit 1; }
            done
            cp ${pkgs.linux-firmware.src}/LICENSES/LICENSE.radeon $fw/LICENSE.radeon 2>/dev/null || \
              cp ${pkgs.linux-firmware.src}/LICENSE.radeon $fw/LICENSE.radeon
          '';
          # the display firmware (DMC) for gemini lake, tiger lake and rocket lake, i915 asks
          # for i915/<name> and the kext reads /Library/Firmware/i915/<name>
          i915Firmware = pkgs.runCommand "puredarwin-i915-firmware" { } ''
            fw=$out/Library/Firmware/i915
            mkdir -p $fw
            # DMC for gen9 to MTL (cfl and cml load kbl's, ehl and jsl icl's, arl mtl's), GuC and HuC for gen12 and MTL, MTL's GSC
            for src in ${pkgs.linux-firmware}/lib/firmware/i915/{skl,kbl,bxt,glk,icl,tgl,rkl,adls,adlp,mtl}_dmc* \
                ${pkgs.linux-firmware}/lib/firmware/i915/{tgl,adlp,mtl}_{guc,huc}* \
                ${pkgs.linux-firmware}/lib/firmware/i915/mtl_gsc*; do
              [ -e "$src" ] || continue
              name=$(basename "$src")
              case "$src" in
                *.zst) ${pkgs.zstd}/bin/zstd -dqf "$src" -o "$fw/''${name%.zst}" ;;
                *.xz) ${pkgs.xz}/bin/xz -dc "$src" > "$fw/''${name%.xz}" ;;
                *) cp -L "$src" "$fw/" ;;
              esac
            done
            cp ${pkgs.linux-firmware.src}/LICENSES/LICENCE.i915 $fw/LICENCE.i915 2>/dev/null || \
              cp ${pkgs.linux-firmware.src}/LICENSES/LICENSE.i915 $fw/LICENSE.i915
          '';
          # pd-gfxinfo: which driver owns the display, /dev/dri, and neuswc's path lines
          gfxInfoTool = pkgs.runCommand "puredarwin-gfxinfo" { } ''
            mkdir -p $out/usr/bin $out/etc
            cat > $out/usr/bin/pd-gfxinfo <<'EOF'
            #!/bin/sh
            echo "== framebuffers and gpu drivers"
            ioreg -l -w0 | grep -E '"IOClass" = "(PDDRMi915|PDDRMamdgpu|PDDRMFramebuffer[A-Za-z0-9]*|IOGOPFramebuffer)"' | sort | uniq -c
            echo "== /dev/dri"
            ls -l /dev/dri 2>&1
            echo "== neuswc"
            grep -E '^neuswc:|^IOFramebuffer:' /var/log/neuswc.log 2>/dev/null || echo "no /var/log/neuswc.log"
            echo "== kernel"
            dmesg | grep -E 'PDDRMi915|PDDRMFramebuffer|IGD stolen|i915' | tail -20
            EOF
            chmod +x $out/usr/bin/pd-gfxinfo
          '';
          # neuswc composites with gles on crocus into the presenter's flip pair, argon_gfx=cpu
          # in the boot-args picks the cpu path for comparison, argon_trace=1 logs each present
          i915GlNeuswcEnv = pkgs.writeTextDir "etc/neuswc.env" ''
            args=$(sysctl -n kern.bootargs 2>/dev/null | tr ' ' '\n')
            gfx=$(echo "$args" | sed -n 's/^argon_gfx=//p')
            trace=$(echo "$args" | grep -qx argon_trace=1 && echo ARGON_GFX_TRACE=1 MESA_DEBUG=1)
            NEUSWC_ENV="DYLD_LIBRARY_PATH=/usr/lib/pd-i915-mesa ARGON_GFX=''${gfx:-gles} ARGON_GFX_STATS=1 $trace SWC_WAIT_HW_FB=30"
            # the serial console in test vms, else where pd-gfxinfo reads it
            NEUSWC_LOG=/var/log/neuswc.log
            echo "$args" | grep -q '^serial=' && NEUSWC_LOG=/dev/console
          '';
          # crocus and iris gl libraries apart in pd-i915-mesa, pdgltri runs on them once at boot
          i915GlFiles = pkgs.runCommand "puredarwin-i915-gl-files" { } ''
            mkdir -p $out/usr/lib/pd-i915-mesa $out/usr/sbin $out/System/Library/LaunchDaemons
            cp -P ${mesaGpuBuild}/usr/lib/lib{EGL,GLESv2,GL,gallium}*.dylib $out/usr/lib/pd-i915-mesa/
            cat > $out/usr/sbin/pd-i915-gltest <<'EOF2'
            #!/bin/zsh
            sleep 20
            echo "gltest: gles triangle on intel"
            DYLD_LIBRARY_PATH=/usr/lib/pd-i915-mesa /usr/sbin/pdgltri 2>&1 | sed 's/^/gltest: /'
            echo "gltest: done"
            EOF2
            chmod 755 $out/usr/sbin/pd-i915-gltest
            cat > $out/System/Library/LaunchDaemons/org.puredarwin.i915gltest.plist <<EOF
            <?xml version="1.0" encoding="UTF-8"?>
            <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
            <plist version="1.0">
            <dict>
            	<key>Label</key><string>org.puredarwin.i915gltest</string>
            	<key>ProgramArguments</key><array><string>/usr/sbin/pd-i915-gltest</string></array>
            	<key>RunAtLoad</key><true/>
            	<key>LaunchOnlyOnce</key><true/>
            	<key>StandardOutPath</key><string>/dev/console</string>
            	<key>StandardErrorPath</key><string>/dev/console</string>
            </dict>
            </plist>
            EOF
          '';
          # amdgpu, i915 and nouveau together: one aux KC (the kernel loads one per boot) with all
          # the drm kexts, PDDRMDri in the boot KC gives them one /dev/dri
          drmGpuKexts = pkgs.symlinkJoin {
            name = "puredarwin-kexts-gpu";
            paths = [ kextsBuild drmAmdgpuBuild drmI915Build drmNouveauBuild drmRadeonBuild ];
          };
          kcGpuBuild = pkgs.callPackage ./nix/pkgs/toolchain/kc.nix {
            kernel = kernelBuild;
            inherit kernelSource;
            kexts = drmGpuKexts;
            kcTools = kc-tools.packages.${system}.default;
            # radeon takes the pre-GCN parts it lists at a higher score than amdgpu's catch-all, amdgpu the rest
            auxKexts = [ "PDDRM.kext" "PDDRMamdgpu.kext" "PDDRMi915.kext" "PDDRMnouveau.kext" "PDDRMradeon.kext" "PDFBSDiwlwifi.kext" "PDFBSDiwn.kext" "PDFBSDrtwn.kext" "PDFBSDrun.kext" "PDFBSDix.kext" "PDSerialHID.kext" ];
          };
          # what every desktop image carries for the drm drivers: the combined aux KC and firmware
          gpuDefaultPackages = [ gpuAuxFiles (mkAmdgpuFirmware [ "granite-ridge" "mendocino" "navi21" "navi22" "navi23" "navi24" "navi31" "navi32" "navi33" "raven" "renoir" "rembrandt" "kaveri" "kabini" "mullins" "polaris10" "polaris11" "polaris12" "vegam" ]) i915Firmware ];
          gpuAuxFiles = pkgs.runCommand "puredarwin-gpu-aux" { } ''
            mkdir -p $out/Library/KernelCollections
            cp ${kcGpuBuild}/aux.kc $out/Library/KernelCollections/AuxiliaryKernelExtensions.kc
          '';
          # CPU compositing, apps still get the gpu through the system mesa
          gpuNeuswcEnv = pkgs.writeTextDir "etc/neuswc.env" ''
            NEUSWC_ENV="ARGON_GFX=cpu SWC_WAIT_HW_FB=30"
            NEUSWC_LOG=/var/log/neuswc.log
          '';
          imageGpuBuild = linuxPackages.image-wayland-minimal.override (old: {
            kc = kcGpuBuild;
            extraPackages = dropGpuDefaults old.extraPackages ++ [ userlandBuild gpuAuxFiles
              (mkAmdgpuFirmware [ "granite-ridge" "mendocino" "navi21" "navi22" "navi23" "navi24" "navi31" "navi32" "navi33" "raven" "renoir" "rembrandt" "kaveri" "kabini" "mullins" "polaris10" "polaris11" "polaris12" "vegam" ]) i915Firmware gfxInfoTool
              radvFiles libdrmAmdgpuBuild vulkanLoaderNoxBuild vulkanToolsNoxBuild i915GlFiles i915GlNeuswcEnv pdgltriBuild libelfBuild nouveauFirmware
              radeonFirmware iwlwifiFirmwareBuild ];
            imageFileName = "puredarwin.img";
            # one disk for every x86 firmware: BIOS through the MBR stages, 64-bit UEFI by BOOTX64.EFI and 32-bit
            # UEFI by BOOTIA32.EFI on the same ESP, each picks its own; the kernel and KC are x86_64 either way
            legacyBoot = xnu-loader.packages.${linuxPackages.x86System}.legacy-boot.override { stage2Lba = 34; };
            extraLoaders = (old.extraLoaders or [ ]) ++ [
              { loader = linuxPackages.xnu-loader-ia32; efiBinary = "BOOTIA32.EFI"; }
            ];
            # every drm firmware family no longer fits the wayland-minimal root
            rootMB = old.rootMB + 600;
            bootArgs = "-v debug=0x210 -nogzalloc_mode keepsyms=1 gopconsole=1 -noprogress csr-active-config=0x23 dtrace_kernel_symbol_mode=1 fbsdkpi_debug=1 pdnouveau=1";
          });
          # Fedora's Microsoft-signed shim and the step that turns an image into its Secure Boot twin
          shimBuild = pd.x86_64.shim;
          shimIa32Build = pd.x86_64.shimIa32;
          mkSecureBootImage = image: pkgs.callPackage ./nix/lib/secureboot.nix { shim = shimBuild; shimIa32 = shimIa32Build; } {
            inherit image;
            key = builtins.path { path = pd-sb-key; name = "pd-sb-key"; };
          };
          # iwlwifi asks for bare names, the kext also looks in /Library/Firmware/iwlwifi.
          # the newest three api versions of each family the matched cards use, plus pnvm
          iwlwifiFirmwareBuild = pkgs.runCommand "puredarwin-iwlwifi-firmware" { } ''
            fw=$out/Library/Firmware/iwlwifi
            mkdir -p $fw
            src=${pkgs.linux-firmware}/lib/firmware
            for fam in 8265 9000-pu-b0-jf-b0 9260-th-b0-jf-b0 cc-a0 Qu-b0-hr-b0 Qu-b0-jf-b0 \
                Qu-c0-hr-b0 Qu-c0-jf-b0 QuZ-a0-hr-b0 QuZ-a0-jf-b0 ty-a0-gf-a0 so-a0-gf-a0 \
                so-a0-gf4-a0 bz-b0-fm-c0 gl-c0-fm-c0; do
              for f in $(ls $src | grep -E "^iwlwifi-$fam-c?[0-9]+\.ucode(\.zst|\.xz)?$" | sort -V | tail -3) \
                  $(ls $src | grep -E "^iwlwifi-$fam\.pnvm(\.zst|\.xz)?$"); do
                case "$f" in
                  *.zst) ${pkgs.zstd}/bin/zstd -dqf "$src/$f" -o "$fw/''${f%.zst}" ;;
                  *.xz) ${pkgs.xz}/bin/xz -dc "$src/$f" > "$fw/''${f%.xz}" ;;
                  *) cp -L "$src/$f" "$fw/" ;;
                esac
              done
            done
            cp ${pkgs.linux-firmware.src}/LICENSES/LICENCE.iwlwifi_firmware $fw/LICENCE.iwlwifi_firmware
          '';
          # radv on /dev/dri and its icd, the same files the system mesa carries
          radvFiles = pkgs.runCommand "puredarwin-radv-files" { } ''
            mkdir -p $out/usr/lib $out/usr/share/vulkan/icd.d
            cp ${mesaGpuBuild}/usr/lib/libvulkan_radeon.dylib $out/usr/lib/
            cp ${mesaGpuBuild}/usr/share/vulkan/icd.d/radeon_icd.x86_64.json $out/usr/share/vulkan/icd.d/
          '';
          pdnvprobeBuild = pd.x86_64.pdnvprobe;
          # NVIDIA's 580 driver on PDNVLoader: the unmodified package, the Linux programs elftrans runs it with,
          # and the image that carries them
          nvidia580 = pd.x86_64.nvidia580;
          pdnvvkBuild = pd.x86_64.pdnvvk;
          nvidiaLinuxRoot = pd.x86_64.nvidiaLinuxRoot;
          # PDNVLoader in the boot collection, nvidia-drm over it in the auxiliary one with the other drm drivers
          kcNvidiaBuild = kcGpuBuild.override (old: {
            extraKexts = [ "PDNVLoader.kext" ];
            kexts = pkgs.symlinkJoin { name = "puredarwin-kexts-nvidia"; paths = [ drmGpuKexts drmNvidiaBuild nvUvmBuild ]; };
            # nvidia-uvm beside nvidia-drm, what CUDA opens as /dev/nvidia-uvm
            auxKexts = old.auxKexts ++ [ "PDDRMnvidia.kext" "PDNVUVM.kext" ];
          });
          # the aux KC is linked against its boot KC, the kernel refuses image-gpu's on this one
          nvidiaAuxFiles = pkgs.runCommand "puredarwin-nvidia-aux" { } ''
            mkdir -p $out/Library/KernelCollections
            cp ${kcNvidiaBuild}/aux.kc $out/Library/KernelCollections/AuxiliaryKernelExtensions.kc
          '';
          imageNvidiaBuild = imageGpuBuild.override (old: {
            kc = kcNvidiaBuild;
            extraPackages = builtins.filter (p: p != gpuAuxFiles) old.extraPackages
              ++ [ nvidiaAuxFiles elftransBuild nvidia580 nvidiaLinuxRoot pdnvprobeBuild ];
            imageFileName = "puredarwin-nvidia.img";
            rootMB = old.rootMB + 600;
            bootArgs = "-v debug=0x210 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 -noprogress csr-active-config=0x23 dtrace_kernel_symbol_mode=1 fbsdkpi_debug=1";
          });
          pdgltriBuild = pd.x86_64.pdgltri;
          # neuswc composites with GL on radeonsi (gles) or with pixman (cpu)
          neuswcEnv = gfx: pkgs.writeTextDir "etc/neuswc.env" ''
            NEUSWC_ENV="ARGON_GFX=${gfx} SWC_WAIT_HW_FB=30"
            NEUSWC_LOG=/dev/console
          '';
        in rec {
          # everything the flake builds, sorted into the outputs by nix/outputs.nix
          all = {
            iovirtiogpu-kext = kextBuilds.IOVirtIOGPU;
            apple-sdk = appleSdk;
            cg-screen-demo = cgScreenDemoBuild;
            pdres = pdresBuild;
            drm-kmod-src = drmKmodSrc;
            drm-amdgpu = drmAmdgpuBuild;
            drm-msm-arm64 = drmMsmArm64Build;
            drm-nouveau-arm64 = drmNouveauArm64Build;
            nouveau-firmware-gm20b = nouveauFirmwareGm20b;
            dtb-tegra210-p2371-2180 = dtbTegra210P2371_2180;
            libdrm-amdgpu = libdrmAmdgpuBuild;
            libelf = libelfBuild;
            mesa-amdgpu = mesaAmdgpuBuild;
            mesa-gpu = mesaGpuBuild;
            drm-kmod-radeon-src = drmKmodRadeonSrc;
            drm-radeon = drmRadeonBuild;
            radeon-firmware = radeonFirmware;
            drm-kmod-i915-src = drmKmodI915Src;
            drm-i915 = drmI915Build;
            drm-kmod-nouveau-src = drmKmodNouveauSrc;
            drm-nouveau = drmNouveauBuild;
            drm-nvidia = drmNvidiaBuild;
            nvidia-uvm = nvUvmBuild;
            nouveau-firmware = nouveauFirmware;
            i915-firmware = i915Firmware;
            kc-gpu = kcGpuBuild;
            pdnvprobe = pdnvprobeBuild;
            nvidia-580 = nvidia580;
            pdnvvk = pdnvvkBuild;
            kc-nvidia = kcNvidiaBuild;
            amdgpu-firmware-default = mkAmdgpuFirmware [ "granite-ridge" "mendocino" "navi21" "navi22" "navi23" "navi24" "renoir" "rembrandt" ];
            amdgpu-firmware-all = mkAmdgpuFirmware "all";
          } // commonPackages // arm64Packages // probePackages // removeAttrs linuxPackages (lib.attrValues testImageNames) // {
            # the one x86 image: every gpu and wifi driver, BIOS, 64-bit and 32-bit UEFI on the same disk
            image = imageGpuBuild;
            image-sb = mkSecureBootImage imageGpuBuild;
            # the same with NVIDIA's 580 driver for Maxwell to Volta
            image-nvidia = imageNvidiaBuild;
          }
            // lib.filterAttrs (_: lib.isDerivation) nixPortPackages
            # the frameworks built for the browser with emscripten (see nix/wasm.nix)
            // import ./nix/wasm.nix { inherit pkgs lib; };
          # every package by target and scope name: nix build .#pd.x86_64.zsh, .#pd.arm64.zsh
          outputs = import ./nix/outputs.nix {
            inherit lib all tests;
            pd = pd.x86_64;
            pdArm64 = arm64.pdArm64 or { };
          };
          inherit (outputs) packages legacyPackages;
          # images that boot to run one test, apart from the shipped ones: nix build .#tests.cg
          tests = lib.optionalAttrs (!isDarwin) (lib.mapAttrs (_: n: linuxPackages.${n}) testImageNames);
          apps = lib.optionalAttrs (!isDarwin) (linuxApps // {
            # metal shaders in a Linux window (pdmtl demo): nix run .#metal-window
            metal-window = {
              type = "app";
              program = "${arm64Packages.metal-window}/bin/metal-window";
            };
          });
          devShells = {
            kernel = devShell;
            default = devShell;
          };
        };
    in {
      packages = forAllSystems (system: (mkSystem system).packages);
      legacyPackages = forAllSystems (system: (mkSystem system).legacyPackages);
      apps = forAllSystems (system: (mkSystem system).apps);
      devShells = forAllSystems (system: (mkSystem system).devShells);

      # what Hydra builds (hydra.lab004.dev, also the binary cache): the release images, named as nix/release.nix
      # says, and the checks that need no hardware. nix build .#hydraJobs.release.wayland
      hydraJobs =
        let
          system = "x86_64-linux";
          pkgs = nixpkgs.legacyPackages.${system};
          out = mkSystem system;
          rel = import ./nix/release.nix { inherit (pkgs) lib runCommand zstd; inherit self; };
          # image output -> release target name and architecture
          releases = {
            full = { image = "image"; arch = "x86_64"; };
            minimal = { image = "image-minimal"; arch = "x86_64"; };
            wayland = { image = "image-wayland"; arch = "x86_64"; };
            nvidia-580 = { image = "image-nvidia"; arch = "x86_64"; };
            arm64-virt-minimal = { image = "image-arm64-virt-minimal"; arch = "arm64"; };
          };
        in
        {
          release = builtins.mapAttrs (target: r: rel.mkRelease {
            inherit target;
            inherit (r) arch;
            image = out.packages.${r.image};
          }) releases;
          checks = {
            inherit (out.legacyPackages.pd.wasm32) web-check web-check-gl wgsl-check;
          };
        };
    };
}
