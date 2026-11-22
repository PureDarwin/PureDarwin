// CALayerModern.m - CALayer properties the renderer does not act on yet, kept so clients can set and read them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <QuartzCore/CALayer.h>
#import <objc/runtime.h>

static char PDAutoresizingKey, PDDisplayOnBoundsKey, PDCornerCurveKey, PDAsyncKey, PDActionsKey,
    PDGroupOpacityKey, PDEdgeAAKey, PDContentsFormatKey, PDAnchorZKey, PDStyleKey, PDFiltersKey,
    PDCompositingFilterKey, PDBackgroundFiltersKey, PDLayoutManagerKey;

static id PDGet(id object, const void *key)
{
    return objc_getAssociatedObject(object, key);
}

static void PDSet(id object, const void *key, id value, objc_AssociationPolicy policy)
{
    objc_setAssociatedObject(object, key, value, policy);
}

static BOOL PDGetBool(id object, const void *key, BOOL fallback)
{
    NSNumber *value = PDGet(object, key);

    return value != nil ? [value boolValue] : fallback;
}

static void PDSetBool(id object, const void *key, BOOL value)
{
    PDSet(object, key, [NSNumber numberWithBool:value], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

@implementation CALayer (Modern)

// stored only: sublayers are not yet resized with their superlayer
- (unsigned int)autoresizingMask
{
    return [PDGet(self, &PDAutoresizingKey) unsignedIntValue];
}

- (void)setAutoresizingMask:(unsigned int)mask
{
    PDSet(self, &PDAutoresizingKey, [NSNumber numberWithUnsignedInt:mask], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (BOOL)needsDisplayOnBoundsChange
{
    return PDGetBool(self, &PDDisplayOnBoundsKey, NO);
}

- (void)setNeedsDisplayOnBoundsChange:(BOOL)flag
{
    PDSetBool(self, &PDDisplayOnBoundsKey, flag);
}

- (NSString *)cornerCurve
{
    NSString *curve = PDGet(self, &PDCornerCurveKey);

    return curve != nil ? curve : @"circular";
}

- (void)setCornerCurve:(NSString *)curve
{
    PDSet(self, &PDCornerCurveKey, curve, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (BOOL)drawsAsynchronously
{
    return PDGetBool(self, &PDAsyncKey, NO);
}

- (void)setDrawsAsynchronously:(BOOL)flag
{
    PDSetBool(self, &PDAsyncKey, flag);
}

- (NSDictionary *)actions
{
    return PDGet(self, &PDActionsKey);
}

- (void)setActions:(NSDictionary *)actions
{
    PDSet(self, &PDActionsKey, actions, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (BOOL)allowsGroupOpacity
{
    return PDGetBool(self, &PDGroupOpacityKey, YES);
}

- (void)setAllowsGroupOpacity:(BOOL)flag
{
    PDSetBool(self, &PDGroupOpacityKey, flag);
}

- (BOOL)allowsEdgeAntialiasing
{
    return PDGetBool(self, &PDEdgeAAKey, NO);
}

- (void)setAllowsEdgeAntialiasing:(BOOL)flag
{
    PDSetBool(self, &PDEdgeAAKey, flag);
}

- (NSString *)contentsFormat
{
    NSString *format = PDGet(self, &PDContentsFormatKey);

    return format != nil ? format : @"RGBA8";
}

- (void)setContentsFormat:(NSString *)format
{
    PDSet(self, &PDContentsFormatKey, format, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (CGFloat)anchorPointZ
{
    return [PDGet(self, &PDAnchorZKey) doubleValue];
}

- (void)setAnchorPointZ:(CGFloat)z
{
    PDSet(self, &PDAnchorZKey, [NSNumber numberWithDouble:z], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (NSDictionary *)style
{
    return PDGet(self, &PDStyleKey);
}

- (void)setStyle:(NSDictionary *)style
{
    PDSet(self, &PDStyleKey, style, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (NSArray *)filters
{
    return PDGet(self, &PDFiltersKey);
}

- (void)setFilters:(NSArray *)filters
{
    PDSet(self, &PDFiltersKey, filters, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (id)compositingFilter
{
    return PDGet(self, &PDCompositingFilterKey);
}

- (void)setCompositingFilter:(id)filter
{
    PDSet(self, &PDCompositingFilterKey, filter, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (NSArray *)backgroundFilters
{
    return PDGet(self, &PDBackgroundFiltersKey);
}

- (void)setBackgroundFilters:(NSArray *)filters
{
    PDSet(self, &PDBackgroundFiltersKey, filters, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (id)layoutManager
{
    return PDGet(self, &PDLayoutManagerKey);
}

- (void)setLayoutManager:(id)manager
{
    PDSet(self, &PDLayoutManagerKey, manager, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

@end
