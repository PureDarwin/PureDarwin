#import <QuartzCore/CALayer.h>
#import <QuartzCore/CAAnimation.h>
#import <QuartzCore/CALayerContext.h>
#import <QuartzCore/CATransaction.h>
#import <Foundation/NSDictionary.h>
#include <stdio.h>
#include <os/lock.h>
#include <dlfcn.h>
#include <string.h>
#import <objc/runtime.h>
#include <stdlib.h>

// PD_CA_TRACE=1 logs what clients hand the layer tree, for porting apps that draw nothing
static int caTrace(void) {
   static int on=-1;

   if(on<0)
    on=getenv("PD_CA_TRACE")!=NULL;
   return on;
}

// image and offset of a caller, for the trace
static const char *caCaller(void *address) {
   static __thread char text[256];
   Dl_info info;

   if(dladdr(address,&info)==0 || info.dli_fname==NULL)
    return "?";
   snprintf(text,sizeof(text),"%s+0x%lx",strrchr(info.dli_fname,'/') ? strrchr(info.dli_fname,'/')+1 : info.dli_fname,(unsigned long)((char *)address-(char *)info.dli_fbase));
   return text;
}

CADynamicRange const CADynamicRangeAutomatic=@"automatic";
CADynamicRange const CADynamicRangeStandard=@"standard";
CADynamicRange const CADynamicRangeConstrainedHigh=@"constrainedHigh";
CADynamicRange const CADynamicRangeHigh=@"high";

NSString * const kCAFilterLinear=@"linear";
NSString * const kCAFilterNearest=@"nearest";
NSString * const kCAGravityCenter=@"center";
NSString * const kCAGravityTop=@"top";
NSString * const kCAGravityBottom=@"bottom";
NSString * const kCAGravityLeft=@"left";
NSString * const kCAGravityRight=@"right";
NSString * const kCAGravityTopLeft=@"topLeft";
NSString * const kCAGravityTopRight=@"topRight";
NSString * const kCAGravityBottomLeft=@"bottomLeft";
NSString * const kCAGravityBottomRight=@"bottomRight";
NSString * const kCAGravityResize=@"resize";
NSString * const kCAGravityResizeAspect=@"resizeAspect";
NSString * const kCAGravityResizeAspectFill=@"resizeAspectFill";
NSString * const kCAFilterTrilinear=@"trilinear";

@implementation CALayer

+layer {
   return [[[self alloc] init] autorelease];
}


-(CALayerContext *)_context {
   return _context;
}

-(void)_setContext:(CALayerContext *)context {
   if(_context!=context){
    [_context deleteTextureId:_textureId];
    [_textureId release];
    _textureId=nil;
   }
   
   _context=context;
   [_sublayers makeObjectsPerformSelector:@selector(_setContext:) withObject:context];
}

-(CALayer *)superlayer {
   return _superlayer;
}

// clients change sublayers and contents from their compositor threads while the renderer walks
// the tree on the main thread, so swaps are locked and readers get their own reference
static os_unfair_lock treeLock=OS_UNFAIR_LOCK_INIT;

static id lockedSwap(id *slot,id value) {
   id old;

   os_unfair_lock_lock(&treeLock);
   old=*slot;
   *slot=value;
   os_unfair_lock_unlock(&treeLock);
   return old;
}

static id lockedRead(id *slot) {
   id value;

   os_unfair_lock_lock(&treeLock);
   value=[*slot retain];
   os_unfair_lock_unlock(&treeLock);
   return [value autorelease];
}

-(NSArray *)sublayers {
   return lockedRead(&_sublayers);
}

-(void)setSublayers:(NSArray *)sublayers {
   if(caTrace())
    fprintf(stderr,"PD-CA %p setSublayers: %lu from %s\n",self,(unsigned long)[sublayers count],caCaller(__builtin_return_address(0)));
   NSArray *old=lockedSwap(&_sublayers,[sublayers copy]);

   // layers dropped here no longer point back at this one, and leave the context so their textures
   // are deleted on its next render, as removeFromSuperlayer does
   for(CALayer *layer in old)
    if(layer->_superlayer==self && [_sublayers indexOfObjectIdenticalTo:layer]==NSNotFound){
     layer->_superlayer=nil;
     [layer _setContext:nil];
    }
   [old release];
   [_sublayers makeObjectsPerformSelector:@selector(_setSuperLayer:) withObject:self];
   [_sublayers makeObjectsPerformSelector:@selector(_setContext:) withObject:_context];
   [self _setNeedsRender];
}

