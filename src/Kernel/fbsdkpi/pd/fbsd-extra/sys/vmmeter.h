// SPDX-License-Identifier: MIT
// FreeBSD's sys/vmmeter.h, memory is never short
#pragma once
static inline int vm_page_count_severe(void) { return 0; }
static inline int vm_page_count_min(void) { return 0; }
