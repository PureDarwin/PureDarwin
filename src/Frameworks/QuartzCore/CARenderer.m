// the GL 2.0 shader entry points the YUV path uses
#define GL_GLEXT_PROTOTYPES 1
#import <QuartzCore/CARenderer.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CAShapeLayer.h>
#import <QuartzCore/CAAnimation.h>
#import <QuartzCore/CAMediaTimingFunction.h>
#import <CoreVideo/CoreVideo.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <Onyx2D/O2Surface.h>
#import <objc/runtime.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

@interface CALayer(private)
-(void)_setContext:(CALayerContext *)context;
-(void)_setTextureId:(NSNumber *)value;
-(NSNumber *)_textureId;
-(uint32_t)_contentsSeed;
@end

@implementation CARenderer

-(CGRect)bounds {
   return _bounds;
}

-(void)setBounds:(CGRect)value {
   _bounds=value;
}

@synthesize layer=_rootLayer;

-initWithCGLContext:(void *)cglContext options:(NSDictionary *)options {
   _cglContext=cglContext;
   _bounds=CGRectZero;
   _rootLayer=nil;
   return self;
}

+(CARenderer *)rendererWithCGLContext:(void *)cglContext options:(NSDictionary *)options {
   return [[[self alloc] initWithCGLContext:cglContext options:options] autorelease];
}

static void startAnimationsInLayer(CALayer *layer,CFTimeInterval currentTime){
   NSArray *keys=[layer animationKeys];
   
   for(NSString *key in keys){
    CAAnimation *check=[layer animationForKey:key];
    
    if([check beginTime]==0.0)
     [check setBeginTime:currentTime];
    if(currentTime>[check beginTime]+[check duration]){
     [layer removeAnimationForKey:key];
    }
    
   }
   
   for(CALayer *child in layer.sublayers)
    startAnimationsInLayer(child,currentTime);
}

-(void)beginFrameAtTime:(CFTimeInterval)currentTime timeStamp:(CVTimeStamp *)timeStamp {
   startAnimationsInLayer(_rootLayer,currentTime);
}

static inline float cubed(float value){
   return value*value*value;
}

static inline float squared(float value){
   return value*value;
}

static float applyMediaTimingFunction(CAMediaTimingFunction *function,float t){
   float result;
   float cp1[2];
   float cp2[2];
   
   [function getControlPointAtIndex:1 values:cp1];
   [function getControlPointAtIndex:2 values:cp2];
   
   double x=cubed(1.0-t)*0.0+3*squared(1-t)*t*cp1[0]+3*(1-t)*squared(t)*cp2[0]+cubed(t)*1.0;
   double y=cubed(1.0-t)*0.0+3*squared(1-t)*t*cp1[1]+3*(1-t)*squared(t)*cp2[1]+cubed(t)*1.0;
   
// this is wrong
   return y;
}

static float mediaTimingScale(CAAnimation *animation,CFTimeInterval currentTime){
   CFTimeInterval begin=[animation beginTime];
   CFTimeInterval duration=[animation duration];
   CFTimeInterval delta=currentTime-begin;
   double         zeroToOne=delta/duration;
   CAMediaTimingFunction *function=[animation timingFunction];
   
   if(function==nil)
    function=[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionDefault];

   return applyMediaTimingFunction(function,zeroToOne);
}

static float interpolateFloatInLayerKey(CALayer *layer,NSString *key,CFTimeInterval currentTime){
   CAAnimation *animation=[layer animationForKey:key];
   
   if(animation==nil)
    return [[layer valueForKey:key] floatValue];
   
   if([animation isKindOfClass:[CABasicAnimation class]]){
    CABasicAnimation *basic=(CABasicAnimation *)animation;
    
    id fromValue=[basic fromValue];
    id toValue=[basic toValue];
    
    if(toValue==nil)
     toValue=[layer valueForKey:key];
    
    float fromFloat=[fromValue floatValue];
    float toFloat=[toValue floatValue];
        
    float        resultFloat;
    double timingScale=mediaTimingScale(animation,currentTime);
    
    resultFloat=fromFloat+(toFloat-fromFloat)*timingScale;
        
    return resultFloat;
   }
   
   return 0;
}

static CGPoint interpolatePointInLayerKey(CALayer *layer,NSString *key,CFTimeInterval currentTime){
   CAAnimation *animation=[layer animationForKey:key];
   
   if(animation==nil)
    return [[layer valueForKey:key] pointValue];
   
   if([animation isKindOfClass:[CABasicAnimation class]]){
    CABasicAnimation *basic=(CABasicAnimation *)animation;
    
    id fromValue=[basic fromValue];
    id toValue=[basic toValue];
    
    if(toValue==nil)
     toValue=[layer valueForKey:key];
    
    CGPoint fromPoint=[fromValue pointValue];
    CGPoint toPoint=[toValue pointValue];
        
    CGPoint        resultPoint;
    double timingScale=mediaTimingScale(animation,currentTime);
    
    resultPoint.x=fromPoint.x+(toPoint.x-fromPoint.x)*timingScale;
    resultPoint.y=fromPoint.y+(toPoint.y-fromPoint.y)*timingScale;
        
    return resultPoint;
   }
   
   return CGPointMake(0,0);
}

static CGRect interpolateRectInLayerKey(CALayer *layer,NSString *key,CFTimeInterval currentTime){
   CAAnimation *animation=[layer animationForKey:key];

   if(animation==nil){
    return [[layer valueForKey:key] rectValue];
   }
   
   if([animation isKindOfClass:[CABasicAnimation class]]){
    CABasicAnimation *basic=(CABasicAnimation *)animation;
    
    id fromValue=[basic fromValue];
    id toValue=[basic toValue];
    
    if(toValue==nil)
     toValue=[layer valueForKey:key];
    
    CGRect fromRect=[fromValue rectValue];
    CGRect toRect=[toValue rectValue];
    
    double timingScale=mediaTimingScale(animation,currentTime);
    
    CGRect        resultRect;
    
    resultRect.origin.x=fromRect.origin.x+(toRect.origin.x-fromRect.origin.x)*timingScale;
    resultRect.origin.y=fromRect.origin.y+(toRect.origin.y-fromRect.origin.y)*timingScale;
    resultRect.size.width=fromRect.size.width+(toRect.size.width-fromRect.size.width)*timingScale;
    resultRect.size.height=fromRect.size.height+(toRect.size.height-fromRect.size.height)*timingScale;
        
    return resultRect;
   }
   
   return CGRectMake(0,0,0,0);
}

