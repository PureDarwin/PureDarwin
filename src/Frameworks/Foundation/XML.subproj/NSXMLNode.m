#import <Foundation/NSXMLNode.h>
#import <Foundation/NSXMLElement.h>
#import <Foundation/NSXMLDocument.h>
#import <Foundation/NSDictionary.h>

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

/* Wrappers are created on demand, so the same xmlNode can be wrapped more than
 * once. Identity therefore comes from the underlying node, not the wrapper. */
@implementation NSXMLNode

+ (void)initialize
{
    if (self == [NSXMLNode class]) {
        xmlInitParser();
    }
}

/* The owner keeps the tree alive: a node handed out from a document must not
 * outlive the document's xmlDoc. A node that owns its own xmlNode - one built
 * standalone and not yet added anywhere - frees it in -dealloc. */
- (instancetype)_initWithNode:(void *)node owner:(id)owner ownsNode:(BOOL)ownsNode
{
    if (node == NULL) {
        [self release];
        return nil;
    }
    if ((self = [super init]) != nil) {
        _node = node;
        _owner = [owner retain];
        _ownsNode = ownsNode;
    }
    return self;
}

+ (id)_wrap:(xmlNodePtr)node owner:(id)owner
{
    if (node == NULL) {
        return nil;
    }

    Class cls = [NSXMLNode class];

    if (node->type == XML_ELEMENT_NODE) {
        cls = [NSXMLElement class];
    } else if (node->type == XML_DOCUMENT_NODE) {
        cls = [NSXMLDocument class];
    }

    return [[[cls alloc] _initWithNode:node owner:owner ownsNode:NO] autorelease];
}

- (void)dealloc
{
    if (_ownsNode && _node != NULL) {
        xmlNodePtr node = (xmlNodePtr)_node;

        /* Only unlink what is still detached; a node that has since been added
         * to a tree is the tree's to free. */
        if (node->parent == NULL) {
            xmlUnlinkNode(node);
            xmlFreeNode(node);
        }
    }
    [_owner release];
    [super dealloc];
}

- (void *)_node
{
    return _node;
}

- (id)_owner
{
    return _owner != nil ? _owner : self;
}

- (void)_setOwned
{
    _ownsNode = NO;
}

+ (id)document
{
    return [[[NSXMLDocument alloc] initWithKind:NSXMLDocumentXMLKind options:0] autorelease];
}

+ (id)elementWithName:(NSString *)name
{
    return [[[NSXMLElement alloc] initWithName:name] autorelease];
}

+ (id)elementWithName:(NSString *)name stringValue:(NSString *)string
{
    return [[[NSXMLElement alloc] initWithName:name stringValue:string] autorelease];
}

+ (id)attributeWithName:(NSString *)name stringValue:(NSString *)string
{
    if (name == nil) {
        return nil;
    }

    xmlAttrPtr attr = xmlNewProp(NULL, (const xmlChar *)[name UTF8String],
                                 (const xmlChar *)[(string != nil ? string : @"") UTF8String]);

    return [[[NSXMLNode alloc] _initWithNode:attr owner:nil ownsNode:YES] autorelease];
}

+ (id)textWithStringValue:(NSString *)string
{
    xmlNodePtr node = xmlNewText((const xmlChar *)[(string != nil ? string : @"") UTF8String]);

    return [[[NSXMLNode alloc] _initWithNode:node owner:nil ownsNode:YES] autorelease];
}

+ (id)commentWithStringValue:(NSString *)string
{
    xmlNodePtr node = xmlNewComment((const xmlChar *)[(string != nil ? string : @"") UTF8String]);

    return [[[NSXMLNode alloc] _initWithNode:node owner:nil ownsNode:YES] autorelease];
}

- (NSXMLNodeKind)kind
{
    if (_node == NULL) {
        return NSXMLInvalidKind;
    }

    switch (((xmlNodePtr)_node)->type) {
        case XML_ELEMENT_NODE:  return NSXMLElementKind;
        case XML_ATTRIBUTE_NODE: return NSXMLAttributeKind;
        case XML_TEXT_NODE:     return NSXMLTextKind;
        case XML_COMMENT_NODE:  return NSXMLCommentKind;
        case XML_DOCUMENT_NODE: return NSXMLDocumentKind;
        case XML_PI_NODE:       return NSXMLProcessingInstructionKind;
        default:                return NSXMLInvalidKind;
    }
}

