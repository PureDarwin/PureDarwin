//
//  align_test.c
//  libstuff_test
//
//  Created by Michael Trent on 7/3/19.
//

#include "test_main.h"

#include "stuff/align.h"

#include <mach-o/loader.h>
#include <stdlib.h>
#include <string.h>

/*
 * utilities for working with load commands
 */

static void mh_add_buffer(struct mach_header* mh,
                          struct load_command** lcmds,
                          void* q,
                          uint32_t size)
{
    *lcmds = reallocf(*lcmds, mh->sizeofcmds + size);
    unsigned char* p = (unsigned char*)*lcmds;
    memcpy(&(p[mh->sizeofcmds]), q, size);
    mh->sizeofcmds += size;
}

static void mh_add_load_command(struct mach_header* mh,
                                struct load_command** lcmds,
                                void* lc,
                                uint32_t size)
{
    mh_add_buffer(mh, lcmds, lc, size);
    mh->ncmds += 1;
}

static void mh_add_segment32(struct mach_header* mh,
                             struct load_command** lcmds,
                             uint32_t vmaddr,
                             uint32_t nsects)
{
    struct segment_command sg = { 0 };
    sg.cmd = LC_SEGMENT;
    sg.vmaddr = vmaddr;
    sg.nsects = nsects;
    sg.cmdsize = (sizeof(struct segment_command) +
                  sizeof(struct section) * sg.nsects);

    mh_add_load_command(mh, lcmds, &sg, sizeof(sg));
}

static void mh_add_section32(struct mach_header* mh,
                           struct load_command** lcmds,
                           uint32_t align)
{
    struct section sc = {0};
    sc.align = align;
    mh_add_buffer(mh, lcmds, &sc, sizeof(sc));
}

static void mh_add_segment64(struct mach_header_64* mh,
                             struct load_command** lcmds,
                             uint32_t vmaddr,
                             uint32_t nsects)
{
    struct segment_command_64 sg = { 0 };
    sg.cmd = LC_SEGMENT_64;
    sg.vmaddr = vmaddr;
    sg.nsects = nsects;
    sg.cmdsize = (sizeof(struct segment_command_64) +
                  sizeof(struct section_64) * sg.nsects);

    mh_add_load_command((struct mach_header*)mh, lcmds, &sg, sizeof(sg));
}

static void mh_add_section64(struct mach_header_64* mh,
                             struct load_command** lcmds,
                             uint32_t align)
{
    struct section_64 sc = {0};
    sc.align = align;
    mh_add_buffer((struct mach_header*)mh, lcmds, &sc, sizeof(sc));
}

static void mh_reset(void* mh_void, struct load_command** lcmds)
{
    struct mach_header* mh = (struct mach_header*)mh_void;
    if (*lcmds)
        free(*lcmds);
    *lcmds = NULL;
    mh->ncmds = 0;
    mh->sizeofcmds = 0;
}

/*
 * tests
 */

static void test_get_align_none(void)
{
    // test reading ridiculous or unknown files will return MAXSEGALIGN
    struct mach_header mh32 = {
        MH_MAGIC,
        CPU_TYPE_ARM,
        CPU_SUBTYPE_ARM_V7,
        MH_CORE,
        0,
        0,
        0,
    };
    struct mach_header_64 mh64 = {
        MH_MAGIC_64,
        CPU_TYPE_ARM64,
        CPU_SUBTYPE_ARM64E,
        MH_CORE,
        0,
        0,
        0,
        0,
    };

    uint32_t align;

    align = get_seg_align(&mh32, NULL, NULL, FALSE, sizeof(mh32), "");
    check_uint32("32-bit core alignment", MAXSEGALIGN, align);
    align = get_seg_align(NULL, &mh64, NULL, FALSE, sizeof(mh64), "");
    check_uint32("64-bit core alignment", MAXSEGALIGN, align);

    mh32.filetype = mh64.filetype = MH_OBJECT;
    align = get_seg_align(&mh32, NULL, NULL, FALSE, sizeof(mh32), "");
    check_uint32("32-bit object alignment", MAXSEGALIGN, align);
    align = get_seg_align(NULL, &mh64, NULL, FALSE, sizeof(mh64), "");
    check_uint32("64-bit object alignment", MAXSEGALIGN, align);

    mh32.filetype = mh64.filetype = MH_DYLIB;
    align = get_seg_align(&mh32, NULL, NULL, FALSE, sizeof(mh32), "");
    check_uint32("32-bit dylib alignment", MAXSEGALIGN, align);
    align = get_seg_align(NULL, &mh64, NULL, FALSE, sizeof(mh64), "");
    check_uint32("64-bit dylib alignment", MAXSEGALIGN, align);
}

static void test_get_align_objects_32(void)
{
    // create an MH_OBJECT
    struct mach_header mh = {
        MH_MAGIC,
        CPU_TYPE_ARM,
        CPU_SUBTYPE_ARM_V7,
        MH_OBJECT,
        0,
        0,
        0,
    };
    struct load_command* lcmds = NULL;
    uint32_t align;

    // add two sections, the last being 4
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 2);
    mh_add_section32(&mh, &lcmds, 3);
    mh_add_section32(&mh, &lcmds, 4);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", 4, align);

    // add two sections, the first being 4
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 2);
    mh_add_section32(&mh, &lcmds, 4);
    mh_add_section32(&mh, &lcmds, 2);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", 4, align);

    // add two sections, both with an illegally small alignment.
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 2);
    mh_add_section32(&mh, &lcmds, 1);
    mh_add_section32(&mh, &lcmds, 1);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", MINSEGALIGN32, align);

    // add two sections, both with an illegally large alignment.
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 2);
    mh_add_section32(&mh, &lcmds, 16);
    mh_add_section32(&mh, &lcmds, 19);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", MAXSEGALIGN, align);

    //cleanup
    mh_reset(&mh, &lcmds);
}

