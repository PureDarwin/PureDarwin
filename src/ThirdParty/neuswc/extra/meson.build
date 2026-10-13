executable('swcsnap',
  ['swcsnap.c', protos.get('swc_snap') ],
  dependencies: [
    libswc,
    dependency('wayland-client'),
  ],
  install: true,
)

# a stand-in decorator with plain coloured strips, for testing the compositor side of pd-decoration-v1
executable('pddecor-test',
  ['pddecor-test.c', protos.get('pd-decoration-v1') ],
  dependencies: [
    dependency('wayland-client'),
  ],
  install: true,
)
