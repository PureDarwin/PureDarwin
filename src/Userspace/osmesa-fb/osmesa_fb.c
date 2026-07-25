#include "pdgl_surface.h"

#include <GL/gl.h>
#include <stdio.h>

int
main(void)
{
    pdgl_surface_t surf;
    if (pdgl_surface_create(&surf) != 0)
        return 1;

    printf("osmesa-fb: %dx%d GL_RENDERER=%s\n", surf.width, surf.height,
           (const char *)glGetString(GL_RENDERER));

    /* Keep the triangle undistorted on a non-square panel by widening the ortho
     * box along X in proportion to the aspect ratio. */
    const float aspect = (float)surf.width / (float)surf.height;
    const int frames = 180;

    for (int f = 0; f < frames; f++) {
        glViewport(0, 0, surf.width, surf.height);
        glClearColor(0.05f, 0.06f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glOrtho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
        glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
        glRotatef((float)f * (360.0f / frames), 0.0f, 0.0f, 1.0f);

        glBegin(GL_TRIANGLES);
            glColor3f(1, 0, 0); glVertex2f(-0.6f, -0.5f);
            glColor3f(0, 1, 0); glVertex2f( 0.6f, -0.5f);
            glColor3f(0, 0, 1); glVertex2f( 0.0f,  0.6f);
        glEnd();

        pdgl_surface_swap(&surf);
    }

    printf("osmesa-fb: %d frames presented\n", frames);

    /* Real teardown (exercises OSMesaDestroyContext + the thread_local/atexit
     * path the allocator fix should have healed). The GOP framebuffer keeps the
     * last frame on screen after we exit. */
    pdgl_surface_destroy(&surf);
    return 0;
}
