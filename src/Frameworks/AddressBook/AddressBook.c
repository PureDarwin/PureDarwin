// AddressBook.c - the framework apps link but call nothing from, there being no contacts store
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <CoreFoundation/CoreFoundation.h>

typedef struct __ABAddressBookRef *ABAddressBookRef;

ABAddressBookRef ABGetSharedAddressBook(void)
{
    return NULL;
}
