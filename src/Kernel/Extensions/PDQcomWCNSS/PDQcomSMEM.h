// PDQcomSMEM.h - qualcomm shared memory item lookup, from openbsd sys/dev/fdt/qcsmem.c
// SPDX-License-Identifier: ISC

#ifndef PDQCOMSMEM_H
#define PDQCOMSMEM_H

#include <stdint.h>

namespace PDQcomSMEM {
bool attach(uint64_t pa, uint64_t size);
bool attachAux(uint64_t pa, uint64_t size);
void *get(int host, int item, uint32_t *size);
}

#endif
