// SPDX-License-Identifier: MIT
// FreeBSD's sys/capsicum.h, no capability mode
#pragma once
// no capability mode here
#define cap_rights_init_one(r, c) (r)
struct cap_rights { int pd_unused; };
typedef struct cap_rights cap_rights_t;
extern cap_rights_t cap_no_rights;
