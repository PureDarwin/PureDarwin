/*
 * Copyright (c) 2016 Lubos Dolezel
 *
 * This file is part of Darling CoreCrypto.
 *
 * Darling is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CORECRYPTO_CCMD2_H
#define _CORECRYPTO_CCMD2_H

#include <corecrypto/ccdigest.h>

#define CCMD2_BLOCK_SIZE   16
#define CCMD2_OUTPUT_SIZE  16
#define CCMD2_STATE_SIZE   64

extern const uint32_t ccmd2_initial_state[16];

#define ccmd2_di ccmd2_ltc_di
extern const struct ccdigest_info ccmd2_ltc_di;

void ccmd2_final(const struct ccdigest_info* di, ccdigest_ctx_t, unsigned char* digest);

#endif

