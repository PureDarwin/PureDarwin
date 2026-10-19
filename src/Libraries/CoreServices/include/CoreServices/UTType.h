// UTType.h - the C uniform type identifier API LaunchServices exports
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// the C uniform type identifier API LaunchServices exports
#ifndef __CORESERVICES_UTTYPE__
#define __CORESERVICES_UTTYPE__

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

CF_EXPORT const CFStringRef kUTTagClassFilenameExtension;
CF_EXPORT const CFStringRef kUTTagClassMIMEType;
CF_EXPORT const CFStringRef kUTTagClassNSPboardType;
CF_EXPORT const CFStringRef kUTTagClassOSType;

CF_EXPORT const CFStringRef kUTTypeItem;
CF_EXPORT const CFStringRef kUTTypeContent;
CF_EXPORT const CFStringRef kUTTypeData;
CF_EXPORT const CFStringRef kUTTypeDirectory;
CF_EXPORT const CFStringRef kUTTypeFolder;
CF_EXPORT const CFStringRef kUTTypeText;
CF_EXPORT const CFStringRef kUTTypePlainText;
CF_EXPORT const CFStringRef kUTTypeUTF8PlainText;
CF_EXPORT const CFStringRef kUTTypeRTF;
CF_EXPORT const CFStringRef kUTTypeHTML;
CF_EXPORT const CFStringRef kUTTypeXML;
CF_EXPORT const CFStringRef kUTTypeURL;
CF_EXPORT const CFStringRef kUTTypeFileURL;
CF_EXPORT const CFStringRef kUTTypeImage;
CF_EXPORT const CFStringRef kUTTypePNG;
CF_EXPORT const CFStringRef kUTTypeJPEG;
CF_EXPORT const CFStringRef kUTTypeGIF;
CF_EXPORT const CFStringRef kUTTypeTIFF;
CF_EXPORT const CFStringRef kUTTypePDF;
CF_EXPORT const CFStringRef kUTTypeApplicationBundle;
CF_EXPORT const CFStringRef kUTTypeAudio;
CF_EXPORT const CFStringRef kUTTypeMovie;

CF_EXPORT CFStringRef UTTypeCreatePreferredIdentifierForTag(CFStringRef inTagClass, CFStringRef inTag, CFStringRef inConformingToUTI);
CF_EXPORT CFArrayRef UTTypeCreateAllIdentifiersForTag(CFStringRef inTagClass, CFStringRef inTag, CFStringRef inConformingToUTI);
CF_EXPORT CFStringRef UTTypeCopyPreferredTagWithClass(CFStringRef inUTI, CFStringRef inTagClass);
CF_EXPORT Boolean UTTypeEqual(CFStringRef inUTI1, CFStringRef inUTI2);
CF_EXPORT Boolean UTTypeConformsTo(CFStringRef inUTI, CFStringRef inConformsToUTI);
CF_EXPORT CFStringRef UTTypeCopyDescription(CFStringRef inUTI);
CF_EXPORT CFDictionaryRef UTTypeCopyDeclaration(CFStringRef inUTI);
CF_EXPORT Boolean UTTypeIsDynamic(CFStringRef inUTI);
CF_EXPORT Boolean UTTypeIsDeclared(CFStringRef inUTI);

CF_EXTERN_C_END

#endif
