/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - David Young <daver@geeks.org>
#import <AppKit/NSInterfaceStyle.h>

NSInterfaceStyle NSInterfaceStyleForKey(NSString *key, NSResponder *responder) {
   static NSString *const names[] = {
      @"NSNoInterfaceStyle", @"NSWindows95InterfaceStyle", @"NSMacintoshInterfaceStyle"
   };
   /* Cached: this is consulted from drawing code - NSMenuView asks once per
    * item per redraw - and a defaults lookup per item is enough to make a
    * menu bar visibly slow. Interface style does not change within a run. */
   static NSMutableDictionary *cache = nil;
   NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
   NSString *value = nil;
   NSString *cacheKey = (key != nil) ? key : @"";

   if (cache == nil) {
      cache = [[NSMutableDictionary alloc] init];
   }
   else {
      NSNumber *cached = [cache objectForKey:cacheKey];

      if (cached != nil)
       return (NSInterfaceStyle)[cached integerValue];
   }

   /* A responder may override the style for itself; otherwise the key's own
    * default wins, then the process-wide one. */
   if ([responder respondsToSelector:@selector(interfaceStyle)]) {
      NSInterfaceStyle style = (NSInterfaceStyle)[(id)responder interfaceStyle];

      if (style != NSNoInterfaceStyle)
       return style;
   }
   if (key != nil)
    value = [defaults stringForKey:key];
   if (value == nil)
    value = [defaults stringForKey:@"NSInterfaceStyle"];
   NSInterfaceStyle result = NSMacintoshInterfaceStyle;

   if (value != nil) {
      for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
         if ([value isEqualToString:names[i]]) {
            result = (NSInterfaceStyle)i;
            break;
         }
      }
   }
   [cache setObject:[NSNumber numberWithInteger:result] forKey:cacheKey];
   return result;
}

