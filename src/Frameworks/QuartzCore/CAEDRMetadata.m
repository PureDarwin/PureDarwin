#import <QuartzCore/CAEDRMetadata.h>

enum { PD_EDR_HDR10 = 1, PD_EDR_HLG = 2 };

@implementation CAEDRMetadata {
  int _kind;
  float _minLuminance, _maxLuminance, _scale;
  NSData *_display, *_content;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

+ (BOOL)isAvailable {
  return YES;
}

+ (CAEDRMetadata *)HDR10MetadataWithDisplayInfo:(NSData *)displayData contentInfo:(NSData *)contentData opticalOutputScale:(float)scale {
  CAEDRMetadata *m = [[[self alloc] init] autorelease];

  m->_kind = PD_EDR_HDR10, m->_scale = scale;
  m->_display = [displayData copy], m->_content = [contentData copy];
  return m;
}

+ (CAEDRMetadata *)HDR10MetadataWithMinLuminance:(float)minNits maxLuminance:(float)maxNits opticalOutputScale:(float)scale {
  CAEDRMetadata *m = [[[self alloc] init] autorelease];

  m->_kind = PD_EDR_HDR10, m->_minLuminance = minNits, m->_maxLuminance = maxNits, m->_scale = scale;
  return m;
}

+ (CAEDRMetadata *)HLGMetadata {
  CAEDRMetadata *m = [[[self alloc] init] autorelease];

  m->_kind = PD_EDR_HLG;
  return m;
}

+ (CAEDRMetadata *)HLGMetadataWithAmbientViewingEnvironment:(NSData *)data {
  CAEDRMetadata *m = [self HLGMetadata];

  m->_content = [data copy];
  return m;
}

- (void)dealloc {
  [_display release];
  [_content release];
  [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
  return [self retain];
}

- (void)encodeWithCoder:(NSCoder *)coder {
  [coder encodeInt:_kind forKey:@"kind"];
  [coder encodeFloat:_minLuminance forKey:@"minLuminance"];
  [coder encodeFloat:_maxLuminance forKey:@"maxLuminance"];
  [coder encodeFloat:_scale forKey:@"scale"];
  if (_display)
    [coder encodeObject:_display forKey:@"display"];
  if (_content)
    [coder encodeObject:_content forKey:@"content"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
  if (!(self = [super init]))
    return nil;
  _kind = [coder decodeIntForKey:@"kind"];
  _minLuminance = [coder decodeFloatForKey:@"minLuminance"];
  _maxLuminance = [coder decodeFloatForKey:@"maxLuminance"];
  _scale = [coder decodeFloatForKey:@"scale"];
  _display = [[coder decodeObjectForKey:@"display"] retain];
  _content = [[coder decodeObjectForKey:@"content"] retain];
  return self;
}

@end
