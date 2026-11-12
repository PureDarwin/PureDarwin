// a690-capture.c - on linux, reads every register PureDarwin's a690 bring-up programs, so the two
// can be diffed with regdiff.py against a pdmsmregs=1 boot log
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
//
// build on the machine: cc -O2 -o a690-capture a690-capture.c
// run: sudo ./a690-capture [--gx] > linux.txt
//
// it reads only, through /dev/mem, so the kernel must allow device memory there (no
// CONFIG_STRICT_DEVMEM, or boot with iomem=relaxed). the windows come from the live device tree
// by compatible, nothing is hard-coded per board. the cx side (gmu, rscc, pdc, smmu) is read only
// while the cx gdsc reports on. --gx adds the gpu's own registers, which answer only while the
// gpu is powered: keep a gl or vulkan program drawing while this runs, a collapsed gx can hang the
// bus instead of returning a value
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../../pd_msm_a6xx_regs.h"

#define DT_ROOT "/sys/firmware/devicetree/base"

// the nodes and the reg entries each window comes from, matched as PDDRMmsm matches them
static const struct {
    const char *compatible;
    int index;
    int window;
} sSources[] = {
    { "qcom,adreno-690.0", 0, W_GPU },
    { "qcom,adreno-gmu-690.0", 0, W_GMU },
    { "qcom,adreno-gmu-690.0", 1, W_RSCC },
    { "qcom,adreno-gmu-690.0", 2, W_PDC },
    { "qcom,adreno-smmu", 0, W_SMMU_GR0 },
    { "qcom,sc8280xp-gpucc", 0, W_GPUCC },
    { "qcom,gcc-sc8280xp", 0, W_GCC },
};

static const char *const sNames[] = { A6XX_REG_WINDOW_NAMES };

struct window {
    uint64_t phys, size;
    volatile uint8_t *va;
    bool found;
    const char *skip;   // why it is not read, NULL to read it
};
static struct window sWin[W_COUNT];
static int sMem = -1;

static int
read_file(const char *path, void *buf, size_t max)
{
    int fd = open(path, O_RDONLY);
    ssize_t n;

    if (fd < 0)
        return (-1);
    n = read(fd, buf, max);
    close(fd);
    return ((int)n);
}

static uint32_t
be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]);
}

// #address-cells or #size-cells of the node's parent, with the spec's defaults
static int
parent_cells(const char *node, const char *prop, int def)
{
    char path[4096], copy[4096];
    uint8_t v[4];

    snprintf(copy, sizeof(copy), "%s", node);
    snprintf(path, sizeof(path), "%s/%s", dirname(copy), prop);
    return (read_file(path, v, 4) == 4 ? (int)be32(v) : def);
}

static bool
compatible_with(const char *node, const char *want)
{
    char path[4096], buf[1024];
    int n;

    snprintf(path, sizeof(path), "%s/compatible", node);
    if ((n = read_file(path, buf, sizeof(buf) - 1)) <= 0)
        return (false);
    buf[n] = 0;
    for (char *s = buf; s < buf + n; s += strlen(s) + 1)
        if (strcmp(s, want) == 0)
            return (true);
    return (false);
}

static void
take_reg(const char *node, int index, int window)
{
    char path[4096];
    uint8_t reg[256];
    int ac = parent_cells(node, "#address-cells", 2), sc = parent_cells(node, "#size-cells", 1);
    int n, stride = (ac + sc) * 4;
    uint64_t addr = 0, size = 0;

    snprintf(path, sizeof(path), "%s/reg", node);
    n = read_file(path, reg, sizeof(reg));
    if (n < (index + 1) * stride)
        return;
    for (int i = 0; i < ac; i++)
        addr = addr << 32 | be32(reg + index * stride + i * 4);
    for (int i = 0; i < sc; i++)
        size = size << 32 | be32(reg + index * stride + ac * 4 + i * 4);
    if (sWin[window].found) {
        fprintf(stderr, "a690-capture: a second %s node at %s, the first one is used\n",
            sNames[window], node);
        return;
    }
    sWin[window].phys = addr;
    sWin[window].size = size;
    sWin[window].found = true;
}

static int
visit(const char *path, const struct stat *st, int type, struct FTW *ftw)
{
    if (type != FTW_D)
        return (0);
    for (size_t i = 0; i < sizeof(sSources) / sizeof(sSources[0]); i++)
        if (compatible_with(path, sSources[i].compatible))
            take_reg(path, sSources[i].index, sSources[i].window);
    return (0);
}

