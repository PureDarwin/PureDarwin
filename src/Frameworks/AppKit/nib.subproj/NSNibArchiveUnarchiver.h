#import <Foundation/NSCoder.h>
#import <Foundation/NSGeometry.h>

@class NSData, NSDictionary, NSMutableDictionary, NSString;

// a keyed decoder for compiled nibs in Interface Builder's binary "NIBArchive" form (ibtool output), decoding like NSKeyedUnarchiver does for NSNib
// with Foundation's strings, numbers, data and collections built from their inlined forms
@interface NSNibArchiveUnarchiver : NSCoder

+ (BOOL)isNibArchiveData:(NSData *)data;

- initForReadingWithData:(NSData *)data;

- delegate;
- (void)setDelegate:delegate;
- (void)setClass:(Class)cls forClassName:(NSString *)className;
- (void)replaceObject:object withObject:replacement;

- (NSRect)decodeRectForKey:(NSString *)key;
- (NSSize)decodeSizeForKey:(NSString *)key;
- (NSPoint)decodePointForKey:(NSString *)key;

@end
