// SPDX-License-Identifier: MIT
// CAShapeLayer, a layer that fills and strokes a CGPath
#import <QuartzCore/CAShapeLayer.h>

CAShapeLayerFillRule const kCAFillRuleNonZero = @"non-zero";
CAShapeLayerFillRule const kCAFillRuleEvenOdd = @"even-odd";
CAShapeLayerLineCap const kCALineCapButt = @"butt";
CAShapeLayerLineCap const kCALineCapRound = @"round";
CAShapeLayerLineCap const kCALineCapSquare = @"square";
CAShapeLayerLineJoin const kCALineJoinMiter = @"miter";
CAShapeLayerLineJoin const kCALineJoinRound = @"round";
CAShapeLayerLineJoin const kCALineJoinBevel = @"bevel";

@implementation CAShapeLayer

-init {
   [super init];
   CGColorRef black=CGColorGetConstantColor(kCGColorBlack);

   _fillColor=CGColorRetain(black);
   _fillRule=[kCAFillRuleNonZero copy];
   _lineCap=[kCALineCapButt copy];
   _lineJoin=[kCALineJoinMiter copy];
   _lineWidth=1;
   _miterLimit=10;
   _strokeEnd=1;
   return self;
}

-(void)dealloc {
   CGPathRelease(_path);
   CGColorRelease(_fillColor);
   CGColorRelease(_strokeColor);
   [_fillRule release];
   [_lineCap release];
   [_lineJoin release];
   [_lineDashPattern release];
   [super dealloc];
}

// the path drawn into a bitmap the size of the layer, as the layer's contents
-(void)_redraw {
   CGRect bounds=[self bounds];
   size_t width=(size_t)ceil(bounds.size.width),height=(size_t)ceil(bounds.size.height);

   if(_path==NULL || width==0 || height==0 || (_fillColor==NULL && _strokeColor==NULL)){
    [self setContents:nil];
    return;
   }

   CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
   CGContextRef context=CGBitmapContextCreate(NULL,width,height,8,width*4,space,kCGImageAlphaPremultipliedFirst|kCGBitmapByteOrder32Little);

   CGColorSpaceRelease(space);
   if(context==NULL)
    return;
   CGContextTranslateCTM(context,-bounds.origin.x,-bounds.origin.y);
   if(_fillColor!=NULL){
    CGContextSetFillColorWithColor(context,_fillColor);
    CGContextAddPath(context,_path);
    if([_fillRule isEqualToString:kCAFillRuleEvenOdd])
     CGContextEOFillPath(context);
    else
     CGContextFillPath(context);
   }
   if(_strokeColor!=NULL && _lineWidth>0){
    CGContextSetStrokeColorWithColor(context,_strokeColor);
    CGContextSetLineWidth(context,_lineWidth);
    CGContextSetMiterLimit(context,_miterLimit);
    CGContextSetLineCap(context,[_lineCap isEqualToString:kCALineCapRound] ? kCGLineCapRound :
                                [_lineCap isEqualToString:kCALineCapSquare] ? kCGLineCapSquare : kCGLineCapButt);
    CGContextSetLineJoin(context,[_lineJoin isEqualToString:kCALineJoinRound] ? kCGLineJoinRound :
                                 [_lineJoin isEqualToString:kCALineJoinBevel] ? kCGLineJoinBevel : kCGLineJoinMiter);
    if([_lineDashPattern count]>0){
     NSUInteger count=[_lineDashPattern count];
     CGFloat lengths[count];

     for(NSUInteger i=0;i<count;i++)
      lengths[i]=[[_lineDashPattern objectAtIndex:i] doubleValue];
     CGContextSetLineDash(context,_lineDashPhase,lengths,count);
    }
    CGContextAddPath(context,_path);
    CGContextStrokePath(context);
   }

   CGImageRef image=CGBitmapContextCreateImage(context);

   CGContextRelease(context);
   [self setContents:(id)image];
   CGImageRelease(image);
}

-(void)display {
   [self _redraw];
}

-(CGPathRef)path {
   return _path;
}

-(void)setPath:(CGPathRef)value {
   if(value==_path)
    return;
   CGPathRelease(_path);
   _path=value ? CGPathCreateCopy(value) : NULL;
   [self _redraw];
}

-(CGColorRef)fillColor {
   return _fillColor;
}

-(void)setFillColor:(CGColorRef)value {
   CGColorRetain(value);
   CGColorRelease(_fillColor);
   _fillColor=value;
   [self _redraw];
}

-(CAShapeLayerFillRule)fillRule {
   return _fillRule;
}

-(void)setFillRule:(CAShapeLayerFillRule)value {
   value=[value copy];
   [_fillRule release];
   _fillRule=value;
   [self _redraw];
}

-(CGColorRef)strokeColor {
   return _strokeColor;
}

-(void)setStrokeColor:(CGColorRef)value {
   CGColorRetain(value);
   CGColorRelease(_strokeColor);
   _strokeColor=value;
   [self _redraw];
}

-(CGFloat)strokeStart {
   return _strokeStart;
}

-(void)setStrokeStart:(CGFloat)value {
   _strokeStart=value;
}

-(CGFloat)strokeEnd {
   return _strokeEnd;
}

-(void)setStrokeEnd:(CGFloat)value {
   _strokeEnd=value;
}

-(CGFloat)lineWidth {
   return _lineWidth;
}

-(void)setLineWidth:(CGFloat)value {
   _lineWidth=value;
   [self _redraw];
}

-(CGFloat)miterLimit {
   return _miterLimit;
}

-(void)setMiterLimit:(CGFloat)value {
   _miterLimit=value;
   [self _redraw];
}

-(CAShapeLayerLineCap)lineCap {
   return _lineCap;
}

-(void)setLineCap:(CAShapeLayerLineCap)value {
   value=[value copy];
   [_lineCap release];
   _lineCap=value;
   [self _redraw];
}

-(CAShapeLayerLineJoin)lineJoin {
   return _lineJoin;
}

-(void)setLineJoin:(CAShapeLayerLineJoin)value {
   value=[value copy];
   [_lineJoin release];
   _lineJoin=value;
   [self _redraw];
}

-(CGFloat)lineDashPhase {
   return _lineDashPhase;
}

-(void)setLineDashPhase:(CGFloat)value {
   _lineDashPhase=value;
   [self _redraw];
}

-(NSArray *)lineDashPattern {
   return _lineDashPattern;
}

-(void)setLineDashPattern:(NSArray *)value {
   value=[value copy];
   [_lineDashPattern release];
   _lineDashPattern=value;
   [self _redraw];
}

-(void)setBounds:(CGRect)value {
   [super setBounds:value];
   [self _redraw];
}

@end
