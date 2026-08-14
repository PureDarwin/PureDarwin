//
//  SecTrustExceptionResetCount.h
//  Security
//

#ifndef SecTrustExceptionResetCount_h
#define SecTrustExceptionResetCount_h

#include <CoreFoundation/CoreFoundation.h>

bool SecTrustServerIncrementExceptionResetCount(CFErrorRef *error);
uint64_t SecTrustServerGetExceptionResetCount(CFErrorRef *error);
void SecTrustServerMigrateExceptionsResetCount(void);

#endif /* SecTrustExceptionResetCount_h */
