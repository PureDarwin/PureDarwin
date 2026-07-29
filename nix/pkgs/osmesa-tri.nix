{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, mesa
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
    #include <GL/osmesa.h>
    #include <GL/gl.h>
    #include <stdio.h>
    #include <stdlib.h>

    /* RAM-only Mesa softpipe validator: render a gouraud triangle off-screen and
     * confirm it rasterized. The on-screen counterpart is /usr/bin/osmesa-fb. */
    int main(void) {
        const int W = 256, H = 256;

        OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (!ctx) { fprintf(stderr, "osmesa-tri: OSMesaCreateContext failed\n"); return 1; }

        unsigned char *buf = (unsigned char *)malloc((size_t)W * H * 4);
        if (!OSMesaMakeCurrent(ctx, buf, GL_UNSIGNED_BYTE, W, H)) {
            fprintf(stderr, "osmesa-tri: OSMesaMakeCurrent failed\n"); return 1;
        }

        printf("osmesa-tri: GL_VERSION=%s\n", (const char *)glGetString(GL_VERSION));
        printf("osmesa-tri: GL_RENDERER=%s\n", (const char *)glGetString(GL_RENDERER));

        glViewport(0, 0, W, H);
        glClearColor(0.0f, 0.0f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
        glBegin(GL_TRIANGLES);
            glColor3f(1, 0, 0); glVertex2f(-0.6f, -0.5f);
            glColor3f(0, 1, 0); glVertex2f( 0.6f, -0.5f);
            glColor3f(0, 0, 1); glVertex2f( 0.0f,  0.6f);
        glEnd();
        glFinish();

        /* Count pixels that differ from the clear colour: the rasterized triangle. */
        long lit = 0;
        for (int i = 0; i < W * H; i++) {
            unsigned char *p = &buf[(size_t)i * 4];
            if (p[0] > 8 || p[1] > 8) lit++;
        }
        printf("osmesa-tri: triangle pixels=%ld/%d\n", lit, W * H);

        OSMesaDestroyContext(ctx);
        free(buf);
        return (lit > 1000) ? 0 : 2;
    }
  '';
in
stdenv.mkDerivation {
  pname = "puredarwin-osmesa-tri";
  version = "1";

  dontUnpack = true;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    cat > osmesa-tri.c <<'EOF'
    ${testSrc}
    EOF

    ${cc} -isysroot "$DARWIN_SDK_ROOT" \
      -I${mesa}/usr/include -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${mesa}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib \
      -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains \
      -lOSMesa -lglapi -lc++ -lc++abi -lSystem \
      -o osmesa-tri osmesa-tri.c
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/bin
    cp osmesa-tri $out/usr/bin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Mesa OSMesa softpipe first-light validator (/usr/bin/osmesa-tri)";
    platforms = platforms.linux;
  };
}
