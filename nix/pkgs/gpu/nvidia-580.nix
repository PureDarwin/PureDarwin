{ stdenvNoCC
, lib
, fetchurl
, zstd
}:

# NVIDIA's 580 Linux driver package, unmodified, laid out for PDNVLoader and elftrans: the resource manager and
# modeset objects PDNVLoader links, and the userland it runs (nvidia-smi, NVML, the Vulkan driver and its EGL
# libraries). Under NVIDIA's own licence, shipped beside it as LICENSE
let
  version = "580.196.07";
  nv = "usr/lib/x86_64-linux-gnu/nvidia";
  libs = [
    "libnvidia-ml" "libGLX_nvidia" "libnvidia-glcore" "libnvidia-glsi" "libnvidia-tls" "libnvidia-gpucomp"
    "libnvidia-glvkspirv" "libEGL_nvidia" "libnvidia-eglcore"
  ];
in
stdenvNoCC.mkDerivation {
  pname = "nvidia-linux-pd";
  inherit version;

  src = fetchurl {
    url = "https://us.download.nvidia.com/XFree86/Linux-x86_64/${version}/NVIDIA-Linux-x86_64-${version}.run";
    sha256 = "0aab0518db53cf73c412dc7f32bbc850ca95e839d8923d0de4bff42c1c9169ce";
  };

  nativeBuildInputs = [ zstd ];

  # the payload after the installer script, through this zstd: the one the installer brings does not run here
  unpackPhase = ''
    runHook preUnpack
    skip=$(sed -n 's/^skip=//p; 4q' $src)
    mkdir x
    tail -n +$skip $src | zstd -d | tar -xf - -C x
    runHook postUnpack
  '';

  installPhase = ''
    runHook preInstall
    cd x
    install -Dm644 kernel/nvidia/nv-kernel.o_binary $out/usr/share/nvidia/${version}/nv-kernel.o_binary
    install -Dm644 kernel/nvidia-modeset/nv-modeset-kernel.o_binary \
      $out/usr/share/nvidia/${version}/nv-modeset-kernel.o_binary
    install -Dm644 LICENSE $out/usr/share/nvidia/${version}/LICENSE

    install -Dm755 nvidia-smi $out/${nv}/nvidia-smi
    for l in ${lib.concatStringsSep " " libs}; do
      install -Dm755 $l.so.${version} $out/${nv}/$l.so.${version}
    done
    ln -s libnvidia-ml.so.${version} $out/${nv}/libnvidia-ml.so.1
    ln -s libnvidia-ml.so.1 $out/${nv}/libnvidia-ml.so
    ln -s libGLX_nvidia.so.${version} $out/${nv}/libGLX_nvidia.so.0
    ln -s libEGL_nvidia.so.${version} $out/${nv}/libEGL_nvidia.so.0
    # glvnd's EGL dispatcher as the package ships it, the Vulkan driver loads libEGL.so.1
    install -Dm755 libEGL.so.1.1.0 $out/${nv}/libEGL.so.1.1.0
    install -Dm755 libGLdispatch.so.0 $out/${nv}/libGLdispatch.so.0
    ln -s libEGL.so.1.1.0 $out/${nv}/libEGL.so.1

    install -Dm644 10_nvidia.json $out/usr/share/glvnd/egl_vendor.d/10_nvidia.json
    install -Dm644 nvidia_icd.json $out/usr/share/vulkan/icd.d/nvidia_icd.json
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "NVIDIA 580 driver objects and userland, unmodified, for PDNVLoader and elftrans";
    homepage = "https://www.nvidia.com/";
    platforms = [ "x86_64-linux" ];
  };
}
