// spv2wgsl.c - spv2wgsl IN.spv OUT.wgsl: one SPIR-V module to WGSL through pd_spv_to_wgsl, for checking
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include "wgsl.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: spv2wgsl IN.spv OUT.wgsl\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        perror(argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t *words = malloc((size_t)size);
    if (words == NULL || fread(words, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "%s: short read\n", argv[1]);
        return 1;
    }
    fclose(f);
    char err[1024];
    char *wgsl = pd_spv_to_wgsl(words, (size_t)size / 4, err, sizeof(err));
    if (wgsl == NULL) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    f = fopen(argv[2], "w");
    if (f == NULL) {
        perror(argv[2]);
        return 1;
    }
    fputs(wgsl, f);
    fclose(f);
    return 0;
}