static void test_get_align_objects_64(void)
{
    // create an MH_OBJECT
    struct mach_header_64 mh = {
        MH_MAGIC_64,
        CPU_TYPE_ARM64,
        CPU_SUBTYPE_ARM64E,
        MH_OBJECT,
        0,
        0,
        0,
    };
    struct load_command* lcmds = NULL;
    uint32_t align;

    // add two sections, the last being 4
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 2);
    mh_add_section64(&mh, &lcmds, 3);
    mh_add_section64(&mh, &lcmds, 4);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", 4, align);

    // add two sections, the first being 4
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 2);
    mh_add_section64(&mh, &lcmds, 4);
    mh_add_section64(&mh, &lcmds, 3);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", 4, align);

    // add two sections, both with an illegally small alignment.
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 2);
    mh_add_section64(&mh, &lcmds, 2);
    mh_add_section64(&mh, &lcmds, 1);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", MINSEGALIGN64, align);

    // add two sections, both with an illegally large alignment.
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 2);
    mh_add_section64(&mh, &lcmds, 16);
    mh_add_section64(&mh, &lcmds, 19);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", MAXSEGALIGN, align);

    //cleanup
    mh_reset(&mh, &lcmds);
}

static void test_get_align_dylibs_32(void)
{
    // create an MH_DYLIB
    struct mach_header mh = {
        MH_MAGIC,
        CPU_TYPE_ARM,
        CPU_SUBTYPE_ARM_V7,
        MH_DYLIB,
        0,
        0,
        0,
    };
    struct load_command* lcmds = NULL;
    uint32_t align;

    // add 3 segments, the smallest being 1K aligned
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 0);
    mh_add_segment32(&mh, &lcmds, 0x400, 0);
    mh_add_segment32(&mh, &lcmds, 0x1400, 0);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", 10, align);

    // add 3 segments, the smallest being 4K aligned
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 0);
    mh_add_segment32(&mh, &lcmds, 0x1000, 0);
    mh_add_segment32(&mh, &lcmds, 0x4000, 0);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", 12, align);

    // add 3 segments, the smallest being 16K aligned
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 0);
    mh_add_segment32(&mh, &lcmds, 0x4000, 0);
    mh_add_segment32(&mh, &lcmds, 0x10000, 0);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", 14, align);

    // add 3 segments, one with an illegally small alignment
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 0);
    mh_add_segment32(&mh, &lcmds, 0x1, 0);
    mh_add_segment32(&mh, &lcmds, 0x1000, 0);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", MINSEGALIGN32, align);

    // add 3 segments, each with illegally large alignments
    mh_reset(&mh, &lcmds);
    mh_add_segment32(&mh, &lcmds, 0x0, 0);
    mh_add_segment32(&mh, &lcmds, 0x10000, 0);
    mh_add_segment32(&mh, &lcmds, 0x40000, 0);
    align = get_seg_align(&mh, NULL, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("32-bit segment alignment", MAXSEGALIGN, align);

    //cleanup
    mh_reset(&mh, &lcmds);
}

static void test_get_align_dylibs_64(void)
{
    // create an MH_DYLIB
    struct mach_header_64 mh = {
        MH_MAGIC_64,
        CPU_TYPE_ARM64,
        CPU_SUBTYPE_ARM64E,
        MH_DYLIB,
        0,
        0,
        0,
    };
    struct load_command* lcmds = NULL;
    uint32_t align;

    // add 3 segments, the smallest being 1K aligned
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 0);
    mh_add_segment64(&mh, &lcmds, 0x400, 0);
    mh_add_segment64(&mh, &lcmds, 0x1400, 0);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", 10, align);

    // add 3 segments, the smallest being 4K aligned
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 0);
    mh_add_segment64(&mh, &lcmds, 0x1000, 0);
    mh_add_segment64(&mh, &lcmds, 0x4000, 0);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", 12, align);

    // add 3 segments, the smallest being 16K aligned
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 0);
    mh_add_segment64(&mh, &lcmds, 0x4000, 0);
    mh_add_segment64(&mh, &lcmds, 0x10000, 0);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", 14, align);

    // add 3 segments, one with an illegally small alignment
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 0);
    mh_add_segment64(&mh, &lcmds, 0x1, 0);
    mh_add_segment64(&mh, &lcmds, 0x1000, 0);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", MINSEGALIGN64, align);

    // add 3 segments, each with illegally large alignments
    mh_reset(&mh, &lcmds);
    mh_add_segment64(&mh, &lcmds, 0x0, 0);
    mh_add_segment64(&mh, &lcmds, 0x10000, 0);
    mh_add_segment64(&mh, &lcmds, 0x40000, 0);
    align = get_seg_align(NULL, &mh, lcmds, FALSE, sizeof(mh) + mh.sizeofcmds,
                          "");
    check_uint32("64-bit segment alignment", MAXSEGALIGN, align);

    //cleanup
    mh_reset(&mh, &lcmds);
}

static int test_main(void)
{
    int err = 0;

    if (!err) err = test_add("test get_seg_align with bad input",
                             test_get_align_none);
    if (!err) err = test_add("test get_seg_align 32-bit objects",
                             test_get_align_objects_32);
    if (!err) err = test_add("test get_seg_align 64-bit objects",
                             test_get_align_objects_64);
    if (!err) err = test_add("test get_seg_align 32-bit dylibs",
                             test_get_align_dylibs_32);
    if (!err) err = test_add("test get_seg_align 64-bit dylibs",
                             test_get_align_dylibs_64);

    return err;
}