static GLint interpolationFromName(NSString *name){
   if(name==kCAFilterLinear)
    return GL_LINEAR;
   else if(name==kCAFilterNearest)
    return GL_NEAREST;
   else if([name isEqualToString:kCAFilterLinear])
    return GL_LINEAR;
   else if([name isEqualToString:kCAFilterNearest])
    return GL_NEAREST;
   else
    return GL_LINEAR;   
}

void CATexImage2DCGImage(CGImageRef image){
    size_t            imageWidth=CGImageGetWidth(image);
    size_t            imageHeight=CGImageGetHeight(image);
    CGBitmapInfo      bitmapInfo=CGImageGetBitmapInfo(image);
   
    CGDataProviderRef provider=CGImageGetDataProvider(image);
    CFDataRef         data=CGDataProviderCopyData(provider);
    const uint8_t    *pixelBytes=CFDataGetBytePtr(data);
   
   
    GLenum glFormat=GL_BGRA_EXT;
    GLenum glType=GL_UNSIGNED_BYTE;
   
    CGImageAlphaInfo alphaInfo=bitmapInfo&kCGBitmapAlphaInfoMask;
    CGBitmapInfo     byteOrder=bitmapInfo&kCGBitmapByteOrderMask;
   
    switch(alphaInfo){
   
        case kCGImageAlphaNone:
            break;

        case kCGImageAlphaPremultipliedLast:
            // default order is component order, the same bytes as 32Big
            if(byteOrder==kO2BitmapByteOrder32Big || byteOrder==kCGBitmapByteOrderDefault){
                glFormat=GL_RGBA;
                glType=GL_UNSIGNED_BYTE;
            }
            break;

        case kCGImageAlphaPremultipliedFirst: // ARGB
            if(byteOrder==kCGBitmapByteOrder32Little){
                glFormat=GL_BGRA_EXT;
                glType=GL_UNSIGNED_BYTE;
            }
            break;

        case kCGImageAlphaLast:
            break;

        case kCGImageAlphaFirst:
            break;

        case kCGImageAlphaNoneSkipLast:
            break;

        case kCGImageAlphaNoneSkipFirst:
            break;

        case kCGImageAlphaOnly:
            break;
    }

    size_t bytesPerRow=CGImageGetBytesPerRow(image);
    BOOL   opaque=(alphaInfo==kCGImageAlphaNone || alphaInfo==kCGImageAlphaNoneSkipFirst || alphaInfo==kCGImageAlphaNoneSkipLast);

    // padding bytes are not alpha, and rows wider than the image need packing, Wine's window surfaces have both
    if(CGImageGetBitsPerPixel(image)==32 && (opaque || bytesPerRow!=imageWidth*4)){
        uint8_t *packed=malloc(imageWidth*imageHeight*4);
        BOOL     first=(alphaInfo==kCGImageAlphaNoneSkipFirst || alphaInfo==kCGImageAlphaPremultipliedFirst || alphaInfo==kCGImageAlphaFirst);
        BOOL     little=(byteOrder==kCGBitmapByteOrder32Little);

        for(size_t y=0;y<imageHeight;y++){
            const uint8_t *src=pixelBytes+y*bytesPerRow;
            uint8_t       *dst=packed+y*imageWidth*4;

            for(size_t x=0;x<imageWidth;x++,src+=4,dst+=4){
                uint8_t r,g,b,a;

                // memory order: little first is B G R A, big first is A R G B, big last is R G B A, little last is A B G R
                if(first && little){ b=src[0]; g=src[1]; r=src[2]; a=src[3]; }
                else if(first){ a=src[0]; r=src[1]; g=src[2]; b=src[3]; }
                else if(little){ a=src[0]; b=src[1]; g=src[2]; r=src[3]; }
                else { r=src[0]; g=src[1]; b=src[2]; a=src[3]; }
                dst[0]=b; dst[1]=g; dst[2]=r; dst[3]=opaque?255:a;
            }
        }
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,imageWidth,imageHeight,0,GL_BGRA_EXT,GL_UNSIGNED_BYTE,packed);
        free(packed);
        CFRelease(data);
        return;
    }

    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,imageWidth,imageHeight,0,glFormat,glType,pixelBytes);
    CFRelease(data);
}


// OpenGL.framework names a view's GL frame with an IOSurface, binding it samples the frame's texture in place
static BOOL bindDrawableSurface(CFTypeRef surface) {
   static int (*bindSurface)(CFTypeRef,int);
   static BOOL looked;

   if(!looked){
    bindSurface=(int (*)(CFTypeRef,int))dlsym(RTLD_DEFAULT,"CGLPDBindSurfaceTexture");
    looked=YES;
   }
   return bindSurface!=NULL && bindSurface(surface,1);
}

// IOSurface is found at run time, so QuartzCore keeps no link dependency on it
static struct {
   CFTypeID (*typeID)(void);
   uint32_t (*seed)(CFTypeRef);
   uint32_t (*pixelFormat)(CFTypeRef);
   void *(*base)(CFTypeRef);
   size_t (*width)(CFTypeRef), (*height)(CFTypeRef), (*rowBytes)(CFTypeRef);
   int (*lock)(CFTypeRef, uint32_t, uint32_t *), (*unlock)(CFTypeRef, uint32_t, uint32_t *);
   size_t (*planeCount)(CFTypeRef);
   void *(*planeBase)(CFTypeRef, size_t);
   size_t (*planeWidth)(CFTypeRef, size_t), (*planeHeight)(CFTypeRef, size_t), (*planeRowBytes)(CFTypeRef, size_t);
   CFTypeRef (*copyValue)(CFTypeRef, CFStringRef);
} surfaceAPI;

