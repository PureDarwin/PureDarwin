// NSURLComponents.m - a URL taken apart into its RFC 3986 components, and put back together
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// the components are kept as they appear in the URL, the plain accessors decode and encode them
#import <Foundation/NSURLComponents.h>
#import <Foundation/NSURLQueryItem.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSCharacterSet.h>

static void replace(NSString **slot, NSString *value) {
    NSString *old = *slot;

    *slot = [value copy];
    [old release];
}

static NSString *encoded(NSString *value, NSCharacterSet *allowed) {
    return value ? [value stringByAddingPercentEncodingWithAllowedCharacters:allowed] : nil;
}

@implementation NSURLComponents

+ (instancetype)componentsWithURL:(NSURL *)url resolvingAgainstBaseURL:(BOOL)resolve {
    return [[[self alloc] initWithURL:url resolvingAgainstBaseURL:resolve] autorelease];
}

+ (instancetype)componentsWithString:(NSString *)string {
    return [[[self alloc] initWithString:string] autorelease];
}

- (instancetype)initWithURL:(NSURL *)url resolvingAgainstBaseURL:(BOOL)resolve {
    NSString *string = resolve ? [url absoluteString] : [url relativeString];

    if (string == nil) {
        [self release];
        return nil;
    }
    return [self initWithString:string];
}

// scheme ":" [ "//" [ userinfo "@" ] host [ ":" port ] ] path [ "?" query ] [ "#" fragment ]
- (instancetype)initWithString:(NSString *)string {
    NSRange hash, question, colon, slash;
    NSString *rest = string;

    if ((self = [super init]) == nil || string == nil) {
        [self release];
        return nil;
    }
    hash = [rest rangeOfString:@"#"];
    if (hash.location != NSNotFound) {
        _fragment = [[rest substringFromIndex:NSMaxRange(hash)] copy];
        rest = [rest substringToIndex:hash.location];
    }
    question = [rest rangeOfString:@"?"];
    if (question.location != NSNotFound) {
        _query = [[rest substringFromIndex:NSMaxRange(question)] copy];
        rest = [rest substringToIndex:question.location];
    }
    colon = [rest rangeOfString:@":"];
    slash = [rest rangeOfString:@"/"];
    if (colon.location != NSNotFound && colon.location > 0 && (slash.location == NSNotFound || colon.location < slash.location)) {
        _scheme = [[rest substringToIndex:colon.location] copy];
        rest = [rest substringFromIndex:NSMaxRange(colon)];
    }
    if ([rest hasPrefix:@"//"]) {
        NSString *authority;
        NSRange end = [rest rangeOfString:@"/" options:0 range:NSMakeRange(2, [rest length] - 2)];
        NSRange at, portColon;

        authority = end.location == NSNotFound ? [rest substringFromIndex:2] : [rest substringWithRange:NSMakeRange(2, end.location - 2)];
        rest = end.location == NSNotFound ? @"" : [rest substringFromIndex:end.location];
        at = [authority rangeOfString:@"@" options:NSBackwardsSearch];
        if (at.location != NSNotFound) {
            NSString *userinfo = [authority substringToIndex:at.location];
            NSRange separator = [userinfo rangeOfString:@":"];

            if (separator.location != NSNotFound) {
                _user = [[userinfo substringToIndex:separator.location] copy];
                _password = [[userinfo substringFromIndex:NSMaxRange(separator)] copy];
            } else
                _user = [userinfo copy];
            authority = [authority substringFromIndex:NSMaxRange(at)];
        }
        // the port follows the last colon, unless that colon is inside an IPv6 literal
        portColon = [authority rangeOfString:@":" options:NSBackwardsSearch];
        NSRange bracket = [authority rangeOfString:@"]" options:NSBackwardsSearch];
        if (portColon.location != NSNotFound && (bracket.location == NSNotFound || portColon.location > bracket.location)) {
            NSString *port = [authority substringFromIndex:NSMaxRange(portColon)];

            if ([port length])
                _port = [[NSNumber numberWithInteger:[port integerValue]] retain];
            authority = [authority substringToIndex:portColon.location];
        }
        _host = [authority copy];
    }
    _path = [rest copy];
    return self;
}

- (instancetype)init {
    if ((self = [super init]))
        _path = @"";
    return self;
}

