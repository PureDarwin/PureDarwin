// UTType.m - the system's declared types with their parents, extensions and MIME types
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// a tag nothing declares gets a dynamic type

#define UT_BUILDING 1
#import "UTType.h"

typedef struct {
    const char *identifier;
    const char *description;
    const char *parents;     // space separated
    const char *extensions;  // space separated, the preferred one first
    const char *mime;
} UTDeclaration;

static const UTDeclaration declarations[] = {
    { "public.item", "item", "", "", NULL },
    { "public.content", "content", "", "", NULL },
    { "public.composite-content", "composite content", "public.content", "", NULL },
    { "public.data", "data", "public.item", "", "application/octet-stream" },
    { "public.directory", "directory", "public.item", "", NULL },
    { "public.folder", "folder", "public.directory", "", NULL },
    { "public.volume", "volume", "public.folder", "", NULL },
    { "com.apple.package", "package", "public.directory", "", NULL },
    { "com.apple.bundle", "bundle", "public.directory", "bundle", NULL },
    { "com.apple.application-bundle", "application", "com.apple.bundle com.apple.package public.executable", "app", NULL },
    { "public.executable", "executable", "public.item", "", NULL },
    { "public.text", "text", "public.data public.content", "", "text/plain" },
    { "public.plain-text", "plain text", "public.text", "txt text", "text/plain" },
    { "public.utf8-plain-text", "UTF-8 plain text", "public.plain-text", "", "text/plain;charset=utf-8" },
    { "public.rtf", "rich text", "public.text", "rtf", "text/rtf" },
    { "public.html", "HTML", "public.text", "html htm", "text/html" },
    { "public.xml", "XML", "public.text", "xml", "application/xml" },
    { "public.source-code", "source code", "public.plain-text", "", NULL },
    { "public.json", "JSON", "public.text", "json", "application/json" },
    { "com.apple.property-list", "property list", "public.data", "plist", NULL },
    { "com.apple.xml-property-list", "XML property list", "public.xml com.apple.property-list", "plist", NULL },
    { "com.apple.binary-property-list", "binary property list", "com.apple.property-list", "", NULL },
    { "com.adobe.pdf", "PDF document", "public.data public.composite-content", "pdf", "application/pdf" },
    { "public.url", "URL", "public.data", "", NULL },
    { "public.file-url", "file URL", "public.url", "", NULL },
    { "public.image", "image", "public.data public.content", "", NULL },
    { "public.png", "PNG image", "public.image", "png", "image/png" },
    { "public.jpeg", "JPEG image", "public.image", "jpeg jpg jpe", "image/jpeg" },
    { "com.compuserve.gif", "GIF image", "public.image", "gif", "image/gif" },
    { "public.tiff", "TIFF image", "public.image", "tiff tif", "image/tiff" },
    { "com.microsoft.bmp", "Windows bitmap image", "public.image", "bmp", "image/bmp" },
    { "com.apple.icns", "Apple icon image", "public.image", "icns", "image/x-icns" },
    { "public.svg-image", "SVG image", "public.image public.xml", "svg", "image/svg+xml" },
    { "public.heic", "HEIC image", "public.image", "heic", "image/heic" },
    { "public.audiovisual-content", "audiovisual content", "public.data public.content", "", NULL },
    { "public.movie", "movie", "public.audiovisual-content", "", NULL },
    { "public.video", "video", "public.movie", "", NULL },
    { "public.audio", "audio", "public.audiovisual-content", "", NULL },
    { "com.apple.quicktime-movie", "QuickTime movie", "public.movie", "mov qt", "video/quicktime" },
    { "public.mpeg-4", "MPEG-4 movie", "public.movie", "mp4 mpeg4", "video/mp4" },
    { "public.mpeg-4-audio", "MPEG-4 audio", "public.mpeg-4 public.audio", "m4a", "audio/mp4" },
    { "public.mp3", "MP3 audio", "public.audio", "mp3", "audio/mpeg" },
    { "com.microsoft.waveform-audio", "WAVE audio", "public.audio", "wav wave", "audio/vnd.wave" },
    { "public.archive", "archive", "public.data", "", NULL },
    { "public.zip-archive", "ZIP archive", "public.archive", "zip", "application/zip" },
    { "org.gnu.gnu-zip-archive", "gzip archive", "public.archive", "gz gzip", "application/x-gzip" },
    { "public.font", "font", "public.data public.content", "", NULL },
    { "public.log", "log", "public.plain-text", "log", NULL },
    { "com.pixar.universal-scene-description", "Universal Scene Description", "public.3d-content public.data", "usd usdc usda", "model/vnd.usd" },
    { "public.3d-content", "3D content", "public.content", "", NULL },
};

