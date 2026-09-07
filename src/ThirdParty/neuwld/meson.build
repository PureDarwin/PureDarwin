project('wld', 'c',
  version: '0.0',
  meson_version: '>=1.1',
  license: 'MIT',
  default_options: [
    # C11: libdrm uses anonymous union extension
    # C23: nouveau.c uses empty initializer extension
    'c_std=c23',
    'warning_level=3',
    'default_library=static',
    'b_ndebug=if-release',
  ],
)

cc = meson.get_compiler('c')
pkg = import('pkgconfig')
inc = include_directories('protocol')

c_args = cc.get_supported_arguments([
  '-fvisibility=hidden',
  '-Wvla',
  '-Werror=implicit-function-declaration',
  '-Werror=implicit-int',
  '-Werror=pointer-sign',
  '-Werror=pointer-arith',

  '-Wno-sign-compare', # pixman.c:630
  '-Wno-gnu-empty-struct', # ARRAY_LENGTH
  '-Wno-gnu-statement-expression-from-macro-expansion', # CONTAINER_OF
  '-Wno-gnu-zero-variadic-macro-arguments', # wld-private.h:57
  '-Wno-static-in-inline', # context.c:unwrap_buffer
  '-Wno-unused-parameter', # pixman.c
  '-Wno-zero-length-array', # libdrm/i915_drm.h:2472
  '-Wno-varargs', # nouveau.c:332
  '-Wno-excess-initializers', # wayland.c:121
  '-I' + meson.current_build_dir(),
]) + [
  '-D_POSIX_C_SOURCE=200809L', '-D_NETBSD_SOURCE',
  get_option('buildtype') == 'debug' ? '-DENABLE_DEBUG=1' : [],
]

requires = [
  dependency('fontconfig'),
  dependency('pixman-1'),
]
requires_private = [
  dependency('freetype2'),
]

libwld_src = files(
  'buffer.c',
  'buffered_surface.c',
  'color.c',
  'context.c',
  'font.c',
  'renderer.c',
  'surface.c',
  'pixman.c',
)

wayland = dependency('wayland-client', required: get_option('wayland'))
if wayland.found()
  libwld_src += ['wayland.c', 'wayland-shm.c']
  c_args += '-DWITH_WAYLAND_SHM=1'
  requires_private += wayland
endif

drm = dependency('libdrm', required: get_option('drm'))
if drm.found()
  libwld_src += ['drm.c', 'dumb.c']  
  requires_private += drm

  drivers = get_option('drivers')
  foreach driver, src : {
    'nouveau': ['nouveau.c'],
    'intel': ['intel.c', 'intel/batch.c'],
  }
    if not drivers.contains('auto') and not drivers.contains(driver)
      continue
    endif
    dep = dependency('libdrm_' + driver, required: drivers.contains(driver))
    if dep.found()
      libwld_src += src
      c_args += '-DWITH_DRM_@0@=1'.format(driver.to_upper())
      requires_private += dep
    endif
  endforeach
endif

if wayland.found() and drm.found()
  libwld_src += 'wayland-drm.c'
  c_args += '-DWITH_WAYLAND_DRM=1'

  protocols = import('wayland').scan_xml(
    files('protocol/wayland-drm.xml'),
    public: false,
    client: true,
  )
  libwld_src += [ protocols[0], protocols[1] ]
endif

install_headers([
  'wld.h', 'pixman.h',
  drm.found() ? 'drm.h' : [],
  wayland.found() ? 'wayland.h' : [],
], subdir: 'wld')

libwld_lib = library(
	'wld',
	libwld_src,
	dependencies: requires + requires_private,
	c_args: c_args,
	include_directories: inc,
	# Prevents clashing with wld pixman.h
	implicit_include_directories: false,
	install: true,
)

# Allows other meson projects to use libwld
libwld = declare_dependency(
	link_with: libwld_lib,
	dependencies: requires + requires_private,
)

pkg.generate(
  libwld_lib,
  name: 'wld',
  description: 'A primitive drawing library library targeted at Wayland',
  requires: requires,
  requires_private: requires_private,
)

doxygen = find_program('doxygen', required: get_option('doxygen'))
if doxygen.found()
  run_target('doxygen', command: [doxygen])
endif