static void loadSurfaceAPI(void) {
   void *lib=dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface",RTLD_LAZY|RTLD_GLOBAL);

   if(lib==NULL)
    return;
   surfaceAPI.typeID=dlsym(lib,"IOSurfaceGetTypeID");
   surfaceAPI.seed=dlsym(lib,"IOSurfaceGetSeed");
   surfaceAPI.pixelFormat=dlsym(lib,"IOSurfaceGetPixelFormat");
   surfaceAPI.base=dlsym(lib,"IOSurfaceGetBaseAddress");
   surfaceAPI.width=dlsym(lib,"IOSurfaceGetWidth");
   surfaceAPI.height=dlsym(lib,"IOSurfaceGetHeight");
   surfaceAPI.rowBytes=dlsym(lib,"IOSurfaceGetBytesPerRow");
   surfaceAPI.lock=dlsym(lib,"IOSurfaceLock");
   surfaceAPI.unlock=dlsym(lib,"IOSurfaceUnlock");
   surfaceAPI.planeCount=dlsym(lib,"IOSurfaceGetPlaneCount");
   surfaceAPI.planeBase=dlsym(lib,"IOSurfaceGetBaseAddressOfPlane");
   surfaceAPI.planeWidth=dlsym(lib,"IOSurfaceGetWidthOfPlane");
   surfaceAPI.planeHeight=dlsym(lib,"IOSurfaceGetHeightOfPlane");
   surfaceAPI.planeRowBytes=dlsym(lib,"IOSurfaceGetBytesPerRowOfPlane");
   surfaceAPI.copyValue=dlsym(lib,"IOSurfaceCopyValue");
}

// contents may be an IOSurfaceRef, a CF instance, which only CF's own class can answer CFGetTypeID for
static BOOL contentsIsSurface(id contents) {
   static pthread_once_t once=PTHREAD_ONCE_INIT;
   static Class cfType;

   if(contents==nil)
    return NO;
   pthread_once(&once,loadSurfaceAPI);
   if(cfType==Nil)
    cfType=objc_getClass("__NSCFType");
   return surfaceAPI.typeID!=NULL && cfType!=Nil && [contents isKindOfClass:cfType] &&
          CFGetTypeID((CFTypeRef)contents)==surfaceAPI.typeID();
}

// what each texture last showed, so a surface is uploaded again when its seed or the contents change
static NSMapTable *uploadedContents;
static NSMapTable *uploadedSeeds;

static BOOL textureIsCurrent(GLuint texture,id contents,uint32_t seed) {
   void *key=(void *)(uintptr_t)texture;

   return uploadedContents!=nil && NSMapGet(uploadedContents,key)==(void *)contents &&
          (uintptr_t)NSMapGet(uploadedSeeds,key)==(uintptr_t)seed+1;
}

static void noteTexture(GLuint texture,id contents,uint32_t seed) {
   void *key=(void *)(uintptr_t)texture;

   if(uploadedContents==nil){
    uploadedContents=NSCreateMapTable(NSNonOwnedPointerMapKeyCallBacks,NSNonOwnedPointerMapValueCallBacks,0);
    uploadedSeeds=NSCreateMapTable(NSNonOwnedPointerMapKeyCallBacks,NSNonOwnedPointerMapValueCallBacks,0);
   }
   NSMapInsert(uploadedContents,key,(void *)contents);
   NSMapInsert(uploadedSeeds,key,(void *)((uintptr_t)seed+1));
}

// the biplanar 4:2:0 formats video decoders hand out: 8-bit and 10-bit (in 16-bit samples), video and full range
enum { CASurfaceBGRA=1, CASurfaceYUV8Video, CASurfaceYUV8Full, CASurfaceYUV16Video, CASurfaceYUV16Full };

static int surfaceKind(CFTypeRef surface) {
   switch(surfaceAPI.pixelFormat(surface)){
    case 'BGRA': return CASurfaceBGRA;
    case '420v': return CASurfaceYUV8Video;
    case '420f': return CASurfaceYUV8Full;
    case 'x420': return CASurfaceYUV16Video;
    case 'xf20': return CASurfaceYUV16Full;
   }
   return 0;
}

static BOOL kindIsYUV(int kind) {
   return kind>=CASurfaceYUV8Video;
}

// the CbCr plane of a layer's texture lives in a second texture, keyed by the first
static NSMapTable *chromaTextures;

static GLuint chromaTextureFor(GLuint texture) {
   void *key=(void *)(uintptr_t)texture;
   GLuint chroma;

   if(chromaTextures==nil)
    chromaTextures=NSCreateMapTable(NSNonOwnedPointerMapKeyCallBacks,NSNonOwnedPointerMapValueCallBacks,0);
   chroma=(GLuint)(uintptr_t)NSMapGet(chromaTextures,key);
   if(chroma==0 || glIsTexture(chroma)==GL_FALSE){
    glGenTextures(1,&chroma);
    NSMapInsert(chromaTextures,key,(void *)(uintptr_t)chroma);
   }
   return chroma;
}

static void uploadPlane(CFTypeRef surface,size_t plane,GLenum format,GLenum type,size_t sampleBytes) {
   size_t width=surfaceAPI.planeWidth(surface,plane),height=surfaceAPI.planeHeight(surface,plane);
   size_t rowBytes=surfaceAPI.planeRowBytes(surface,plane),channels=format==GL_LUMINANCE_ALPHA ? 2 : 1;

   glPixelStorei(GL_UNPACK_ALIGNMENT,1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH,(GLint)(rowBytes/(sampleBytes*channels)));
   glTexImage2D(GL_TEXTURE_2D,0,format,(GLsizei)width,(GLsizei)height,0,format,type,surfaceAPI.planeBase(surface,plane));
   glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
   glPixelStorei(GL_UNPACK_ALIGNMENT,4);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}

