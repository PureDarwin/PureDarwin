#import <Foundation/NSXMLNode.h>
#import <Foundation/NSXMLElement.h>

typedef NS_ENUM(NSUInteger, NSXMLDocumentContentKind) {
    NSXMLDocumentXMLKind = 0,
    NSXMLDocumentXHTMLKind,
    NSXMLDocumentHTMLKind,
    NSXMLDocumentTextKind,
};

@interface NSXMLDocument : NSXMLNode

- (instancetype)initWithKind:(NSXMLDocumentContentKind)kind options:(NSUInteger)options;
- (instancetype)initWithData:(NSData *)data options:(NSUInteger)options error:(NSError **)error;
- (instancetype)initWithContentsOfURL:(NSURL *)url options:(NSUInteger)options error:(NSError **)error;
- (instancetype)initWithXMLString:(NSString *)string options:(NSUInteger)options error:(NSError **)error;

- (NSXMLElement *)rootElement;
- (void)setRootElement:(NSXMLNode *)root;

- (NSData *)XMLData;
- (NSData *)XMLDataWithOptions:(NSUInteger)options;

@end
