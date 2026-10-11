/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSColor_catalog.h>
#import <AppKit/NSColorList.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSAppearance.h>

@interface NSColor(NSAppKitPrivate)
-(CGColorRef)CGColorRef;
+(NSColor *)_systemColorNamed:(NSString *)colorName dark:(BOOL)dark;
@end

void NSColorSetCatalogColor(NSString *catalogName,NSString *colorName,NSColor *color);
NSColor *NSColorGetCatalogColor(NSString *catalogName,NSString *colorName);

@implementation NSColor_catalog

-initWithCatalogName:(NSString *)catalogName colorName:(NSString *)colorName color:(NSColor *)color {
   _catalogName=[catalogName copy];
   _colorName=[colorName copy];
   _color=[color copy];
   return self;
}

-(void)dealloc {
   [_colorName release];
   [_catalogName release];
   [_color release];
   NSDeallocateObject(self);
   return;
   [super dealloc];
}

-(BOOL)isEqual:otherObject {
   if(self==otherObject)
    return YES;

   if([otherObject isKindOfClass:[self class]]){
    NSColor_catalog *other=otherObject;

    return ([_catalogName isEqualToString:other->_catalogName] && [_colorName isEqualToString:other-> _colorName]);
   }

   return NO;
}

-(NSString *)description {
    return [NSString stringWithFormat:@"<%@ catalogName: %@ colorName: %@ color:%@>",
        [[self class] description], _catalogName, _colorName, _color];
}

+(NSColor *)colorWithCatalogName:(NSString *)catalogName colorName:(NSString *)colorName color:(NSColor *)color {
   return [[[self alloc] initWithCatalogName:catalogName colorName:colorName color:color] autorelease];
}

-(NSColor *)color { return _color; }