static volatile uint8_t *
map(uint64_t phys, uint64_t size)
{
    long pg = sysconf(_SC_PAGESIZE);
    uint64_t base = phys & ~(uint64_t)(pg - 1);
    size_t len = (size_t)(phys - base + size + pg - 1) & ~(size_t)(pg - 1);
    void *p = mmap(NULL, len, PROT_READ, MAP_SHARED, sMem, (off_t)base);

    if (p == MAP_FAILED) {
        fprintf(stderr, "a690-capture: mmap 0x%llx: %s\n", (unsigned long long)phys, strerror(errno));
        return (NULL);
    }
    return ((volatile uint8_t *)p + (phys - base));
}

static uint32_t
rd(int w, uint32_t off)
{
    return (*(volatile uint32_t *)(sWin[w].va + off));
}

int
main(int argc, char **argv)
{
    bool gx = argc > 1 && strcmp(argv[1], "--gx") == 0;

    if (nftw(DT_ROOT, visit, 32, FTW_PHYS) != 0) {
        fprintf(stderr, "a690-capture: cannot walk %s\n", DT_ROOT);
        return (1);
    }
    if ((sMem = open("/dev/mem", O_RDONLY | O_SYNC)) < 0) {
        fprintf(stderr, "a690-capture: /dev/mem: %s (root, and device memory allowed?)\n",
            strerror(errno));
        return (1);
    }
    for (int w = 0; w < W_COUNT; w++) {
        if (w == W_SMMU_GR1 || w == W_SMMU_CB0 || w == W_SMMU_CB1)
            continue;
        if (!sWin[w].found) {
            sWin[w].skip = "no node";
            continue;
        }
        if ((sWin[w].va = map(sWin[w].phys, sWin[w].size)) == NULL)
            sWin[w].skip = "not mapped";
        fprintf(stderr, "a690-capture: %s at 0x%llx +0x%llx\n", sNames[w],
            (unsigned long long)sWin[w].phys, (unsigned long long)sWin[w].size);
    }

    // the cx windows only while the cx gdsc is on, the gx one only when asked for
    bool cx = sWin[W_GPUCC].va != NULL && (rd(W_GPUCC, 0x1540) & 0x80000000u) != 0;
    if (!cx) {
        fprintf(stderr, "a690-capture: cx gdsc is off (or gpucc unreadable): gmu, rscc, pdc and "
            "smmu skipped. keep the gpu busy and run again\n");
        for (int w = W_GMU; w <= W_SMMU_CB1; w++)
            if (sWin[w].skip == NULL)
                sWin[w].skip = "cx off";
    }
    if (!gx && sWin[W_GPU].skip == NULL)
        sWin[W_GPU].skip = "gx not asked for";
    if (gx && !cx && sWin[W_GPU].skip == NULL)
        sWin[W_GPU].skip = "cx off";

    // the smmu's global space 1 and banks, where its idr1 puts them (as smmu_reset finds them)
    if (sWin[W_SMMU_GR0].skip == NULL) {
        uint32_t idr1 = rd(W_SMMU_GR0, 0x024);
        uint32_t pagesize = (idr1 & (1u << 31)) ? 0x10000 : 0x1000;
        uint32_t npages = 1u << (((idr1 >> 28) & 7) + 1);

        sWin[W_SMMU_GR1].va = sWin[W_SMMU_GR0].va + pagesize;
        sWin[W_SMMU_CB0].va = sWin[W_SMMU_GR0].va + (uint64_t)npages * pagesize;
        sWin[W_SMMU_CB1].va = sWin[W_SMMU_CB0].va + pagesize;
        if ((uint64_t)npages * pagesize + 2 * pagesize > sWin[W_SMMU_GR0].size)
            sWin[W_SMMU_CB0].skip = sWin[W_SMMU_CB1].skip = "outside the node's reg";
    } else
        sWin[W_SMMU_GR1].skip = sWin[W_SMMU_CB0].skip = sWin[W_SMMU_CB1].skip = sWin[W_SMMU_GR0].skip;

#define CAPTURE(w, off, name) \
    if (sWin[w].skip != NULL) \
        printf("%s 0x%05x skipped %s (%s)\n", sNames[w], (unsigned)(off), name, sWin[w].skip); \
    else \
        printf("%s 0x%05x 0x%08x %s\n", sNames[w], (unsigned)(off), rd(w, off), name);
    A6XX_REGS(CAPTURE)
#undef CAPTURE
    return (0);
}
