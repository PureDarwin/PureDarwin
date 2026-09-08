{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libobjc
, corefoundation
, src
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
  mmSrcs = [
    "String.subproj/NSString"
    "String.subproj/NSCFString"
    "Collections.subproj/NSArray"
    "Collections.subproj/NSData"
    "Collections.subproj/NSDictionary"
    "Collections.subproj/NSSet"
    "Numeric.subproj/NSNumber"
    "Date.subproj/NSDate"
    "Date.subproj/NSTimer"
    "URL.subproj/NSURL"
    "Coding.subproj/NSCoder"
    "Coding.subproj/NSKeyedUnarchiver"
    "Runtime.subproj/NSAutoreleasePool"
    "Collections.subproj/NSEnumerator"
    "Collections.subproj/NSIndexSet"
    "Notifications.subproj/NSNotification"
    "UndoManager.subproj/NSUndoManager"
    "Scanner.subproj/NSScanner"
    "Locale.subproj/NSLocale"
    "String.subproj/NSTextCheckingResult"
    "String.subproj/NSOrthography"
    "Predicates.subproj/NSSortDescriptor"
    "Predicates.subproj/NSValueTransformer"
    "Predicates.subproj/NSPredicate"
    "Runtime.subproj/NSSpellEngine"
    "Runtime.subproj/NSPlatform"
    "Coding.subproj/NSKeyedArchiver"
    "Coding.subproj/NSArchiver"
    "Collections.subproj/NSPropertyListSerialization"
    "Notifications.subproj/NSNotificationQueue"
    "Formatters.subproj/NSDateFormatter"
    "KVC.subproj/NSKeyValueCoding"
    "KVC.subproj/NSKeyValueObserving"
    "Locking.subproj/NSLock"
    "Collections.subproj/NSHashTable"
    "Collections.subproj/NSRangeEntries"
    "Collections.subproj/NSCountedSet"
    "Runtime.subproj/NSInvocation"
    "Date.subproj/NSCalendar"
    "Formatters.subproj/NSNumberFormatter"
    "RunLoop.subproj/NSSocket_bsd"
    "RunLoop.subproj/NSSelectInputSource"
    "RunLoop.subproj/NSRunLoop"
    "Formatters.subproj/NSFormatter"
    "String.subproj/NSAttributedString"
    "Runtime.subproj/NSError"
    "Runtime.subproj/NSZone"
    "Runtime.subproj/NSLog"
    "Stream.subproj/NSStream"
    "Runtime.subproj/NSObjCRuntime"
    "Runtime.subproj/NSGeometry"
    "Runtime.subproj/NSAffineTransform"
    "Runtime.subproj/NSException"
    "Runtime.subproj/NSValue"
    "Runtime.subproj/NSDebug"
    "Runtime.subproj/NSBundle"
    "Runtime.subproj/NSProcessInfo"
    "Runtime.subproj/NSThread"
    "Runtime.subproj/NSUserDefaults"
    "Collections.subproj/NSMapTable"
    "String.subproj/NSCharacterSet"
    "FileManager.subproj/NSPathUtilities"
    "FileManager.subproj/NSFileManager"
    "FileManager.subproj/NSFileHandle"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-foundation";
  version = "1";

  inherit src;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    mkdir -p foundation-headers/Foundation
    find Runtime.subproj Coding.subproj Formatters.subproj Notifications.subproj UndoManager.subproj Scanner.subproj Locale.subproj RunLoop.subproj Predicates.subproj KVC.subproj Locking.subproj String.subproj Collections.subproj URL.subproj Numeric.subproj Date.subproj Stream.subproj XPC.subproj FileManager.subproj -name '*.h' -exec cp {} foundation-headers/Foundation/ \;

    # corefoundation.nix installs its headers flattened into $out/include
    # (no "CoreFoundation/" subdirectory) - stage the same
    # "<CoreFoundation/Foo.h>" layout our sources expect.
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    CFLAGS="-x objective-c -fno-objc-arc -fPIC -Os -DNDEBUG -D__PUREDARWIN__=1 \
      -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 \
      -isysroot $DARWIN_SDK_ROOT \
      -Ifoundation-headers \
      -Icf-headers \
      -I${libSystem}/usr/include \
      -I${libobjc}/usr/include \
      -I${corefoundation}/include"

    objs=""
    for s in ${lib.concatStringsSep " " mmSrcs}; do
      objfile="$(basename $s).o"
      ${cc} $CFLAGS -c "$s.m" -o "$objfile"
      objs="$objs $objfile"
    done

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,/usr/lib/libFoundation.dylib \
      -Wl,-fixup_chains \
      -lobjc -lCoreFoundation -lSystem \
      -o libFoundation.dylib $objs

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/usr/include/Foundation
    cp libFoundation.dylib $out/usr/lib/
    find Runtime.subproj Coding.subproj Formatters.subproj Notifications.subproj UndoManager.subproj Scanner.subproj Locale.subproj RunLoop.subproj Predicates.subproj KVC.subproj Locking.subproj String.subproj Collections.subproj URL.subproj Numeric.subproj Date.subproj Stream.subproj XPC.subproj FileManager.subproj -name '*.h' -exec cp {} $out/usr/include/Foundation/ \;

    fwdir=$out/System/Library/Frameworks/Foundation.framework
    mkdir -p "$fwdir/Versions/A/Resources"
    ln -s ../../../../../../usr/lib/libFoundation.dylib "$fwdir/Versions/A/Foundation"
    ln -s ../../../../../usr/include/Foundation "$fwdir/Versions/A/Headers"
    cp Info.plist "$fwdir/Versions/A/Resources/Info.plist"
    ln -s A "$fwdir/Versions/Current"
    ln -s Versions/Current/Foundation "$fwdir/Foundation"
    ln -s Versions/Current/Headers "$fwdir/Headers"
    ln -s Versions/Current/Resources "$fwdir/Resources"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin Foundation";
    platforms = platforms.unix;
  };
}
