// Accessibility.c - the user's accessibility display preferences from com.apple.universalaccess
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <CoreFoundation/CoreFoundation.h>

const CFStringRef AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification =
    CFSTR("AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification");
const CFStringRef AXPrefersHorizontalTextLayoutDidChangeNotification = CFSTR("AXPrefersHorizontalTextLayoutDidChangeNotification");

static Boolean preference(CFStringRef key)
{
    Boolean valid = false;
    Boolean value = CFPreferencesGetAppBooleanValue(key, CFSTR("com.apple.universalaccess"), &valid);
    return valid && value;
}

Boolean AXPrefersNonBlinkingTextInsertionIndicator(void)
{
    return preference(CFSTR("prefersNonBlinkingTextInsertionIndicator"));
}

Boolean AXPrefersHorizontalTextLayout(void)
{
    return preference(CFSTR("prefersHorizontalTextLayout"));
}
