/* SPDX-License-Identifier: MIT */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <sys/ucontext.h>
#include <unistd.h>
#define GL_CHECK() do { GLenum e=glGetError(); if(e) { printf("radeon-gl: GL error %04x at line %d\n",e,__LINE__); return 1; } } while(0)
static void crash(int sig, siginfo_t *info, void *context) {
    ucontext_t *uc=context;
    void *pc=(void *)(uintptr_t)uc->uc_mcontext->__ss.__rip;
    Dl_info where={0}; dladdr(pc,&where);
    fprintf(stderr,"radeon-gl: signal %d address %p PC %p image %s base %p symbol %s\n",sig,info->si_addr,pc,where.dli_fname,where.dli_fbase,where.dli_sname);
    void *frames[24]; int n=backtrace(frames,24);
    for(int i=0;i<n;i++) {
        Dl_info d={0}; dladdr(frames[i],&d);
        fprintf(stderr,"radeon-gl: frame %p image %s base %p symbol %s\n",frames[i],d.dli_fname,d.dli_fbase,d.dli_sname);
    }
    _exit(128+sig);
}
static GLuint shader(GLenum type,const char *source) {
    GLuint s=glCreateShader(type); glShaderSource(s,1,&source,NULL); glCompileShader(s);
    GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[4096]; glGetShaderInfoLog(s,sizeof(log),NULL,log); printf("radeon-gl: shader failed %s\n",log); exit(1); }
    return s;
}
static double seconds(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec*1e-9; }
int main(int argc, char **argv) {
    unsigned width=64, height=64;
    if (argc==3) { width=strtoul(argv[1],NULL,10); height=strtoul(argv[2],NULL,10); }
    if ((argc!=1 && argc!=3) || width<16 || height<16 || width>4096 || height>4096) {
        fprintf(stderr,"usage: radeon-gl-smoke [width height] (16..4096)\n"); return 1;
    }
    setvbuf(stdout,NULL,_IONBF,0);
    struct sigaction sa={0}; sa.sa_sigaction=crash; sa.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&sa,NULL); sigaction(SIGBUS,&sa,NULL); sigaction(SIGABRT,&sa,NULL);
    puts("radeon-gl: starting EGL");
    setenv("EGL_PLATFORM","surfaceless",1);
    setenv("MESA_LOADER_DRIVER_OVERRIDE","swrast",1);
    EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    puts("radeon-gl: display acquired");
    EGLint major,minor;
    if(!eglInitialize(display,&major,&minor)) { printf("radeon-gl: eglInitialize error %04x\n",eglGetError()); return 1; }
    puts("radeon-gl: EGL initialized");
    EGLint config_attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig config; EGLint count;
    if(!eglChooseConfig(display,config_attrs,&config,1,&count)||!count) { puts("radeon-gl: no config"); return 1; }
    EGLint context_attrs[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,context_attrs);
    EGLint surface_attrs[]={EGL_WIDTH,width,EGL_HEIGHT,height,EGL_NONE};
    EGLSurface surface=eglCreatePbufferSurface(display,config,surface_attrs);
    if(!eglMakeCurrent(display,surface,surface,context)) { printf("radeon-gl: makeCurrent error %04x\n",eglGetError()); return 1; }
    const char *renderer=(const char *)glGetString(GL_RENDERER);
    printf("radeon-gl: renderer=%s EGL=%d.%d GL=%s\n",renderer,major,minor,glGetString(GL_VERSION));
    if(!renderer || !strstr(renderer,"RV730") || strstr(renderer,"llvmpipe")) { puts("radeon-gl: FAIL hardware renderer"); return 1; }
    GLuint vs=shader(GL_VERTEX_SHADER,"attribute vec2 position; void main(){gl_Position=vec4(position,0.0,1.0);}");
    GLuint fs=shader(GL_FRAGMENT_SHADER,"precision mediump float; void main(){gl_FragColor=vec4(0.25,0.75,0.5,1.0);}");
    GLuint program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs);
    glBindAttribLocation(program,0,"position"); glLinkProgram(program);
    GLint linked=0; glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked) { char log[4096]; glGetProgramInfoLog(program,sizeof(log),NULL,log); printf("radeon-gl: link failed %s\n",log); return 1; }
    glUseProgram(program);
    const GLfloat vertices[]={-0.8f,-0.8f,0.8f,-0.8f,0.0f,0.8f};
    GLuint bo; glGenBuffers(1,&bo); glBindBuffer(GL_ARRAY_BUFFER,bo); glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0); glEnableVertexAttribArray(0);
    glViewport(0,0,width,height); glClearColor(0.0,0.0,1.0,1.0);
    glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLES,0,3); glFinish(); GL_CHECK();
    unsigned char center[4]={0},corner[4]={0};
    glReadPixels(width/2,height*3/8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center);
    glReadPixels(0,height-1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,corner); GL_CHECK();
    printf("radeon-gl: center=%u,%u,%u,%u corner=%u,%u,%u,%u\n",center[0],center[1],center[2],center[3],corner[0],corner[1],corner[2],corner[3]);
    if(center[0]<62||center[0]>66||center[1]<189||center[1]>193||center[2]<126||center[2]>130||center[3]!=255||corner[0]||corner[1]||corner[2]!=255||corner[3]!=255) { puts("radeon-gl: FAIL pixels"); return 1; }
    double start=seconds();
    for(unsigned i=0;i<200;i++) { glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLES,0,3); glFinish(); }
    double elapsed=seconds()-start; GL_CHECK();
    printf("radeon-gl: 200 offscreen %ux%u frames %.3f seconds %.1f fps\n",width,height,elapsed,200.0/elapsed);
    // gpu-bound bandwidth: back to back full-surface clears and full-screen quads, one finish each
    {
        const GLfloat quad[]={-1,-1,1,-1,-1,1,1,-1,1,1,-1,1};
        glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
        glFinish(); start=seconds();
        for(unsigned i=0;i<200;i++) glClear(GL_COLOR_BUFFER_BIT);
        glFinish(); elapsed=seconds()-start;
        printf("radeon-gl: 200 clears %.3f s = %.0f MB/s written\n",elapsed,200.0*width*height*4/elapsed/1e6);
        start=seconds();
        for(unsigned i=0;i<200;i++) glDrawArrays(GL_TRIANGLES,0,6);
        glFinish(); elapsed=seconds()-start;
        printf("radeon-gl: 200 quads %.3f s = %.0f Mpix/s\n",elapsed,200.0*width*height/elapsed/1e6);
        glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    }
    // present readback: cpu glReadPixels over the aperture against the gpu copy into system memory
    {
        int (*gpu_readback)(void *, unsigned)=(int (*)(void *, unsigned))dlsym(RTLD_DEFAULT,"pd_radeon_readback");
        size_t bytes=(size_t)width*height*4;
        uint8_t *a=malloc(bytes), *b=malloc(bytes);
        glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLES,0,3); glFinish();
        start=seconds();
        for(unsigned i=0;i<3;i++) glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,a);
        elapsed=seconds()-start;
        printf("radeon-gl: cpu readback %.1f ms per frame\n",elapsed*1000/3);
        if(gpu_readback) {
            memset(b,0,bytes);
            start=seconds();
            int rc=0;
            for(unsigned i=0;i<20;i++) rc|=gpu_readback(b,width*4);
            elapsed=seconds()-start;
            unsigned bad=0;
            // gpu copy is top row first, bgra. glReadPixels is bottom row first, rgba
            for(unsigned y=0;y<height;y+=7) for(unsigned x=0;x<width;x+=5) {
                const uint8_t *p=a+((size_t)(height-1-y)*width+x)*4, *q=b+((size_t)y*width+x)*4;
                if(p[0]!=q[2]||p[1]!=q[1]||p[2]!=q[0]) bad++;
            }
            printf("radeon-gl: gpu readback rc=%d %.1f ms per frame, %u sampled pixels differ\n",rc,elapsed*1000/20,bad);
        } else puts("radeon-gl: no pd_radeon_readback");
        free(a); free(b);
    }
    // alu bound: a long dependent mad chain per pixel on a small surface, scales with the engine clock only
    {
        GLuint avs=shader(GL_VERTEX_SHADER,"attribute vec2 position; void main(){gl_Position=vec4(position,0.0,1.0);}");
        GLuint afs=shader(GL_FRAGMENT_SHADER,"precision highp float; uniform float k; void main(){vec4 a=vec4(gl_FragCoord.x*0.001,k,0.5,0.25);"
            "for(int i=0;i<512;i++){a=a*vec4(0.999,0.998,0.997,0.996)+vec4(0.001,0.002,0.003,0.004);}"
            "gl_FragColor=vec4(a.x*0.001,a.y,a.z,1.0);}");
        GLuint ap=glCreateProgram(); glAttachShader(ap,avs); glAttachShader(ap,afs);
        glBindAttribLocation(ap,0,"position"); glLinkProgram(ap); glUseProgram(ap);
        const GLfloat q[]={-1,-1,1,-1,-1,1,1,-1,1,1,-1,1};
        glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);
        glViewport(0,0,256,256); glUniform1f(glGetUniformLocation(ap,"k"),0.5f);
        glDrawArrays(GL_TRIANGLES,0,6); glFinish();
        start=seconds();
        for(unsigned i=0;i<20;i++) glDrawArrays(GL_TRIANGLES,0,6);
        glFinish(); elapsed=seconds()-start;
        // 512 iterations of one vec4 mad is 512*4*2 flops per pixel
        printf("radeon-gl: alu 20 draws of 256x256 in %.3f s = %.1f Mpix/s, about %.1f GFLOPS\n",elapsed,20.0*65536/elapsed/1e6,20.0*65536*512*8/elapsed/1e9);
        glViewport(0,0,width,height);
        glUseProgram(program); glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    }
    /* Exercise texture upload, sampler descriptors and GPU texture fetch. */
    GLuint texture_vs=shader(GL_VERTEX_SHADER,"attribute vec2 position; varying vec2 uv; void main(){uv=(position+1.0)*0.5;gl_Position=vec4(position,0.0,1.0);}");
    GLuint texture_fs=shader(GL_FRAGMENT_SHADER,"precision mediump float; varying vec2 uv; uniform sampler2D image; void main(){gl_FragColor=texture2D(image,uv);}");
    GLuint texture_program=glCreateProgram();
    glAttachShader(texture_program,texture_vs); glAttachShader(texture_program,texture_fs);
    glBindAttribLocation(texture_program,0,"position"); glLinkProgram(texture_program);
    glGetProgramiv(texture_program,GL_LINK_STATUS,&linked);
    if(!linked) { puts("radeon-gl: FAIL texture program link"); return 1; }
    glUseProgram(texture_program); glUniform1i(glGetUniformLocation(texture_program,"image"),0);
    GLuint texture; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    const unsigned char texels[]={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLES,0,3); glFinish();
    glReadPixels(width/2,height*3/8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center); GL_CHECK();
    printf("radeon-gl: texture sample=%u,%u,%u,%u\n",center[0],center[1],center[2],center[3]);
    if(center[0]||center[1]!=255||center[2]||center[3]!=255) { puts("radeon-gl: FAIL texture sample"); return 1; }
    glDeleteTextures(1,&texture); glDeleteProgram(texture_program);
    glDeleteShader(texture_vs); glDeleteShader(texture_fs);
    glDeleteBuffers(1,&bo); glDeleteProgram(program); glDeleteShader(vs); glDeleteShader(fs);
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroySurface(display,surface); eglDestroyContext(display,context); eglTerminate(display);
    puts("radeon-gl: PASS"); return 0;
}
