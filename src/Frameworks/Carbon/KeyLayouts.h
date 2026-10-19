// KeyLayouts.h - the keyboard layouts Carbon's input sources offer
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PD_KEYLAYOUTS_H
#define PD_KEYLAYOUTS_H

#include <CoreFoundation/CoreFoundation.h>

enum PDKeyLayout {
    kPDKeyLayoutUS,
    kPDKeyLayoutGerman,
    kPDKeyLayoutFrench,
    kPDKeyLayoutCount,
};

// the layout's 'uchr' data, as kTISPropertyUnicodeKeyLayoutData carries it
CFDataRef PDCreateKeyLayoutData(enum PDKeyLayout which);

// the last part of the input source ID, com.apple.keylayout.<name>
const char *PDKeyLayoutName(enum PDKeyLayout which);

#endif
