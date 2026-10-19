// FileManager.h - the parts of the Carbon File Manager and Folder Manager still in use
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// the parts of the Carbon File Manager and Folder Manager still in use
#ifndef __CORESERVICES_FILEMANAGER__
#define __CORESERVICES_FILEMANAGER__

#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

typedef SInt16 FSVolumeRefNum;

typedef struct FSRef {
    UInt8 hidden[80];
} FSRef;
typedef FSRef *FSRefPtr;

enum {
    ioErr = -36,
    bdNamErr = -37,
    fnfErr = -43,
    paramErr = -50,
    permErr = -54,
    memFullErr = -108,
    diffVolErr = -1303,
    errFSRefsDifferent = -1420,
    pathTooLongErr = -2110,
};

enum {
    kOnAppropriateDisk = -32767,
    kSystemDomain = -32766,
    kLocalDomain = -32765,
    kNetworkDomain = -32764,
    kUserDomain = -32763,
};

enum {
    kSystemFolderType = 'macs',
    kDesktopFolderType = 'desk',
    kTrashFolderType = 'trsh',
    kPreferencesFolderType = 'pref',
    kTemporaryFolderType = 'temp',
    kChewableItemsFolderType = 'flnt',
    kApplicationSupportFolderType = 'asup',
    kFontsFolderType = 'font',
    kInternetPlugInFolderType = 0xC46E6574, // 'ƒnet'
    kDocumentsFolderType = 'docs',
    kCurrentUserFolderType = 'cusr',
    kApplicationsFolderType = 'apps',
    kCachedDataFolderType = 'cach',
    kDomainLibraryFolderType = 'dlib',
    kLogsFolderType = 'logs',
    kDownloadsFolderType = 'down',
    kMovieDocumentsFolderType = 'mdoc',
    kMusicDocumentsFolderType = 0xB5646F63, // 'µdoc'
    kPictureDocumentsFolderType = 'pdoc',
    kPublicFolderType = 'pubb',
};

enum {
    kFSPathMakeRefDefaultOptions = 0,
    kFSPathMakeRefDoNotFollowLeafSymlink = 0x01,
};

CF_EXPORT OSStatus FSPathMakeRef(const UInt8 *path, FSRef *ref, Boolean *isDirectory);
CF_EXPORT OSStatus FSPathMakeRefWithOptions(const UInt8 *path, OptionBits options, FSRef *ref, Boolean *isDirectory);
CF_EXPORT OSStatus FSRefMakePath(const FSRef *ref, UInt8 *path, UInt32 maxPathSize);
CF_EXPORT OSErr FSCompareFSRefs(const FSRef *ref1, const FSRef *ref2);
CF_EXPORT OSErr FSFindFolder(FSVolumeRefNum vRefNum, OSType folderType, Boolean createFolder, FSRef *foundRef);

CF_EXTERN_C_END

#endif