// BGRA uploads with its own stride, a biplanar surface puts luma in the bound texture and CbCr in its chroma texture
static void CATexImage2DSurface(CFTypeRef surface,GLuint texture) {
   size_t width=surfaceAPI.width(surface),height=surfaceAPI.height(surface),rowBytes=surfaceAPI.rowBytes(surface);
   int kind=surfaceKind(surface);

   if(kind==0 || width==0 || height==0)
    return;
   if(kindIsYUV(kind) && (surfaceAPI.planeCount==NULL || surfaceAPI.planeCount(surface)!=2))
    return;
   surfaceAPI.lock(surface,1,NULL);
   if(kind==CASurfaceBGRA){
    glPixelStorei(GL_UNPACK_ROW_LENGTH,(GLint)(rowBytes/4));
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(GLsizei)width,(GLsizei)height,0,GL_BGRA_EXT,GL_UNSIGNED_BYTE,surfaceAPI.base(surface));
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
   }
   else {
    BOOL wide=kind==CASurfaceYUV16Video || kind==CASurfaceYUV16Full;
    GLenum type=wide ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;
    size_t sampleBytes=wide ? 2 : 1;

    uploadPlane(surface,0,GL_LUMINANCE,type,sampleBytes);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,chromaTextureFor(texture));
    uploadPlane(surface,1,GL_LUMINANCE_ALPHA,type,sampleBytes);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,texture);
   }
   surfaceAPI.unlock(surface,1,NULL);
}

// luma from unit 0, CbCr from unit 1 (L and A of a luminance-alpha texture), out through the layer's opacity
static const char *yuvFragmentSource=
   "uniform sampler2D luma, chroma;\n"
   "uniform vec3 offset, scale;\n"
   "uniform mat3 matrix;\n"
   "void main() {\n"
   "   vec3 yuv=vec3(texture2D(luma,gl_TexCoord[0].st).r,texture2D(chroma,gl_TexCoord[0].st).ra);\n"
   "   gl_FragColor=vec4(clamp(matrix*((yuv-offset)*scale),0.0,1.0),1.0)*gl_Color;\n"
   "}\n";

static GLuint yuvProgram(void) {
   static GLuint program;
   static BOOL failed;
   GLint ok=0;

   if(program!=0 || failed)
    return program;
   GLuint shader=glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(shader,1,&yuvFragmentSource,NULL);
   glCompileShader(shader);
   glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
   if(ok){
    program=glCreateProgram();
    glAttachShader(program,shader);
    glLinkProgram(program);
    glGetProgramiv(program,GL_LINK_STATUS,&ok);
   }
   if(!ok){
    char log[512]="";
    glGetShaderInfoLog(shader,sizeof(log),NULL,log);
    NSLog(@"CARenderer: no YUV shader: %s",log);
    failed=YES;
    program=0;
   }
   return program;
}

// BT.601 below HD unless the surface says, BT.709 from 720 lines up
static void useYUVProgram(CFTypeRef surface,GLuint texture) {
   GLuint program=yuvProgram();
   int kind=surfaceKind(surface);
   BOOL full=kind==CASurfaceYUV8Full || kind==CASurfaceYUV16Full;
   BOOL bt709=surfaceAPI.height(surface)>=720;
   CFTypeRef matrixName=surfaceAPI.copyValue ? surfaceAPI.copyValue(surface,CFSTR("IOSurfaceYCbCrMatrix")) : NULL;

   if(program==0)
    return;
   if(matrixName!=NULL){
    if(CFGetTypeID(matrixName)==CFStringGetTypeID())
     bt709=CFStringFind((CFStringRef)matrixName,CFSTR("709"),0).location!=kCFNotFound;
    CFRelease(matrixName);
   }
   // column-major: R=Y+Vr*Cr, G=Y-Gb*Cb-Gr*Cr, B=Y+Bb*Cb
   float kr=bt709 ? 0.2126f : 0.299f,kb=bt709 ? 0.0722f : 0.114f,kg=1-kr-kb;
   float m[9]={ 1,1,1, 0,-2*(1-kb)*kb/kg,2*(1-kb), 2*(1-kr),-2*(1-kr)*kr/kg,0 };

   glUseProgram(program);
   glUniform1i(glGetUniformLocation(program,"luma"),0);
   glUniform1i(glGetUniformLocation(program,"chroma"),1);
   if(full){
    glUniform3f(glGetUniformLocation(program,"offset"),0,128/255.0f,128/255.0f);
    glUniform3f(glGetUniformLocation(program,"scale"),1,1,1);
   }
   else {
    glUniform3f(glGetUniformLocation(program,"offset"),16/255.0f,128/255.0f,128/255.0f);
    glUniform3f(glGetUniformLocation(program,"scale"),255/219.0f,255/224.0f,255/224.0f);
   }
   glUniformMatrix3fv(glGetUniformLocation(program,"matrix"),1,GL_FALSE,m);
   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D,chromaTextureFor(texture));
   glActiveTexture(GL_TEXTURE0);
}

// a rounded rectangle as a triangle fan around its centre, the masked corners rounded
#define CORNER_SEGMENTS 8
#define SHAPE_VERTICES (2+4*(CORNER_SEGMENTS+1))

static int roundedRectVertices(GLfloat *v,CGFloat w,CGFloat h,CGFloat radius,CACornerMask corners,float z){
   // corner centres counter-clockwise from bottom left, with the mask bit for each
   const struct { CGFloat x,y; CACornerMask bit; double start; } c[4]={
    { 0,0,kCALayerMinXMinYCorner,M_PI },
    { 1,0,kCALayerMaxXMinYCorner,1.5*M_PI },
    { 1,1,kCALayerMaxXMaxYCorner,0 },
    { 0,1,kCALayerMinXMaxYCorner,0.5*M_PI },
   };
   int n=0;

   v[n++]=w/2; v[n++]=h/2; v[n++]=z;
   for(int i=0;i<4;i++){
    CGFloat r=(corners&c[i].bit)?radius:0;
    CGFloat cx=c[i].x?w-r:r;
    CGFloat cy=c[i].y?h-r:r;

    for(int k=0;k<=CORNER_SEGMENTS;k++){
     double a=c[i].start+(M_PI/2)*k/CORNER_SEGMENTS;

     v[n++]=cx+r*cos(a); v[n++]=cy+r*sin(a); v[n++]=z;
    }
   }
   v[n++]=v[3]; v[n++]=v[4]; v[n++]=z;
   return n/3;
}

