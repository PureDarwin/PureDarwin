#import <QuartzCore/CALayerContext.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CARenderer.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <OpenGL/gl.h>
#import <CoreGraphics/CoreGraphics.h>
#include <string.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>
#include <os/lock.h>

@interface CALayer(private)
-(void)_setContext:(CALayerContext *)context;
-(NSNumber *)_textureId;
-(void)_setTextureId:(NSNumber *)value;
@end

// every live context, so a committed transaction reaches them all: an app can redraw an
// IOSurface it already set as contents and only commit, which changes no layer property
#define MAX_LIVE_CONTEXTS 64
static CALayerContext *liveContexts[MAX_LIVE_CONTEXTS];
static os_unfair_lock liveContextsLock=OS_UNFAIR_LOCK_INIT;

@implementation CALayerContext

+(void)_transactionCommitted {
   CALayerContext *contexts[MAX_LIVE_CONTEXTS];
   int count=0;

   os_unfair_lock_lock(&liveContextsLock);
   for(int i=0;i<MAX_LIVE_CONTEXTS;i++)
    if(liveContexts[i]!=nil)
     contexts[count++]=[liveContexts[i] retain];
   os_unfair_lock_unlock(&liveContextsLock);
   for(int i=0;i<count;i++){
    [contexts[i] _layerTreeChanged];
    [contexts[i] release];
   }
}


-initWithFrame:(CGRect)rect {
   CGLError error;
   
   // alpha for the readback over the window, stencil for rounded clipping and masks
   CGLPixelFormatAttribute attributes[]={
    kCGLPFAColorSize,24,
    kCGLPFAAlphaSize,8,
    kCGLPFADepthSize,16,
    kCGLPFAStencilSize,8,
    0,
   };
   GLint numberOfVirtualScreens;
      
   CGLChoosePixelFormat(attributes,&_pixelFormat,&numberOfVirtualScreens);

   if((error=CGLCreateContext(_pixelFormat,NULL,&_glContext))!=kCGLNoError)
    NSLog(@"CGLCreateContext failed with %d in %s %d",error,__FILE__,__LINE__);

   _frame=rect;
   
   GLint width=rect.size.width;
   GLint height=rect.size.height;
   
   GLint backingOrigin[2]={rect.origin.x,rect.origin.y};
   GLint backingSize[2]={width,height};

   CGLSetParameter(_glContext,kCGLCPSurfaceBackingOrigin,backingOrigin);
   CGLSetParameter(_glContext,kCGLCPSurfaceBackingSize,backingSize);
   
   GLint opacity=0;
   CGLSetParameter(_glContext,kCGLCPSurfaceOpacity,&opacity);

   _renderer=[[CARenderer rendererWithCGLContext:_glContext options:nil] retain];
   os_unfair_lock_lock(&liveContextsLock);
   for(int i=0;i<MAX_LIVE_CONTEXTS;i++)
    if(liveContexts[i]==nil){
     liveContexts[i]=self;
     break;
    }
   os_unfair_lock_unlock(&liveContextsLock);
      
   return self;
}

-(void)dealloc {
   os_unfair_lock_lock(&liveContextsLock);
   for(int i=0;i<MAX_LIVE_CONTEXTS;i++)
    if(liveContexts[i]==self)
     liveContexts[i]=nil;
   os_unfair_lock_unlock(&liveContextsLock);
   [_timer invalidate];
   [_timer release];
   [super dealloc];
}

-(void)setFrame:(CGRect)rect {
   _frame=rect;
   
   GLint width=rect.size.width;
   GLint height=rect.size.height;
   
   GLint backingOrigin[2]={rect.origin.x,rect.origin.y};
   GLint backingSize[2]={width,height};

   CGLSetParameter(_glContext,kCGLCPSurfaceBackingOrigin,backingOrigin);
   CGLSetParameter(_glContext,kCGLCPSurfaceBackingSize,backingSize);
}

-(void)setLayer:(CALayer *)layer {
   layer=[layer retain];
   [_layer release];
   _layer=layer;

   [_layer _setContext:self];
   [_renderer setLayer:layer];
}

