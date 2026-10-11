// MTKTextureLoader on PureDarwin: PNG, JPEG, TIFF, BMP and GIF through CGImageSource (Onyx2D), KTX (version 1)
// read directly. Asset catalogs are not compiled on PD: newTextureWithName: looks for NAME.png/.jpg/.ktx and
// friends in the bundle's resources instead. ModelIO textures load through MTKModel.h
#ifndef PD_METALKIT_MTKTEXTURELOADER_H
#define PD_METALKIT_MTKTEXTURELOADER_H

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>

typedef NSString *MTKTextureLoaderError;
extern MTKTextureLoaderError const MTKTextureLoaderErrorDomain;
extern MTKTextureLoaderError const MTKTextureLoaderErrorKey;

typedef NSString *MTKTextureLoaderOption;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionAllocateMipmaps;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionGenerateMipmaps;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionSRGB;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionTextureUsage;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionTextureCPUCacheMode;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionTextureStorageMode;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionCubeLayout;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionOrigin;
extern MTKTextureLoaderOption const MTKTextureLoaderOptionLoadAsArray;

typedef NSString *MTKTextureLoaderCubeLayout;
extern MTKTextureLoaderCubeLayout const MTKTextureLoaderCubeLayoutVertical;

typedef NSString *MTKTextureLoaderOrigin;
extern MTKTextureLoaderOrigin const MTKTextureLoaderOriginTopLeft;
extern MTKTextureLoaderOrigin const MTKTextureLoaderOriginBottomLeft;
extern MTKTextureLoaderOrigin const MTKTextureLoaderOriginFlippedVertically;

typedef void (^MTKTextureLoaderCallback)(id<MTLTexture> texture, NSError *error);
typedef void (^MTKTextureLoaderArrayCallback)(NSArray<id<MTLTexture>> *textures, NSError *error);

@interface MTKTextureLoader : NSObject

@property(nonatomic, readonly) id<MTLDevice> device;

- (instancetype)initWithDevice:(id<MTLDevice>)device;

- (id<MTLTexture>)newTextureWithContentsOfURL:(NSURL *)URL
                                      options:(NSDictionary<MTKTextureLoaderOption, id> *)options
                                        error:(NSError **)error;
- (NSArray<id<MTLTexture>> *)newTexturesWithContentsOfURLs:(NSArray<NSURL *> *)URLs
                                                   options:(NSDictionary<MTKTextureLoaderOption, id> *)options
                                                     error:(NSError **)error;
- (id<MTLTexture>)newTextureWithData:(NSData *)data
                             options:(NSDictionary<MTKTextureLoaderOption, id> *)options
                               error:(NSError **)error;
- (id<MTLTexture>)newTextureWithCGImage:(CGImageRef)cgImage
                                options:(NSDictionary<MTKTextureLoaderOption, id> *)options
                                  error:(NSError **)error;
- (id<MTLTexture>)newTextureWithName:(NSString *)name
                         scaleFactor:(CGFloat)scaleFactor
                              bundle:(NSBundle *)bundle
                             options:(NSDictionary<MTKTextureLoaderOption, id> *)options
                               error:(NSError **)error;

// the asynchronous forms load at once and call the handler before returning
- (void)newTextureWithContentsOfURL:(NSURL *)URL
                            options:(NSDictionary<MTKTextureLoaderOption, id> *)options
                  completionHandler:(MTKTextureLoaderCallback)completionHandler;
- (void)newTexturesWithContentsOfURLs:(NSArray<NSURL *> *)URLs
                              options:(NSDictionary<MTKTextureLoaderOption, id> *)options
                    completionHandler:(MTKTextureLoaderArrayCallback)completionHandler;
- (void)newTextureWithData:(NSData *)data
                   options:(NSDictionary<MTKTextureLoaderOption, id> *)options
         completionHandler:(MTKTextureLoaderCallback)completionHandler;
- (void)newTextureWithCGImage:(CGImageRef)cgImage
                      options:(NSDictionary<MTKTextureLoaderOption, id> *)options
            completionHandler:(MTKTextureLoaderCallback)completionHandler;
- (void)newTextureWithName:(NSString *)name
               scaleFactor:(CGFloat)scaleFactor
                    bundle:(NSBundle *)bundle
                   options:(NSDictionary<MTKTextureLoaderOption, id> *)options
         completionHandler:(MTKTextureLoaderCallback)completionHandler;
- (void)newTexturesWithNames:(NSArray<NSString *> *)names
                 scaleFactor:(CGFloat)scaleFactor
                      bundle:(NSBundle *)bundle
                     options:(NSDictionary<MTKTextureLoaderOption, id> *)options
           completionHandler:(MTKTextureLoaderArrayCallback)completionHandler;

@end

#endif
