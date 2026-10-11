// UTType.h - uniform type identifiers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>

@interface UTType : NSObject <NSCopying, NSCoding>

+ (instancetype)typeWithIdentifier:(NSString *)identifier;
+ (instancetype)typeWithFilenameExtension:(NSString *)filenameExtension;
+ (instancetype)typeWithFilenameExtension:(NSString *)filenameExtension conformingToType:(UTType *)supertype;
+ (instancetype)typeWithMIMEType:(NSString *)mimeType;
+ (instancetype)typeWithMIMEType:(NSString *)mimeType conformingToType:(UTType *)supertype;

@property (readonly, copy) NSString *identifier;
@property (readonly, copy) NSString *preferredFilenameExtension;
@property (readonly, copy) NSString *preferredMIMEType;
@property (readonly, copy) NSString *localizedDescription;
@property (readonly, copy) NSDictionary *tags;
@property (readonly, copy) NSSet *supertypes;
@property (readonly, getter=isDeclared) BOOL declared;
@property (readonly, getter=isDynamic) BOOL dynamic;
@property (readonly, getter=isPublicType) BOOL publicType;

- (BOOL)conformsToType:(UTType *)type;
- (BOOL)isSubtypeOfType:(UTType *)type;
- (BOOL)isSupertypeOfType:(UTType *)type;

@end

#define UT_TYPES(X) \
    X(Item, "public.item") X(Content, "public.content") X(CompositeContent, "public.composite-content") \
    X(Data, "public.data") X(Directory, "public.directory") X(Folder, "public.folder") \
    X(Volume, "public.volume") X(Package, "com.apple.package") X(Bundle, "com.apple.bundle") \
    X(ApplicationBundle, "com.apple.application-bundle") X(Executable, "public.executable") \
    X(Text, "public.text") X(PlainText, "public.plain-text") X(UTF8PlainText, "public.utf8-plain-text") \
    X(RTF, "public.rtf") X(HTML, "public.html") X(XML, "public.xml") X(SourceCode, "public.source-code") \
    X(JSON, "public.json") X(PropertyList, "com.apple.property-list") X(XMLPropertyList, "com.apple.xml-property-list") \
    X(BinaryPropertyList, "com.apple.binary-property-list") X(PDF, "com.adobe.pdf") X(URL, "public.url") \
    X(FileURL, "public.file-url") X(Image, "public.image") X(PNG, "public.png") X(JPEG, "public.jpeg") \
    X(GIF, "com.compuserve.gif") X(TIFF, "public.tiff") X(BMP, "com.microsoft.bmp") X(ICNS, "com.apple.icns") \
    X(SVG, "public.svg-image") X(HEIC, "public.heic") X(AudiovisualContent, "public.audiovisual-content") \
    X(Movie, "public.movie") X(Video, "public.video") X(Audio, "public.audio") \
    X(QuickTimeMovie, "com.apple.quicktime-movie") X(MPEG4Movie, "public.mpeg-4") \
    X(MPEG4Audio, "public.mpeg-4-audio") X(MP3, "public.mp3") X(WAV, "com.microsoft.waveform-audio") \
    X(Archive, "public.archive") X(ZIP, "public.zip-archive") X(GZIP, "org.gnu.gnu-zip-archive") \
    X(Font, "public.font") X(Log, "public.log") X(USD, "com.pixar.universal-scene-description") \
    X(ThreeDContent, "public.3d-content")

// the framework fills these in as it loads, so only users see them as constant
#ifdef UT_BUILDING
#define UT_DECLARE(name, identifier) FOUNDATION_EXPORT UTType *UTType##name;
#else
#define UT_DECLARE(name, identifier) FOUNDATION_EXPORT UTType *const UTType##name;
#endif
UT_TYPES(UT_DECLARE)
#undef UT_DECLARE