// component queries answer through the colour this one stands for, in rgb
-(NSColor *)_rgb {
   return [[self _resolvedColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
}

// system colours follow the appearance being drawn, other catalogs keep their colour
-(NSColor *)_resolvedColor {
   if([_catalogName isEqualToString:@"System"]){
    NSColor *color=[NSColor _systemColorNamed:_colorName dark:[[NSAppearance currentDrawingAppearance] _isDark]];

    if(color!=nil)
     return color;
   }
   return _color;
}


-(NSString *)colorSpaceName {
   return NSNamedColorSpace;
}

-(NSString *)catalogName {
   return _catalogName;
}

-(NSString *)colorName {
   return _colorName;
}

-(NSColor *)colorUsingColorSpaceName:(NSString *)colorSpace device:(NSDictionary *)device {
   NSColor *result;

   if ([colorSpace isEqualToString:[self colorSpaceName]])
    return self;
   if([colorSpace isEqualToString:[[self _resolvedColor] colorSpaceName]])
    return [self _resolvedColor];

   result=[[self _resolvedColor] colorUsingColorSpaceName:colorSpace device:device];
   
   if(result==nil)
    NSLog(@"result ==nil %@ %@",_colorName,colorSpace);
    
   return result;
}

-(CGColorRef)CGColorRef {
   return [[self _resolvedColor] CGColorRef];
}

-(void)setFill {
   if([self _resolvedColor]==nil)
    [NSException raise:@"NSUnknownColor" format:@"Unknown color %@ in catalog %@",_colorName,_catalogName];
    
   [[self _resolvedColor] setFill];
}

-(void)setStroke {
   if([self _resolvedColor]==nil)
    [NSException raise:@"NSUnknownColor" format:@"Unknown color %@ in catalog %@",_colorName,_catalogName];
    
   [[self _resolvedColor] setStroke];
}

-(NSInteger)numberOfComponents {
   return [[self _rgb] numberOfComponents];
}

-(void)getComponents:(CGFloat *)components {
   [[self _rgb] getComponents:components];
}

// alpha is defined for every color on macOS, catalog colors answer through the color they stand for
-(CGFloat)alphaComponent {
   return [[self _resolvedColor] alphaComponent];
}

-(NSColor *)colorWithAlphaComponent:(CGFloat)alpha {
   return [[self _resolvedColor] colorWithAlphaComponent:alpha];
}

-(void)getWhite:(CGFloat *)white alpha:(CGFloat *)alpha {
   [[[self _resolvedColor] colorUsingColorSpaceName:NSCalibratedWhiteColorSpace] getWhite:white alpha:alpha];
}

-(void)getRed:(CGFloat *)red green:(CGFloat *)green blue:(CGFloat *)blue alpha:(CGFloat *)alpha {
   [[self _rgb] getRed:red green:green blue:blue alpha:alpha];
}

-(void)getHue:(CGFloat *)huep saturation:(CGFloat *)saturationp brightness:(CGFloat *)brightnessp alpha:(CGFloat *)alphap {
   [[self _rgb] getHue:huep saturation:saturationp brightness:brightnessp alpha:alphap];
}

-(void)getCyan:(CGFloat *)cyan magenta:(CGFloat *)magenta yellow:(CGFloat *)yellow black:(CGFloat *)black alpha:(CGFloat *)alpha {
   [NSException raise:NSInvalidArgumentException format:@"-getCyan not valid for the NSColor Catalog color: %@ %@; need to first convert colorspace.",_catalogName,_colorName];
}

@end

// a colour that asks its provider each time it is used, as +colorWithName:dynamicProvider: hands out
@interface NSColor_dynamic : NSColor {
    NSString *_name;
    NSColor *(^_provider)(NSAppearance *);
}
@end

@implementation NSColor_dynamic

-initWithName:(NSString *)name provider:(NSColor *(^)(NSAppearance *))provider {
   _name=[name copy];
   _provider=[provider copy];
   return self;
}

-(void)dealloc {
   [_name release];
   [_provider release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-(NSColor *)_resolvedColor {
   return _provider([NSAppearance currentDrawingAppearance]);
}

-(NSString *)colorSpaceName {
   return NSNamedColorSpace;
}

-(NSString *)colorNameComponent {
   return _name;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@ %@ %@>",[self class],_name,[self _resolvedColor]];
}

-(NSColor *)colorUsingColorSpaceName:(NSString *)colorSpace device:(NSDictionary *)device {
   return [[self _resolvedColor] colorUsingColorSpaceName:colorSpace device:device];
}

-(NSColor *)colorUsingColorSpace:(NSColorSpace *)space {
   return [[self _resolvedColor] colorUsingColorSpace:space];
}

-(CGColorRef)CGColorRef {
   return [[self _resolvedColor] CGColorRef];
}

-(void)set {
   [[self _resolvedColor] set];
}

-(void)setFill {
   [[self _resolvedColor] setFill];
}

-(void)setStroke {
   [[self _resolvedColor] setStroke];
}

-(CGFloat)alphaComponent {
   return [[self _resolvedColor] alphaComponent];
}

-(void)getRed:(CGFloat *)red green:(CGFloat *)green blue:(CGFloat *)blue alpha:(CGFloat *)alpha {
   [[[self _resolvedColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace] getRed:red green:green blue:blue alpha:alpha];
}

-(void)getWhite:(CGFloat *)white alpha:(CGFloat *)alpha {
   [[[self _resolvedColor] colorUsingColorSpaceName:NSCalibratedWhiteColorSpace] getWhite:white alpha:alpha];
}

// a dynamic colour with alpha stays dynamic
-(NSColor *)colorWithAlphaComponent:(CGFloat)alpha {
   NSColor *(^provider)(NSAppearance *)=_provider;

   return [NSColor colorWithName:_name dynamicProvider:^NSColor *(NSAppearance *appearance) {
      return [provider(appearance) colorWithAlphaComponent:alpha];
   }];
}

-(NSColor *)blendedColorWithFraction:(CGFloat)fraction ofColor:(NSColor *)color {
   return [[self _resolvedColor] blendedColorWithFraction:fraction ofColor:color];
}

// the rest of NSColor's questions are answered by the colour for the current appearance,
// the base class raises for them, which text drawing swallows and window teardown does not
-(NSColor *)_rgb {
   return [[self _resolvedColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
}

-(NSInteger)numberOfComponents {
   return [[self _rgb] numberOfComponents];
}

-(void)getComponents:(CGFloat *)components {
   [[self _rgb] getComponents:components];
}

-(void)getHue:(CGFloat *)huep saturation:(CGFloat *)saturationp brightness:(CGFloat *)brightnessp alpha:(CGFloat *)alphap {
   [[self _rgb] getHue:huep saturation:saturationp brightness:brightnessp alpha:alphap];
}

-(NSColorSpace *)colorSpace {
   return [[self _resolvedColor] colorSpace];
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [[self _resolvedColor] encodeWithCoder:coder];
}

-(BOOL)isEqual:otherObject {
   return otherObject==self;
}

-(NSUInteger)hash {
   return (NSUInteger)self;
}

@end

@implementation NSColor(NSDynamicColor)

+(NSColor *)colorWithName:(NSString *)name dynamicProvider:(NSColor *(^)(NSAppearance *))provider {
   return [[[NSColor_dynamic alloc] initWithName:name provider:provider] autorelease];
}

@end
