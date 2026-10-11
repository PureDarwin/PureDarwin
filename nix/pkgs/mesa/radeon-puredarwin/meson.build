libradeonwinsys = static_library('radeonwinsys',
  files('radeon_puredarwin.c', 'radeon_puredarwin_surface.c', 'radeon_surface.c'),
  include_directories: [inc_src, inc_include, inc_gallium, inc_gallium_aux, inc_gallium_drivers],
  dependencies: [idep_mesautil, idep_nir, idep_nir_headers],
  gnu_symbol_visibility: 'hidden',
)
