//
//  debugging_test.h
//  Copyright (c) 2014 Apple Inc. All Rights Reserved.
//

//
// Interfaces exported for tests for debugging code.
//

#ifndef _SECURITY_UTILITIES_DEBUGGING_TEST_H_
#define _SECURITY_UTILITIES_DEBUGGING_TEST_H_

#include <CoreFoundation/CoreFoundation.h>
#include "utilities/debugging.h"

__BEGIN_DECLS

//
// These would all be static inside
// debugging.c, but unit tests can use them
//

void __security_debug_init(void);

bool IsScopeActive(int level, CFStringRef scope);
bool IsScopeActiveC(int level, const char *scope);

void ApplyScopeListForIDC(const char *scopeList, SecDebugScopeID whichID);

__END_DECLS

#endif
