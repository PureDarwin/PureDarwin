project('swc', 'c',
  version: '0.0',
  meson_version: '>=1.8.0',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=1',
    'default_library=both',
  ],
)

cc = meson.get_compiler('c')
os = host_machine.system()
pkg = import('pkgconfig')
wl = import('wayland')

add_project_arguments(cc.get_supported_arguments([
  '-fvisibility=hidden',

  '-Werror=implicit-function-declaration',
  '-Werror=implicit-int',
  '-Werror=pointer-sign',
  '-Werror=pointer-arith',
  '-Wno-missing-braces',
]) + [
  '-D_GNU_SOURCE', # Required for mkostemp
  get_option('buildtype').startswith('debug') ? '-DENABLE_DEBUG=1' : [],
], language: 'c')

requires = [
  dependency('wayland-server', version: '>=1.6.0'),
]
requires_private = [
  dependency('pixman-1'),
  dependency('wayland-protocols'),
  dependency('wld'),
  dependency('xkbcommon'),
]
platform_deps = []

video_backend = get_option('video')
framebuffer_backend = get_option('fb')
drm = disabler()
if video_backend == 'drm'
  if framebuffer_backend != 'auto'
    error('-Dfb applies only when -Dvideo=fb')
  endif
  drm = dependency('libdrm')
  requires_private += drm
  add_project_arguments('-DENABLE_DRM=1', language: 'c')
elif video_backend == 'fb'
  if framebuffer_backend == 'auto'
    if os == 'linux'
      framebuffer_backend = 'fbdev'
    elif os == 'darwin'
      framebuffer_backend = 'darwin'
    elif os == 'netbsd' or os == 'openbsd'
      framebuffer_backend = 'wsdisplay'
    else
      error('Failed to determine framebuffer interface for ' + os)
    endif
  endif

  if framebuffer_backend == 'darwin'
    if os != 'darwin'
      error('The Darwin framebuffer interface is supported only on Darwin')
    endif
    add_project_arguments('-DENABLE_DARWIN_FB=1', language: 'c')
    darwin_iokit = dependency('appleframeworks', modules: ['IOKit'],
                              required: false)
    if darwin_iokit.found()
      platform_deps += darwin_iokit
    endif
  elif framebuffer_backend == 'fbdev'
    if os != 'linux'
      error('The fbdev framebuffer interface is supported only on Linux')
    endif
    add_project_arguments([
      '-DENABLE_FBDEV=1',
      '-DFBDEV_DEVICE="@0@"'.format(get_option('fbdev-device')),
    ], language: 'c')
  elif framebuffer_backend == 'wsdisplay'
    if os != 'netbsd' and os != 'openbsd'
      error('The wsdisplay framebuffer interface is supported only on NetBSD and OpenBSD')
    endif
    add_project_arguments([
      '-DENABLE_WSDISPLAY=1',
      '-DWSDISPLAY_DEVICE="@0@"'.format(get_option('wsdisplay-device')),
    ], language: 'c')
  endif
endif

if video_backend != 'drm' and get_option('xwayland').enabled()
  error('Xwayland requires the DRM video backend')
endif
xwayland = video_backend == 'drm' \
  ? dependency('xwayland', required: get_option('xwayland')) \
  : disabler()
xwayland_deps = [
  'xcb',
  'xcb-composite',
  'xcb-ewmh',
  'xcb-icccm',
]
if xwayland.found()
  add_project_arguments('-DENABLE_XWAYLAND', language: 'c')
  requires_private += xwayland
  foreach dep : xwayland_deps
    requires_private += dependency(dep, required: xwayland.found())
	endforeach
endif

input_backend = get_option('input')
libinput = dependency('libinput', version: '>=0.4', required: input_backend == 'libinput')

if input_backend == 'auto'
  if os == 'linux'
    if not libinput.found()
      error('libinput is required for the Linux input backend')
    endif
    input_backend = 'libinput'
  elif os == 'netbsd' or os == 'openbsd'
    input_backend = 'wscons'
  elif os == 'darwin'
    input_backend = 'darwin'
  else
    error('Failed to determine input backend for ' + os)
  endif
endif  

if input_backend == 'darwin'
  if os != 'darwin'
    error('The Darwin input backend is supported only on Darwin')
  endif
  add_project_arguments('-DENABLE_DARWIN=1', language: 'c')
  darwin_frameworks = dependency('appleframeworks',
    modules: ['CoreFoundation', 'IOKit'], required: false)
  if darwin_frameworks.found()
    platform_deps += darwin_frameworks
  endif
endif

if input_backend == 'libinput'
  udev = dependency('libudev', required: get_option('udev'))
  if udev.found()
    add_project_arguments('-DENABLE_LIBUDEV', language: 'c')
  endif
  requires_private += [libinput, udev]
endif

subdir('protocol')
subdir('cursor')
subdir('launch')
subdir('libswc')

if get_option('extra')
  subdir('extra')
endif

if get_option('example')
  subdir('example')
endif

summary({
  'Xwayland support': xwayland.found(),
  'Input backend': input_backend,
  'Video backend': video_backend,
  'Framebuffer interface': video_backend == 'fb' ? framebuffer_backend : 'n/a',
}, bool_yn: true)
