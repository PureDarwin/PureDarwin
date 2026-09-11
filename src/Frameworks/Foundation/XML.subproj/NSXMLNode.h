/*
 * NSXMLNode and friends, backed by libxml2's tree - the same library Apple's
 * implementation uses, so parsing and XPath are real rather than an
 * approximation of them.
 *
 * The tree belongs to the document (or, for a node built standalone, to that
 * node until it is added to a parent). Wrappers are handed out around
 * xmlNodePtr and keep the owner alive, so a node outliving the variable that
 * produced it still points at live memory.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSError.h>
#import <Foundation/NSURL.h>

typedef NS_ENUM(NSUInteger, NSXMLNodeKind) {
    NSXMLInvalidKind = 0,
    NSXMLDocumentKind,
    NSXMLElementKind,
    NSXMLAttributeKind,
    NSXMLNamespaceKind,
    NSXMLProcessingInstructionKind,
    NSXMLCommentKind,
    NSXMLTextKind,
    NSXMLDTDKind,
    NSXMLEntityDeclarationKind,
    NSXMLAttributeDeclarationKind,
    NSXMLElementDeclarationKind,
    NSXMLNotationDeclarationKind,
};

/* Input/output options. Only the ones Foundation's callers pass are acted on;
 * the rest are accepted and ignored, as they are hints. */
enum {
    NSXMLNodeOptionsNone                   = 0,
    NSXMLNodePreserveWhitespace            = 1 << 20,
    NSXMLNodePreserveCharacterReferences   = 1 << 27,
    NSXMLNodePrettyPrint                   = 1 << 17,
    NSXMLDocumentTidyXML                   = 1 << 9,
};

@class NSXMLElement, NSXMLDocument;

@interface NSXMLNode : NSObject {
    void     *_node;    /* xmlNodePtr / xmlAttrPtr / xmlDocPtr */
    id        _owner;   /* whoever frees the tree this node lives in */
    BOOL      _ownsNode;
}

+ (id)document;
+ (id)elementWithName:(NSString *)name;
+ (id)elementWithName:(NSString *)name stringValue:(NSString *)string;
+ (id)attributeWithName:(NSString *)name stringValue:(NSString *)string;
+ (id)textWithStringValue:(NSString *)string;
+ (id)commentWithStringValue:(NSString *)string;

- (NSXMLNodeKind)kind;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)stringValue;
- (void)setStringValue:(NSString *)string;
- (NSUInteger)childCount;
- (NSArray *)children;
- (NSXMLNode *)childAtIndex:(NSUInteger)index;
- (NSXMLNode *)parent;
- (NSXMLDocument *)rootDocument;
- (void)detach;

- (NSString *)XMLString;
- (NSArray *)nodesForXPath:(NSString *)xpath error:(NSError **)error;

@end
