// SPDX-License-Identifier: MIT
// FreeBSD's vm/swap_pager.h, pages are never swapped
#pragma once
// pages are never swapped on PureDarwin
static inline void
swap_pager_status(int *total, int *used)
{
	*total = 0;
	*used = 0;
}
