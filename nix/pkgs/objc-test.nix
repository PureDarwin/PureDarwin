{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, libobjc
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };

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
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    cat > objc-test.m <<'EOF'
    ${testSrc}
    EOF

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -x objective-c -fno-objc-arc \
      -I${libobjc}/usr/include -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libobjc}/usr/lib -L${libSystem}/usr/lib \
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
    platforms = platforms.linux;
  };
}
