# Copyright © 2026 PureDarwin Project
# SPDX-License-Identifier: MIT
#
# libGL without GLX. gen_gldispatch_mapi.py's "opengl" target is libglvnd's
# libOpenGL set: every desktop GL entry point, no GLX. Dispatch goes through
# shared_glapi exactly as libGLESv2's does, so a context made current through
# EGL_OPENGL_API drives these entry points.

opengl_glapi_mapi_tmp_h = custom_target(
  'opengl_glapi_mapi_tmp.h',
  input : ['../new/gen_gldispatch_mapi.py', '../glapi/registry/gl.xml'],
  output : 'opengl_glapi_mapi_tmp.h',
  command : [prog_python, '@INPUT0@', 'opengl', '@INPUT1@'],
  depend_files : glapi_xml_py_deps,
  capture : true,
)

libopengl = shared_library(
  'GL',
  ['libopengl_public.c', opengl_glapi_mapi_tmp_h],
  c_args : [c_msvc_compat_args, gcc_lto_quirk],
  gnu_symbol_visibility : 'hidden',
  link_args : [ld_args_gc_sections],
  include_directories : [inc_mesa, inc_src, inc_include],
  link_with : shared_glapi_lib,
  dependencies : [dep_thread, dep_libdrm, dep_m, dep_dl, idep_mesautilc11],
  soversion : '1',
  version : '1.2.0',
  darwin_versions : '4.0.0',
  name_prefix : [],
  install : true,
)

pkg.generate(
  name : 'gl',
  filebase : 'gl',
  description : 'Mesa desktop OpenGL library (no GLX)',
  version : meson.project_version(),
  libraries : libopengl,
  libraries_private : gl_priv_libs,
)