- (NSString *)name
{
    xmlNodePtr node = (xmlNodePtr)_node;

    if (node == NULL || node->name == NULL) {
        return nil;
    }
    return [NSString stringWithUTF8String:(const char *)node->name];
}

- (void)setName:(NSString *)name
{
    if (_node == NULL) {
        return;
    }
    xmlNodeSetName((xmlNodePtr)_node, (const xmlChar *)[name UTF8String]);
}

- (NSString *)stringValue
{
    if (_node == NULL) {
        return nil;
    }

    xmlChar *content = xmlNodeGetContent((xmlNodePtr)_node);

    if (content == NULL) {
        return @"";
    }

    NSString *result = [NSString stringWithUTF8String:(const char *)content];

    xmlFree(content);
    return result != nil ? result : @"";
}

- (void)setStringValue:(NSString *)string
{
    if (_node == NULL) {
        return;
    }
    xmlNodeSetContent((xmlNodePtr)_node,
                      (const xmlChar *)[(string != nil ? string : @"") UTF8String]);
}

- (NSUInteger)childCount
{
    return [[self children] count];
}

- (NSArray *)children
{
    xmlNodePtr node = (xmlNodePtr)_node;

    if (node == NULL) {
        return nil;
    }

    NSMutableArray *result = [NSMutableArray array];

    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        NSXMLNode *wrapped = [NSXMLNode _wrap:child owner:[self _owner]];

        if (wrapped != nil) {
            [result addObject:wrapped];
        }
    }
    return result;
}

- (NSXMLNode *)childAtIndex:(NSUInteger)index
{
    NSArray *children = [self children];

    return index < [children count] ? [children objectAtIndex:index] : nil;
}

- (NSXMLNode *)parent
{
    xmlNodePtr node = (xmlNodePtr)_node;

    if (node == NULL || node->parent == NULL) {
        return nil;
    }
    return [NSXMLNode _wrap:node->parent owner:[self _owner]];
}

- (void)detach
{
    if (_node == NULL) {
        return;
    }
    xmlUnlinkNode((xmlNodePtr)_node);
    _ownsNode = YES;
}

- (NSString *)XMLString
{
    xmlNodePtr node = (xmlNodePtr)_node;

    if (node == NULL) {
        return @"";
    }

    xmlBufferPtr buffer = xmlBufferCreate();

    if (buffer == NULL) {
        return @"";
    }

    xmlNodeDump(buffer, node->doc, node, 0, 0);

    NSString *result = [NSString stringWithUTF8String:(const char *)xmlBufferContent(buffer)];

    xmlBufferFree(buffer);
    return result != nil ? result : @"";
}

- (NSArray *)nodesForXPath:(NSString *)xpath error:(NSError **)error
{
    if (error != NULL) {
        *error = nil;
    }

    xmlNodePtr node = (xmlNodePtr)_node;

    if (node == NULL || xpath == nil) {
        return [NSArray array];
    }

    xmlDocPtr doc = (node->type == XML_DOCUMENT_NODE) ? (xmlDocPtr)node : node->doc;

    if (doc == NULL) {
        return [NSArray array];
    }

    xmlXPathContextPtr context = xmlXPathNewContext(doc);

    if (context == NULL) {
        return [NSArray array];
    }

    /* Relative expressions are evaluated against the receiver, which is what
     * -nodesForXPath: on a node means. */
    context->node = node;

    xmlXPathObjectPtr object = xmlXPathEvalExpression((const xmlChar *)[xpath UTF8String], context);

    if (object == NULL) {
        xmlXPathFreeContext(context);
        return [NSArray array];
    }

    NSMutableArray *result = [NSMutableArray array];

    if (object->nodesetval != NULL) {
        for (int i = 0; i < object->nodesetval->nodeNr; i++) {
            NSXMLNode *wrapped = [NSXMLNode _wrap:object->nodesetval->nodeTab[i]
                                            owner:[self _owner]];

            if (wrapped != nil) {
                [result addObject:wrapped];
            }
        }
    }

    xmlXPathFreeObject(object);
    xmlXPathFreeContext(context);
    return result;
}

@end