-(void)invalidate {
   // the view is going away: a redisplay block already queued must not message it
   _delegate=nil;
   os_unfair_lock_lock(&liveContextsLock);
   for(int i=0;i<MAX_LIVE_CONTEXTS;i++)
    if(liveContexts[i]==self)
     liveContexts[i]=nil;
   os_unfair_lock_unlock(&liveContextsLock);
}

-(void)assignTextureIdsToLayerTree:(CALayer *)layer {

   if([layer _textureId]==nil){
    GLuint texture;
    
    glGenTextures(1,&texture);
    [layer _setTextureId:[NSNumber numberWithUnsignedInt:texture]];
   }
   
   for(CALayer *child in layer.sublayers)
    [self assignTextureIdsToLayerTree:child];
}

-(void)renderLayer:(CALayer *)layer {
   @autoreleasepool {
   CGLSetCurrentContext(_glContext);

   glEnable(GL_DEPTH_TEST);
   //glShadeModel(GL_SMOOTH);

   GLint width=_frame.size.width;
   GLint height=_frame.size.height;
   
   glViewport(0,0,width,height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   // deep enough for nested sublayers, each a step nearer than its parent
   glOrtho(0,width,0,height,-1000,1000);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glEnable(GL_TEXTURE_2D);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   
   GLsizei i=0;
   GLuint  deleteIds[[_deleteTextureIds count]];
   
   for(NSNumber *number in _deleteTextureIds)
    deleteIds[i++]=[number unsignedIntValue];
   
   if(i>0)
    glDeleteTextures(i,deleteIds);
   
   [_deleteTextureIds removeAllObjects];
   
   [self assignTextureIdsToLayerTree:layer];
   
   [_renderer render];
   }
}

-(void)render {
   [self renderLayer:_layer];
}

-(void)timer:(NSTimer *)timer {
   [_renderer beginFrameAtTime:CACurrentMediaTime() timeStamp:NULL];

   [self render];

   [_renderer endFrame];
}

-(void)startTimerIfNeeded {
   if(_timer==nil)
    _timer=[[NSTimer scheduledTimerWithTimeInterval:1.0/60.0 target:self selector:@selector(timer:) userInfo:nil repeats:YES] retain];
}

-(void)deleteTextureId:(NSNumber *)textureId {
   [_deleteTextureIds addObject:textureId];
}

-(void)setDelegate:delegate {
   _delegate=delegate;
}

// coalesced so a burst of property changes costs one display
-(void)_layerTreeChanged {
   if(_renderScheduled || _delegate==nil)
    return;
   _renderScheduled=YES;
   [self retain];
   // runs inside a run loop wait, so it drains its own pool (a frame's contents and images)
   dispatch_async(dispatch_get_main_queue(),^{
    @autoreleasepool {
     _renderScheduled=NO;
     if([_delegate respondsToSelector:@selector(layerContextNeedsDisplay:)])
      [_delegate performSelector:@selector(layerContextNeedsDisplay:) withObject:self];
    }
    [self release];
   });
}

static void releasePixels(void *info,const void *data,size_t size){
   free((void *)data);
}

-(CGImageRef)createImage {
   size_t width=_frame.size.width,height=_frame.size.height,row=width*4;
   uint8_t *flipped,*pixels;

   if(width==0 || height==0)
    return NULL;
   CGLSetCurrentContext(_glContext);
   pixels=malloc(row*height);
   flipped=malloc(row*height);
   if(pixels==NULL || flipped==NULL){
    free(pixels);
    free(flipped);
    return NULL;
   }
   glPixelStorei(GL_PACK_ALIGNMENT,1);
   glReadPixels(0,0,width,height,GL_BGRA,GL_UNSIGNED_BYTE,pixels);
   for(size_t y=0;y<height;y++)
    memcpy(flipped+y*row,pixels+(height-1-y)*row,row);
   free(pixels);

   CGDataProviderRef provider=CGDataProviderCreateWithData(NULL,flipped,row*height,releasePixels);
   CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
   CGImageRef image=CGImageCreate(width,height,8,32,row,space,kCGImageAlphaPremultipliedFirst|kCGBitmapByteOrder32Little,provider,NULL,NO,kCGRenderingIntentDefault);

   CGColorSpaceRelease(space);
   CGDataProviderRelease(provider);
   return image;
}

@end

