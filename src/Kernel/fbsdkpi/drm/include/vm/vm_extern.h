// SPDX-License-Identifier: MIT
// FreeBSD's vm/vm_extern.h with what its asserts need
#pragma once
// haiku's vm_extern asserts with KASSERT without including systm itself
#include <sys/param.h>
#include <sys/systm.h>
#include_next <vm/vm_extern.h>
