
#import <Foundation/Foundation.h>
#import <ApplicationServices/ApplicationServices.h>
#import <QuartzCore/CATransform3D.h>
#import <QuartzCore/CAAction.h>
#import <OpenGL/OpenGL.h>

@class CAAnimation, CALayerContext;

enum {
    kCALayerNotSizable = 0x00,
    kCALayerMinXMargin = 0x01,
    kCALayerWidthSizable = 0x02,
    kCALayerMaxXMargin = 0x04,
    kCALayerMinYMargin = 0x08,
    kCALayerHeightSizable = 0x10,
    kCALayerMaxYMargin = 0x20,
};

typedef NS_OPTIONS(unsigned int, CAEdgeAntialiasingMask) {
    kCALayerLeftEdge = 1U << 0,
    kCALayerRightEdge = 1U << 1,
    kCALayerBottomEdge = 1U << 2,
    kCALayerTopEdge = 1U << 3,
};

typedef NS_OPTIONS(NSUInteger, CACornerMask) {
    kCALayerMinXMinYCorner = 1U << 0,
    kCALayerMaxXMinYCorner = 1U << 1,
    kCALayerMinXMaxYCorner = 1U << 2,
    kCALayerMaxXMaxYCorner = 1U << 3,
};

typedef NSString *CADynamicRange;
CA_EXPORT CADynamicRange const CADynamicRangeAutomatic;
CA_EXPORT CADynamicRange const CADynamicRangeStandard;
CA_EXPORT CADynamicRange const CADynamicRangeConstrainedHigh;
CA_EXPORT CADynamicRange const CADynamicRangeHigh;

CA_EXPORT NSString *const kCAFilterLinear;
CA_EXPORT NSString *const kCAFilterNearest;
CA_EXPORT NSString *const kCAFilterTrilinear;

// how contents sit in the bounds (contentsGravity)
CA_EXPORT NSString *const kCAGravityCenter;
CA_EXPORT NSString *const kCAGravityTop;
CA_EXPORT NSString *const kCAGravityBottom;
CA_EXPORT NSString *const kCAGravityLeft;
CA_EXPORT NSString *const kCAGravityRight;
CA_EXPORT NSString *const kCAGravityTopLeft;
CA_EXPORT NSString *const kCAGravityTopRight;
CA_EXPORT NSString *const kCAGravityBottomLeft;
CA_EXPORT NSString *const kCAGravityBottomRight;
CA_EXPORT NSString *const kCAGravityResize;
CA_EXPORT NSString *const kCAGravityResizeAspect;
CA_EXPORT NSString *const kCAGravityResizeAspectFill;

@interface CALayer : NSObject {
    CALayerContext *_context;
    CALayer *_superlayer;
    NSArray *_sublayers;
    id _delegate;
    CGPoint _anchorPoint;
    CGPoint _position;
    CGRect _bounds;
    float _opacity;
    BOOL _opaque;
    id _contents;
    CATransform3D _transform;
    CATransform3D _sublayerTransform;
    NSString *_minificationFilter;
    NSString *_magnificationFilter;
    BOOL _needsDisplay;
    NSMutableDictionary *_animations;
    NSNumber *_textureId;
    CGFloat _contentsScale;
    BOOL _shouldRasterize;
    CGFloat _rasterizationScale;
    NSString *_contentsGravity;
    NSString *_name;
    BOOL _hidden;
    BOOL _masksToBounds;
    BOOL _geometryFlipped;
    BOOL _contentsOpaque;
    BOOL _doubleSided;
    CAEdgeAntialiasingMask _edgeAntialiasingMask;
    CGFloat _cornerRadius;
    CACornerMask _maskedCorners;
    CGFloat _zPosition;
    CGFloat _borderWidth;
    CGRect _contentsRect;
    CGRect _contentsCenter;
    CGColorRef _backgroundColor;
    CGColorRef _borderColor;
    CALayer *_mask;
    CADynamicRange _preferredDynamicRange;
    CGColorRef _shadowColor;
    float _shadowOpacity;
    CGSize _shadowOffset;
    CGFloat _shadowRadius;
    CGPathRef _shadowPath;
}

+ layer;

@property(readonly) CALayer *superlayer;
@property(copy) NSArray *sublayers;
@property(assign) id delegate;
@property CGPoint anchorPoint;
@property CGPoint position;
@property CGRect bounds;
@property CGRect frame;
@property float opacity;
@property BOOL opaque;
@property(retain) id contents;
@property CATransform3D transform;
@property CATransform3D sublayerTransform;

@property(copy) NSString *minificationFilter;
@property(copy) NSString *magnificationFilter;
// backing pixels per point, stored only, the renderer draws at 1
@property CGFloat contentsScale;
// rasterizing is a caching hint, PD's renderer draws the layer tree every frame either way
@property BOOL shouldRasterize;
@property CGFloat rasterizationScale;
// stored for the app: the renderer places contents at the origin
@property(copy) NSString *contentsGravity;
@property(copy) NSString *name;
@property (getter=isHidden) BOOL hidden;
@property BOOL masksToBounds;
@property (getter=isGeometryFlipped) BOOL geometryFlipped;
@property BOOL contentsOpaque;
@property (getter=isDoubleSided) BOOL doubleSided;
@property CAEdgeAntialiasingMask edgeAntialiasingMask;
@property CGFloat cornerRadius;
@property CACornerMask maskedCorners;
@property CGFloat zPosition;
@property CGFloat borderWidth;
@property CGRect contentsRect;
@property CGRect contentsCenter;
@property CGColorRef backgroundColor;
@property CGColorRef borderColor;
@property (retain) CALayer *mask;
@property CGColorRef shadowColor;
@property float shadowOpacity;
@property CGSize shadowOffset;
@property CGFloat shadowRadius;
@property CGPathRef shadowPath;
@property (copy) CADynamicRange preferredDynamicRange;
- (BOOL)isOpaque;

- init;

- (void)addSublayer:(CALayer *)layer;
- (void)replaceSublayer:(CALayer *)layer with:(CALayer *)other;
- (void)display;
- (void)displayIfNeeded;
- (void)drawInContext:(CGContextRef)context;
- (BOOL)needsDisplay;
- (void)removeFromSuperlayer;
- (void)setNeedsDisplay;
- (void)setNeedsDisplayInRect:(CGRect)rect;

- (void)addAnimation:(CAAnimation *)animation forKey:(NSString *)key;
- (CAAnimation *)animationForKey:(NSString *)key;
- (void)removeAllAnimations;
- (void)removeAnimationForKey:(NSString *)key;
- (NSArray *)animationKeys;

- (id<CAAction>)actionForKey:(NSString *)key;

- (void)insertSublayer:(CALayer *)layer atIndex:(unsigned)index;
- (void)insertSublayer:(CALayer *)layer below:(CALayer *)sibling;
- (void)insertSublayer:(CALayer *)layer above:(CALayer *)sibling;
@end

@interface NSObject (CALayerDelegate)

- (void)displayLayer:(CALayer *)layer;
- (void)drawLayer:(CALayer *)layer inContext:(CGContextRef)context;

@end
