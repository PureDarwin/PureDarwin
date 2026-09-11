#import <Foundation/NSXMLElement.h>
#import <Foundation/NSXMLDocument.h>

#include <libxml/tree.h>

@interface NSXMLNode (PDInternal)
- (instancetype)_initWithNode:(void *)node owner:(id)owner ownsNode:(BOOL)ownsNode;
+ (id)_wrap:(xmlNodePtr)node owner:(id)owner;
- (void *)_node;
- (id)_owner;
- (void)_setOwned;
@end

@implementation NSXMLElement

- (instancetype)initWithName:(NSString *)name
{
    if (name == nil) {
        [self release];
        return nil;
    }

    xmlNodePtr node = xmlNewNode(NULL, (const xmlChar *)[name UTF8String]);

    return [self _initWithNode:node owner:nil ownsNode:YES];
}

- (instancetype)initWithName:(NSString *)name stringValue:(NSString *)string
{
    self = [self initWithName:name];

    if (self != nil && string != nil) {
        [self setStringValue:string];
    }
    return self;
}

- (NSArray *)elementsForName:(NSString *)name
{
    xmlNodePtr node = (xmlNodePtr)[self _node];

    if (node == NULL || name == nil) {
        return [NSArray array];
    }

    const xmlChar  *wanted = (const xmlChar *)[name UTF8String];
    NSMutableArray *result = [NSMutableArray array];

    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type != XML_ELEMENT_NODE || child->name == NULL) {
            continue;
        }
        if (xmlStrcmp(child->name, wanted) != 0) {
            continue;
        }

        NSXMLNode *wrapped = [NSXMLNode _wrap:child owner:[self _owner]];

        if (wrapped != nil) {
            [result addObject:wrapped];
        }
    }
    return result;
}

- (void)addChild:(NSXMLNode *)child
{
    xmlNodePtr node = (xmlNodePtr)[self _node];
    xmlNodePtr childNode = (xmlNodePtr)[child _node];

    if (node == NULL || childNode == NULL) {
        return;
    }

    xmlAddChild(node, childNode);
    /* The tree owns it now; the wrapper must not free it in -dealloc. */
    [child _setOwned];
}

- (void)insertChild:(NSXMLNode *)child atIndex:(NSUInteger)index
{
    NSArray *children = [self children];

    if (index >= [children count]) {
        [self addChild:child];
        return;
    }

    xmlNodePtr before = (xmlNodePtr)[[children objectAtIndex:index] _node];
    xmlNodePtr childNode = (xmlNodePtr)[child _node];

    if (before == NULL || childNode == NULL) {
        return;
    }

    xmlAddPrevSibling(before, childNode);
    [child _setOwned];
}

- (void)removeChildAtIndex:(NSUInteger)index
{
    NSArray *children = [self children];

    if (index >= [children count]) {
        return;
    }

    xmlNodePtr child = (xmlNodePtr)[[children objectAtIndex:index] _node];

    if (child != NULL) {
        xmlUnlinkNode(child);
        xmlFreeNode(child);
    }
}

- (NSArray *)attributes
{
    xmlNodePtr node = (xmlNodePtr)[self _node];

    if (node == NULL) {
        return [NSArray array];
    }

    NSMutableArray *result = [NSMutableArray array];

    for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
        NSXMLNode *wrapped = [NSXMLNode _wrap:(xmlNodePtr)attr owner:[self _owner]];

        if (wrapped != nil) {
            [result addObject:wrapped];
        }
    }
    return result;
}

- (void)addAttribute:(NSXMLNode *)attribute
{
    xmlNodePtr node = (xmlNodePtr)[self _node];
    xmlAttrPtr attr = (xmlAttrPtr)[attribute _node];

    if (node == NULL || attr == NULL || attr->name == NULL) {
        return;
    }

    /* +attributeWithName:stringValue: builds a parentless xmlAttr, which
     * cannot simply be linked in; copy its name and value onto this element
     * and let the standalone one be freed with its wrapper. */
    xmlChar *value = xmlNodeGetContent((xmlNodePtr)attr);

    xmlSetProp(node, attr->name, value != NULL ? value : (const xmlChar *)"");

    if (value != NULL) {
        xmlFree(value);
    }
}

- (NSXMLNode *)attributeForName:(NSString *)name
{
    xmlNodePtr node = (xmlNodePtr)[self _node];

    if (node == NULL || name == nil) {
        return nil;
    }

    xmlAttrPtr attr = xmlHasProp(node, (const xmlChar *)[name UTF8String]);

    return attr != NULL ? [NSXMLNode _wrap:(xmlNodePtr)attr owner:[self _owner]] : nil;
}

- (void)removeAttributeForName:(NSString *)name
{
    xmlNodePtr node = (xmlNodePtr)[self _node];

    if (node == NULL || name == nil) {
        return;
    }

    xmlAttrPtr attr = xmlHasProp(node, (const xmlChar *)[name UTF8String]);

    if (attr != NULL) {
        xmlRemoveProp(attr);
    }
}

@end