static void drawShape(CGFloat w,CGFloat h,CGFloat radius,CACornerMask corners,float z){
   GLfloat v[SHAPE_VERTICES*3];
   GLboolean texturing=glIsEnabled(GL_TEXTURE_2D);
   int count=roundedRectVertices(v,w,h,radius,corners,z);

   glDisable(GL_TEXTURE_2D);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glVertexPointer(3,GL_FLOAT,0,v);
   glDrawArrays(GL_TRIANGLE_FAN,0,count);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   if(texturing)
    glEnable(GL_TEXTURE_2D);
}

// premultiplied, through the layer's opacity
static BOOL setColor(CGColorRef color,float opacity){
   const CGFloat *c;
   size_t count;

   if(color==NULL || (c=CGColorGetComponents(color))==NULL)
    return NO;
   count=CGColorGetNumberOfComponents(color);
   CGFloat r=c[0],g=count>=3?c[1]:c[0],b=count>=3?c[2]:c[0],a=c[count-1]*opacity;

   if(a<=0)
    return NO;
   glColor4f(r*a,g*a,b*a,a);
   return YES;
}

// the border is the ring between the outer shape and one inset by the width, drawn as a strip
static void drawBorder(CGFloat w,CGFloat h,CGFloat radius,CACornerMask corners,CGFloat width,float z){
   GLfloat outer[SHAPE_VERTICES*3],inner[SHAPE_VERTICES*3],strip[SHAPE_VERTICES*6];
   GLboolean texturing=glIsEnabled(GL_TEXTURE_2D);
   CGFloat innerRadius=radius>width?radius-width:0;
   int count=roundedRectVertices(outer,w,h,radius,corners,z);

   roundedRectVertices(inner,w-2*width,h-2*width,innerRadius,corners,z);
   // skip the fan centres, pair each outline point with its inset twin
   for(int i=1;i<count;i++){
    strip[(i-1)*6+0]=outer[i*3]; strip[(i-1)*6+1]=outer[i*3+1]; strip[(i-1)*6+2]=z;
    strip[(i-1)*6+3]=inner[i*3]+width; strip[(i-1)*6+4]=inner[i*3+1]+width; strip[(i-1)*6+5]=z;
   }
   glDisable(GL_TEXTURE_2D);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glVertexPointer(3,GL_FLOAT,0,strip);
   glDrawArrays(GL_TRIANGLE_STRIP,0,(count-1)*2);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   if(texturing)
    glEnable(GL_TEXTURE_2D);
}

// clipping nests through the stencil buffer, or a scissor box on targets without one
static int clipDepth;

// how many flipped superlayers enclose the layer being drawn
static int flipDepth;

static void windowRect(CGFloat w,CGFloat h,GLint box[4]){
   GLfloat m[16],p[16];
   GLint view[4];
   double x0=1e9,y0=1e9,x1=-1e9,y1=-1e9;

   glGetFloatv(GL_MODELVIEW_MATRIX,m);
   glGetFloatv(GL_PROJECTION_MATRIX,p);
   glGetIntegerv(GL_VIEWPORT,view);
   for(int i=0;i<4;i++){
    double x=(i&1)?w:0,y=(i&2)?h:0;
    double ex=m[0]*x+m[4]*y+m[12],ey=m[1]*x+m[5]*y+m[13],ez=m[2]*x+m[6]*y+m[14],ew=m[3]*x+m[7]*y+m[15];
    double cx=p[0]*ex+p[4]*ey+p[8]*ez+p[12]*ew,cy=p[1]*ex+p[5]*ey+p[9]*ez+p[13]*ew,cw=p[3]*ex+p[7]*ey+p[11]*ez+p[15]*ew;
    double wx=view[0]+(cx/cw+1)*view[2]/2,wy=view[1]+(cy/cw+1)*view[3]/2;

    x0=MIN(x0,wx); y0=MIN(y0,wy); x1=MAX(x1,wx); y1=MAX(y1,wy);
   }
   box[0]=floor(x0); box[1]=floor(y0); box[2]=ceil(x1)-box[0]; box[3]=ceil(y1)-box[1];
}

static void stencilShape(CGFloat w,CGFloat h,CGFloat radius,CACornerMask corners,float z,GLenum op){
   glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
   glDepthMask(GL_FALSE);
   glStencilFunc(GL_EQUAL,op==GL_INCR?clipDepth:clipDepth+1,0xff);
   // the clip shape lies behind what was drawn before it, so count it even where depth fails
   glStencilOp(GL_KEEP,op,op);
   drawShape(w,h,radius,corners,z);
   glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
   glDepthMask(GL_TRUE);
   glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
}

static BOOL pushClip(CGFloat w,CGFloat h,CGFloat radius,CACornerMask corners,float z,GLint savedScissor[5]){
   GLint bits=0;

   glGetIntegerv(GL_STENCIL_BITS,&bits);
   if(bits>0){
    if(clipDepth==0){
     glClearStencil(0);
     glClear(GL_STENCIL_BUFFER_BIT);
     glEnable(GL_STENCIL_TEST);
    }
    stencilShape(w,h,radius,corners,z,GL_INCR);
    clipDepth++;
    glStencilFunc(GL_EQUAL,clipDepth,0xff);
    return YES;
   }

   GLint box[4];

   savedScissor[4]=glIsEnabled(GL_SCISSOR_TEST);
   glGetIntegerv(GL_SCISSOR_BOX,savedScissor);
   windowRect(w,h,box);
   if(savedScissor[4]){
    GLint x0=MAX(box[0],savedScissor[0]),y0=MAX(box[1],savedScissor[1]);
    GLint x1=MIN(box[0]+box[2],savedScissor[0]+savedScissor[2]),y1=MIN(box[1]+box[3],savedScissor[1]+savedScissor[3]);

    box[0]=x0; box[1]=y0; box[2]=MAX(0,x1-x0); box[3]=MAX(0,y1-y0);
   }
   glEnable(GL_SCISSOR_TEST);
   glScissor(box[0],box[1],box[2],box[3]);
   return NO;
}

