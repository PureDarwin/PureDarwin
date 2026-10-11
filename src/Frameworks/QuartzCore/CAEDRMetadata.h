// CAEDRMetadata: how a layer's extended dynamic range content maps to the display. PureDarwin composites in
// standard range, so the metadata is kept for the app and does not change what is shown
#import <Foundation/Foundation.h>

@interface CAEDRMetadata : NSObject <NSCopying, NSSecureCoding>

+ (CAEDRMetadata *)HDR10MetadataWithDisplayInfo:(NSData *)displayData contentInfo:(NSData *)contentData opticalOutputScale:(float)scale;
+ (CAEDRMetadata *)HDR10MetadataWithMinLuminance:(float)minNits maxLuminance:(float)maxNits opticalOutputScale:(float)scale;
+ (CAEDRMetadata *)HLGMetadata;
+ (CAEDRMetadata *)HLGMetadataWithAmbientViewingEnvironment:(NSData *)data;
@property(class, readonly, getter=isAvailable) BOOL available;

@end
