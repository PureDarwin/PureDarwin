fs = import('fs')

# Allow easy lookup for protocol for utility applications
protos = {}
foreach proto : [
  'server-decoration.xml',
  'swc.xml',
  'swc_snap.xml',
  'swc_select.xml',
  'wlr-layer-shell-unstable-v1.xml',
  'wayland-drm.xml',
  wl.find_protocol('xdg-shell', state: 'stable'),
  wl.find_protocol('linux-dmabuf', state: 'unstable', version: 1),
  wl.find_protocol('xdg-decoration', state: 'unstable', version: 1),
  wl.find_protocol('xdg-output', state: 'unstable', version: 1),
]
  protocols = wl.scan_xml(proto,
    public: true,
    client: true,
    server: true)
  protos += {fs.stem(fs.name(proto)): protocols}
endforeach
install_data('swc.xml', install_dir: get_option('datadir') / 'swc')