-(id)delegate {
   return _delegate;
}

-(void)setDelegate:value {
   _delegate=value;
}

-(CGPoint)anchorPoint {
   return _anchorPoint;
}

-(void)setAnchorPoint:(CGPoint)value {
   if(CGPointEqualToPoint(_anchorPoint,value))
    return;
   _anchorPoint=value;
   [self _setNeedsRender];
}

-(CGPoint)position {
   return _position;
}

-(void)setPosition:(CGPoint)value {
   if(CGPointEqualToPoint(_position,value))
    return;
   CAAnimation *animation=[self animationForKey:@"position"];
      
   if(animation==nil && ![CATransaction disableActions]){
    id action=[self actionForKey:@"position"];
    
    if(action!=nil)
     [self addAnimation:action forKey:@"position"];
   }
   
   _position=value;
   [self _setNeedsRender];
}

-(CGRect)bounds {
   return _bounds;
}

-(void)setBounds:(CGRect)value {
   if(caTrace())
    fprintf(stderr,"PD-CA %p setBounds: %g %g %g %g\n",self,value.origin.x,value.origin.y,value.size.width,value.size.height);
   if(CGRectEqualToRect(_bounds,value))
    return;
   CAAnimation *animation=[self animationForKey:@"bounds"];
   
   if(animation==nil && ![CATransaction disableActions]){
    id action=[self actionForKey:@"bounds"];

    if(action!=nil)
     [self addAnimation:action forKey:@"bounds"];
   }
   
   _bounds=value;
   [self _setNeedsRender];
}

-(CGRect)frame {
   CGRect result;
   
   result.size=_bounds.size;
   result.origin.x=_position.x-result.size.width*_anchorPoint.x;
   result.origin.y=_position.y-result.size.height*_anchorPoint.y;
   
   return result;
}

-(void)setFrame:(CGRect)value {

   CGPoint position;
   
   position.x=value.origin.x+value.size.width*_anchorPoint.x;
   position.y=value.origin.y+value.size.height*_anchorPoint.y;
   
   [self setPosition:position];
   
   CGRect bounds=_bounds;

   bounds.size=value.size;
      
   [self setBounds:bounds];
}

-(float)opacity {
   return _opacity;
}

-(void)setOpacity:(float)value {
   CAAnimation *animation=[self animationForKey:@"opacity"];
   
   if(animation==nil && ![CATransaction disableActions]){
    id action=[self actionForKey:@"opacity"];

    if(action!=nil)
     [self addAnimation:action forKey:@"opacity"];
   }

   _opacity=value;
   [self _setNeedsRender];
}

-(BOOL)opaque {
   return _opaque;
}

-(void)setOpaque:(BOOL)value {
   _opaque=value;
}

-(id)contents {
   return lockedRead(&_contents);
}

-(void)setContents:(id)value {
   if(caTrace())
    fprintf(stderr,"PD-CA %p setContents: %s %p from %s\n",self,value ? object_getClassName(value) : "nil",value,caCaller(__builtin_return_address(0)));
   [lockedSwap(&_contents,[value retain]) release];
   __atomic_add_fetch(&_contentsSeed,1,__ATOMIC_RELEASE);
   [self _setNeedsRender];
}

-(CATransform3D)transform {
   return _transform;
}

-(void)setTransform:(CATransform3D)value {
   _transform=value;
   [self _setNeedsRender];
}

-(CATransform3D)sublayerTransform {
   return _sublayerTransform;
}

-(void)setSublayerTransform:(CATransform3D)value {
   _sublayerTransform=value;
   [self _setNeedsRender];
}

-(NSString *)minificationFilter {
   return _minificationFilter;
}

