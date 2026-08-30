{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libobjc
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let

  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";

  testSrc = ''
    #import <objc/runtime.h>
    #import <objc/message.h>
    #import <objc/NSObject.h>
    #include <stdio.h>

    @interface Foo : NSObject
    - (int)answer;
    @end
    @implementation Foo
    - (int)answer { return 42; }
    @end

    int main(void) {
        Foo *f = [Foo new];
        int a = ((int (*)(id, SEL))objc_msgSend)(f, sel_registerName("answer"));
        printf("objc ok: class=%s answer=%d\n", class_getName(object_getClass(f)), a);
        return a == 42 ? 0 : 1;
    }
  '';
in
stdenv.mkDerivation {
  pname = "puredarwin-objc-test";
  version = "1";

  dontUnpack = true;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cat > objc-test.m <<'EOF'
    ${testSrc}
    EOF

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -x objective-c -fno-objc-arc \
      -I${libobjc}/usr/include -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libobjc}/usr/lib -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains \
      -lobjc -lSystem \
      -o objc-test objc-test.m
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/bin
    cp objc-test $out/usr/bin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "libobjc smoke test binary (/usr/bin/objc-test)";
    platforms = platforms.unix;
  };
}
