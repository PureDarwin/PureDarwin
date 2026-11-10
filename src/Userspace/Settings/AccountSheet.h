// AccountSheet.h - the small forms the Users & Groups pane opens: passwords, new users, pictures
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/AppKit.h>

// runs off the main thread with what was typed, returns nil when it worked or the reason it didn't
typedef NSString *(^AccountSubmit)(NSArray *values, BOOL checked, NSInteger choice, NSString *picture);

@interface AccountSheet : NSObject

// fields are @[label, @(secure), initial value], check is a checkbox label, choices a list of radio labels,
// pictures a list of image paths to pick one from, any of them may be nil
+ (void)showTitle:(NSString *)title message:(NSString *)message fields:(NSArray *)fields
            check:(NSString *)check choices:(NSArray *)choices pictures:(NSArray *)pictures selected:(NSString *)selected
           button:(NSString *)button destructive:(BOOL)destructive accent:(NSColor *)accent
           submit:(AccountSubmit)submit done:(void (^)(void))done;

@end