#define COUNT (sizeof(declarations) / sizeof(declarations[0]))

@interface UTType () {
    NSString *_identifier;
    const UTDeclaration *_declaration;
    NSString *_extension, *_mime;
}
@end

static NSMutableDictionary *cache;

static NSArray *words(const char *list) {
    NSString *string = [NSString stringWithUTF8String:list];
    NSMutableArray *result = [NSMutableArray array];

    for (NSString *word in [string componentsSeparatedByString:@" "]) {
        if ([word length] != 0)
            [result addObject:word];
    }
    return result;
}

@implementation UTType

+ (instancetype)_typeForIdentifier:(NSString *)identifier declaration:(const UTDeclaration *)declaration
                         extension:(NSString *)extension mime:(NSString *)mime {
    @synchronized ([UTType class]) {
        if (cache == nil)
            cache = [[NSMutableDictionary alloc] init];
        UTType *type = [cache objectForKey:identifier];
        if (type == nil) {
            type = [[[UTType alloc] init] autorelease];
            type->_identifier = [identifier copy];
            type->_declaration = declaration;
            type->_extension = [extension copy];
            type->_mime = [mime copy];
            [cache setObject:type forKey:identifier];
        }
        return type;
    }
}

static const UTDeclaration *declarationFor(NSString *identifier) {
    for (size_t i = 0; i < COUNT; i++) {
        if ([identifier caseInsensitiveCompare:[NSString stringWithUTF8String:declarations[i].identifier]] == NSOrderedSame)
            return &declarations[i];
    }
    return NULL;
}

+ (instancetype)typeWithIdentifier:(NSString *)identifier {
    const UTDeclaration *declaration = declarationFor(identifier);

    if (declaration != NULL)
        return [self _typeForIdentifier:[NSString stringWithUTF8String:declaration->identifier]
                            declaration:declaration extension:nil mime:nil];
    if ([identifier hasPrefix:@"dyn."])
        return [self _typeForIdentifier:identifier declaration:NULL extension:nil mime:nil];
    // an identifier nobody declared is still a valid type, which conforms to nothing but itself
    return [identifier length] != 0 ? [self _typeForIdentifier:identifier declaration:NULL extension:nil mime:nil] : nil;
}

+ (instancetype)typeWithFilenameExtension:(NSString *)filenameExtension {
    return [self typeWithFilenameExtension:filenameExtension conformingToType:UTTypeData];
}

+ (instancetype)typeWithFilenameExtension:(NSString *)filenameExtension conformingToType:(UTType *)supertype {
    if ([filenameExtension length] == 0)
        return nil;
    for (size_t i = 0; i < COUNT; i++) {
        for (NSString *extension in words(declarations[i].extensions)) {
            if ([extension caseInsensitiveCompare:filenameExtension] != NSOrderedSame)
                continue;
            UTType *type = [self typeWithIdentifier:[NSString stringWithUTF8String:declarations[i].identifier]];
            if (supertype == nil || [type conformsToType:supertype])
                return type;
        }
    }
    // nothing declares it: a dynamic type that carries the extension
    NSString *identifier = [NSString stringWithFormat:@"dyn.ext.%@", [filenameExtension lowercaseString]];
    return [self _typeForIdentifier:identifier declaration:NULL extension:filenameExtension mime:nil];
}

+ (instancetype)typeWithMIMEType:(NSString *)mimeType {
    return [self typeWithMIMEType:mimeType conformingToType:UTTypeData];
}

