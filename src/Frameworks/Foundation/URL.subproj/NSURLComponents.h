// NSURLComponents.h - a URL taken apart into its RFC 3986 components, and put back together
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>

@class NSString, NSNumber, NSURL, NSArray;

@interface NSURLComponents : NSObject <NSCopying> {
    NSString *_scheme;
    NSString *_user;
    NSString *_password;
    NSString *_host;
    NSNumber *_port;
    NSString *_path;
    NSString *_query;
    NSString *_fragment;
}

+ (instancetype)componentsWithURL:(NSURL *)url resolvingAgainstBaseURL:(BOOL)resolve;
+ (instancetype)componentsWithString:(NSString *)string;
- (instancetype)initWithURL:(NSURL *)url resolvingAgainstBaseURL:(BOOL)resolve;
- (instancetype)initWithString:(NSString *)string;

@property (readonly, copy) NSURL *URL;
@property (readonly, copy) NSString *string;

// decoded, as they read
@property (copy) NSString *scheme;
@property (copy) NSString *user;
@property (copy) NSString *password;
@property (copy) NSString *host;
@property (copy) NSNumber *port;
@property (copy) NSString *path;
@property (copy) NSString *query;
@property (copy) NSString *fragment;
@property (copy) NSArray *queryItems;

// as they appear in the URL
@property (copy) NSString *percentEncodedUser;
@property (copy) NSString *percentEncodedPassword;
@property (copy) NSString *percentEncodedHost;
@property (copy) NSString *percentEncodedPath;
@property (copy) NSString *percentEncodedQuery;
@property (copy) NSString *percentEncodedFragment;

@end
