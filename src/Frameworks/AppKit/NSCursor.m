/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSCursor.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSImage.h>
#import <Foundation/NSNull.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSGraphicsContext.h>
#ifdef WINDOWS
#import <windows.h>
#import <AppKit/Win32Cursor.h>
#import <Foundation/NSPlatform_win32.h>
#endif

id NSPlatformCreateCursorImpWithName(NSString *name) {
   return [[[NSDisplay currentDisplay] cursorWithName:name] retain];
}

id NSPlatformCreateCursorImpWithImage(NSImage *image,NSPoint hotSpot) {
#ifndef WINDOWS
   // premultiplied ARGB in host order, which is what Wayland's ARGB8888 shm format is
   size_t width=[image size].width;
   size_t height=[image size].height;

   if(width==0 || height==0)
    return nil;

   NSMutableData *pixels=[NSMutableData dataWithLength:width*height*4];
   CGColorSpaceRef colorSpace=CGColorSpaceCreateDeviceRGB();
   CGContextRef context=CGBitmapContextCreate([pixels mutableBytes],width,height,8,width*4,colorSpace,kCGImageAlphaPremultipliedFirst|kCGBitmapByteOrder32Little);
   CGColorSpaceRelease(colorSpace);
   if(context==NULL)
    return nil;

   NSAutoreleasePool *pool=[NSAutoreleasePool new];
   NSGraphicsContext *graphicsContext=[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];

   [NSGraphicsContext saveGraphicsState];
   [NSGraphicsContext setCurrentContext:graphicsContext];
   [image drawInRect:NSMakeRect(0,0,width,height) fromRect:NSZeroRect operation:NSCompositeCopy fraction:1.0];
   [NSGraphicsContext restoreGraphicsState];
   [pool release];
   CGContextRelease(context);

   return [[NSDictionary alloc] initWithObjectsAndKeys:pixels,@"pixels",
           [NSNumber numberWithInt:(int)width],@"width",[NSNumber numberWithInt:(int)height],@"height",
           [NSNumber numberWithInt:(int)hotSpot.x],@"hotX",[NSNumber numberWithInt:(int)hotSpot.y],@"hotY",nil];
#else
/// move to the platform files
   size_t width=[image size].width;
   size_t height=[image size].height;

   CGColorSpaceRef    colorSpace=CGColorSpaceCreateDeviceRGB();
   CGContextRef       context=CGBitmapContextCreate(NULL,width,height,8,0,colorSpace,kCGImageAlphaPremultipliedFirst|kCGBitmapByteOrder32Little);
   CGColorSpaceRelease(colorSpace);

   NSAutoreleasePool *pool=[NSAutoreleasePool new];
   NSGraphicsContext *graphicsContext=[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];

   [NSGraphicsContext saveGraphicsState];
   [NSGraphicsContext setCurrentContext:graphicsContext];

   [image drawInRect:NSMakeRect(0,0,width,height) fromRect:NSZeroRect operation:NSCompositeCopy fraction:1.0];

   [NSGraphicsContext restoreGraphicsState];

   [pool release];

   uint8_t *rowBytes=CGBitmapContextGetData(context);
   size_t   bytesPerRow=CGBitmapContextGetBytesPerRow(context);

   HDC displayDC=GetDC(NULL);
   HBITMAP colorBitmap;
   HBITMAP maskBitmap;

   if(NSPlatformGreaterThanOrEqualToWindows2000()){
    // Cursor with alpha channel, no mask. Win2k and above
    BITMAPV5HEADER bi;
    void          *lpBits;
    uint8_t       *dibRowBytes;

    ZeroMemory(&bi,sizeof(BITMAPV5HEADER));
    bi.bV5Size=sizeof(BITMAPV5HEADER);
    bi.bV5Width=width;
    bi.bV5Height=-height;
    bi.bV5Planes=1;
    bi.bV5BitCount=32;
    bi.bV5Compression=BI_BITFIELDS;
    bi.bV5RedMask=0x00FF0000;
    bi.bV5GreenMask=0x0000FF00;
    bi.bV5BlueMask=0x000000FF;
    bi.bV5AlphaMask=0xFF000000;

    colorBitmap=CreateDIBSection(displayDC,(BITMAPINFO *)&bi,DIB_RGB_COLORS,&lpBits,NULL,0);
    dibRowBytes=lpBits;

    maskBitmap=CreateBitmap(width,height,1,1,NULL);
    int row,column;

    for(row=0;row<height;row++,rowBytes+=bytesPerRow,dibRowBytes+=width*4){
     for(column=0;column<width;column++){
      dibRowBytes[column*4]=rowBytes[column*4];
      dibRowBytes[column*4+1]=rowBytes[column*4+1];
      dibRowBytes[column*4+2]=rowBytes[column*4+2];
      dibRowBytes[column*4+3]=rowBytes[column*4+3];
     }
    }

   }
   else {
    // This works for versions lower than 2k, not really needed, but here.
    HDC colorDC=CreateCompatibleDC(displayDC);
    HDC maskDC=CreateCompatibleDC(displayDC);

    colorBitmap=CreateCompatibleBitmap(displayDC,width,height);
    maskBitmap=CreateCompatibleBitmap(displayDC,width,height);


    HBITMAP oldColorBitmap=SelectObject(colorDC,colorBitmap);
    HBITMAP oldMaskBitmap=SelectObject(maskDC,maskBitmap);

    int      row,column;

    for(row=0;row<height;row++,rowBytes+=bytesPerRow){
     for(column=0;column<width;column++){
      uint8_t b=rowBytes[column*4];
      uint8_t g=rowBytes[column*4+1];
      uint8_t r=rowBytes[column*4+2];
      uint8_t a=rowBytes[column*4+3];

      if(a<255){
       SetPixel(colorDC,column,row,RGB(r,g,b));
       SetPixel(maskDC,column,row,RGB(255,255,255));
      }
      else {
       SetPixel(colorDC,column,row,RGB(r,g,b));
       SetPixel(maskDC,column,row,RGB(0,0,0));
      }
     }
    }
    SelectObject(colorDC,oldColorBitmap);
    SelectObject(maskDC,oldMaskBitmap);
    DeleteDC(colorDC);
    DeleteDC(maskDC);

   }

   ReleaseDC(NULL,displayDC);

   CGContextRelease(context);

   ICONINFO iconInfo;

   iconInfo.fIcon=FALSE;
   iconInfo.xHotspot=hotSpot.x;
   iconInfo.yHotspot=hotSpot.y;
   iconInfo.hbmMask=maskBitmap;
   iconInfo.hbmColor=colorBitmap;

   HCURSOR hCursor=CreateIconIndirect(&iconInfo);

   DeleteObject(colorBitmap);
   DeleteObject(maskBitmap);

   return [[Win32Cursor alloc] initWithHCURSOR:hCursor];
#endif
}

