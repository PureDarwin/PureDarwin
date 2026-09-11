#import <Foundation/NSXMLDocument.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

@interface NSXMLNode (PDInternal)
- (instancetype)_initWithNode:(void *)node owner:(id)owner ownsNode:(BOOL)ownsNode;
+ (id)_wrap:(xmlNodePtr)node owner:(id)owner;
- (void *)_node;
- (id)_owner;
- (void)_setOwned;
@end

@implementation NSXMLDocument

/* The document owns the xmlDoc outright; nodes handed out retain the document
 * rather than the tree, so the tree cannot be freed while a node refers in. */
- (instancetype)_initWithDocument:(xmlDocPtr)doc
{
    if (doc == NULL) {
        [self release];
        return nil;
    }
    return [self _initWithNode:doc owner:nil ownsNode:NO];
}

- (instancetype)initWithKind:(NSXMLDocumentContentKind)kind options:(NSUInteger)options
{
    (void)kind;
    (void)options;

    return [self _initWithDocument:xmlNewDoc((const xmlChar *)"1.0")];
}

/* NSXMLNodePreserveWhitespace maps to *not* setting libxml2's blank-stripping;
 * the others Foundation's callers pass are hints with no parse-time effect. */
static int pd_parser_options(NSUInteger options)
{
    int result = XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING;

    if ((options & NSXMLNodePreserveWhitespace) == 0) {
        result |= XML_PARSE_NOBLANKS;
    }
    return result;
}

- (instancetype)initWithData:(NSData *)data options:(NSUInteger)options error:(NSError **)error
{
    if (error != NULL) {
        *error = nil;
    }

    if (data == nil) {
        [self release];
        return nil;
    }

    xmlDocPtr doc = xmlReadMemory((const char *)[data bytes], (int)[data length],
                                  NULL, "UTF-8", pd_parser_options(options));

    if (doc == NULL) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:@"NSXMLParserErrorDomain"
                                         code:1
                                     userInfo:nil];
        }
        [self release];
        return nil;
    }
    return [self _initWithDocument:doc];
}

- (instancetype)initWithXMLString:(NSString *)string options:(NSUInteger)options error:(NSError **)error
{
    if (string == nil) {
        [self release];
        return nil;
    }
    return [self initWithData:[string dataUsingEncoding:NSUTF8StringEncoding]
                      options:options
                        error:error];
}

- (instancetype)initWithContentsOfURL:(NSURL *)url options:(NSUInteger)options error:(NSError **)error
{
    if (url == nil) {
        [self release];
        return nil;
    }

    NSData *data = [NSData dataWithContentsOfFile:[url path]];

    if (data == nil) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:NSCocoaErrorDomain code:260 userInfo:nil];
        }
        [self release];
        return nil;
    }
    return [self initWithData:data options:options error:error];
}

- (void)dealloc
{
    xmlDocPtr doc = (xmlDocPtr)[self _node];

    if (doc != NULL) {
        xmlFreeDoc(doc);
    }
    [super dealloc];
}

- (NSXMLElement *)rootElement
{
    xmlDocPtr doc = (xmlDocPtr)[self _node];

    if (doc == NULL) {
        return nil;
    }
    return (NSXMLElement *)[NSXMLNode _wrap:xmlDocGetRootElement(doc) owner:self];
}

- (void)setRootElement:(NSXMLNode *)root
{
    xmlDocPtr  doc = (xmlDocPtr)[self _node];
    xmlNodePtr node = (xmlNodePtr)[root _node];

    if (doc == NULL || node == NULL) {
        return;
    }

    xmlDocSetRootElement(doc, node);
    [root _setOwned];
}

- (NSData *)XMLData
{
    return [self XMLDataWithOptions:0];
}

- (NSData *)XMLDataWithOptions:(NSUInteger)options
{
    xmlDocPtr doc = (xmlDocPtr)[self _node];

    if (doc == NULL) {
        return [NSData data];
    }

    xmlChar *buffer = NULL;
    int      length = 0;

    xmlDocDumpFormatMemoryEnc(doc, &buffer, &length, "UTF-8",
                              (options & NSXMLNodePrettyPrint) != 0 ? 1 : 0);

    if (buffer == NULL || length <= 0) {
        if (buffer != NULL) {
            xmlFree(buffer);
        }
        return [NSData data];
    }

    NSData *result = [NSData dataWithBytes:buffer length:(NSUInteger)length];

    xmlFree(buffer);
    return result;
}

@end
