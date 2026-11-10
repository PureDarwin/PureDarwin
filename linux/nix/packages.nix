# packages.nix: Argon, PureDarwin's desktop, built for Linux on GNUstep's libobjc2 from this tree.
# each layer installs lib/ and include/ into its own output and finds the earlier layers through a joined root
{ pkgs, argonSrc, revision ? "dirty" }:

let
  inherit (pkgs) lib;

  # only what the Linux build reads, so unrelated edits elsewhere in the tree do not rebuild Argon
  src = lib.fileset.toSource {
    root = argonSrc;
    fileset = lib.fileset.unions (map (p: argonSrc + p) [
      "/linux/objc-compat" "/linux/iokit" "/linux/pdwifi-nm.c" "/linux/argon-session"
      "/nix/pkgs/apple/foundation.nix" "/image.nix"
      "/src/Frameworks/CoreFoundation" "/src/Frameworks/Foundation" "/src/Frameworks/Onyx2D"
      "/src/Frameworks/CoreGraphics" "/src/Frameworks/CoreText" "/src/Frameworks/QuartzCore"
      "/src/Frameworks/AppKit" "/src/Frameworks/WindowServer" "/src/Frameworks/ApplicationServices"
      "/src/Frameworks/OpenGL" "/src/Frameworks/CoreVideo" "/src/Libraries/CoreServices"
      "/src/ThirdParty/neuswc" "/src/ThirdParty/neuwld"
      "/src/Userspace/Calculator" "/src/Userspace/Finder" "/src/Userspace/Dock" "/src/Userspace/SystemInformation"
      "/src/Userspace/Terminal" "/src/Userspace/MenuBar" "/src/Userspace/Settings" "/src/Userspace/pddecor"
      "/src/Userspace/wifiutil" "/src/Userspace/pdwm"
    ]);
  };

  nativeBuildInputs = with pkgs; [ cmake ninja meson pkg-config python3 wayland-scanner bison flex librsvg ];
  buildInputs = with pkgs; [
    gnustep-libobjc swift-corelibs-libdispatch icu libbsd libuuid
    wayland wayland-protocols pixman libxkbcommon libinput libdrm
    fontconfig freetype libpng libjpeg zlib expat libxml2 curl libffi libtiff libGL libglvnd mesa libx11 udev seatd libevdev mtdev libvterm-neovim
  ];

  # a build step on clang with the earlier layers joined at $root
  layer = { pname, deps ? [ ], script }: pkgs.clangStdenv.mkDerivation {
    inherit pname src nativeBuildInputs buildInputs;
    version = "0-unstable";
    # fortify turns open into an overload argon-root.c cannot interpose, the rest follow PD's own flags
    hardeningDisable = [ "all" ];
    # crash backtraces need the symbols, the frameworks are small enough to keep them
    dontStrip = true;
    dontUseCmakeConfigure = true;
    dontUseMesonConfigure = true;
    dontUseNinjaBuild = true;
    dontUseNinjaInstall = true;
    dontConfigure = true;
    root = pkgs.symlinkJoin { name = "${pname}-root"; paths = deps; };
    buildPhase = ''
      runHook preBuild
      top=$PWD
      mkdir -p $out/lib $out/include
      flagsFor() { echo "-I$out/include -I$out/include/Foundation -I$root/include -I$root/include/Foundation"; }
      libs="-L$out/lib -L$root/lib -Wl,-rpath,$root/lib -Wl,-rpath,$out/lib"
      ${script}
      runHook postBuild
    '';
    dontInstall = true;
  };

  objc = layer {
    pname = "argon-objc";
    script = ''
      clang -shared -fPIC -fobjc-runtime=gnustep-2.2 -fblocks -fno-objc-arc -O1 -g -DARGON_LIBOBJC2=1 \
        -I$top/linux/objc-compat/include -o $out/lib/libargonobjc.so \
        linux/objc-compat/NSObject.m linux/objc-compat/darwin-compat.c linux/objc-compat/notify-compat.c \
        linux/objc-compat/weak-compat.m linux/objc-compat/argon-root.c -lobjc -ldl -ldispatch
      # the compositor follows window style through notify(3), from a static copy
      clang -c -fPIC -O1 -fblocks -I$top/linux/objc-compat/include -o notify-compat.o linux/objc-compat/notify-compat.c
      ar rcs $out/lib/libargonnotify.a notify-compat.o
      cp -r linux/objc-compat/include $out/include/argon-compat
      # the compat TargetConditionals.h reaches into the source tree, the installed copy is the header itself
      cp src/Frameworks/CoreFoundation/Base.subproj/SwiftRuntime/TargetConditionals.h $out/include/argon-compat/
    '';
  };

  corefoundation = layer {
    pname = "argon-corefoundation";
    deps = [ objc ];
    script = ''
      mkdir -p headers/Foundation
      find src/Frameworks/Foundation -name '*.h' -exec cp {} headers/Foundation/ \;
      flags="-I$root/include/argon-compat -I$top/headers -fobjc-runtime=gnustep-2.2 -fblocks -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 -DARGON_LIBOBJC2=1 -D_GNU_SOURCE"
      cmake -S src/Frameworks/CoreFoundation -B build -G Ninja \
        -DCMAKE_C_COMPILER=clang -DCMAKE_OBJC_COMPILER=clang -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_C_FLAGS="$flags" -DCMAKE_OBJC_FLAGS="$flags -fno-objc-arc" \
        -DCMAKE_EXE_LINKER_FLAGS="-lobjc -L$root/lib -largonobjc" \
        -DCMAKE_SHARED_LINKER_FLAGS="-L$root/lib -Wl,-rpath,$root/lib -largonobjc -lobjc -ldispatch -licui18n -licuuc -licudata -luuid -Wl,--no-undefined"
      ninja -C build
      mkdir -p $out/include/CoreFoundation
      cp -a build/CoreFoundation.framework/Headers/. build/CoreFoundation.framework/PrivateHeaders/. $out/include/CoreFoundation/
      cp -a build/CoreFoundation.framework/libCoreFoundation.so $out/lib/
    '';
  };

  # compile NAME DIR SOURCES... and link NAME LIBS..., every source must build
  compileLink = ''
    base="-fobjc-runtime=gnustep-2.2 -fblocks -fno-objc-arc -fPIC -O1 -g -D__PUREDARWIN__=1 -DARGON_LIBOBJC2=1 -D_GNU_SOURCE
      -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 -include argon-darwin.h -I$root/include/argon-compat $(flagsFor) -Wno-everything
      $(pkg-config --cflags freetype2 fontconfig libpng libjpeg libtiff-4 wayland-client xkbcommon libxml-2.0 libffi egl gl) -I$(pkg-config --variable=includedir fontconfig)/fontconfig"
    compile() {
      name=$1; dir=$2; shift 2
      o=$top/build/$name; mkdir -p $o; objs=""
      for s in "$@"; do
        obj=$o/$(echo "$s" | tr '/' '_').o
        lang=objective-c; case "$s" in *.c) lang=c ;; esac
        clang -x $lang $base $CFLAGS_EXTRA -c "$dir/$s" -o "$obj"
        objs="$objs $obj"
      done
      eval "OBJS_$name=\"\$objs\""
    }
    link() {
      name=$1; shift
      eval "objs=\$OBJS_$name"
      clang -shared -o $out/lib/lib$name.so $objs $libs "$@" -Wl,--no-undefined
    }
    core="-lFoundation -lCoreFoundation -largonobjc -lobjc -ldispatch -lm -lpthread"
  '';

  foundation = layer {
    pname = "argon-foundation";
    deps = [ objc corefoundation ];
    script = compileLink + ''
      mkdir -p $out/include/Foundation
      find src/Frameworks/Foundation -name '*.h' -exec cp {} $out/include/Foundation/ \;
      srcs=$(sed -n '/mmSrcs = \[/,/\];/p' nix/pkgs/apple/foundation.nix | grep -o '"[^"]*"' | tr -d '"')
      # Darwin services with no Linux counterpart yet: Mach ports, notifyd, mDNSResponder
      skip="RunLoop.subproj/NSPort RunLoop.subproj/NSPortMessage URL.subproj/NSNetServices"
      srcs=$(for s in $srcs; do case " $skip " in *" $s "*) ;; *) echo "$s.m" ;; esac; done)
      # the class @"..." literals point at under libobjc2
      compile Foundation src/Frameworks/Foundation $srcs String.subproj/NSConstantString.m
      link Foundation -lCoreFoundation -largonobjc -lobjc -ldispatch $(pkg-config --libs libxml-2.0 libffi) -lpthread -luuid -lm
    '';
  };

  graphics = layer {
    pname = "argon-graphics";
    deps = [ objc corefoundation foundation ];
    script = compileLink + ''
      fw=src/Frameworks
      inc=$out/include
      mkdir -p $inc/OpenGL $inc/CoreVideo $inc/CoreServices $inc/IOKit
      cp $fw/OpenGL/include/OpenGL/*.h $inc/OpenGL/
      cp $fw/CoreVideo/*.h $inc/CoreVideo/
      cp src/Libraries/CoreServices/include/CoreServices/*.h $inc/CoreServices/
      cp linux/iokit/include/IOKit/*.h $inc/IOKit/
      for f in Onyx2D CoreGraphics CoreText QuartzCore AppKit WindowServer ApplicationServices; do
        mkdir -p $inc/$f
        find $fw/$f -name '*.h' -exec cp {} $inc/$f/ \;
      done

      # the registry calls the apps make, answered from sysfs
      compile IOKit linux/iokit IOKit.c
      link IOKit -lCoreFoundation -lpthread
      compile Onyx2D $fw/Onyx2D $(cd $fw/Onyx2D && ls *.m)
      link Onyx2D $core $(pkg-config --libs freetype2 fontconfig libpng libjpeg libtiff-4) -lz
      compile CoreGraphics $fw/CoreGraphics $(cd $fw/CoreGraphics && ls *.m | grep -vxE 'CGLPixelSurface.m|CGDirectDisplay.m|KTFont\+PDF.m')
      link CoreGraphics -lOnyx2D $core
      compile CoreText $fw/CoreText CTFont.m KTFont.m
      link CoreText -lCoreGraphics -lOnyx2D $core
      # PD's OpenGL.framework, CGL over EGL. glvnd already owns libOpenGL.so, so it ships as libCGL.so
      compile CGL $fw/OpenGL CGL.c
      link CGL $(pkg-config --libs egl gl x11) -lCoreFoundation -lpthread
      compile CoreVideo $fw/CoreVideo CVDisplayLink.m
      link CoreVideo -lCGL $core
      compile CoreServices src/Libraries/CoreServices LaunchServices.c MultiprocessingCompat.c
      link CoreServices -lCoreFoundation -lpthread
      compile QuartzCore $fw/QuartzCore $(cd $fw/QuartzCore && ls *.m)
      link QuartzCore -lCoreVideo -lCGL -lCoreText -lCoreGraphics -lOnyx2D $core $(pkg-config --libs egl gl)

      ws=$top/build/ws-gen; mkdir -p $ws
      wp=$(pkg-config --variable=pkgdatadir wayland-protocols)
      for x in $wp/stable/xdg-shell/xdg-shell.xml:xdg-shell $wp/stable/linux-dmabuf/linux-dmabuf-v1.xml:linux-dmabuf-v1 \
               $fw/WindowServer/wlr-layer-shell-unstable-v1.xml:wlr-layer-shell-unstable-v1 \
               $fw/WindowServer/wlr-output-management-unstable-v1.xml:wlr-output-management-unstable-v1 \
               $fw/WindowServer/pd-window-position-v1.xml:pd-window-position-v1 \
               $wp/staging/ext-background-effect/ext-background-effect-v1.xml:ext-background-effect-v1; do
        wayland-scanner client-header ''${x%%:*} $ws/''${x##*:}-client-protocol.h
        wayland-scanner private-code ''${x%%:*} $ws/''${x##*:}-protocol.c
      done
      cp $fw/WindowServer/rpc_wayland.c $ws/
      CFLAGS_EXTRA="-I$ws" compile WindowServer $ws $(cd $ws && ls *.c)
      link WindowServer -lCoreGraphics -lCoreFoundation $(pkg-config --libs wayland-client wayland-cursor xkbcommon) $core

      compile AppKit $fw/AppKit $(cd $fw/AppKit && find . -name '*.m' | sed 's|^\./||' | grep -v '^CoreData_/' | sort)
      link AppKit -lWindowServer -lQuartzCore -lCoreVideo -lCGL -lCoreServices -lCoreText -lCoreGraphics -lOnyx2D $core \
        $(pkg-config --libs egl gl wayland-client wayland-cursor xkbcommon fontconfig freetype2)
      # AppKit's localized strings sit next to the library
      cp -a $fw/AppKit/*.lproj $out/lib/
    '';
  };

  neuswc = layer {
    pname = "argon-neuswc";
    deps = [ objc ];
    script = ''
      meson setup build/neuwld src/ThirdParty/neuwld --prefix $out -Dgpu=disabled -Ddoxygen=disabled
      ninja -C build/neuwld install
      export PKG_CONFIG_PATH=$out/lib/pkgconfig:$out/share/pkgconfig:$PKG_CONFIG_PATH
      CFLAGS="-I$root/include/argon-compat" \
      LDFLAGS="-L$root/lib -Wl,--push-state,--whole-archive -largonnotify -Wl,--pop-state -lobjc -ldispatch" \
        meson setup build/neuswc src/ThirdParty/neuswc --prefix $out -Dvideo=drm -Dinput=libinput -Dxwayland=disabled
      ninja -C build/neuswc install
      # pdwm, PureDarwin's window manager on libswc, which PD installs as /bin/neuswc
      clang -O1 -g -o $out/bin/neuswc src/Userspace/pdwm/pdwm.c $(pkg-config --cflags --libs swc wayland-server xkbcommon) \
        -L$root/lib -Wl,--push-state,--whole-archive -largonnotify -Wl,--pop-state -lobjc -ldispatch -Wl,-rpath,$out/lib
    '';
  };

  fonts = pkgs.callPackage (argonSrc + "/nix/pkgs/base/system-fonts.nix") { };

  # the apps, their artwork and the root layout: /System, /Library and /Applications as ARGON_ROOT sees them
  argon = layer {
    pname = "argon";
    deps = [ objc corefoundation foundation graphics neuswc ];
    script = ''
      u=src/Userspace
      w=$top/$u/wifiutil
      # one tree for the frameworks, libargonobjc goes first so its file calls come ahead of libc's
      cp -a $root/lib/. $out/lib/
      chmod -R u+w $out/lib
      rm -rf $out/lib/pkgconfig $out/lib/*.a
      app() {
        name=$1; dir=$2; shift 2
        # the bundle and executable take CFBundleExecutable, which can differ from the source directory
        exe=$(sed -n '/<key>CFBundleExecutable<\/key>/{n;s/.*<string>\(.*\)<\/string>.*/\1/p}' $u/$name/Info.plist)
        a="$out/$dir/$exe.app/Contents"
        mkdir -p "$a/MacOS" "$a/Resources" build/app-$name
        objs=""
        for f in $u/$name/*.m $EXTRA_SRCS; do
          lang=objective-c; case "$f" in *.c) lang=c ;; esac
          o=build/app-$name/$(basename "$f").o
          clang -x $lang -DARGON_DARWIN_STATFS -fobjc-runtime=gnustep-2.2 -fblocks -fno-objc-arc -O1 -g -D__PUREDARWIN__=1 -DARGON_LIBOBJC2=1 -D_GNU_SOURCE \
            -include argon-darwin.h -I$root/include/argon-compat -I$root/include -I$root/include/Foundation -I$root/include/AppKit -I$w -Wno-everything $EXTRA_CFLAGS \
            -c "$f" -o "$o"
          objs="$objs $o"
        done
        clang -o "$a/MacOS/$exe" $objs -L$out/lib -Wl,--no-as-needed -largonobjc -Wl,--as-needed -lAppKit -lQuartzCore -lCoreVideo -lCGL -lCoreServices -lCoreText \
          -lCoreGraphics -lOnyx2D -lWindowServer -lIOKit -lFoundation -lCoreFoundation -largonobjc -lobjc -ldispatch -lm -Wl,-rpath,$out/lib $EXTRA_LIBS
        cp $u/$name/Info.plist "$a/"
        for l in $u/$name/*.lproj; do [ -d "$l" ] && mkdir -p "$a/Resources/$(basename $l)" && cp $l/*.strings "$a/Resources/$(basename $l)/"; done
        if [ -d $u/$name/Resources ]; then cp -r $u/$name/Resources/. "$a/Resources/"; fi
        EXTRA_SRCS= EXTRA_LIBS= EXTRA_CFLAGS=
      }
      app Calculator Applications
      app Finder System/Library/CoreServices
      app Dock System/Library/CoreServices
      app SystemInformation Applications/Utilities
      EXTRA_LIBS=-lvterm app Terminal Applications/Utilities
      # Wi-Fi goes through NetworkManager on Linux, PureDarwin's pdwifi.c drives its own 802.11 stack
      EXTRA_SRCS="linux/pdwifi-nm.c $w/PDWiFiUI.m" app MenuBar System/Library/CoreServices
      EXTRA_SRCS="linux/pdwifi-nm.c $w/PDWiFiUI.m" app Settings Applications
      # pddecor speaks neuswc's decoration protocol
      gen=$top/build/pddecor-gen; mkdir -p $gen
      wayland-scanner client-header src/ThirdParty/neuswc/protocol/pd-decoration-v1.xml $gen/pd-decoration-v1-client-protocol.h
      wayland-scanner private-code src/ThirdParty/neuswc/protocol/pd-decoration-v1.xml $gen/pd-decoration-v1-protocol.c
      EXTRA_CFLAGS="-I$gen" EXTRA_SRCS="$gen/pd-decoration-v1-protocol.c" EXTRA_LIBS=-lwayland-client app pddecor System/Library/CoreServices

      # each app's SVG artwork at the sizes nix/pkgs/apps/*.nix uses
      r() { rsvg-convert "$@"; }
      res() { echo "$out/$1.app/Contents/Resources"; }
      d=$(res System/Library/CoreServices/Dock)
      for svg in $u/Dock/Resources/*.svg; do r -w 96 -h 96 $svg -o $d/$(basename $svg .svg).png; done
      d=$(res System/Library/CoreServices/MenuBar)
      for name in beak wifi volume battery search; do
        r -w 16 -h 16 $u/MenuBar/Resources/$name.svg -o $d/$name.png
        r -w 32 -h 32 $u/MenuBar/Resources/$name.svg -o $d/$name@2x.png
      done
      r -w 192 -h 192 --keep-aspect-ratio $u/MenuBar/Resources/beak.svg -o $d/beak-large.png
      r -w 44 -h 44 $u/MenuBar/Resources/search.svg -o $d/search-large.png
      d=$(res System/Library/CoreServices/Finder)
      for svg in $u/Finder/Resources/*.svg; do
        name=$(basename $svg .svg)
        r -w 128 -h 128 $svg -o $d/$name.png
        r -w 32 -h 32 $svg -o $d/$name-small.png
      done
      r -w 256 -h 256 $u/Finder/Resources/files.svg -o $d/AppIcon.png
      d=$(res Applications/Settings)
      for svg in $u/Settings/Resources/colour/*.svg; do r -w 60 -h 60 $svg -o $d/$(basename $svg .svg).png; done
      for svg in $u/Settings/Resources/symbolic/*.svg; do
        name=$(basename $svg .svg)
        # the logo colours its path itself, the symbolic icons inherit the root's fill
        if [ $name = beak ]; then sed -e 's/fill="black"/fill="#ffffff"/g' $svg > white.svg
        elif [ $name = search ]; then cp $svg white.svg
        else sed -e 's/<svg /<svg fill="#ffffff" /' $svg > white.svg; fi
        if [ $name = beak ]; then name=beak-white; size=400; else size=48; fi
        r -w $size -h $size --keep-aspect-ratio white.svg -o $d/$name.png
      done
      r -w 256 -h 256 $u/Settings/Resources/colour/settings.svg -o $d/AppIcon.png
      r -w 256 -h 256 $u/Terminal/Resources/terminal.svg -o $(res Applications/Utilities/Terminal)/AppIcon.png
      r -w 256 -h 256 $u/Calculator/Resources/calculator.svg -o $(res Applications/Calculator)/AppIcon.png
      find $out/Applications $out/System/Library/CoreServices -name '*.svg' -delete

      # the compositor, its launcher and the session
      mkdir -p $out/bin "$out/System/Library/Desktop Pictures" $out/System/Library/Fonts $out/Library/Preferences $out/etc/fonts
      cp -a $root/bin/neuswc $root/bin/swc-launch $out/bin/
      install -m755 linux/argon-session $out/bin/argon-session
      substituteInPlace $out/bin/argon-session --replace-fail '@argonRoot@' $out
      cp -a "$u/Dock/DesktopPictures/"*.jpg "$out/System/Library/Desktop Pictures/"
      # an eighth of the size for Settings' picker, as nix/pkgs/apps/dock.nix makes them
      mkdir -p "$out/System/Library/Desktop Pictures/.thumbnails"
      for jpg in $u/Dock/DesktopPictures/*.jpg; do
        ${pkgs.libjpeg.bin}/bin/djpeg -scale 1/8 "$jpg" | ${pkgs.libjpeg.bin}/bin/cjpeg -quality 88 \
          > "$out/System/Library/Desktop Pictures/.thumbnails/$(basename "$jpg")"
      done
      cp -rL --no-preserve=mode ${fonts}/System/Library/Fonts/. $out/System/Library/Fonts/
      cat > $out/etc/fonts/argon.conf <<FC
      <?xml version="1.0"?>
      <!DOCTYPE fontconfig SYSTEM "fonts.dtd">
      <fontconfig>
        <include ignore_missing="yes">/etc/fonts/fonts.conf</include>
        <dir>$out/System/Library/Fonts</dir>
        <cachedir prefix="xdg">argon-fontconfig</cachedir>
      </fontconfig>
      FC
      # Argon's SystemVersion.plist, the version PD's image.nix gives PureDarwin and this tree's revision as the build
      version=$(sed -n '/<key>ProductVersion<\/key>/{n;s/.*<string>\(.*\)<\/string>.*/\1/p;q}' image.nix)
      mkdir -p $out/System/Library/CoreServices
      cat > $out/System/Library/CoreServices/SystemVersion.plist <<PLIST
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
      <dict>
      	<key>ProductBuildVersion</key>
      	<string>${revision}</string>
      	<key>ProductName</key>
      	<string>Argon</string>
      	<key>ProductUserVisibleVersion</key>
      	<string>$version</string>
      	<key>ProductVersion</key>
      	<string>$version</string>
      </dict>
      </plist>
      PLIST
      rm -rf $out/include
    '';
  };
in
{
  inherit objc corefoundation foundation graphics neuswc fonts argon;
}
