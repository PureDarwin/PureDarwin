// hello-abort.c - abort() ends the process with SIGABRT, an exit status of 134 in the shell
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <stdlib.h>

int
main(void)
{
    abort();
}
