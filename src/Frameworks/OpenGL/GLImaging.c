#include <OpenGL/OpenGL.h>

#ifdef PD_CGL_USE_EGL
#include <EGL/egl.h>
#endif
#include <GL/gl.h>
#include <stddef.h>

/* ARB_imaging entry points Mesa's libGL leaves out of its exports. Apple's
 * OpenGL.framework has them, and Wine's winemac disables its GL driver when one
 * is missing, so they forward to the driver's implementation when it has one. */

typedef void (*pd_copy_color_table_fn)(GLenum, GLenum, GLint, GLint, GLsizei);

static pd_copy_color_table_fn
pd_gl_imaging_lookup(void)
{
#ifdef PD_CGL_USE_EGL
	extern void *pd_cgl_egl_proc(const char *name);
#endif
	static pd_copy_color_table_fn fn;
	static int looked;

	if (!looked) {
#ifdef PD_CGL_USE_EGL
		fn = (pd_copy_color_table_fn)pd_cgl_egl_proc("glCopyColorTable");
#endif
		looked = 1;
	}
	return fn;
}

void
glCopyColorTable(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
	pd_copy_color_table_fn fn = pd_gl_imaging_lookup();

	// a core or ES driver has no colour tables, the call is then a no-op like a disabled extension
	if (fn != NULL && fn != glCopyColorTable)
		fn(target, internalformat, x, y, width);
}
