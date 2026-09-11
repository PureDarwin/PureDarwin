#import "O2Context_builtin_FT.h"
#import <Onyx2D/O2GraphicsState.h>
#import <Onyx2D/O2Image.h>
#import "O2Font_FT.h"
#import <Onyx2D/O2Paint_color.h>

@implementation O2Context_builtin_FT

+(BOOL)canInitBackingWithContext:(O2Context *)context deviceDictionary:(NSDictionary *)deviceDictionary {
   return YES;
}

-initWithSurface:(O2Surface *)surface flipped:(BOOL)flipped {
   if([super initWithSurface:surface flipped:flipped]==nil)
    return nil;

   return self;
}

-(void)establishFontStateInDeviceIfDirty {
   O2GState *gState=O2ContextCurrentGState(self);
   
   if(gState->_fontIsDirty){
    O2GStateClearFontIsDirty(gState);
   }
}

static O2Paint *paintFromColor(O2ColorRef color) {
   size_t count = O2ColorGetNumberOfComponents(color);
   const CGFloat *components = O2ColorGetComponents(color);

   if(count==2)
       return [[O2Paint_color alloc] initWithGray:components[0] alpha:components[1] surfaceToPaintTransform:O2AffineTransformIdentity];
   if(count==4)
       return [[O2Paint_color alloc] initWithRed:components[0] green:components[1] blue:components[2] alpha:components[3] surfaceToPaintTransform:O2AffineTransformIdentity];
    
   return [[O2Paint_color alloc] initWithGray:0 alpha:1 surfaceToPaintTransform:O2AffineTransformIdentity];
}

static void applyCoverageToSpan_lRGBA8888_PRE(O2argb8u *dst,unsigned char *coverageSpan,O2argb8u *src,int length){
   int i;
   
   for(i=0;i<length;i++,src++,dst++){
    int coverage=coverageSpan[i];
    int oneMinusCoverage=inverseCoverage(coverage);
    O2argb8u r=*src;
    O2argb8u d=*dst;
    
    *dst=O2argb8uAdd(O2argb8uMultiplyByCoverage(r , coverage) , O2argb8uMultiplyByCoverage(d , oneMinusCoverage));
   }
}


/* A glyph can land partly or wholly outside the surface - a window laying out
 * text before it is sized, or a label running past an edge. The spans were
 * read and written unclipped, which walks off the end of the backing store. */
static void drawFreeTypeBitmap(O2Context_builtin_FT *self,O2Surface *surface,FT_Bitmap *bitmap,int x,int y,O2Paint *paint){
   int            width=bitmap->width;
   int            height=bitmap->rows;
   /* Glyphs are written straight to the surface, so the rasterizer viewport -
    * which is where CGContextClipToRect ends up - has to be applied here too.
    * Without it text ignored its view's clip and drew over whatever sat on
    * top of it. */
   int            clipLeft=MAX(0,self->_vpx);
   int            clipTop=MAX(0,self->_vpy);
   int            clipRight=MIN((int)O2ImageGetWidth((O2ImageRef)surface),
                                self->_vpx+self->_vpwidth);
   int            clipBottom=MIN((int)O2ImageGetHeight((O2ImageRef)surface),
                                 self->_vpy+self->_vpheight);
   int            pitch=(bitmap->pitch>0)?bitmap->pitch:width;
   int            row;
   O2argb8u      *dstBuffer;
   O2argb8u      *srcBuffer;
   unsigned char *coverageRow=bitmap->buffer;

   if(width<=0 || height<=0 || coverageRow==NULL)
    return;

   dstBuffer=__builtin_alloca(width*sizeof(O2argb8u));
   srcBuffer=__builtin_alloca(width*sizeof(O2argb8u));

   if(clipRight<=clipLeft || clipBottom<=clipTop)
    return;

   for(row=0;row<height;row++,y++,coverageRow+=pitch){
    if(y<clipTop || y>=clipBottom)
     continue;

    int leading=(x<clipLeft)?clipLeft-x:0;
    int visible=width-leading;

    if(x+width>clipRight)
     visible-=(x+width)-clipRight;

    if(visible<=0)
     continue;

    int            spanX=x+leading;
    int            length=visible;
    unsigned char *coverage=coverageRow+leading;
    O2argb8u      *dst=dstBuffer;
    O2argb8u      *src=srcBuffer;
    O2argb8u      *direct=surface->_read_argb8u(surface,spanX,y,dst,length);

    if(direct!=NULL)
     dst=direct;

    while(YES){
     int chunk=O2PaintReadSpan_argb8u_PRE(paint,spanX,y,src,length);

     if(chunk<0)
      chunk=-chunk;
     else {
      applyCoverageToSpan_lRGBA8888_PRE(dst,coverage,src,chunk);

      if(direct==NULL)
       O2SurfaceWriteSpan_argb8u_PRE(surface,spanX,y,dst,chunk);
     }
     coverage+=chunk;

     length-=chunk;
     spanX+=chunk;
     src+=chunk;
     dst+=chunk;

     if(length<=0)
      break;
    }
   }
}

-(void)showGlyphs:(const O2Glyph *)glyphs advances:(const O2Size *)advances count:(unsigned)count {
// FIXME: use advances if not NULL

   O2AffineTransform transform=O2ContextGetUserSpaceToDeviceSpaceTransform(self);
   O2GState         *gState=O2ContextCurrentGState(self);
   O2Paint          *paint=paintFromColor(gState->_fillColor);
   // FIXME: _textTransform no longer exists--is this line still needed?
   //transform = O2AffineTransformConcat(gState->_textTransform, transform);
   O2Point           point=O2PointApplyAffineTransform(
   	NSMakePoint(_textMatrix.tx,_textMatrix.ty),transform);

   [self establishFontStateInDeviceIfDirty];

   O2Font_FT *font=(O2Font_FT *)gState->_font;
   FT_Face    face=[font face];

   int        i;
   FT_Error   ftError;
   
   if(face==NULL){
    NSLog(@"face is NULL");
    return;
   }

   FT_GlyphSlot slot=face->glyph;

   if ((ftError = FT_Set_Char_Size(face,0,gState->_pointSize*64,72.0,72.0))) {
    NSLog(@"FT_Set_Char_Size returned %d",ftError);
    return;
   }
    
   for(i=0;i<count;i++){

    ftError=FT_Load_Glyph(face,glyphs[i],FT_LOAD_DEFAULT);
    if(ftError)
     continue; 
      
    ftError=FT_Render_Glyph(face->glyph,FT_RENDER_MODE_NORMAL);
    if(ftError)
     continue;

    drawFreeTypeBitmap(self,_surface,&slot->bitmap,point.x+slot->bitmap_left,point.y-slot->bitmap_top,paint);

    point.x += slot->advance.x >> 6;
   }
   
   O2PaintRelease(paint);
   
   int     glyphAdvances[count];
   O2Float unitsPerEm=O2FontGetUnitsPerEm(font);
   
   O2FontGetGlyphAdvances(font,glyphs,count,glyphAdvances);
   
   O2Float total=0;
   
   for(i=0;i<count;i++)
    total+=glyphAdvances[i];
    
   total=(total/O2FontGetUnitsPerEm(font))*gState->_pointSize;
   
   //FIXME: _textTransform no longer exists--are these lines still needed?
   //O2ContextCurrentGState(self)->_textTransform.tx+=total;
   //O2ContextCurrentGState(self)->_textTransform.ty+=0;
   _textMatrix.tx += total;
}

@end