static void popClip(BOOL stencil,CGFloat w,CGFloat h,CGFloat radius,CACornerMask corners,float z,GLint savedScissor[5]){
   if(stencil){
    clipDepth--;
    stencilShape(w,h,radius,corners,z,GL_DECR);
    if(clipDepth==0)
     glDisable(GL_STENCIL_TEST);
    else
     glStencilFunc(GL_EQUAL,clipDepth,0xff);
    return;
   }
   glScissor(savedScissor[0],savedScissor[1],savedScissor[2],savedScissor[3]);
   if(!savedScissor[4])
    glDisable(GL_SCISSOR_TEST);
}


// a soft shadow as stacked shapes, each a little smaller and adding to the alpha, under the layer
static void drawShadow(CALayer *layer,CGFloat w,CGFloat h,CGFloat radius,CACornerMask corners,float z,float opacity){
   CGRect shape=CGRectMake(0,0,w,h);
   const CGFloat *c;
   size_t count;
   float alpha=layer.shadowOpacity*opacity;

   if(alpha<=0 || layer.shadowColor==NULL || (c=CGColorGetComponents(layer.shadowColor))==NULL)
    return;
   if(layer.shadowPath!=NULL){
    shape=CGPathGetBoundingBox(layer.shadowPath);
    radius=0;
   }
   count=CGColorGetNumberOfComponents(layer.shadowColor);
   CGFloat r=c[0],g=count>=3?c[1]:c[0],b=count>=3?c[2]:c[0];
   CGFloat blur=MAX(0,layer.shadowRadius);
   int steps=blur>0?MIN(8,(int)ceil(blur/2)+1):1;
   float a=alpha*c[count-1]/steps;
   GLboolean depth=glIsEnabled(GL_DEPTH_TEST);

   glDisable(GL_DEPTH_TEST);
   glPushMatrix();
   glTranslatef(shape.origin.x+layer.shadowOffset.width,shape.origin.y+layer.shadowOffset.height,0);
   for(int i=0;i<steps;i++){
    CGFloat grow=steps>1?blur*(1-(CGFloat)i/(steps-1))-blur/2:0;

    glColor4f(r*a,g*a,b*a,a);
    glPushMatrix();
    glTranslatef(-grow,-grow,0);
    drawShape(shape.size.width+2*grow,shape.size.height+2*grow,MAX(0,radius+grow),corners,z);
    glPopMatrix();
   }
   glPopMatrix();
   if(depth)
    glEnable(GL_DEPTH_TEST);
}

// a shape layer's path flattened into closed contours, curves split into straight segments
#define CURVE_SEGMENTS 8

struct contourBuilder {
   GLfloat *v;
   int count,capacity;
   int *starts;
   int contours,startCapacity;
   CGPoint current,first;
   CGRect bounds;
};

static void contourAdd(struct contourBuilder *b,CGPoint p,float z){
   if(b->count==b->capacity){
    b->capacity=b->capacity ? b->capacity*2 : 64;
    b->v=realloc(b->v,b->capacity*3*sizeof(GLfloat));
   }
   b->v[b->count*3]=p.x; b->v[b->count*3+1]=p.y; b->v[b->count*3+2]=z;
   b->count++;
   b->bounds=CGRectIsNull(b->bounds) ? CGRectMake(p.x,p.y,0,0) : CGRectUnion(b->bounds,CGRectMake(p.x,p.y,0,0));
   b->current=p;
}

static void contourStart(struct contourBuilder *b,CGPoint p,float z){
   if(b->contours==b->startCapacity){
    b->startCapacity=b->startCapacity ? b->startCapacity*2 : 8;
    b->starts=realloc(b->starts,(b->startCapacity+1)*sizeof(int));
   }
   b->starts[b->contours++]=b->count;
   b->first=p;
   contourAdd(b,p,z);
}

struct contourApply {
   struct contourBuilder *builder;
   float z;
};

static void contourElement(void *info,const CGPathElement *e){
   struct contourApply *a=info;
   struct contourBuilder *b=a->builder;
   CGPoint p0=b->current;

   switch(e->type){
    case kCGPathElementMoveToPoint:
     contourStart(b,e->points[0],a->z);
     break;
    case kCGPathElementAddLineToPoint:
     if(b->contours==0) contourStart(b,p0,a->z);
     contourAdd(b,e->points[0],a->z);
     break;
    case kCGPathElementAddQuadCurveToPoint:
     if(b->contours==0) contourStart(b,p0,a->z);
     for(int i=1;i<=CURVE_SEGMENTS;i++){
      CGFloat t=(CGFloat)i/CURVE_SEGMENTS,u=1-t;
      CGPoint c=e->points[0],p=e->points[1];

      contourAdd(b,CGPointMake(u*u*p0.x+2*u*t*c.x+t*t*p.x,u*u*p0.y+2*u*t*c.y+t*t*p.y),a->z);
     }
     break;
    case kCGPathElementAddCurveToPoint:
     if(b->contours==0) contourStart(b,p0,a->z);
     for(int i=1;i<=CURVE_SEGMENTS;i++){
      CGFloat t=(CGFloat)i/CURVE_SEGMENTS,u=1-t;
      CGPoint c1=e->points[0],c2=e->points[1],p=e->points[2];

      contourAdd(b,CGPointMake(u*u*u*p0.x+3*u*u*t*c1.x+3*u*t*t*c2.x+t*t*t*p.x,
                               u*u*u*p0.y+3*u*u*t*c1.y+3*u*t*t*c2.y+t*t*t*p.y),a->z);
     }
     break;
    case kCGPathElementCloseSubpath:
     if(b->contours>0) contourAdd(b,b->first,a->z);
     break;
   }
}

