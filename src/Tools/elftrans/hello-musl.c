// hello-musl.c - dynamically linked Linux libc smoke test
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <stdio.h>

int
main(void)
{
    puts("hello from musl on XNU");
    return 0;
}