void NSPlatformReleaseCursorImp(id object){
   [object release];
}


void NSPlatformSetCursorImp(id object) {
   [[NSDisplay currentDisplay] setCursor:object];
}

@implementation NSCursor

static NSMutableArray *_cursorStack=nil;

+(void)initialize {
   if(self==[NSCursor class]){
    _cursorStack=[[NSMutableArray alloc] init];
   }
}

+(NSCursor *)currentCursor {
   return [[[_cursorStack lastObject] retain] autorelease];
}

+(NSCursor *)currentSystemCursor {
   return [[[_cursorStack lastObject] retain] autorelease];
}

-initWithCoder:(NSCoder *)coder {
   [self dealloc];
   return [NSNull null];
}

-(void)encodeWithCoder:(NSCoder *)coder {
   NSUnimplementedMethod();
}

-initWithName:(NSString *)name {
   _platformCursor=NSPlatformCreateCursorImpWithName(name);
   return self;
}

+(NSCursor *)arrowCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)closedHandCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)contextualMenuCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)crosshairCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)disappearingItemCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)IBeamCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)openHandCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)pointingHandCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeDownCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeLeftCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeLeftRightCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeRightCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeUpCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeUpDownCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)dragCopyCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)dragLinkCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

// the shapes over a window's resize edges and corners, by kWSResize* edge mask
+(NSCursor *)_windowResizeCursorForEdges:(int)edges {
   static NSMutableDictionary *shared=nil;
   NSString *name;

   switch(edges){
    case 1: name=@"top_side"; break;
    case 2: name=@"bottom_side"; break;
    case 4: name=@"left_side"; break;
    case 8: name=@"right_side"; break;
    case 5: name=@"top_left_corner"; break;
    case 6: name=@"bottom_left_corner"; break;
    case 9: name=@"top_right_corner"; break;
    case 10: name=@"bottom_right_corner"; break;
    default: return [self arrowCursor];
   }
   if(shared==nil)
    shared=[[NSMutableDictionary alloc] init];

   NSCursor *cursor=[shared objectForKey:name];
   if(cursor==nil){
    cursor=[[[self alloc] initWithName:name] autorelease];
    [shared setObject:cursor forKey:name];
   }
   return cursor;
}

+(NSCursor *)operationNotAllowedCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(void)hide {
   [[NSDisplay currentDisplay] hideCursor];
}

+(void)unhide {
   [[NSDisplay currentDisplay] unhideCursor];
}

+(void)setHiddenUntilMouseMoves:(BOOL)flag {
   if(flag)
    [self hide];
   else
    [self unhide];
}

