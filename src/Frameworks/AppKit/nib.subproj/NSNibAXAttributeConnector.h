// NSNibAXAttributeConnector.h - an accessibility attribute a nib sets on an object
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <AppKit/NSNibConnector.h>

@interface NSNibAXAttributeConnector : NSNibConnector {
   NSString *_attributeName;
   NSString *_attributeType;
   id _attributeValue;
}
@end
