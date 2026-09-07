launch_protocol = files('protocol.c')
if os != 'darwin'
  executable('swc-launch',
    ['launch.c', launch_protocol, 'devmajor-@0@.c'.format(os)],
    dependencies: video_backend == 'drm' ? drm : [],
    install: true,
    install_mode: 'rwsr-xr-x',
  )
endif