-(void)setMinificationFilter:(NSString *)value {
   value=[value copy];
   [_minificationFilter release];
   _minificationFilter=value;
}

-(NSString *)name {
   return _name;
}

-(void)setName:(NSString *)value {
   value=[value copy];
   [_name release];
   _name=value;
}

-(NSString *)contentsGravity {
   return _contentsGravity ? _contentsGravity : kCAGravityResize;
}

-(void)setContentsGravity:(NSString *)value {
   value=[value copy];
   [_contentsGravity release];
   _contentsGravity=value;
   [self _setNeedsRender];
}

-(BOOL)shouldRasterize {
   return _shouldRasterize;
}

-(void)setShouldRasterize:(BOOL)value {
   _shouldRasterize=value;
}

-(CGFloat)rasterizationScale {
   return _rasterizationScale>0 ? _rasterizationScale : 1;
}

-(void)setRasterizationScale:(CGFloat)value {
   _rasterizationScale=value;
}

-(CGFloat)contentsScale {
   return _contentsScale;
}

-(void)setContentsScale:(CGFloat)value {
   _contentsScale=value;
}

-(NSString *)magnificationFilter {
   return _magnificationFilter;
}

-(void)setMagnificationFilter:(NSString *)value {
   value=[value copy];
   [_magnificationFilter release];
   _magnificationFilter=value;
}

-(BOOL)isHidden {
   return _hidden;
}

-(void)setHidden:(BOOL)value {
   _hidden=value;
   [self _setNeedsRender];
}

-(BOOL)masksToBounds {
   return _masksToBounds;
}

-(void)setMasksToBounds:(BOOL)value {
   _masksToBounds=value;
   [self _setNeedsRender];
}

-(BOOL)isGeometryFlipped {
   return _geometryFlipped;
}

-(void)setGeometryFlipped:(BOOL)value {
   _geometryFlipped=value;
   [self _setNeedsRender];
}

-(BOOL)contentsOpaque {
   return _contentsOpaque;
}

-(void)setContentsOpaque:(BOOL)value {
   _contentsOpaque=value;
}

-(BOOL)isDoubleSided {
   return _doubleSided;
}

-(void)setDoubleSided:(BOOL)value {
   _doubleSided=value;
}

-(CAEdgeAntialiasingMask)edgeAntialiasingMask {
   return _edgeAntialiasingMask;
}

-(void)setEdgeAntialiasingMask:(CAEdgeAntialiasingMask)value {
   _edgeAntialiasingMask=value;
}

-(CGFloat)cornerRadius {
   return _cornerRadius;
}

-(void)setCornerRadius:(CGFloat)value {
   _cornerRadius=value;
   [self _setNeedsRender];
}

-(CACornerMask)maskedCorners {
   return _maskedCorners;
}

-(void)setMaskedCorners:(CACornerMask)value {
   _maskedCorners=value;
   [self _setNeedsRender];
}

-(CGFloat)zPosition {
   return _zPosition;
}

-(void)setZPosition:(CGFloat)value {
   _zPosition=value;
   [self _setNeedsRender];
}

-(CGFloat)borderWidth {
   return _borderWidth;
}

-(void)setBorderWidth:(CGFloat)value {
   _borderWidth=value;
   [self _setNeedsRender];
}

-(CGRect)contentsRect {
   return _contentsRect;
}

-(void)setContentsRect:(CGRect)value {
   _contentsRect=value;
   [self _setNeedsRender];
}

-(CGRect)contentsCenter {
   return _contentsCenter;
}

-(void)setContentsCenter:(CGRect)value {
   _contentsCenter=value;
}

-(CGColorRef)backgroundColor {
   return _backgroundColor;
}

-(void)setBackgroundColor:(CGColorRef)value {
   CGColorRetain(value);
   CGColorRelease(_backgroundColor);
   _backgroundColor=value;
   [self _setNeedsRender];
}

-(CGColorRef)borderColor {
   return _borderColor;
}

-(void)setBorderColor:(CGColorRef)value {
   CGColorRetain(value);
   CGColorRelease(_borderColor);
   _borderColor=value;
   [self _setNeedsRender];
}

-(CALayer *)mask {
   return _mask;
}