+ (instancetype)typeWithMIMEType:(NSString *)mimeType conformingToType:(UTType *)supertype {
    if ([mimeType length] == 0)
        return nil;
    for (size_t i = 0; i < COUNT; i++) {
        if (declarations[i].mime == NULL || [mimeType caseInsensitiveCompare:[NSString stringWithUTF8String:declarations[i].mime]] != NSOrderedSame)
            continue;
        UTType *type = [self typeWithIdentifier:[NSString stringWithUTF8String:declarations[i].identifier]];
        if (supertype == nil || [type conformsToType:supertype])
            return type;
    }
    NSString *identifier = [NSString stringWithFormat:@"dyn.mime.%@", [mimeType lowercaseString]];
    return [self _typeForIdentifier:identifier declaration:NULL extension:nil mime:mimeType];
}

- (void)dealloc {
    [_identifier release];
    [_extension release];
    [_mime release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_identifier forKey:@"UTTypeIdentifier"];
}

- (id)initWithCoder:(NSCoder *)coder {
    NSString *identifier = [coder decodeObjectForKey:@"UTTypeIdentifier"];

    [self release];
    return [[UTType typeWithIdentifier:identifier] retain];
}

- (NSString *)identifier {
    return _identifier;
}

- (NSString *)preferredFilenameExtension {
    if (_extension != nil)
        return _extension;
    return _declaration != NULL ? [words(_declaration->extensions) firstObject] : nil;
}

- (NSString *)preferredMIMEType {
    if (_mime != nil)
        return _mime;
    return _declaration != NULL && _declaration->mime != NULL ? [NSString stringWithUTF8String:_declaration->mime] : nil;
}

- (NSString *)localizedDescription {
    return _declaration != NULL ? [NSString stringWithUTF8String:_declaration->description] : nil;
}

- (NSDictionary *)tags {
    NSMutableDictionary *tags = [NSMutableDictionary dictionary];
    NSArray *extensions = _declaration != NULL ? words(_declaration->extensions) : (_extension ? @[ _extension ] : @[]);

    if ([extensions count] != 0)
        [tags setObject:extensions forKey:@"public.filename-extension"];
    if ([self preferredMIMEType] != nil)
        [tags setObject:@[ [self preferredMIMEType] ] forKey:@"public.mime-type"];
    return tags;
}

- (NSSet *)supertypes {
    NSMutableSet *result = [NSMutableSet set];
    NSMutableArray *pending = [NSMutableArray arrayWithObject:self];

    // dynamic types from a tag sit under public.data or public.item
    if (_declaration == NULL && [_identifier hasPrefix:@"dyn."])
        [pending addObject:UTTypeData];
    while ([pending count] != 0) {
        UTType *type = [pending lastObject];
        [pending removeLastObject];
        if (type->_declaration == NULL)
            continue;
        for (NSString *parent in words(type->_declaration->parents)) {
            UTType *supertype = [UTType typeWithIdentifier:parent];
            if (![result containsObject:supertype]) {
                [result addObject:supertype];
                [pending addObject:supertype];
            }
        }
    }
    if (_declaration == NULL && [_identifier hasPrefix:@"dyn."])
        [result addObject:UTTypeData];
    return result;
}

- (BOOL)conformsToType:(UTType *)type {
    return type != nil && (self == type || [[self supertypes] containsObject:type]);
}

- (BOOL)isSubtypeOfType:(UTType *)type {
    return self != type && [self conformsToType:type];
}

- (BOOL)isSupertypeOfType:(UTType *)type {
    return [type isSubtypeOfType:self];
}

- (BOOL)isDeclared {
    return _declaration != NULL;
}

- (BOOL)isDynamic {
    return [_identifier hasPrefix:@"dyn."];
}

- (BOOL)isPublicType {
    return [_identifier hasPrefix:@"public."];
}

- (NSString *)description {
    return _identifier;
}

@end

#define UT_DEFINE(name, identifier) UTType *UTType##name;
UT_TYPES(UT_DEFINE)
#undef UT_DEFINE

// the constants are objects, so they are filled in once the runtime knows the class
__attribute__((constructor))
static void UTTypeConstantsInit(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

#define UT_SET(name, identifier) UTType##name = [[UTType typeWithIdentifier:@identifier] retain];
    UT_TYPES(UT_SET)
#undef UT_SET
    [pool release];
}
