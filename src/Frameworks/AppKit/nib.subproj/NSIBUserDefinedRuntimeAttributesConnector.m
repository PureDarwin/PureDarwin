#import <AppKit/NSNibConnector.h>
#import <Foundation/Foundation.h>

// interface Builder's "user defined runtime attributes" (and storyboards' content view controllers): values set
// on an object by key path once the nib's objects exist
@interface NSIBUserDefinedRuntimeAttributesConnector : NSNibConnector {
    id _object;
    NSArray *_keyPaths;
    NSMutableArray *_values;
}
@end

@implementation NSIBUserDefinedRuntimeAttributesConnector

-initWithCoder:(NSCoder *)coder {
    _object=[[coder decodeObjectForKey:@"NSObject"] retain];
    _keyPaths=[[coder decodeObjectForKey:@"NSKeyPaths"] retain];
    _values=[[coder decodeObjectForKey:@"NSValues"] mutableCopy];
    return self;
}

-(void)dealloc {
    [_object release];
    [_keyPaths release];
    [_values release];
    [super dealloc];
}

-(void)replaceObject:original withObject:replacement {
    NSUInteger i;

    if(original==_object){
        [replacement retain];
        [_object release];
        _object=replacement;
    }
    while((i=[_values indexOfObjectIdenticalTo:original])!=NSNotFound)
        [_values replaceObjectAtIndex:i withObject:replacement];
}

-(void)establishConnection {
    NSUInteger i,count=[_keyPaths count];

    for(i=0;i<count && i<[_values count];i++){
        NSString *keyPath=[_keyPaths objectAtIndex:i];
        id        value=[_values objectAtIndex:i];

        if([keyPath rangeOfString:@"."].location==NSNotFound)
            [_object setValue:value forKey:keyPath];
        else
            [_object setValue:value forKeyPath:keyPath];
    }
}

@end
