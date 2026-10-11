// NSNibAXAttributeConnector.m - an accessibility attribute a nib sets on an object
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "NSNibAXAttributeConnector.h"
#import <Foundation/NSCoder.h>
#import <Foundation/NSString.h>

@implementation NSNibAXAttributeConnector

-initWithCoder:(NSCoder *)coder {
   if((self=[super initWithCoder:coder])!=nil){
      _attributeName=[[coder decodeObjectForKey:@"AXAttributeName"] retain];
      _attributeType=[[coder decodeObjectForKey:@"AXAttributeType"] retain];
      _attributeValue=[[coder decodeObjectForKey:@"AXAttributeValue"] retain];
   }
   return self;
}

-(void)dealloc {
   [_attributeName release];
   [_attributeType release];
   [_attributeValue release];
   [super dealloc];
}

// accessibility attributes have no client here, they are only kept
-(void)establishConnection {
}

@end