-initWithImage:(NSImage *)image foregroundColorHint:(NSColor *)foregroundHint backgroundColorHint:(NSColor *)backgroundHint hotSpot:(NSPoint)hotSpot {
// the hints are unused per doc.s
   return [self initWithImage:image hotSpot:hotSpot];
}

-initWithImage:(NSImage *)image hotSpot:(NSPoint)hotSpot {
   _image=[image retain];
   _hotSpot=hotSpot;
   _platformCursor=NSPlatformCreateCursorImpWithImage(image,hotSpot);
   return self;
}

-(void)dealloc {
   [_image release];
   NSPlatformReleaseCursorImp(_platformCursor);
   [super dealloc];
}

-(NSImage *)image {
   return _image;
}

-(NSPoint)hotSpot {
   return _hotSpot;
}

-(BOOL)isSetOnMouseEntered {
   return _isSetOnMouseEntered;
}

-(BOOL)isSetOnMouseExited {
   return _isSetOnMouseExited;
}

-(void)setOnMouseEntered:(BOOL)value {
   _isSetOnMouseEntered=value;
}

-(void)setOnMouseExited:(BOOL)value {
   _isSetOnMouseExited=value;
}

-(void)mouseEntered:(NSEvent *)event {
   NSUnimplementedMethod();
}

-(void)mouseExited:(NSEvent *)event {
   NSUnimplementedMethod();
}

-(void)pop {
   [[self class] pop];
}

-(void)set {
   if([_cursorStack count])
    [_cursorStack removeLastObject];

   [_cursorStack addObject:self];
   NSPlatformSetCursorImp(_platformCursor);
}

-(void)push {
   [_cursorStack addObject:self];
   NSPlatformSetCursorImp(_platformCursor);
}

+(void)pop {
   if([_cursorStack count]<2)
    return;

   [_cursorStack removeLastObject];

   NSCursor *cursor=[_cursorStack lastObject];

   if(cursor==nil)
       cursor=[NSCursor arrowCursor];

   NSPlatformSetCursorImp(cursor->_platformCursor);
}

@end

// cursors added after the original set, each made by name like the others and drawn from the cursor theme
@implementation NSCursor (ModernCursors)

+ (NSCursor *)_sharedCursorNamed:(NSString *)name
{
    static NSMutableDictionary *cursors;
    NSCursor *cursor;

    if (cursors == nil)
        cursors = [[NSMutableDictionary alloc] init];
    if ((cursor = [cursors objectForKey:name]) == nil) {
        cursor = [[[self alloc] initWithName:name] autorelease];
        [cursors setObject:cursor forKey:name];
    }
    return cursor;
}

+ (NSCursor *)IBeamCursorForVerticalLayout
{
    return [self _sharedCursorNamed:NSStringFromSelector(_cmd)];
}

+ (NSCursor *)busyButClickableCursor
{
    return [self _sharedCursorNamed:NSStringFromSelector(_cmd)];
}

+ (NSCursor *)columnResizeCursor
{
    return [self _sharedCursorNamed:NSStringFromSelector(_cmd)];
}

+ (NSCursor *)rowResizeCursor
{
    return [self _sharedCursorNamed:NSStringFromSelector(_cmd)];
}

+ (NSCursor *)zoomInCursor
{
    return [self _sharedCursorNamed:NSStringFromSelector(_cmd)];
}

+ (NSCursor *)zoomOutCursor
{
    return [self _sharedCursorNamed:NSStringFromSelector(_cmd)];
}

+ (NSCursor *)columnResizeCursorInDirections:(NSUInteger)directions
{
    return [self columnResizeCursor];
}

+ (NSCursor *)rowResizeCursorInDirections:(NSUInteger)directions
{
    return [self rowResizeCursor];
}

// an edge resizes along one axis, a corner along both, whichever directions are allowed
+ (NSCursor *)frameResizeCursorFromPosition:(NSCursorFrameResizePosition)position inDirections:(NSCursorFrameResizeDirections)directions
{
    switch (position) {
    case NSCursorFrameResizePositionTop:
    case NSCursorFrameResizePositionBottom:
        return [self resizeUpDownCursor];
    case NSCursorFrameResizePositionLeft:
    case NSCursorFrameResizePositionRight:
        return [self resizeLeftRightCursor];
    case NSCursorFrameResizePositionTopLeft:
        return [self _sharedCursorNamed:@"frameResizeTopLeftCursor"];
    case NSCursorFrameResizePositionTopRight:
        return [self _sharedCursorNamed:@"frameResizeTopRightCursor"];
    case NSCursorFrameResizePositionBottomLeft:
        return [self _sharedCursorNamed:@"frameResizeBottomLeftCursor"];
    case NSCursorFrameResizePositionBottomRight:
        return [self _sharedCursorNamed:@"frameResizeBottomRightCursor"];
    default:
        return [self arrowCursor];
    }
}

@end
