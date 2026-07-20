{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    iig-tools.url = "github:PureDarwin/iig-tools";
    kc-tools.url = "github:PureDarwin/kc-tools";
    xnu-loader.url = "github:PureDarwin/xnu-loader";
  };

  outputs = { self, nixpkgs, iig-tools, kc-tools, xnu-loader }:
    let
      lib = nixpkgs.lib;
      systems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = lib.genAttrs systems;

      mkSystem = system:
        let
          pkgs = import nixpkgs {
            inherit system;
            config.allowUnfreePredicate = pkg: lib.getName pkg == "MacOSX11.3.sdk.tar.xz";
          };

          isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
          # fbDOOM (GPL, opt-in - see src/Userspace/fbdoom/CMakeLists.txt) is
          # an external checkout, not a flake input: point PUREDARWIN_FBDOOM_SOURCE_ENV
          # at it (requires --impure). Mirrors the SDK tarball's requireFile
          # pattern - never hardcode a personal machine path into this file.
          fbdoomExternalSrcEnv = builtins.getEnv "PUREDARWIN_FBDOOM_SOURCE_ENV";
          fbdoomExternalSrc =
            if fbdoomExternalSrcEnv == "" then null
            else builtins.path { path = /. + fbdoomExternalSrcEnv; name = "fbdoom-external-src"; };

          chocolateDoomPatchedSrc = pkgs.runCommand "puredarwin-chocolate-doom-patched" { } ''
            mkdir -p $out
            cp -r ${pkgs.chocolate-doom.src}/opl $out/opl
            cp ${pkgs.chocolate-doom.src}/src/midifile.c ${pkgs.chocolate-doom.src}/src/midifile.h \
               ${pkgs.chocolate-doom.src}/src/mus2mid.c ${pkgs.chocolate-doom.src}/src/mus2mid.h \
               $out/
            chmod -R u+w $out
            cd $out
            patch -p1 < ${./src/Userspace/fbdoom/patches/midifile.c.patch}
            patch -p1 < ${./src/Userspace/fbdoom/patches/midifile.h.patch}
            patch -p1 < ${./src/Userspace/fbdoom/patches/mus2mid.c.patch}
            patch -p1 < ${./src/Userspace/fbdoom/patches/opl.c.patch}
            patch -p1 < ${./src/Userspace/fbdoom/patches/opl_internal.h.patch}
          '';
          iig = iig-tools.packages.${system}.default or (
            (pkgs.callPackage iig-tools { }).overrideAttrs (old: {
              meta = (old.meta or { }) // {
                platforms = pkgs.lib.platforms.unix;
              };
            })
          );
          darwinCrossToolchain = if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain.nix { };
          arm64CrossToolchain = if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain.nix {
            target = "arm64-apple-darwin20.4";
            clangTarget = "arm64-apple-macosx11.0";
          };
          libtapi = if isDarwin then null else pkgs.callPackage ./nix/pkgs/libtapi.nix { };
          nativeLd =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/native-ld.nix {
              inherit darwinCrossToolchain libtapi iig;
            };
          nativeUnifdef = if isDarwin then null else pkgs.callPackage ./nix/pkgs/unifdef.nix { };
          nativeMigcom = if isDarwin then null else pkgs.callPackage ./nix/pkgs/migcom.nix { };
          libapfsrwBuild = pkgs.callPackage ./nix/pkgs/libapfsrw.nix { };

          sourceWith = name: prefixes:
            lib.cleanSourceWith {
              src = ./.;
              filter = path: type:
                let
                  rel = lib.removePrefix "${toString ./.}/" (toString path);
                  isParentOfPrefix = prefix:
                    lib.hasPrefix "${rel}/" prefix;
                in
                  rel == "CMakeLists.txt"
                  || rel == "src/CMakeLists.txt"
                  || rel == "cmake"
                  || lib.hasPrefix "cmake/" rel
                  || lib.any (prefix:
                    rel == prefix
                    || lib.hasPrefix "${prefix}/" rel
                    || (type == "directory" && isParentOfPrefix prefix)
                  ) prefixes;
            };
          kernelSource = sourceWith "puredarwin-kernel-source" [
            "projects"
            "src/Kernel/CMakeLists.txt"
            "src/Kernel/xnu"
            "src/Kernel/libfirehose_kernel"
            "src/Libraries/CMakeLists.txt"
            "src/Libraries/AvailabilityVersions"
            "src/Libraries/libSystem/CMakeLists.txt"
            "src/Libraries/libSystem/libplatform"
            "src/Libraries/libSystem/pthread"
            "tools"
          ];
          libSystemSource = sourceWith "puredarwin-libsystem-source" [
            "src/Kernel/xnu/osfmk"
            "src/Kernel/xnu/libkern/libkern"
            "src/Kernel/xnu/libkern/os"
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
            "src/Libraries"
            "src/Libraries/libSystem/libmalloc/compat-include"
            "tools/mig"
          ];
          kextsSource = sourceWith "puredarwin-kexts-source" [
            "projects"
            "src/Kernel/CMakeLists.txt"
            "src/Kernel/Extensions"
            "src/Kernel/libkmod"
            "src/Kernel/xnu"
            "src/Kernel/libfirehose_kernel"
            "src/Libraries"
            "tools"
          ];
          userlandSource = sourceWith "puredarwin-userland-source" [
            "src/Kernel/xnu/osfmk"
            "src/Libraries/IOKit"
            "src/Libraries/PDGOP"
            "src/Libraries/libSystem/libmalloc/compat-include"
            "src/Libraries/libSystem/libsystem_kernel/mach"
            "src/Userspace"
            "tools/mig"
          ];
          fbdoomSource = sourceWith "puredarwin-fbdoom-source" [
            "src/Kernel/xnu/osfmk"
            "src/Kernel/xnu/libkern/libkern"
            "src/Kernel/xnu/libkern/os"
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
            "src/Libraries"
            "src/Libraries/libSystem/libmalloc/compat-include"
            "src/Userspace"
            "tools/mig"
          ];
          cctoolsSource = sourceWith "puredarwin-cctools-source" [
            "src/Kernel/xnu/osfmk"
            "src/Libraries/IOKit"
            "src/Libraries/PDGOP"
            "src/Libraries/libSystem/libmalloc/compat-include"
            "src/Libraries/libSystem/libsystem_kernel/mach"
            "src/Libraries/libcxx/include"
            "src/Libraries/libSystem/corecrypto/include"
            "src/Libraries/CommonCrypto/include"
            "src/Libraries/CommonCrypto/libcn/pd_cc_digest_bridge.c"
            "src/Libraries/libSystem/libc/stdlib/FreeBSD/reallocf.c"
            "src/Userspace"
            "tools"
          ];
          coreFoundationSource = sourceWith "puredarwin-corefoundation-source" [
            "src/Libraries/CoreFoundation"
            "src/Libraries/libSystem/libc/pd-compat-include"
          ];
          mkPureDarwinBuild = args: pkgs.callPackage ./build.nix ({
            inherit darwinCrossToolchain nativeLd nativeUnifdef nativeMigcom iig;
          } // args);

          userlandBuild = mkPureDarwinBuild {
            pname = "puredarwin-userland";
            src = userlandSource;
            buildTargets = [ "launchd" "sw_vers" "ps" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "cpuctl" "mean" "reboot" "halt" "poweroff" "shutdown" "netsetup" "ping" "pcmplay" "startx" "mousemon" "mount" "umount" "ext4tool" ]
              # shell_cmds (+ tsort/uuencode/uudecode) - real BSD userland, displacing toybox applets
              ++ [ "basename" "chown" "dirname" "echo" "false" "getopt" "hostname" "jot" "kill" "logname" "mktemp" "nice" "nohup" "passwd" "printenv" "pwd" "renice" "seq" "shlock" "sleep" "tee" "test_cmd" "true" "tsort" "uname" "yes" "uuencode" "uudecode" ]
              # text_cmds
              ++ [ "banner" "cat" "colrm" "comm" "cut" "expand" "fold" "head" "lam" "look" "nl" "paste" "rev" "split" "tail" "tr" "unexpand" "uniq" "wc" ]
              ++ lib.optionals (!isDarwin) [ "puredarwingop_drv" "puredarwininput_drv" ];
            enableProjects = false;
            enableKernel = false;
            enableLibraries = false;
            enableTools = false;
            installUserland = true;
            installKernel = false;
            prebuiltLibSystem = libSystemBuild;
            xorgDriverIncludes = if isDarwin then null else [
              "${xorgBuild}/usr/include/xorg"
              "${xorgBuild}/usr/include"
              "${lib.getDev pkgs.xorgproto}/include"
              "${xvfbPixmanBuild}/include/pixman-1"
            ];
          };
          tccBuild = mkPureDarwinBuild {
            pname = "puredarwin-tcc";
            src = userlandSource;
            buildTargets = [ "tcc" ];
            enableProjects = false;
            enableKernel = false;
            enableLibraries = false;
            enableTools = false;
            enableTcc = true;
            installUserland = true;
            installKernel = false;
            prebuiltLibSystem = libSystemBuild;
          };
          cctoolsBuild = mkPureDarwinBuild {
            pname = "puredarwin-cctools";
            src = cctoolsSource;
            buildTargets = [ "lipo_selfhost" "size_selfhost" "strings_selfhost" "checksyms_selfhost" "iig_selfhost" "ld64_selfhost" ];
            enableProjects = false;
            enableKernel = false;
            enableLibraries = false;
            enableUserspace = false;
            enableTools = true;
            installUserland = true;
            installKernel = false;
            prebuiltLibSystem = libSystemBuild;
            extraCmakeFlags = [
              "-DPUREDARWIN_ENABLE_SELFHOST_CCTOOLS=ON"
              "-DPUREDARWIN_IIG_SOURCE=${iig-tools}"
            ];
          };
          xvfbPixmanBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-pixman.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) pixman;
            };
          xvfbLibXauBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-stub-lib.nix {
              inherit darwinCrossToolchain;
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
          xvfbLibXdmcpBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-stub-lib.nix {
              inherit darwinCrossToolchain;
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
          xvfbZlibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-zlib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) zlib;
            };
          freetype2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-freetype.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) zlib freetype;
            };
          libfontencBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libfontenc";
              version = pkgs.libfontenc.version;
              src = pkgs.libfontenc.src;
              deps = [ pkgs.xorgproto xvfbZlibBuild ];
            };
          xvfbLibXfont2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXfont2";
              version = pkgs.libxfont_2.version;
              src = pkgs.libxfont_2.src;
              deps = [
                pkgs.xorgproto
                pkgs.xtrans
                xvfbZlibBuild
                freetype2Build
                libfontencBuild
              ];
              configureFlags = [
                "--disable-devel-docs"
              ];
            };
          xlibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libX11";
              version = pkgs.libX11.version;
              src = pkgs.libX11.src;
              deps = [
                pkgs.xorgproto
                pkgs.xtrans
                xcbBuild
                xvfbLibXauBuild
                xvfbLibXdmcpBuild
              ];
              configureFlags = [
                "--disable-specs"
              ];
            };
          xcbBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb";
              version = pkgs.libxcb.version;
              src = pkgs.libxcb.src;
              deps = [
                pkgs.xorgproto
                xvfbLibXauBuild
                xvfbLibXdmcpBuild
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
          xcbUtilBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-util";
              version = pkgs.libxcb-util.version;
              src = pkgs.libxcb-util.src;
              deps = [ pkgs.xorgproto xcbBuild ];
            };
          xcbKeysymsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-keysyms";
              version = pkgs.libxcb-keysyms.version;
              src = pkgs.libxcb-keysyms.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
            };
          xcbWmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-wm";
              version = pkgs.libxcb-wm.version;
              src = pkgs.libxcb-wm.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
              nativeDeps = [ pkgs.m4 ];
            };
          xcbRenderUtilBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-render-util";
              version = pkgs.libxcb-render-util.version;
              src = pkgs.libxcb-render-util.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
            };
          xcbImageBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-image";
              version = pkgs.libxcb-image.version;
              src = pkgs.libxcb-image.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild xcbRenderUtilBuild ];
              postPatchExtra = ''
                sed -i 's/^SUBDIRS = image test/SUBDIRS = image/' Makefile.in
              '';
            };
          xcbCursorBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-cursor";
              version = pkgs.libxcb-cursor.version;
              src = pkgs.libxcb-cursor.src;
              deps = [
                pkgs.xorgproto
                xcbBuild
                xcbUtilBuild
                xcbKeysymsBuild
                xcbImageBuild
                xcbRenderUtilBuild
              ];
              nativeDeps = [ pkgs.m4 ];
            };
          xcbXrmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-xcb-util-xrm";
              version = pkgs.xcbutilxrm.version;
              src = pkgs.xcbutilxrm.src;
              deps = [ pkgs.xorgproto xlibBuild xcbBuild xcbUtilBuild ];
              nativeDeps = [ pkgs.m4 pkgs.xorg.utilmacros ];
              configureFlags = [
                "--disable-devel-docs"
              ];
            };
          libevBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libev";
              version = pkgs.libev.version;
              src = pkgs.libev.src;
              preConfigureExtra = ''
                export ac_cv_func_poll=yes
                export ac_cv_func_select=yes
                export ac_cv_header_poll_h=yes
              '';
            };
          pcre2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
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
          yajlBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/yajl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) yajl;
            };
          startupNotificationBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-startup-notification";
              version = pkgs.libstartup_notification.version;
              src = pkgs.libstartup_notification.src;
              deps = [ pkgs.xorgproto xlibBuild xcbBuild xcbUtilBuild ];
              configureFlags = [
                "--x-includes=${lib.getDev xlibBuild}/include"
                "--x-libraries=${xlibBuild}/lib"
              ];
              preConfigureExtra = ''
                export lf_cv_sane_realloc=yes
              '';
              postPatchExtra = ''
                sed -i 's/^SUBDIRS=libsn test doc/SUBDIRS=libsn/' Makefile.in
              '';
            };
          cairoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/cairo.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) cairo xorgproto;
              pixman = xvfbPixmanBuild;
              zlib = xvfbZlibBuild;
              libX11 = xlibBuild;
              libXext = xvfbLibXextBuild;
              libXrender = xvfbLibXrenderBuild;
              libxcb = xcbBuild;
              freetype = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
            };
          libffiBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libffi";
              version = pkgs.libffi.version;
              src = pkgs.libffi.src;
              configureFlags = [
                "--disable-docs"
                "--disable-multi-os-directory"
              ];
            };
          glibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/glib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) glib;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
            };
          expatBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-expat";
              version = pkgs.expat.version;
              src = pkgs.expat.src;
              configureFlags = [
                "--without-docbook"
                "--without-examples"
                "--without-tests"
              ];
            };
          fontconfigBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/fontconfig.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) fontconfig;
              freetype = freetype2Build;
              expat = expatBuild;
            };
          fribidiBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/fribidi.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) fribidi;
            };
          harfbuzzBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/harfbuzz.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) harfbuzz;
              freetype = freetype2Build;
            };
          pangoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/pango.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) pango;
              glib = glibBuild;
              fribidi = fribidiBuild;
              harfbuzz = harfbuzzBuild;
              cairo = cairoBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              pixman = xvfbPixmanBuild;
              libxcb = xcbBuild;
              fontconfig = fontconfigBuild;
              freetype = freetype2Build;
              expat = expatBuild;
            };
          i3Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/i3.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) i3;
              inherit (pkgs) xorgproto;
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
              xkbcommon = xkbcommonBuild;
              yajl = yajlBuild;
              pcre2 = pcre2Build;
              cairo = cairoBuild;
              pango = pangoBuild;
              glib = glibBuild;
              fribidi = fribidiBuild;
              harfbuzz = harfbuzzBuild;
              libev = libevBuild;
              libiconv = libiconvBuild;
              zlib = xvfbZlibBuild;
              libffi = libffiBuild;
              pixman = xvfbPixmanBuild;
              fontconfig = fontconfigBuild;
              freetype = freetype2Build;
              expat = expatBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
            };
          i3statusShimBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/i3status-shim.nix { };
          xvfbLibICEBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libICE";
              version = pkgs.libICE.version;
              src = pkgs.libICE.src;
              deps = [ pkgs.xorgproto pkgs.xtrans ];
              preConfigureExtra = ''
                export ac_cv_func_arc4random_buf=yes
              '';
            };
          xvfbLibSMBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libSM";
              version = pkgs.libSM.version;
              src = pkgs.libSM.src;
              deps = [ pkgs.xorgproto pkgs.xtrans xvfbLibICEBuild ];
              configureFlags = [
                "--without-libuuid"
              ];
            };
          xvfbLibXtBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXt";
              version = pkgs.libXt.version;
              src = pkgs.libXt.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibICEBuild
                xvfbLibSMBuild
              ];
            };
          xvfbLibXextBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXext";
              version = pkgs.libXext.version;
              src = pkgs.libXext.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXauBuild ];
            };
          xvfbLibXmuBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXmu";
              version = pkgs.libXmu.version;
              src = pkgs.libXmu.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXextBuild
                xvfbLibXtBuild
                xvfbLibSMBuild
                xvfbLibICEBuild
              ];
            };
          xvfbLibXpmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXpm";
              version = pkgs.libXpm.version;
              src = pkgs.libXpm.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xvfbLibXawBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXaw";
              version = pkgs.libXaw.version;
              src = pkgs.libXaw.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXextBuild
                xvfbLibXmuBuild
                xvfbLibXpmBuild
                xvfbLibXtBuild
                # Same transitive header need as libXmu: Xt's Shell.h pulls in
                # <X11/SM/SMlib.h>, so libSM/libICE headers must be reachable.
                xvfbLibSMBuild
                xvfbLibICEBuild
              ];
              # XawI18n.c uses MB_LEN_MAX without including <limits.h> - it
              # relies on it arriving transitively, which happens on glibc but
              # not through the Darwin SDK header chain. Force-include it.
              preConfigureExtra = ''
                export CFLAGS="$CFLAGS -include limits.h"
              '';
              # libXaw installs versioned Athena archives (libXaw6.a/libXaw7.a)
              # but no plain libXaw.a, and its .dylib symlinks dangle under
              # --disable-shared. Consumers (xterm) link -lXaw, so provide the
              # unversioned static alias.
              postInstallExtra = ''
                ln -sf libXaw7.a $out/lib/libXaw.a
              '';
            };
          xvfbLibXkbfileBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxkbfile";
              version = pkgs.libxkbfile.version;
              src = pkgs.libxkbfile.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xkbcompBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-xkbcomp.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) xkbcomp xorgproto;
              libX11 = xlibBuild;
              libxkbfile = xvfbLibXkbfileBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libxcb = xcbBuild;
            };
          xvfbFontsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-fonts.nix { };
          xkeyboardConfigBuild =
            if isDarwin then null else pkgs.runCommand "puredarwin-xkeyboard-config" { } ''
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
          xvfbBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xorg-server = pkgs.xorg-server;
              pixman = xvfbPixmanBuild;
              libXau = xvfbLibXauBuild;
              libXfont2 = xvfbLibXfont2Build;
              zlib = xvfbZlibBuild;
              freetype2 = freetype2Build;
              libfontenc = libfontencBuild;
              xvfbZlib = xvfbZlibBuild;
              inherit (pkgs) xorgproto xtrans;
              libxkbfile = xvfbLibXkbfileBuild;
              libXdmcp = pkgs.libxdmcp;
            };
          xvfbLibxcvtBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-libxcvt.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libxcvt;
            };
          xorgBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xorg-server = pkgs.xorg-server;
              pixman = xvfbPixmanBuild;
              libXau = xvfbLibXauBuild;
              libXfont2 = xvfbLibXfont2Build;
              zlib = xvfbZlibBuild;
              freetype2 = freetype2Build;
              libfontenc = libfontencBuild;
              xvfbZlib = xvfbZlibBuild;
              inherit (pkgs) xorgproto xtrans;
              libxkbfile = xvfbLibXkbfileBuild;
              libXdmcp = pkgs.libxdmcp;
              libxcvt = xvfbLibxcvtBuild;
            };
          xvfbLibXrenderBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXrender";
              version = pkgs.libXrender.version;
              src = pkgs.libXrender.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xvfbLibXfixesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXfixes";
              version = pkgs.libXfixes.version;
              src = pkgs.libXfixes.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          libXftBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXft";
              version = pkgs.libXft.version;
              src = pkgs.libXft.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXrenderBuild
                freetype2Build
                fontconfigBuild
                expatBuild
              ];
              nativeDeps = [ pkgs.xorg.utilmacros ];
            };
          dmenuBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/dmenu.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) dmenu;
              inherit (pkgs) xorgproto;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXft = libXftBuild;
              libXrender = xvfbLibXrenderBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
            };
          xvfbLibXiBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXi";
              version = pkgs.libXi.version;
              src = pkgs.libXi.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXextBuild
                xvfbLibXfixesBuild
              ];
              configureFlags = [
                "--disable-malloc0returnsnull"
              ];
            };
          xeyesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xeyes.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xeyes = pkgs.xeyes;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXi = xvfbLibXiBuild;
              libXrender = xvfbLibXrenderBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXmu = xvfbLibXmuBuild;
              libXt = xvfbLibXtBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              inherit (pkgs) xorgproto;
            };
          ncursesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/ncurses.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              ncurses = pkgs.ncurses;
            };
          libiconvBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libiconv.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libiconvReal = pkgs.libiconvReal;
            };
          toyboxBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toybox.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              toybox = pkgs.toybox;
              zlib = xvfbZlibBuild;
            };
          nanoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/nano.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nano = pkgs.nano;
              ncurses = ncursesBuild;
            };
          bmakeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/bmake.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              bmake = pkgs.bmake;
            };
          zshBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/zsh.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              zsh = pkgs.zsh;
              ncurses = ncursesBuild;
            };
          fileBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/file.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              file = pkgs.file;
              zlib = xvfbZlibBuild;
            };
          opensslBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/openssl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              openssl = pkgs.openssl;
            };
          curlBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/curl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              curl = pkgs.curl;
              openssl = opensslBuild;
              zlib = xvfbZlibBuild;
            };
          opensshBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/openssh.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              openssh = pkgs.openssh;
              openssl = opensslBuild;
              zlib = xvfbZlibBuild;
            };
          gitBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/git.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              git = pkgs.git;
              zlib = xvfbZlibBuild;
              curl = curlBuild;
              openssl = opensslBuild;
            };
          migcomDarwinBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/migcom-darwin.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
            };
          ioregBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/ioreg.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
            };
          xkbcommonBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xkbcommon.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              xkeyboard-config = xkeyboardConfigBuild;
            };
          fastfetchBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/fastfetch.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              fastfetch = pkgs.fastfetch;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
            };
          xtermBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xterm.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xterm = pkgs.xterm;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              libXt = xvfbLibXtBuild;
              libXext = xvfbLibXextBuild;
              libXmu = xvfbLibXmuBuild;
              libXpm = xvfbLibXpmBuild;
              libXaw = xvfbLibXawBuild;
              inherit (pkgs) xorgproto;
            };
          icuCoreBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/icucore.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              icuSrc = pkgs.icu.src;
            };
          coreFoundationBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/corefoundation.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) icu;
              src = "${coreFoundationSource}/src/Libraries/CoreFoundation";
              # libSystem's own package output only ships dylibs + dyld's
              # "guest" header tree (see build.nix's installLibSystem
              # comment) - not a real usr/include, so CF's -isysroot never
              # sees our hand-written pd-compat-include headers
              # (mach-o/dyld_priv.h etc). Point straight at the source dir.
              pdCompatInclude = "${coreFoundationSource}/src/Libraries/libSystem/libc/pd-compat-include";
            };
          iokitBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/iokit.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
            };
          libSystemBuild = mkPureDarwinBuild {
            pname = "puredarwin-libsystem";
            src = libSystemSource;
            buildTargets = [ "libSystem_B_stub" "dyld" "libsystem_kernel_static" "libdispatch_static" "IOKitCF" ];
            enableUserspace = false;
            enableKernel = false;
            installUserland = false;
            installKernel = false;
            installLibSystem = true;
          };
          fbdoomBuild = (mkPureDarwinBuild {
            pname = "puredarwin-fbdoom";
            src = fbdoomSource;
            buildTargets = [ "fbdoom" ];
            enableProjects = false;
            enableKernel = false;
            installUserland = false;
            installKernel = false;
            extraCmakeFlags = [
              "-DPUREDARWIN_ENABLE_FBDOOM=ON"
              "-DPUREDARWIN_FBDOOM_SOURCE=${fbdoomExternalSrc}"
              "-DPUREDARWIN_CHOCOLATE_DOOM_SOURCE=${chocolateDoomPatchedSrc}"
            ];
          }).overrideAttrs (old: {
            installPhase = ''
              runHook preInstall
              mkdir -p $out/usr/bin
              cp build-nix/src/Userspace/fbdoom/fbdoom $out/usr/bin/fbdoom
              runHook postInstall
            '';
          });
          kernelBuild = mkPureDarwinBuild {
            pname = "puredarwin-kernel";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "RELEASE";
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
          kernelDebugBuild = mkPureDarwinBuild {
            pname = "puredarwin-kernel-debug";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "DEBUG";
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
          xnuHeadersBuild = mkPureDarwinBuild {
            pname = "puredarwin-xnu-headers";
            src = kernelSource;
            buildTargets = [ "xnu_headers.extproj" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installXnuHeaders = true;
            xnuKernelConfig = "RELEASE";
          };
          kextsBuild = mkPureDarwinBuild {
            pname = "puredarwin-kexts";
            src = kextsSource;
            buildTargets = [ "kexts" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            enableIOGraphicsFamily = true;
          };
          kextsArm64Build = mkPureDarwinBuild {
            pname = "puredarwin-kexts-arm64";
            src = kextsSource;
            buildTargets = [ "corecrypto.kext" "pthread.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "corecrypto.kext" "pthread.kext" ];
            enableIOGraphicsFamily = false;
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
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
          fullBuild = mkPureDarwinBuild {
            pname = "puredarwin";
            src = ./.;
            buildTargets = [ "launchd" "xnu" "kexts" "libsystem_kernel" "pcmplay" ];
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

          imageExtraPackageSet = lib.optionalAttrs (!isDarwin) {
            xvfb = xvfbBuild;
            xorg = xorgBuild;
            libxcvt = xvfbLibxcvtBuild;
            xeyes = xeyesBuild;
            xterm = xtermBuild;
            xkbcomp = xkbcompBuild;
            xkeyboard-config = xkeyboardConfigBuild;
            fonts = xvfbFontsBuild;
            libiconv = libiconvBuild;
            nano = nanoBuild;
            bmake = bmakeBuild;
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
            iokit = iokitBuild;
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
          };

          commonPackages = {
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
          };

          linuxPackages =
            let
              kcBuild = pkgs.callPackage ./nix/pkgs/kc.nix {
                kernel = kernelBuild;
                kexts = kextsBuild;
                kcTools = kc-tools.packages.${system}.default;
              };
              kcDebugBuild = pkgs.callPackage ./nix/pkgs/kc.nix {
                kernel = kernelDebugBuild;
                kexts = kextsBuild;
                kcTools = kc-tools.packages.${system}.default;
              };
              kcArm64DebugBuild = pkgs.callPackage ./nix/pkgs/kc-arm64.nix {
                kernel = kernelArm64VirtDebugBuild;
                kexts = kextsArm64Build;
                kcTools = kc-tools.packages.${system}.default;
              };
              imageBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystem;
                extraPackages = lib.attrValues imageExtraPackageSet;
                kc = kcBuild;
                xnuLoader = xnu-loader.packages.${system}.default;
                apfsprogs = pkgs.apfsprogs;
                testAudioFile = /home/vali/development/darwin/stillalive.pcm;
              };
              imageHfsBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystem;
                extraPackages = lib.attrValues imageExtraPackageSet;
                kc = kcBuild;
                xnuLoader = xnu-loader.packages.${system}.default;
                apfsprogs = pkgs.apfsprogs;
                hfsprogs = pkgs.hfsprogs;
                libdmg-hfsplus = pkgs.callPackage ./nix/pkgs/libdmg-hfsplus.nix { };
                rootFsType = "hfs";
                testAudioFile = /home/vali/development/darwin/badapple.pcm;
              };
              imageDebugBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystem;
                extraPackages = lib.attrValues imageExtraPackageSet;
                kc = kcDebugBuild;
                xnuLoader = xnu-loader.packages.${system}.default;
                apfsprogs = pkgs.apfsprogs;
                imageFileName = "puredarwin-debug.img";
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
                    -cpu IvyBridge,vendor=GenuineIntel \
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

                  exec qemu-system-x86_64 \
                    -machine q35,accel=kvm \
                    -cpu "''${PUREDARWIN_KVM_CPU:-host}" \
                    -smp "''${PUREDARWIN_VM_SMP:-4}" \
                    -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
                    -drive if=pflash,format=raw,unit=0,readonly=on,file="$ovmf_code" \
                    -drive if=pflash,format=raw,unit=1,file="$ovmf_vars" \
                    -device ich9-ahci,id=sata \
                    -drive if=none,id=system,file="$image",format=raw,cache=writeback"$image_readonly_opt" \
                    -device ide-hd,bus=sata.0,drive=system \
                    -device e1000-82545em,netdev=net0 \
                    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
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
            in {
              darwin-cross-toolchain = darwinCrossToolchain;
              native-ld = nativeLd;
              kc = kcBuild;
              kc-debug = kcDebugBuild;
              kc-arm64-debug = kcArm64DebugBuild;
              corefoundation = coreFoundationBuild;
              icucore = icuCoreBuild;
              iokit = iokitBuild;
              image = imageBuild;
              image-hfs = imageHfsBuild;
              image-debug = imageDebugBuild;
              xorg = xorgBuild;
              libxcvt = xvfbLibxcvtBuild;
              userland = userlandBuild;
              vm-runner = runVm;
              kvm-runner = runKvm;
            };

          linuxApps =
            let
              runVm = linuxPackages.vm-runner;
              runKvm = linuxPackages.kvm-runner;
            in {
              default = {
                type = "app";
                program = "${runVm}/bin/puredarwin-vm";
              };
              vm = {
                type = "app";
                program = "${runVm}/bin/puredarwin-vm";
              };
              kvm = {
                type = "app";
                program = "${runKvm}/bin/puredarwin-kvm";
              };
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
            ] ++ lib.optionals (!isDarwin) [
              darwinCrossToolchain
              nativeMigcom
              nativeUnifdef
              pkgs.libuuid
            ];
          } // lib.optionalAttrs (!isDarwin) {
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
          } // lib.optionalAttrs isDarwin {
            shellHook = ''
              echo "PureDarwin Darwin shell: using the native Apple host toolchain path."
            '';
          });
        in {
          packages = commonPackages // lib.optionalAttrs (!isDarwin) linuxPackages;
          apps = lib.optionalAttrs (!isDarwin) linuxApps;
          devShells = {
            kernel = devShell;
            default = devShell;
          };
        };
    in {
      packages = forAllSystems (system: (mkSystem system).packages);
      apps = forAllSystems (system: (mkSystem system).apps);
      devShells = forAllSystems (system: (mkSystem system).devShells);
    };
}
