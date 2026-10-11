/* SPDX-License-Identifier: MIT */
/* Mesa renders; IOFramebuffer owns the display and exposes its scanout aperture. */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include "PDRadeonRenderShared.h"

/* Public OSS IOGraphicsFamily IOFramebuffer user-client ABI. */
enum { FB_PIXEL_INFO=1, FB_CURRENT_MODE=2, FB_VRAM=110 };
struct pixel_info {
    uint32_t stride, plane, bpp, type, components, component_bits, masks[16];
    char format[64];
    uint32_t flags, width, height, reserved[2];
};
static volatile sig_atomic_t stopped;
static void stop(int sig) { (void)sig; stopped=1; }
static double seconds(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec*1e-9;
}
static GLuint shader(GLenum type,const char *source) {
    GLuint s=glCreateShader(type); glShaderSource(s,1,&source,NULL); glCompileShader(s);
    GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[2048]; glGetShaderInfoLog(s,sizeof(log),NULL,log); fprintf(stderr,"demo shader: %s\n",log); exit(1); }
    return s;
}
int main(int argc,char **argv) {
    unsigned duration=30;
    if(argc==2) { char *end; unsigned long n=strtoul(argv[1],&end,10);
        if(!*argv[1] || *end || !n || n>3600) return 1;
        duration=(unsigned)n;
    } else if(argc!=1) { fprintf(stderr,"usage: radeon-demo [seconds, 1..3600]\n"); return 1; }
    setvbuf(stdout,NULL,_IONBF,0); signal(SIGINT,stop); signal(SIGTERM,stop);
    int result=1, current=0;
    int (*present)(void)=NULL;
    io_connect_t fb=0;
    EGLDisplay display=EGL_NO_DISPLAY; EGLSurface surface=EGL_NO_SURFACE;
    EGLContext context=EGL_NO_CONTEXT;
    GLuint program=0,vs=0,fs=0,bo=0;
    struct pixel_info info={0};
    io_iterator_t iter=0; io_service_t service;
    if(IOServiceGetMatchingServices(kIOMasterPortDefault,IOServiceMatching("PDRadeonFramebuffer"),&iter)) goto out;
    /* Only the primary, rendering-capable head accepts the render client. */
    while((service=IOIteratorNext(iter))) {
        io_connect_t render=0;
        kern_return_t kr=IOServiceOpen(service,mach_task_self(),PD_RADEON_RENDER_CLIENT,&render);
        if(!kr) { IOServiceClose(render); kr=IOServiceOpen(service,mach_task_self(),0,&fb); }
        IOObjectRelease(service);
        if(!kr && fb) break;
    }
    IOObjectRelease(iter);
    if(!fb) { fprintf(stderr,"demo: no available Radeon framebuffer (run as administrator)\n"); goto out; }
    uint64_t mode[2],query[3]; uint32_t count=2; size_t size=sizeof(info);
    if(IOConnectCallScalarMethod(fb,FB_CURRENT_MODE,NULL,0,mode,&count)) goto out;
    query[0]=mode[0]; query[1]=mode[1]; query[2]=0;
    if(IOConnectCallMethod(fb,FB_PIXEL_INFO,query,3,NULL,0,NULL,NULL,&info,&size)) goto out;
    if(!info.width || !info.height || info.width>4096 || info.height>4096 ||
       info.bpp!=32 || info.stride<(uint64_t)info.width*4 ||
       info.masks[0]!=0xff0000 || info.masks[1]!=0xff00 || info.masks[2]!=0xff) goto out;
    printf("radeon-demo: display %ux%u stride %u; GPU blit to scanout\n",info.width,info.height,info.stride);
    setenv("EGL_PLATFORM","surfaceless",1); setenv("MESA_LOADER_DRIVER_OVERRIDE","swrast",1);
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(!eglInitialize(display,NULL,NULL)) goto out;
    EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig config; EGLint n;
    if(!eglChooseConfig(display,attrs,&config,1,&n)||!n) goto out;
    EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLint pa[]={EGL_WIDTH,info.width,EGL_HEIGHT,info.height,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,ca);
    surface=eglCreatePbufferSurface(display,config,pa);
    if(!eglMakeCurrent(display,surface,surface,context)) goto out;
    current=1;
    present=(int (*)(void))dlsym(RTLD_DEFAULT,"pd_radeon_present");
    if(!present) { fprintf(stderr,"demo: Mesa native presentation bridge is missing\n"); goto out; }
    const char *renderer=(const char *)glGetString(GL_RENDERER);
    printf("radeon-demo: renderer=%s\n",renderer?renderer:"unknown");
    if(!renderer || !strstr(renderer,"RV730") || strstr(renderer,"llvmpipe")) goto out;
    vs=shader(GL_VERTEX_SHADER,"attribute vec2 pos; void main(){gl_Position=vec4(pos,0.,1.);}");
    fs=shader(GL_FRAGMENT_SHADER,
        "precision mediump float; uniform vec2 resolution; uniform float time;"
        "float edge(vec2 a,vec2 b,vec2 p){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);}"
        "void main(){vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;"
        "float c=cos(time),s=sin(time);vec2 q=vec2(c*p.x-s*p.y,s*p.x+c*p.y);"
        "vec2 a=vec2(0.,.7),b=vec2(-.65,-.4),d=vec2(.65,-.4);"
        "float inside=step(0.,min(edge(a,b,q),min(edge(b,d,q),edge(d,a,q))));"
        "float wave=.5+.5*sin(12.*length(p)-time*3.);"
        "vec3 bg=vec3(.02,.04,.10)+vec3(.02,.08,.15)*wave;"
        "vec3 color=vec3(edge(b,d,q),edge(d,a,q),edge(a,b,q))/edge(a,b,d);"
        "gl_FragColor=vec4(mix(bg,color,inside),1.);}");
    program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs);
    glBindAttribLocation(program,0,"pos"); glLinkProgram(program);
    GLint linked; glGetProgramiv(program,GL_LINK_STATUS,&linked); if(!linked) goto out;
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program,"resolution"),info.width,info.height);
    GLint time_uniform=glGetUniformLocation(program,"time");
    const GLfloat verts[]={-1,-1,3,-1,-1,3};
    glGenBuffers(1,&bo); glBindBuffer(GL_ARRAY_BUFFER,bo); glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0); glEnableVertexAttribArray(0);
    glViewport(0,0,info.width,info.height);
    int static_frame=getenv("PD_RADEON_STATIC")!=NULL;
    unsigned frames=0; double start=seconds(),report=start;
    puts(static_frame ? "radeon-demo: presenting STATIC triangle and blue waves; GPU idle after first frame" : "radeon-demo: presenting rotating rainbow triangle and blue waves; Ctrl-C stops");
    while(!stopped && seconds()-start<duration) {
        if(static_frame && frames) { sleep(1); continue; }
        glUniform1f(time_uniform,static_frame ? 0.f : (GLfloat)(seconds()-start));
        glDrawArrays(GL_TRIANGLES,0,3);
        if(present()) { fprintf(stderr,"demo: GPU presentation failed\n"); goto out; }
        GLenum error=glGetError();
        if(error) { fprintf(stderr,"demo: GL error %04x\n",error); goto out; }
        frames++;
        double now=seconds();
        if(now-report>=5.) { printf("radeon-demo: %u presented frames, %.1f fps including GPU scanout blit\n",frames,frames/(now-start)); report=now; }
        double remaining=start+frames/60.0-now;
        if(remaining>0.) usleep((useconds_t)(remaining*1e6));
    }
    printf("radeon-demo: PASS %u frames presented in %.3f seconds (%.1f fps)\n",frames,seconds()-start,frames/(seconds()-start));
    result=0;
out:
    if(result) fprintf(stderr,"radeon-demo: failed (EGL %04x)\n",eglGetError());
    if(context!=EGL_NO_CONTEXT) {
        if(current && present) { glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT); present(); }
        if(current) { glDeleteBuffers(1,&bo); glDeleteProgram(program); glDeleteShader(vs); glDeleteShader(fs); }
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        eglDestroyContext(display,context);
    }
    if(surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
    if(display!=EGL_NO_DISPLAY) eglTerminate(display);
    if(fb) IOServiceClose(fb);
    return result;
}