-(void)setMask:(CALayer *)value {
   [value retain];
   [_mask release];
   _mask=value;
   [self _setNeedsRender];
}

-(CADynamicRange)preferredDynamicRange {
   return _preferredDynamicRange;
}

-(void)setPreferredDynamicRange:(CADynamicRange)value {
   value=[value copy];
   [_preferredDynamicRange release];
   _preferredDynamicRange=value;
}

-(BOOL)isOpaque {
   return _opaque;
}

-(void)insertSublayer:(CALayer *)layer atIndex:(unsigned)index {
   NSMutableArray *layers=[_sublayers mutableCopy];

   [layer removeFromSuperlayer];
   [layers insertObject:layer atIndex:MIN((NSUInteger)index,[layers count])];
   [self setSublayers:layers];
   [layers release];
   [self _setNeedsRender];
}

-(void)insertSublayer:(CALayer *)layer below:(CALayer *)sibling {
   NSUInteger index=[_sublayers indexOfObjectIdenticalTo:sibling];

   [self insertSublayer:layer atIndex:index==NSNotFound ? 0 : (unsigned)index];
   [self _setNeedsRender];
}

-(void)insertSublayer:(CALayer *)layer above:(CALayer *)sibling {
   NSUInteger index=[_sublayers indexOfObjectIdenticalTo:sibling];

   [self insertSublayer:layer atIndex:index==NSNotFound ? (unsigned)[_sublayers count] : (unsigned)index+1];
   [self _setNeedsRender];
}

-init {
   _superlayer=nil;
   _sublayers=[NSArray new];
   _delegate=nil;
   _anchorPoint=CGPointMake(0.5,0.5);
   _position=CGPointZero;
   _bounds=CGRectZero;
   _opacity=1.0;
   _opaque=YES;
   _contents=nil;
   _transform=CATransform3DIdentity;
   _sublayerTransform=CATransform3DIdentity;
   _minificationFilter=kCAFilterLinear;
   _magnificationFilter=kCAFilterLinear;
   _contentsScale=1.0;
   _animations=[[NSMutableDictionary alloc] init];
   _doubleSided=YES;
   _edgeAntialiasingMask=kCALayerLeftEdge|kCALayerRightEdge|kCALayerBottomEdge|kCALayerTopEdge;
   _maskedCorners=kCALayerMinXMinYCorner|kCALayerMaxXMinYCorner|kCALayerMinXMaxYCorner|kCALayerMaxXMaxYCorner;
   _contentsRect=CGRectMake(0,0,1,1);
   _contentsCenter=CGRectMake(0,0,1,1);
   _shadowColor=CGColorCreateGenericRGB(0,0,0,1);
   _shadowOffset=CGSizeMake(0,-3);
   _shadowRadius=3;
   return self;
}

-(void)dealloc {
   for(CALayer *layer in _sublayers)
    if(layer->_superlayer==self)
     layer->_superlayer=nil;
   [_sublayers release];
   [_animations release];
   [_minificationFilter release];
   [_magnificationFilter release];
   [_contentsGravity release];
   [_name release];
   [_mask release];
   [_preferredDynamicRange release];
   CGColorRelease(_backgroundColor);
   CGColorRelease(_borderColor);
   CGColorRelease(_shadowColor);
   CGPathRelease(_shadowPath);
   [super dealloc];
}

-(void)_setSuperLayer:(CALayer *)parent {
   _superlayer=parent;
}

-(void)_removeSublayer:(CALayer *)child {
   NSMutableArray *layers=[_sublayers mutableCopy];
   [layers removeObjectIdenticalTo:child];
   [self setSublayers:layers];
   [layers release];
   [self _setNeedsRender];
}

-(void)addSublayer:(CALayer *)layer {
   if(caTrace())
    fprintf(stderr,"PD-CA %p addSublayer: %s %p from %s\n",self,object_getClassName(layer),layer,caCaller(__builtin_return_address(0)));
   [self setSublayers:[_sublayers arrayByAddingObject:layer]];
}