// marks the path's even-odd interior in the stencil's top bit, then moves the clip count where it is set
static void stencilPathCoverage(CGPathRef path,float z,GLint base,GLenum op){
   struct contourBuilder b={ .bounds=CGRectNull };
   struct contourApply a={ &b,z };

   CGPathApply(path,&a,contourElement);
   if(b.contours>0 && !CGRectIsNull(b.bounds)){
    GLfloat quad[12]={
     b.bounds.origin.x,b.bounds.origin.y,z, CGRectGetMaxX(b.bounds),b.bounds.origin.y,z,
     CGRectGetMaxX(b.bounds),CGRectGetMaxY(b.bounds),z, b.bounds.origin.x,CGRectGetMaxY(b.bounds),z,
    };
    GLboolean texturing=glIsEnabled(GL_TEXTURE_2D);

    b.starts[b.contours]=b.count;
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glStencilMask(0x80);
    glStencilFunc(GL_ALWAYS,0,0xff);
    glStencilOp(GL_INVERT,GL_INVERT,GL_INVERT);
    glVertexPointer(3,GL_FLOAT,0,b.v);
    for(int i=0;i<b.contours;i++){
     int n=b.starts[i+1]-b.starts[i];

     if(n>=3)
      glDrawArrays(GL_TRIANGLE_FAN,b.starts[i],n);
    }

    glStencilMask(0xff);
    glStencilFunc(GL_EQUAL,base|0x80,0xff);
    glStencilOp(GL_KEEP,op,op);
    glVertexPointer(3,GL_FLOAT,0,quad);
    glDrawArrays(GL_TRIANGLE_FAN,0,4);

    glStencilMask(0x80);
    glStencilFunc(GL_ALWAYS,0,0xff);
    glStencilOp(GL_ZERO,GL_ZERO,GL_ZERO);
    glDrawArrays(GL_TRIANGLE_FAN,0,4);
    glStencilMask(0xff);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    if(texturing)
     glEnable(GL_TEXTURE_2D);
   }
   free(b.v);
   free(b.starts);
}

// a mask layer limits drawing to where it has coverage: a shape layer's path, otherwise its bounds shape.
// op is GL_INCR to push the clip and GL_DECR to pop it, counted even where the depth test fails
static void drawMaskCoverage(CALayer *mask,float z,GLenum op){
   CGPoint anchor=mask.anchorPoint,position=mask.position;
   CGRect  bounds=mask.bounds;
   CGFloat radius=MIN(mask.cornerRadius,MIN(bounds.size.width,bounds.size.height)/2);
   GLint   base=op==GL_INCR ? clipDepth : clipDepth+1;

   glPushMatrix();
   glTranslatef(position.x-bounds.size.width*anchor.x,position.y-bounds.size.height*anchor.y,0);
   if([mask isKindOfClass:[CAShapeLayer class]] && [(CAShapeLayer *)mask path]!=NULL){
    glTranslatef(-bounds.origin.x,-bounds.origin.y,0);
    stencilPathCoverage([(CAShapeLayer *)mask path],z,base,op);
   }
   else {
    glStencilFunc(GL_EQUAL,base,0xff);
    glStencilOp(GL_KEEP,op,op);
    drawShape(bounds.size.width,bounds.size.height,radius,mask.maskedCorners,z);
   }
   glPopMatrix();
}

