/* Public desktop-OpenGL entry points, generated the same way es2api generates
 * libGLESv2's. Mesa only builds a desktop GL library as part of the GLX target,
 * which requires X11; this target has no windowing dependency at all.
 *
 * Modelled on src/mesa/glapi/es2api/libgles2_public.c.
 * SPDX-License-Identifier: MIT
 */

#include "glapi/glapi_priv.h"

#if defined(_GLAPI_ENTRY_ARCH_TLS_H)
#include _GLAPI_ENTRY_ARCH_TLS_H
#define MAPI_TMP_STUB_ASM_GCC_NO_HIDDEN
#else
/* C version of the public entries */
#define MAPI_TMP_DEFINES
#define MAPI_TMP_PUBLIC_ENTRIES_NO_HIDDEN
#endif /* defined(_GLAPI_ENTRY_ARCH_TLS_H) */

#include "opengl_glapi_mapi_tmp.h"
