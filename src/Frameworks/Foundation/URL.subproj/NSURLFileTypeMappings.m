// NSURLFileTypeMappings.m - file name extensions and MIME types, both ways
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// a built-in table of common types, then /etc/mime.types or /usr/share/mime/mime.types when present
#import <Foundation/NSURLFileTypeMappings.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSCharacterSet.h>
#include <dispatch/dispatch.h>

// type first, then its extensions with the preferred one first
static const char *const builtinTypes[][6] = {
    { "text/html", "html", "htm", "shtml" },
    { "text/plain", "txt", "text", "log" },
    { "text/css", "css" },
    { "text/csv", "csv" },
    { "text/xml", "xml" },
    { "text/markdown", "md", "markdown" },
    { "text/calendar", "ics" },
    { "text/vcard", "vcf" },
    { "text/javascript", "js", "mjs" },
    { "application/json", "json" },
    { "application/xhtml+xml", "xhtml", "xht" },
    { "application/rss+xml", "rss" },
    { "application/atom+xml", "atom" },
    { "application/pdf", "pdf" },
    { "application/postscript", "ps", "eps", "ai" },
    { "application/rtf", "rtf" },
    { "application/zip", "zip" },
    { "application/gzip", "gz", "tgz" },
    { "application/x-bzip2", "bz2" },
    { "application/x-xz", "xz" },
    { "application/x-tar", "tar" },
    { "application/x-7z-compressed", "7z" },
    { "application/x-apple-diskimage", "dmg" },
    { "application/x-xpinstall", "xpi" },
    { "application/x-shockwave-flash", "swf" },
    { "application/wasm", "wasm" },
    { "application/octet-stream", "bin", "exe", "dll" },
    { "application/msword", "doc" },
    { "application/vnd.openxmlformats-officedocument.wordprocessingml.document", "docx" },
    { "application/vnd.ms-excel", "xls" },
    { "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "xlsx" },
    { "application/vnd.ms-powerpoint", "ppt" },
    { "application/vnd.openxmlformats-officedocument.presentationml.presentation", "pptx" },
    { "application/vnd.oasis.opendocument.text", "odt" },
    { "application/vnd.oasis.opendocument.spreadsheet", "ods" },
    { "application/epub+zip", "epub" },
    { "application/x-bittorrent", "torrent" },
    { "image/png", "png" },
    { "image/jpeg", "jpg", "jpeg", "jpe" },
    { "image/gif", "gif" },
    { "image/webp", "webp" },
    { "image/avif", "avif" },
    { "image/heic", "heic" },
    { "image/bmp", "bmp" },
    { "image/tiff", "tiff", "tif" },
    { "image/svg+xml", "svg", "svgz" },
    { "image/x-icon", "ico" },
    { "image/vnd.microsoft.icon", "ico" },
    { "image/x-icns", "icns" },
    { "audio/mpeg", "mp3" },
    { "audio/mp4", "m4a" },
    { "audio/aac", "aac" },
    { "audio/ogg", "oga", "ogg", "opus" },
    { "audio/wav", "wav" },
    { "audio/x-wav", "wav" },
    { "audio/flac", "flac" },
    { "audio/webm", "weba" },
    { "video/mp4", "mp4", "m4v" },
    { "video/webm", "webm" },
    { "video/ogg", "ogv" },
    { "video/quicktime", "mov", "qt" },
    { "video/x-msvideo", "avi" },
    { "video/x-matroska", "mkv" },
    { "font/ttf", "ttf" },
    { "font/otf", "otf" },
    { "font/woff", "woff" },
    { "font/woff2", "woff2" },
};

@implementation NSURLFileTypeMappings

+ (NSURLFileTypeMappings *)sharedMappings {
    static NSURLFileTypeMappings *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        shared = [[NSURLFileTypeMappings alloc] init];
    });
    return shared;
}

- (void)_addType:(NSString *)type extension:(NSString *)extension {
    NSMutableArray *list;

    type = [type lowercaseString];
    extension = [extension lowercaseString];
    if ([_typeForExtension objectForKey:extension] == nil)
        [_typeForExtension setObject:type forKey:extension];
    list = [_extensionsForType objectForKey:type];
    if (list == nil) {
        list = [NSMutableArray array];
        [_extensionsForType setObject:list forKey:type];
    }
    if (![list containsObject:extension])
        [list addObject:extension];
}

// mime.types lines: a type, then its extensions, # starts a comment
- (void)_readMimeTypes:(NSString *)path {
    NSString *text = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL];
    NSCharacterSet *space = [NSCharacterSet whitespaceCharacterSet];

    for (NSString *line in [text componentsSeparatedByString:@"\n"]) {
        NSMutableArray *words = [NSMutableArray array];

        if ([line hasPrefix:@"#"])
            continue;
        for (NSString *word in [line componentsSeparatedByCharactersInSet:space]) {
            if ([word length])
                [words addObject:word];
        }
        for (NSUInteger i = 1; i < [words count]; i++)
            [self _addType:[words objectAtIndex:0] extension:[words objectAtIndex:i]];
    }
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _typeForExtension = [[NSMutableDictionary alloc] init];
        _extensionsForType = [[NSMutableDictionary alloc] init];
        for (size_t i = 0; i < sizeof(builtinTypes) / sizeof(builtinTypes[0]); i++) {
            for (size_t j = 1; j < 6 && builtinTypes[i][j]; j++)
                [self _addType:[NSString stringWithUTF8String:builtinTypes[i][0]]
                     extension:[NSString stringWithUTF8String:builtinTypes[i][j]]];
        }
        [self _readMimeTypes:@"/etc/mime.types"];
        [self _readMimeTypes:@"/usr/share/mime/mime.types"];
    }
    return self;
}

- (void)dealloc {
    [_typeForExtension release];
    [_extensionsForType release];
    [super dealloc];
}

- (NSString *)MIMETypeForExtension:(NSString *)extension {
    return extension ? [_typeForExtension objectForKey:[extension lowercaseString]] : nil;
}

- (NSArray *)extensionsForMIMEType:(NSString *)type {
    return type ? [[[_extensionsForType objectForKey:[type lowercaseString]] copy] autorelease] : nil;
}

- (NSString *)preferredExtensionForMIMEType:(NSString *)type {
    NSArray *list = [self extensionsForMIMEType:type];
    return [list count] ? [list objectAtIndex:0] : nil;
}

@end