-(void)_renderLayer:(CALayer *)layer z:(float)z currentTime:(CFTimeInterval)currentTime {
   if(layer.isHidden)
    return;

   id contents=layer.contents;
   NSNumber *textureId=[layer _textureId];
   GLuint    texture=[textureId unsignedIntValue];
   GLboolean loadPixelData=GL_FALSE;
   
   if(contents!=nil){
    // a generated name is no texture until first bound, so bind before asking or uploading
    if(texture==0 || glIsTexture(texture)==GL_FALSE)
     loadPixelData=GL_TRUE;
    if(texture!=0)
     glBindTexture(GL_TEXTURE_2D,texture);
   }

    BOOL surface=contentsIsSurface(contents);
    uint32_t seed=surface ? surfaceAPI.seed((CFTypeRef)contents) : [layer _contentsSeed];

    // a surface's pixels change under the same object, an image is replaced, either seed asks for a new upload
    if(!loadPixelData && contents!=nil && !textureIsCurrent(texture,contents,seed))
        loadPixelData=GL_TRUE;

    if(loadPixelData){
        if(!surface)
            CATexImage2DCGImage((CGImageRef)contents);
        else if(!bindDrawableSurface((CFTypeRef)contents))
            CATexImage2DSurface((CFTypeRef)contents,texture);
        noteTexture(texture,contents,seed);

        GLint minFilter=interpolationFromName(layer.minificationFilter);
        GLint magFilter=interpolationFromName(layer.magnificationFilter);
   
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minFilter);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,magFilter);

        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
   }
   
   CGPoint anchorPoint=interpolatePointInLayerKey(layer,@"anchorPoint",currentTime);
   CGPoint position=interpolatePointInLayerKey(layer,@"position",currentTime);
   CGRect  bounds=interpolateRectInLayerKey(layer,@"bounds",currentTime);
   float   opacity=interpolateFloatInLayerKey(layer,@"opacity",currentTime);
   CGFloat width=bounds.size.width,height=bounds.size.height;
   CGFloat radius=MIN(layer.cornerRadius,MIN(width,height)/2);
   CACornerMask corners=layer.maskedCorners;
   CGRect  contentsRect=layer.contentsRect;

   z+=layer.zPosition;

   GLfloat textureVertices[4*2];
   GLfloat vertices[4*3];
   
   // contentsRect picks the part of the contents shown, in unit coordinates. Contents stay
   // upright inside a flipped superlayer, so there the rows run the other way
   CGFloat top=contentsRect.origin.y,bottom=contentsRect.origin.y+contentsRect.size.height;

   if(flipDepth&1){
    CGFloat swap=top;

    top=bottom;
    bottom=swap;
   }
   contentsRect.origin.y=top;
   contentsRect.size.height=bottom-top;
   textureVertices[0]=contentsRect.origin.x;
   textureVertices[1]=contentsRect.origin.y+contentsRect.size.height;
   textureVertices[2]=contentsRect.origin.x+contentsRect.size.width;
   textureVertices[3]=contentsRect.origin.y+contentsRect.size.height;
   textureVertices[4]=contentsRect.origin.x;
   textureVertices[5]=contentsRect.origin.y;
   textureVertices[6]=contentsRect.origin.x+contentsRect.size.width;
   textureVertices[7]=contentsRect.origin.y;

   vertices[0]=0;
   vertices[1]=0;
   vertices[2]=z;
   
   vertices[3]=width;
   vertices[4]=0;
   vertices[5]=z;
   
   vertices[6]=0;
   vertices[7]=height;
   vertices[8]=z;

   vertices[9]=width;
   vertices[10]=height;
   vertices[11]=z;

   glPushMatrix();
   // the transform applies about the anchor point, as Core Animation composes it
   CATransform3D transform=[layer transform];

   glTranslatef(position.x,position.y,0);
   if(!CATransform3DIsIdentity(transform))
    glMultMatrixd((const GLdouble *)&transform);
   glTranslatef(-(width*anchorPoint.x),-(height*anchorPoint.y),0);

   if(layer.shadowOpacity>0 && (layer.backgroundColor!=NULL || contents!=nil || layer.shadowPath!=NULL))
    drawShadow(layer,width,height,radius,corners,z,opacity);

   // a mask clips the layer and its sublayers to the mask's shape, through the stencil when there is one
   CALayer *mask=layer.mask;
   GLint   maskBits=0;
   BOOL    masked=NO;

   if(mask!=nil && !mask.isHidden){
    glGetIntegerv(GL_STENCIL_BITS,&maskBits);
    if(maskBits>0){
     if(clipDepth==0){
      glClearStencil(0);
      glClear(GL_STENCIL_BUFFER_BIT);
      glEnable(GL_STENCIL_TEST);
     }
     glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
     glDepthMask(GL_FALSE);
     drawMaskCoverage(mask,z,GL_INCR);
     glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
     glDepthMask(GL_TRUE);
     glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
     clipDepth++;
     glStencilFunc(GL_EQUAL,clipDepth,0xff);
     masked=YES;
    }
   }

   if(setColor(layer.backgroundColor,opacity))
    drawShape(width,height,radius,corners,z);

   // rounded contents are clipped to the corners like the sublayers
   GLint savedScissor[5];
   BOOL  clipping=layer.masksToBounds && width>0 && height>0;
   BOOL  stencil=NO;

   if(clipping)
    stencil=pushClip(width,height,radius,corners,z,savedScissor);

   if(contents!=nil){
    BOOL yuv=surface && kindIsYUV(surfaceKind((CFTypeRef)contents));

    glTexCoordPointer(2, GL_FLOAT, 0, textureVertices);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glColor4f(opacity,opacity,opacity,opacity);
    if(yuv)
     useYUVProgram((CFTypeRef)contents,texture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if(yuv)
     glUseProgram(0);
   }

   // a flipped layer puts its sublayers' origin at the top
   // sublayers see the sublayer transform, about the layer's anchor point
   CATransform3D sublayerTransform=layer.sublayerTransform;
   BOOL sublayerTransformed=!CATransform3DIsIdentity(sublayerTransform);

   if(sublayerTransformed){
    glPushMatrix();
    glTranslatef(width*anchorPoint.x,height*anchorPoint.y,0);
    glMultMatrixd((const GLdouble *)&sublayerTransform);
    glTranslatef(-(width*anchorPoint.x),-(height*anchorPoint.y),0);
   }
   if(layer.isGeometryFlipped){
    flipDepth++;
    glPushMatrix();
    glTranslatef(0,height,0);
    glScalef(1,-1,1);
   }
   for(CALayer *child in layer.sublayers)
    [self _renderLayer:child z:z+1 currentTime:currentTime];
   if(layer.isGeometryFlipped){
    flipDepth--;
    glPopMatrix();
   }
   if(sublayerTransformed)
    glPopMatrix();

   if(clipping)
    popClip(stencil,width,height,radius,corners,z,savedScissor);

   // the border sits above the contents and sublayers
   if(layer.borderWidth>0 && setColor(layer.borderColor,opacity)){
    GLboolean depth=glIsEnabled(GL_DEPTH_TEST);

    glDisable(GL_DEPTH_TEST);
    drawBorder(width,height,radius,corners,MIN(layer.borderWidth,MIN(width,height)/2),z);
    if(depth)
     glEnable(GL_DEPTH_TEST);
   }

   if(masked){
    clipDepth--;
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    glDepthMask(GL_FALSE);
    drawMaskCoverage(mask,z,GL_DECR);
    glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    if(clipDepth==0)
     glDisable(GL_STENCIL_TEST);
    else
     glStencilFunc(GL_EQUAL,clipDepth,0xff);
   }

   glPopMatrix();
}

-(void)render {
   //glMatrixMode(GL_MODELVIEW);                                           
   glLoadIdentity();

   glClearColor(0, 0, 0, 0);
   glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

   glEnable(GL_DEPTH_TEST);
   glDepthFunc(GL_LEQUAL);					

   //glEnable( GL_TEXTURE_2D );
   //glEnableClientState(GL_VERTEX_ARRAY);
   //glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glEnable (GL_BLEND);
   glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
   
   glAlphaFunc ( GL_GREATER, 0 ) ;
   //glEnable ( GL_ALPHA_TEST ) ;
  
   [self _renderLayer:_rootLayer z:0 currentTime:CACurrentMediaTime()];

   glFlush();
}

-(void)endFrame {
}

@end
