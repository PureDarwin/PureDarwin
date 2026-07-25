#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static GLfloat view_rotx = 20.0f, view_roty = 30.0f, view_rotz = 0.0f;
static GLint   gear1, gear2, gear3;
static GLfloat angle = 0.0f;

/* Draw a gear wheel as a display list (standard glxgears geometry). */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth)
{
    GLint i;
    GLfloat r0, r1, r2, a, da, u, v, len;
    const GLfloat PI = 3.14159265f;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0f;
    r2 = outer_radius + tooth_depth / 2.0f;
    da = 2.0f * PI / teeth / 4.0f;

    glShadeModel(GL_FLAT);
    glNormal3f(0.0f, 0.0f, 1.0f);

    /* front face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        a = i * 2.0f * PI / teeth;
        glVertex3f(r0 * cosf(a), r0 * sinf(a), width * 0.5f);
        glVertex3f(r1 * cosf(a), r1 * sinf(a), width * 0.5f);
        if (i < teeth) {
            glVertex3f(r0 * cosf(a), r0 * sinf(a), width * 0.5f);
            glVertex3f(r1 * cosf(a + 3 * da), r1 * sinf(a + 3 * da), width * 0.5f);
        }
    }
    glEnd();

    /* front sides of teeth */
    glBegin(GL_QUADS);
    for (i = 0; i < teeth; i++) {
        a = i * 2.0f * PI / teeth;
        glVertex3f(r1 * cosf(a), r1 * sinf(a), width * 0.5f);
        glVertex3f(r2 * cosf(a + da), r2 * sinf(a + da), width * 0.5f);
        glVertex3f(r2 * cosf(a + 2 * da), r2 * sinf(a + 2 * da), width * 0.5f);
        glVertex3f(r1 * cosf(a + 3 * da), r1 * sinf(a + 3 * da), width * 0.5f);
    }
    glEnd();

    glNormal3f(0.0f, 0.0f, -1.0f);

    /* back face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        a = i * 2.0f * PI / teeth;
        glVertex3f(r1 * cosf(a), r1 * sinf(a), -width * 0.5f);
        glVertex3f(r0 * cosf(a), r0 * sinf(a), -width * 0.5f);
        if (i < teeth) {
            glVertex3f(r1 * cosf(a + 3 * da), r1 * sinf(a + 3 * da), -width * 0.5f);
            glVertex3f(r0 * cosf(a), r0 * sinf(a), -width * 0.5f);
        }
    }
    glEnd();

    /* back sides of teeth */
    glBegin(GL_QUADS);
    for (i = 0; i < teeth; i++) {
        a = i * 2.0f * PI / teeth;
        glVertex3f(r1 * cosf(a + 3 * da), r1 * sinf(a + 3 * da), -width * 0.5f);
        glVertex3f(r2 * cosf(a + 2 * da), r2 * sinf(a + 2 * da), -width * 0.5f);
        glVertex3f(r2 * cosf(a + da), r2 * sinf(a + da), -width * 0.5f);
        glVertex3f(r1 * cosf(a), r1 * sinf(a), -width * 0.5f);
    }
    glEnd();

    /* outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < teeth; i++) {
        a = i * 2.0f * PI / teeth;
        glVertex3f(r1 * cosf(a), r1 * sinf(a), width * 0.5f);
        glVertex3f(r1 * cosf(a), r1 * sinf(a), -width * 0.5f);
        u = r2 * cosf(a + da) - r1 * cosf(a);
        v = r2 * sinf(a + da) - r1 * sinf(a);
        len = sqrtf(u * u + v * v);
        u /= len; v /= len;
        glNormal3f(v, -u, 0.0f);
        glVertex3f(r2 * cosf(a + da), r2 * sinf(a + da), width * 0.5f);
        glVertex3f(r2 * cosf(a + da), r2 * sinf(a + da), -width * 0.5f);
        glNormal3f(cosf(a), sinf(a), 0.0f);
        glVertex3f(r2 * cosf(a + 2 * da), r2 * sinf(a + 2 * da), width * 0.5f);
        glVertex3f(r2 * cosf(a + 2 * da), r2 * sinf(a + 2 * da), -width * 0.5f);
        u = r1 * cosf(a + 3 * da) - r2 * cosf(a + 2 * da);
        v = r1 * sinf(a + 3 * da) - r2 * sinf(a + 2 * da);
        glNormal3f(v, -u, 0.0f);
        glVertex3f(r1 * cosf(a + 3 * da), r1 * sinf(a + 3 * da), width * 0.5f);
        glVertex3f(r1 * cosf(a + 3 * da), r1 * sinf(a + 3 * da), -width * 0.5f);
        glNormal3f(cosf(a), sinf(a), 0.0f);
    }
    glVertex3f(r1 * cosf(0.0f), r1 * sinf(0.0f), width * 0.5f);
    glVertex3f(r1 * cosf(0.0f), r1 * sinf(0.0f), -width * 0.5f);
    glEnd();

    glShadeModel(GL_SMOOTH);

    /* inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        a = i * 2.0f * PI / teeth;
        glNormal3f(-cosf(a), -sinf(a), 0.0f);
        glVertex3f(r0 * cosf(a), r0 * sinf(a), -width * 0.5f);
        glVertex3f(r0 * cosf(a), r0 * sinf(a), width * 0.5f);
    }
    glEnd();
}

static void
draw(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glRotatef(view_rotx, 1.0f, 0.0f, 0.0f);
    glRotatef(view_roty, 0.0f, 1.0f, 0.0f);
    glRotatef(view_rotz, 0.0f, 0.0f, 1.0f);

    glPushMatrix();
    glTranslatef(-3.0f, -2.0f, 0.0f);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    glCallList(gear1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3.1f, -2.0f, 0.0f);
    glRotatef(-2.0f * angle - 9.0f, 0.0f, 0.0f, 1.0f);
    glCallList(gear2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3.1f, 4.2f, 0.0f);
    glRotatef(-2.0f * angle - 25.0f, 0.0f, 0.0f, 1.0f);
    glCallList(gear3);
    glPopMatrix();

    glPopMatrix();
}

static void
reshape(int width, int height)
{
    GLfloat h = (GLfloat)height / (GLfloat)width;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -40.0f);
}

static void
init(void)
{
    static const GLfloat pos[4]  = { 5.0f, 5.0f, 10.0f, 0.0f };
    static const GLfloat red[4]  = { 0.8f, 0.1f, 0.0f, 1.0f };
    static const GLfloat green[4]= { 0.0f, 0.8f, 0.2f, 1.0f };
    static const GLfloat blue[4] = { 0.2f, 0.2f, 1.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);

    gear1 = glGenLists(1);
    glNewList(gear1, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
    gear(1.0f, 4.0f, 1.0f, 20, 0.7f);
    glEndList();

    gear2 = glGenLists(1);
    glNewList(gear2, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
    gear(0.5f, 2.0f, 2.0f, 10, 0.7f);
    glEndList();

    gear3 = glGenLists(1);
    glNewList(gear3, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
    gear(1.3f, 2.0f, 0.5f, 10, 0.7f);
    glEndList();

    glEnable(GL_NORMALIZE);
}

static double
now_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

int
main(void)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "glxgears: cannot open display\n"); return 1; }

    int attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 16, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
    if (!vi) { fprintf(stderr, "glxgears: no suitable GLX visual\n"); return 1; }

    Window root = RootWindow(dpy, vi->screen);
    Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof swa);
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;

    const int W = 480, H = 480;
    Window win = XCreateWindow(dpy, root, 0, 0, W, H, 0, vi->depth,
                               InputOutput, vi->visual,
                               CWColormap | CWEventMask, &swa);
    XStoreName(dpy, win, "glxgears (PureDarwin softpipe)");
    XMapWindow(dpy, win);

    GLXContext ctx = glXCreateContext(dpy, vi, NULL, True);
    if (!ctx) { fprintf(stderr, "glxgears: glXCreateContext failed\n"); return 1; }
    glXMakeCurrent(dpy, win, ctx);

    printf("glxgears: GL_RENDERER = %s\n", (const char *)glGetString(GL_RENDERER));
    printf("glxgears: GL_VERSION  = %s\n", (const char *)glGetString(GL_VERSION));

    init();
    reshape(W, H);

    int running = 1, frames = 0;
    double t0 = now_sec(), t_rot = t0;
    while (running) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == ConfigureNotify)
                reshape(ev.xconfigure.width, ev.xconfigure.height);
            else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape) running = 0;
            }
        }

        double t = now_sec();
        angle += 70.0f * (float)(t - t_rot);   /* 70 deg/sec */
        t_rot = t;

        draw();
        glXSwapBuffers(dpy, win);

        frames++;
        if (t - t0 >= 5.0) {
            printf("glxgears: %d frames in %.1f s = %.1f FPS\n",
                   frames, t - t0, frames / (t - t0));
            t0 = t;
            frames = 0;
        }
    }

    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, ctx);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
