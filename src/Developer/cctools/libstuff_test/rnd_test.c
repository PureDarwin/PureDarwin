//
//  rnd_test.c
//  libstuff_test
//
//  Created by Michael Trent on 5/25/19.
//

#include "test_main.h"

#include "stuff/rnd.h"

static void test_rnd64(void)
{
    // weird illegal values
    check_uint64("rnd(0, 0)", 0, rnd64(0,0));
    check_uint64("rnd(1, 0)", 0, rnd64(1,0));
    check_uint64("rnd(1, 1)", 1, rnd64(1,1));
    check_uint64("rnd(2, 1)", 2, rnd64(2,1));
    check_uint64("rnd(1, 3)", 1, rnd64(1,3));
    
    // 4-byte alignment
    check_uint64("rnd(0, 4)", 0, rnd64(0, 4));
    check_uint64("rnd(1, 4)", 4, rnd64(1, 4));
    check_uint64("rnd(2, 4)", 4, rnd64(2, 4));
    check_uint64("rnd(3, 4)", 4, rnd64(3, 4));
    check_uint64("rnd(4, 4)", 4, rnd64(4, 4));
    check_uint64("rnd(5, 4)", 8, rnd64(5, 4));
    
    // 16-byte alignment
    check_uint64("rnd(0, 16)", 0, rnd64(0, 16));
    check_uint64("rnd(1, 16)", 16, rnd64(1, 16));
    check_uint64("rnd(2, 16)", 16, rnd64(2, 16));
    check_uint64("rnd(3, 16)", 16, rnd64(3, 16));
    check_uint64("rnd(4, 16)", 16, rnd64(4, 16));
    check_uint64("rnd(5, 16)", 16, rnd64(5, 16));
    check_uint64("rnd(6, 16)", 16, rnd64(6, 16));
    check_uint64("rnd(7, 16)", 16, rnd64(7, 16));
    check_uint64("rnd(8, 16)", 16, rnd64(8, 16));
    check_uint64("rnd(9, 16)", 16, rnd64(9, 16));
    check_uint64("rnd(10, 16)", 16, rnd64(10, 16));
    check_uint64("rnd(11, 16)", 16, rnd64(11, 16));
    check_uint64("rnd(12, 16)", 16, rnd64(12, 16));
    check_uint64("rnd(13, 16)", 16, rnd64(13, 16));
    check_uint64("rnd(14, 16)", 16, rnd64(14, 16));
    check_uint64("rnd(15, 16)", 16, rnd64(15, 16));
    check_uint64("rnd(16, 16)", 16, rnd64(16, 16));
    check_uint64("rnd(17, 16)", 32, rnd64(17, 16));
    
    // large values around 32-bits
    check_uint64("rnd(0xFFFFFFFF, 16)", 0x100000000, rnd64(0xFFFFFFFF, 16));
    check_uint64("rnd(0x100000000, 16)", 0x100000000, rnd64(0x100000000, 16));
    check_uint64("rnd(0x100000001, 16)", 0x100000010, rnd64(0x100000001, 16));
    check_uint64("rnd(0x1000000F1, 16)", 0x100000100, rnd64(0x1000000F1, 16));
    
    // the bit-buster
    check_uint64("rnd(0xFFFFFFFFFFFFFFF1, 16)", 0, rnd64(0xFFFFFFFFFFFFFFF1, 16));
    check_uint64("rnd(0xFFFFFFFFFFFFFFFF, 16)", 0, rnd64(0xFFFFFFFFFFFFFFFF, 16));
}

static void test_rnd32(void)
{
    // weird illegal values
    check_uint32("rnd(0, 0)", 0, rnd32(0,0));
    check_uint32("rnd(1, 0)", 0, rnd32(1,0));
    check_uint32("rnd(1, 1)", 1, rnd32(1,1));
    check_uint32("rnd(2, 1)", 2, rnd32(2,1));
    check_uint32("rnd(1, 3)", 1, rnd32(1,3));
    
    // 4-byte alignment
    check_uint32("rnd(0, 4)", 0, rnd32(0, 4));
    check_uint32("rnd(1, 4)", 4, rnd32(1, 4));
    check_uint32("rnd(2, 4)", 4, rnd32(2, 4));
    check_uint32("rnd(3, 4)", 4, rnd32(3, 4));
    check_uint32("rnd(4, 4)", 4, rnd32(4, 4));
    check_uint32("rnd(5, 4)", 8, rnd32(5, 4));
    
    // 16-byte alignment
    check_uint32("rnd(0, 16)", 0, rnd32(0, 16));
    check_uint32("rnd(1, 16)", 16, rnd32(1, 16));
    check_uint32("rnd(2, 16)", 16, rnd32(2, 16));
    check_uint32("rnd(3, 16)", 16, rnd32(3, 16));
    check_uint32("rnd(4, 16)", 16, rnd32(4, 16));
    check_uint32("rnd(5, 16)", 16, rnd32(5, 16));
    check_uint32("rnd(6, 16)", 16, rnd32(6, 16));
    check_uint32("rnd(7, 16)", 16, rnd32(7, 16));
    check_uint32("rnd(8, 16)", 16, rnd32(8, 16));
    check_uint32("rnd(9, 16)", 16, rnd32(9, 16));
    check_uint32("rnd(10, 16)", 16, rnd32(10, 16));
    check_uint32("rnd(11, 16)", 16, rnd32(11, 16));
    check_uint32("rnd(12, 16)", 16, rnd32(12, 16));
    check_uint32("rnd(13, 16)", 16, rnd32(13, 16));
    check_uint32("rnd(14, 16)", 16, rnd32(14, 16));
    check_uint32("rnd(15, 16)", 16, rnd32(15, 16));
    check_uint32("rnd(16, 16)", 16, rnd32(16, 16));
    check_uint32("rnd(17, 16)", 32, rnd32(17, 16));
    
    // large values around 32-bits
    check_uint32("rnd(0xFFFFFFF1, 16)", 0, rnd32(0xFFFFFFF1, 16));
    check_uint32("rnd(0xFFFFFFFF, 16)", 0, rnd32(0xFFFFFFFF, 16));
}

static int test_main(void)
{
    int err = 0;
    
    if (!err) err = test_add("test rnd64", test_rnd64);
    if (!err) err = test_add("test rnd32", test_rnd32);

    return err;
}
