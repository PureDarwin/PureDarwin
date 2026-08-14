//
//  SecTrustExceptionResetCount.m
//  Security_ios
//

#import <Foundation/Foundation.h>
#import "SecTrustExceptionResetCount.h"

#import <utilities/SecCFWrappers.h>
#import <utilities/SecFileLocations.h>
#import "trust/trustd/trustdFileLocations.h"

static NSString *kExceptionResetCountKey = @"ExceptionResetCount";
static NSString *exceptionResetCounterFile = @"com.apple.security.exception_reset_counter.plist";

static NSURL *ExceptionsResetCounterOldUrl (void) {
    return CFBridgingRelease(SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)exceptionResetCounterFile));
}

static NSURL *ExceptionsResetCounterUrl(void) {
    return CFBridgingRelease(SecCopyURLForFileInPrivateUserTrustdDirectory((__bridge CFStringRef)exceptionResetCounterFile));
}

static uint64_t ReadExceptionsCountFromUrl(NSURL *url, CFErrorRef *error) {
    @autoreleasepool {
        uint64_t value = 0;
        NSError *nserror = nil;
        NSMutableDictionary *plDict = [[NSDictionary dictionaryWithContentsOfURL:url error:&nserror] mutableCopy];
        if (!plDict) {
            secerror("Failed to read from permanent storage at '%{public}@' or the data is bad. Defaulting to value %llu.", url, value);
            if (error) {
                *error = (CFErrorRef)CFBridgingRetain(nserror);
            }
            return value;
        }

        id valueObject = [plDict objectForKey:kExceptionResetCountKey];
        if (!valueObject) {
            secinfo("trust", "Could not find key '%{public}@'. Defaulting to value %llu.", kExceptionResetCountKey, value);
            if (error) {
                *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ENXIO, NULL);
            }
            return value;
        }
        if (![valueObject isKindOfClass:[NSNumber class]]) {
            secerror("The value for key '%{public}@' is not a number.", kExceptionResetCountKey);
            if (error) {
                *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, EDOM, NULL);
            }
            return value;
        }

        value = [valueObject unsignedIntValue];

        secinfo("trust", "'%{public}@' is %llu.", kExceptionResetCountKey, value);
        return value;
    }
}

static bool WriteExceptionsCounterToUrl(uint64_t exceptionResetCount, NSURL *url, CFErrorRef *error) {
    @autoreleasepool {
        bool status = false;
        NSMutableDictionary *dataToSave = [NSMutableDictionary new];
        if (!dataToSave) {
            secerror("Failed to allocate memory for the exceptions epoch structure.");
            if (error) {
                *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ENOMEM, NULL);
            }
            return status;
        }
        dataToSave[@"Version"] = [NSNumber numberWithUnsignedInteger:1];
        dataToSave[kExceptionResetCountKey] = [NSNumber numberWithUnsignedInteger:exceptionResetCount];

        NSError *nserror = nil;
        status = [dataToSave writeToClassDURL:url permissions:0600 error:&nserror];
        if (!status) {
            secerror("Failed to write to permanent storage at '%{public}@'.", url);
            if (error) {
                *error = (CFErrorRef)CFBridgingRetain(nserror);
            }
            return status;
        }

        secinfo("trust", "'%{public}@' has been committed to permanent storage at '%{public}@'.", kExceptionResetCountKey, url);
        return status;
    }
}

uint64_t SecTrustServerGetExceptionResetCount(CFErrorRef *error) {
    CFErrorRef localError = NULL;
    uint64_t exceptionResetCount = ReadExceptionsCountFromUrl(ExceptionsResetCounterUrl(), &localError);
    if (localError) {
        if (error) {
            *error = localError;
        } else {
            CFReleaseNull(localError);
        }
    }
    secinfo("trust", "exceptionResetCount: %llu (%s)", exceptionResetCount, error ? (*error ? "Error" : "OK") : "N/A");
    return exceptionResetCount;
}

bool SecTrustServerIncrementExceptionResetCount(CFErrorRef *error) {
    bool status = false;

    uint64_t currentExceptionResetCount = SecTrustServerGetExceptionResetCount(error);
    if (error && *error) {
        secerror("Failed to increment the extensions epoch.");
        return status;
    }
    if (currentExceptionResetCount >= INT64_MAX) {
        secerror("Current exceptions epoch value is too large. (%llu) Won't increment.", currentExceptionResetCount);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ERANGE, NULL);
        }
        return status;
    }

    return WriteExceptionsCounterToUrl(currentExceptionResetCount + 1, ExceptionsResetCounterUrl(), error);
}

void SecTrustServerMigrateExceptionsResetCount(void) {
    CFErrorRef error = NULL;
    (void)ReadExceptionsCountFromUrl(ExceptionsResetCounterUrl(), &error);
    if (!error) {
        secdebug("trust", "already migrated exceptions reset counter");
        return;
    }
    CFReleaseNull(error);
    secdebug("trust", "migrating exceptions reset counter");
    uint64_t currentExceptionResetCount = ReadExceptionsCountFromUrl(ExceptionsResetCounterOldUrl(), NULL);

    if (!WriteExceptionsCounterToUrl(currentExceptionResetCount, ExceptionsResetCounterUrl(), &error)) {
        secerror("Failed to migrate exceptions reset count: %@", error);
        CFReleaseNull(error);
        return;
    }

    WithPathInKeychainDirectory((__bridge CFStringRef)exceptionResetCounterFile, ^(const char *utf8String) {
        remove(utf8String);
    });
}
