{ runCommand
, lib
, closureInfo
, glibc
, pdnvvk
, vulkan-tools
, virglrenderer
, vulkan-loader
, libx11
, libxext
, libxcb
, libxau
, libxdmcp
}:

# the Linux side of the NVIDIA image: a glibc where NVIDIA's programs look for it, the Vulkan programs with
# their Nix closure at its store paths, and commands that run each under elftrans
let
  nv = "/usr/lib/x86_64-linux-gnu/nvidia";
  # NVIDIA's GLX driver dlopens the X client libraries by soname
  x = map lib.getLib [ libx11 libxext libxcb libxau libxdmcp ];
  xlibs = lib.concatMapStringsSep ":" (p: "${p}/lib") x;
  closure = closureInfo { rootPaths = [ glibc pdnvvk vulkan-tools virglrenderer vulkan-loader ] ++ x; };
  vkenv = "LD_LIBRARY_PATH=${nv}:${xlibs} VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json";
  wrap = name: env: prog: ''
    cat > $out/usr/local/bin/${name} <<EOF
    #!/bin/sh
    ${env} exec /usr/bin/elftrans ${prog} "\$@"
    EOF
    chmod 755 $out/usr/local/bin/${name}
  '';
in
runCommand "puredarwin-nvidia-linux-root" { } ''
  mkdir -p $out/nix/store $out/lib64 $out/lib/x86_64-linux-gnu $out/usr/local/bin
  for p in $(cat ${closure}/store-paths); do
    cp -a $p $out/nix/store/
  done
  install -m755 ${glibc}/lib/ld-linux-x86-64.so.2 $out/lib64/ld-linux-x86-64.so.2
  for f in libc.so.6 libm.so.6 libdl.so.2 libpthread.so.0 librt.so.1; do
    install -m755 ${glibc}/lib/$f $out/lib/x86_64-linux-gnu/$f
  done

  ${wrap "nvidia-smi" "LD_LIBRARY_PATH=/lib/x86_64-linux-gnu:${nv}" "${nv}/nvidia-smi"}
  ${wrap "vulkaninfo" vkenv "${vulkan-tools}/bin/vulkaninfo"}
  ${wrap "pdvkhead" vkenv "${pdnvvk}/bin/pdvkhead"}
  # pdvktri itself is the darwin triangle the venus image runs
  ${wrap "pdvktri-linux" vkenv "${pdnvvk}/bin/pdvktri"}
  ${wrap "pdvkfill" vkenv "${pdnvvk}/bin/pdvkfill"}
  # the CUDA driver API on NVIDIA's libcuda: needs PDNVUVM's /dev/nvidia-uvm
  ${wrap "pdcuda" "LD_LIBRARY_PATH=/lib/x86_64-linux-gnu:${nv}" "${pdnvvk}/bin/pdcuda"}
  # the vtest server PD's venus driver reaches with VN_DEBUG=vtest: vulkan for darwin programs on NVIDIA's driver
  ${wrap "virgl_test_server" "${vkenv} LD_LIBRARY_PATH=${vulkan-loader}/lib:${nv}:${xlibs}" "${virglrenderer}/bin/virgl_test_server"}
''
