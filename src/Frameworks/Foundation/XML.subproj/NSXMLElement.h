#import <Foundation/NSXMLNode.h>

@interface NSXMLElement : NSXMLNode

- (instancetype)initWithName:(NSString *)name;
- (instancetype)initWithName:(NSString *)name stringValue:(NSString *)string;

- (NSArray *)elementsForName:(NSString *)name;
- (void)addChild:(NSXMLNode *)child;
- (void)insertChild:(NSXMLNode *)child atIndex:(NSUInteger)index;
- (void)removeChildAtIndex:(NSUInteger)index;

- (NSArray *)attributes;
- (void)addAttribute:(NSXMLNode *)attribute;
- (NSXMLNode *)attributeForName:(NSString *)name;
- (void)removeAttributeForName:(NSString *)name;

@end