-(void)replaceSublayer:(CALayer *)layer with:(CALayer *)other {
   NSMutableArray *layers=[_sublayers mutableCopy];
   NSUInteger      index=[_sublayers indexOfObjectIdenticalTo:layer];
   
   [layers replaceObjectAtIndex:index withObject:other];
   
   [self setSublayers:layers];
   [layers release];
   
   layer->_superlayer=nil;
   [self _setNeedsRender];
}

-(void)display {
   if([_delegate respondsToSelector:@selector(displayLayer:)])
    [_delegate displayLayer:self];
   else {
#if 0


#warning create bitmap context

    [self drawInContext:context];
    _contents=image;
    [self setContents:image];
#endif
}
}

-(void)displayIfNeeded {
}

-(void)drawInContext:(CGContextRef)context {
   if([_delegate respondsToSelector:@selector(drawLayer:inContext:)])
    [_delegate drawLayer:self inContext:context];
}

-(BOOL)needsDisplay {
   return _needsDisplay;
}

-(void)removeFromSuperlayer {
   [_superlayer _removeSublayer:self];
   _superlayer=nil;
   [self _setContext:nil];
}

// a change shows on the next pass of whatever presents the context
-(CGColorRef)shadowColor {
   return _shadowColor;
}

-(void)setShadowColor:(CGColorRef)value {
   CGColorRetain(value);
   CGColorRelease(_shadowColor);
   _shadowColor=value;
   [self _setNeedsRender];
}

-(float)shadowOpacity {
   return _shadowOpacity;
}

-(void)setShadowOpacity:(float)value {
   _shadowOpacity=value;
   [self _setNeedsRender];
}

-(CGSize)shadowOffset {
   return _shadowOffset;
}

-(void)setShadowOffset:(CGSize)value {
   _shadowOffset=value;
   [self _setNeedsRender];
}

-(CGFloat)shadowRadius {
   return _shadowRadius;
}

-(void)setShadowRadius:(CGFloat)value {
   _shadowRadius=value;
   [self _setNeedsRender];
}

-(CGPathRef)shadowPath {
   return _shadowPath;
}

-(void)setShadowPath:(CGPathRef)value {
   CGPathRetain(value);
   CGPathRelease(_shadowPath);
   _shadowPath=value;
   [self _setNeedsRender];
}

// for AppKit putting a view's own drawing into its backing layer during display, which must
// not ask for another display
-(void)_setContentsWithoutRender:(id)value {
   [lockedSwap(&_contents,[value retain]) release];
}

-(void)_setNeedsRender {
   [_context _layerTreeChanged];
}

-(void)setNeedsDisplay {
   _needsDisplay=YES;
}

-(void)setNeedsDisplayInRect:(CGRect)rect {
   _needsDisplay=YES;
}

-(void)addAnimation:(CAAnimation *)animation forKey:(NSString *)key {
   if(_context==nil)
    return;
    
   [_animations setObject:animation forKey:key];
   [_context startTimerIfNeeded];
}

-(CAAnimation *)animationForKey:(NSString *)key {
   return [_animations objectForKey:key];
}

-(void)removeAllAnimations {
   [_animations removeAllObjects];
}

-(void)removeAnimationForKey:(NSString *)key {
   [_animations removeObjectForKey:key];
}

-(NSArray *)animationKeys {
   return [_animations allKeys];
}

-valueForKey:(NSString *)key {
// FIXME: KVC appears broken for structs

   if([key isEqualToString:@"bounds"])
    return [NSValue valueWithRect:_bounds];
   if([key isEqualToString:@"frame"])
    return [NSValue valueWithRect:[self frame]];
    
   return [super valueForKey:key];
}

-(id <CAAction>)actionForKey:(NSString *)key {
   CABasicAnimation *basic=[CABasicAnimation animationWithKeyPath:key];
   
   [basic setFromValue:[self valueForKey:key]];
   
   return basic;
}

-(NSNumber *)_textureId {
   return _textureId;
}

-(uint32_t)_contentsSeed {
   return __atomic_load_n(&_contentsSeed,__ATOMIC_ACQUIRE);
}

-(void)_setTextureId:(NSNumber *)value {
   value=[value copy];
   [_textureId release];
   _textureId=value;
}

@end
