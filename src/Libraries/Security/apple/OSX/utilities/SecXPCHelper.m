//
//  SecXPCHelper.m
//  Security
//

#import <Foundation/Foundation.h>
#import <objc/objc-class.h>
#import "SecXPCHelper.h"

@implementation SecXPCHelper : NSObject

+ (NSSet<Class> *)safeErrorPrimitiveClasses
{
    static NSMutableSet<Class> *errorClasses = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        errorClasses = [NSMutableSet set];
        char *classes[] = {
            "NSData",
            "NSDate",
            "NSNull",
            "NSNumber",
            "NSString",
            "NSURL",
        };

        for (unsigned n = 0; n < sizeof(classes) / sizeof(classes[0]); n++) {
            Class class = objc_getClass(classes[n]);
            if (class) {
                [errorClasses addObject:class];
            }
        }
    });

    return errorClasses;
}

+ (NSSet<Class> *)safeCKErrorPrimitiveClasses
{
    static NSMutableSet<Class> *errorClasses = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        errorClasses = [NSMutableSet set];
        char *classes[] = {
            "CKArchivedAnchoredPackage",
            "CKAsset",
            "CKPackage",
            "CKRecordID",
            "CKReference",
            "CLLocation",
        };

        for (unsigned n = 0; n < sizeof(classes) / sizeof(classes[0]); n++) {
            Class class = objc_getClass(classes[n]);
            if (class) {
                [errorClasses addObject:class];
            }
        }
    });

    return errorClasses;
}

+ (NSSet<Class> *)safeErrorCollectionClasses
{
    static NSMutableSet<Class> *errorClasses = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        errorClasses = [NSMutableSet set];
        char *classes[] = {
            "NSArray",
            "NSDictionary",
            "NSError",
            "NSOrderedSet",
            "NSSet",
            "NSURLError",
        };

        for (unsigned n = 0; n < sizeof(classes) / sizeof(classes[0]); n++) {
            Class class = objc_getClass(classes[n]);
            if (class) {
                [errorClasses addObject:class];
            }
        }
    });

    return errorClasses;
}

+ (NSSet<Class> *)safeErrorClasses
{
    static NSMutableSet<Class> *errorClasses = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        errorClasses = [NSMutableSet set];
        for (Class class in [SecXPCHelper safeErrorPrimitiveClasses]) {
            [errorClasses addObject:class];
        }
        for (Class class in [SecXPCHelper safeCKErrorPrimitiveClasses]) {
            [errorClasses addObject:class];
        }
        for (Class class in [SecXPCHelper safeErrorCollectionClasses]) {
            [errorClasses addObject:class];
        }
    });

    return errorClasses;
}

+ (NSDictionary *)cleanDictionaryForXPC:(NSDictionary *)dict
{
    if (!dict) {
        return nil;
    }

    NSMutableDictionary *mutableDictionary = [dict mutableCopy];
    for (id key in mutableDictionary.allKeys) {
        id object = mutableDictionary[key];
        mutableDictionary[key] = [SecXPCHelper cleanObjectForXPC:object];
    }
    return mutableDictionary;
}

+ (id)cleanObjectForXPC:(id)object
{
    if (!object) {
        return nil;
    }

    // Check for primitive classes first, and return them as is
    for (Class class in [SecXPCHelper safeErrorPrimitiveClasses]) {
        if ([object isKindOfClass:class]) {
            return object;
        }
    }

    // Else, check for known collection classes. We only handle those collection classes we whitelist as
    // safe, as per the result of `[SecXPCHelper safeErrorCollectionClasses]`. For each collection class,
    // we also handle the contents uniquely, since not all collections share the same APIs.
    for (Class class in [SecXPCHelper safeErrorCollectionClasses]) {
        if ([object isKindOfClass:class]) {
            if ([object isKindOfClass:[NSError class]]) {
                NSError *errorObject = (NSError *)object;
                return [NSError errorWithDomain:errorObject.domain code:errorObject.code userInfo:[SecXPCHelper cleanDictionaryForXPC:errorObject.userInfo]];
            } if ([object isKindOfClass:[NSDictionary class]]) {
                return [SecXPCHelper cleanDictionaryForXPC:(NSDictionary *)object];
            } else if ([object isKindOfClass:[NSArray class]]) {
                NSArray *arrayObject = (NSArray *)object;
                NSMutableArray* cleanArray = [NSMutableArray arrayWithCapacity:arrayObject.count];
                for (id x in arrayObject) {
                    [cleanArray addObject:[SecXPCHelper cleanObjectForXPC:x]];
                }
                return cleanArray;
            } else if ([object isKindOfClass:[NSSet class]]) {
                NSSet *setObject = (NSSet *)object;
                NSMutableSet *cleanSet = [NSMutableSet setWithCapacity:setObject.count];
                for (id x in setObject) {
                    [cleanSet addObject:[SecXPCHelper cleanObjectForXPC:x]];
                }
                return cleanSet;
            } else if ([object isKindOfClass:[NSOrderedSet class]]) {
                NSOrderedSet *setObject = (NSOrderedSet *)object;
                NSMutableOrderedSet *cleanSet = [NSMutableOrderedSet orderedSetWithCapacity:setObject.count];
                for (id x in setObject) {
                    [cleanSet addObject:[SecXPCHelper cleanObjectForXPC:x]];
                }
                return cleanSet;
            }
        }
    }

    // If all else fails, just return the object's class description
    return NSStringFromClass([object class]);
}

+ (NSError * _Nullable)cleanseErrorForXPC:(NSError * _Nullable)error
{
    if (!error) {
        return nil;
    }

    NSDictionary<NSErrorUserInfoKey, id> *userInfo = [SecXPCHelper cleanDictionaryForXPC:error.userInfo];
    return [NSError errorWithDomain:error.domain code:error.code userInfo:userInfo];
}

static NSString *kArchiveKeyError = @"error";

+ (NSError * _Nullable)errorFromEncodedData:(NSData *)data
{
    NSKeyedUnarchiver *unarchiver = nil;
    NSError *error = nil;

    unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:NULL];
    if (unarchiver != nil) {
        error = [unarchiver decodeObjectOfClass:[NSError class] forKey:kArchiveKeyError];
    }

    return error;
}

+ (NSData *)encodedDataFromError:(NSError *)error
{
    NSKeyedArchiver *archiver = nil;
    NSData *data = nil;

    archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [archiver encodeObject:error forKey:kArchiveKeyError];
    data = archiver.encodedData;

    return data;
}

@end