- (void)dealloc {
    [_scheme release];
    [_user release];
    [_password release];
    [_host release];
    [_port release];
    [_path release];
    [_query release];
    [_fragment release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSURLComponents *copy = [[NSURLComponents allocWithZone:zone] init];

    copy.scheme = _scheme;
    copy.percentEncodedUser = _user;
    copy.percentEncodedPassword = _password;
    copy.percentEncodedHost = _host;
    copy.port = _port;
    copy.percentEncodedPath = _path;
    copy.percentEncodedQuery = _query;
    copy.percentEncodedFragment = _fragment;
    return copy;
}

- (NSString *)string {
    NSMutableString *result = [NSMutableString string];

    if (_scheme)
        [result appendFormat:@"%@:", _scheme];
    if (_host || _user || _port) {
        [result appendString:@"//"];
        if (_user) {
            [result appendString:_user];
            if (_password)
                [result appendFormat:@":%@", _password];
            [result appendString:@"@"];
        }
        if (_host)
            [result appendString:_host];
        if (_port)
            [result appendFormat:@":%@", _port];
    }
    if (_path)
        [result appendString:_path];
    if (_query)
        [result appendFormat:@"?%@", _query];
    if (_fragment)
        [result appendFormat:@"#%@", _fragment];
    return result;
}

- (NSURL *)URL {
    return [NSURL URLWithString:[self string]];
}

- (NSString *)scheme { return _scheme; }
- (void)setScheme:(NSString *)scheme { replace(&_scheme, scheme); }
- (NSNumber *)port { return _port; }
- (void)setPort:(NSNumber *)port { [_port release]; _port = [port copy]; }

- (NSString *)user { return [_user stringByRemovingPercentEncoding]; }
- (void)setUser:(NSString *)user { replace(&_user, encoded(user, [NSCharacterSet URLUserAllowedCharacterSet])); }
- (NSString *)password { return [_password stringByRemovingPercentEncoding]; }
- (void)setPassword:(NSString *)password { replace(&_password, encoded(password, [NSCharacterSet URLPasswordAllowedCharacterSet])); }
- (NSString *)host { return [_host stringByRemovingPercentEncoding]; }
- (void)setHost:(NSString *)host { replace(&_host, encoded(host, [NSCharacterSet URLHostAllowedCharacterSet])); }
- (NSString *)path { return [_path stringByRemovingPercentEncoding]; }
- (void)setPath:(NSString *)path { replace(&_path, encoded(path, [NSCharacterSet URLPathAllowedCharacterSet])); }
- (NSString *)query { return [_query stringByRemovingPercentEncoding]; }
- (void)setQuery:(NSString *)query { replace(&_query, encoded(query, [NSCharacterSet URLQueryAllowedCharacterSet])); }
- (NSString *)fragment { return [_fragment stringByRemovingPercentEncoding]; }
- (void)setFragment:(NSString *)fragment { replace(&_fragment, encoded(fragment, [NSCharacterSet URLFragmentAllowedCharacterSet])); }

- (NSString *)percentEncodedUser { return _user; }
- (void)setPercentEncodedUser:(NSString *)user { replace(&_user, user); }
- (NSString *)percentEncodedPassword { return _password; }
- (void)setPercentEncodedPassword:(NSString *)password { replace(&_password, password); }
- (NSString *)percentEncodedHost { return _host; }
- (void)setPercentEncodedHost:(NSString *)host { replace(&_host, host); }
- (NSString *)percentEncodedPath { return _path; }
- (void)setPercentEncodedPath:(NSString *)path { replace(&_path, path ?: @""); }
- (NSString *)percentEncodedQuery { return _query; }
- (void)setPercentEncodedQuery:(NSString *)query { replace(&_query, query); }
- (NSString *)percentEncodedFragment { return _fragment; }
- (void)setPercentEncodedFragment:(NSString *)fragment { replace(&_fragment, fragment); }

- (NSArray *)queryItems {
    NSMutableArray *items;

    if (_query == nil)
        return nil;
    items = [NSMutableArray array];
    for (NSString *pair in [_query componentsSeparatedByString:@"&"]) {
        NSRange equals = [pair rangeOfString:@"="];
        NSString *name = equals.location == NSNotFound ? pair : [pair substringToIndex:equals.location];
        NSString *value = equals.location == NSNotFound ? nil : [pair substringFromIndex:NSMaxRange(equals)];

        [items addObject:[NSURLQueryItem queryItemWithName:[name stringByRemovingPercentEncoding]
                                                     value:[value stringByRemovingPercentEncoding]]];
    }
    return items;
}

// '&' and '=' separate the items, so they are encoded inside names and values
- (void)setQueryItems:(NSArray *)queryItems {
    NSMutableCharacterSet *allowed;
    NSMutableArray *pairs;

    if (queryItems == nil) {
        replace(&_query, nil);
        return;
    }
    allowed = [[[NSCharacterSet URLQueryAllowedCharacterSet] mutableCopy] autorelease];
    [allowed removeCharactersInString:@"&="];
    pairs = [NSMutableArray array];
    for (NSURLQueryItem *item in queryItems) {
        NSString *name = encoded([item name], allowed) ?: @"";

        [pairs addObject:[item value] ? [NSString stringWithFormat:@"%@=%@", name, encoded([item value], allowed)] : name];
    }
    replace(&_query, [pairs componentsJoinedByString:@"&"]);
}

@end
