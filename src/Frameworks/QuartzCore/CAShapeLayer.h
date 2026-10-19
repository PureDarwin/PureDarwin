// SPDX-License-Identifier: MIT
// CAShapeLayer, a layer that fills and strokes a CGPath
// as a mask CARenderer clips to the path itself, otherwise the path is drawn into the contents
#import <QuartzCore/CALayer.h>

typedef NSString *CAShapeLayerFillRule;
typedef NSString *CAShapeLayerLineCap;
typedef NSString *CAShapeLayerLineJoin;

extern CAShapeLayerFillRule const kCAFillRuleNonZero;
extern CAShapeLayerFillRule const kCAFillRuleEvenOdd;
extern CAShapeLayerLineCap const kCALineCapButt;
extern CAShapeLayerLineCap const kCALineCapRound;
extern CAShapeLayerLineCap const kCALineCapSquare;
extern CAShapeLayerLineJoin const kCALineJoinMiter;
extern CAShapeLayerLineJoin const kCALineJoinRound;
extern CAShapeLayerLineJoin const kCALineJoinBevel;

@interface CAShapeLayer : CALayer {
    CGPathRef _path;
    CGColorRef _fillColor;
    CGColorRef _strokeColor;
    CAShapeLayerFillRule _fillRule;
    CAShapeLayerLineCap _lineCap;
    CAShapeLayerLineJoin _lineJoin;
    CGFloat _lineWidth;
    CGFloat _miterLimit;
    CGFloat _strokeStart;
    CGFloat _strokeEnd;
    CGFloat _lineDashPhase;
    NSArray *_lineDashPattern;
}

@property CGPathRef path;
@property CGColorRef fillColor;
@property(copy) CAShapeLayerFillRule fillRule;
@property CGColorRef strokeColor;
@property CGFloat strokeStart;
@property CGFloat strokeEnd;
@property CGFloat lineWidth;
@property CGFloat miterLimit;
@property(copy) CAShapeLayerLineCap lineCap;
@property(copy) CAShapeLayerLineJoin lineJoin;
@property CGFloat lineDashPhase;
@property(copy) NSArray *lineDashPattern;

@end
