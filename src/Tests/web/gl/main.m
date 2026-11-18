// main.m - an NSOpenGLView in a web page: a triangle on blue, drawn through CGL on WebGL
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <AppKit/AppKit.h>
#include <OpenGL/gl.h>
#include <stdio.h>

@interface TriangleView : NSOpenGLView {
    BOOL _reported;
}
@end

@implementation TriangleView

- (void)drawRect:(NSRect)rect
{
    [[self openGLContext] makeCurrentContext];
    NSRect bounds = [self bounds];
    glViewport(0, 0, (GLsizei)bounds.size.width, (GLsizei)bounds.size.height);
    glClearColor(0.1f, 0.2f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(-0.8f, -0.8f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(0.8f, -0.8f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex2f(0.0f, 0.8f);
    glEnd();
    glFlush();

    // a headless browser waits for the pixel at the centre, inside the triangle
    if (!_reported) {
        unsigned char centre[4] = { 0 };
        glReadPixels((GLint)bounds.size.width / 2, (GLint)bounds.size.height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centre);
        printf("PD-GL-PIXEL %d %d %d\n", centre[0], centre[1], centre[2]);
        printf("PD-READY gl %s\n", (const char *)glGetString(GL_RENDERER));
        fflush(stdout);
        _reported = YES;
    }
    [[self openGLContext] flushBuffer];
}

@end

@interface GLDelegate : NSObject
@end

@implementation GLDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(80, 120, 400, 300)
                                                   styleMask:NSTitledWindowMask | NSClosableWindowMask
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setTitle:@"OpenGL"];
    NSOpenGLPixelFormatAttribute attrs[] = { NSOpenGLPFADoubleBuffer, NSOpenGLPFADepthSize, 24, 0 };
    NSOpenGLPixelFormat *format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    TriangleView *view = [[TriangleView alloc] initWithFrame:[[window contentView] bounds] pixelFormat:format];
    [window setContentView:view];
    [window makeKeyAndOrderFront:nil];
    [view setNeedsDisplay:YES];
}

@end

int main(int argc, char **argv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    [app setDelegate:[[GLDelegate alloc] init]];
    [app run];
    [pool release];
    return 0;
}
