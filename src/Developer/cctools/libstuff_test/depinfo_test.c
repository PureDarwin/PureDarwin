//
//  depinfo_test.c
//  libstuff_test
//
//  Created by Michael Trent on 9/9/19.
//

#include <stdio.h>

#include "test_main.h"
#include "test_util.h"

#include "stuff/depinfo.h"

static void test_depinfo_alloc(void)
{
  struct depinfo* depinfo = depinfo_alloc();
  if (check_nonnull("depinfo", depinfo))
    return;

  check_uint32("depinfo count", 0, depinfo_count(depinfo));
  depinfo_free(depinfo);
}

static void test_depinfo_add(void)
{
  struct depinfo* depinfo = depinfo_alloc();
  if (check_nonnull("depinfo", depinfo))
    return;

  check_uint32("depinfo count", 0, depinfo_count(depinfo));
  
  depinfo_add(depinfo, DEPINFO_TOOL, "libstuff_test");
  check_uint32("depinfo count", 1, depinfo_count(depinfo));
  depinfo_add(depinfo, DEPINFO_INPUT_FOUND, "/usr/lib/libfoo.a");
  check_uint32("depinfo count", 2, depinfo_count(depinfo));
  depinfo_add(depinfo, DEPINFO_INPUT_MISSING, "/usr/bad/libfoo.a");
  check_uint32("depinfo count", 3, depinfo_count(depinfo));

  depinfo_free(depinfo);
}

static void test_depinfo_get(void)
{
  int err;
  uint8_t opcode;
  const char* string;
  
  struct depinfo* depinfo = depinfo_alloc();
  if (check_nonnull("depinfo", depinfo))
    return;

  check_uint32("depinfo count", 0, depinfo_count(depinfo));
  
  depinfo_add(depinfo, DEPINFO_TOOL, "libstuff_test");
  err = depinfo_get(depinfo, 0, &opcode, &string);
  check_uint32("depinfo get 0 err", 0, err);
  check_uint32("depinfo get 0 opcode", DEPINFO_TOOL, opcode);
  check_string("depinfo get 0 string", "libstuff_test", string);
  
  depinfo_add(depinfo, DEPINFO_INPUT_FOUND, "/usr/lib/libfoo.a");
  err = depinfo_get(depinfo, 1, &opcode, &string);
  check_uint32("depinfo get 1 err", 0, err);
  check_uint32("depinfo get 1 opcode", DEPINFO_INPUT_FOUND, opcode);
  check_string("depinfo get 1 string", "/usr/lib/libfoo.a", string);

  depinfo_add(depinfo, DEPINFO_INPUT_MISSING, "/usr/bad/libfoo.a");
  err = depinfo_get(depinfo, 2, &opcode, &string);
  check_uint32("depinfo get 2 err", 0, err);
  check_uint32("depinfo get 2 opcode", DEPINFO_INPUT_MISSING, opcode);
  check_string("depinfo get 2 string", "/usr/bad/libfoo.a", string);

  err = depinfo_get(depinfo, 3, &opcode, &string);
  check_uint32("depinfo get 3 err", 1, err);

  depinfo_free(depinfo);
}

static void test_depinfo_sort(void)
{
  int err;
  uint8_t opcode;
  const char* string;

  struct depinfo* depinfo = depinfo_alloc();
  if (check_nonnull("depinfo", depinfo))
    return;
  
  depinfo_add(depinfo, DEPINFO_INPUT_MISSING, "/usr/bad/libfoo.a");
  depinfo_add(depinfo, DEPINFO_INPUT_MISSING, "/usr/bad/goo.a");
  depinfo_add(depinfo, DEPINFO_INPUT_FOUND, "/usr/lib/libfoo.a");
  depinfo_add(depinfo, DEPINFO_INPUT_FOUND, "/usr/lib/libbar.a");
  depinfo_add(depinfo, DEPINFO_TOOL, "libstuff_test");
  depinfo_add(depinfo, DEPINFO_INPUT_FOUND, "/usr/lib/libbaz.a");
  check_uint32("depinfo count", 6, depinfo_count(depinfo));

  depinfo_sort(depinfo);
  check_uint32("depinfo count", 6, depinfo_count(depinfo));

  err = depinfo_get(depinfo, 0, &opcode, &string);
  if (!check_uint32("depinfo get 0 err", 0, err)) {
    check_uint32("depinfo get 0 opcode", DEPINFO_TOOL, opcode);
    check_string("depinfo get 0 string", "libstuff_test", string);
  }

  err = depinfo_get(depinfo, 1, &opcode, &string);
  if (!check_uint32("depinfo get 1 err", 0, err)) {
    check_uint32("depinfo get 1 opcode", DEPINFO_INPUT_FOUND, opcode);
    check_string("depinfo get 1 string", "/usr/lib/libbar.a", string);
  }
  err = depinfo_get(depinfo, 2, &opcode, &string);
  if (!check_uint32("depinfo get 2 err", 0, err)) {
    check_uint32("depinfo get 2 opcode", DEPINFO_INPUT_FOUND, opcode);
    check_string("depinfo get 2 string", "/usr/lib/libbaz.a", string);
  }
  err = depinfo_get(depinfo, 3, &opcode, &string);
  if (!check_uint32("depinfo get 3 err", 0, err)) {
    check_uint32("depinfo get 3 opcode", DEPINFO_INPUT_FOUND, opcode);
    check_string("depinfo get 3 string", "/usr/lib/libfoo.a", string);
  }
  err = depinfo_get(depinfo, 4, &opcode, &string);
  if (!check_uint32("depinfo get 4 err", 0, err)) {
    check_uint32("depinfo get 4 opcode", DEPINFO_INPUT_MISSING, opcode);
    check_string("depinfo get 4 string", "/usr/bad/goo.a", string);
  }
  err = depinfo_get(depinfo, 5, &opcode, &string);
  if (!check_uint32("depinfo get 5 err", 0, err)) {
    check_uint32("depinfo get 5 opcode", DEPINFO_INPUT_MISSING, opcode);
    check_string("depinfo get 5 string", "/usr/bad/libfoo.a", string);
  }
  
  err = depinfo_get(depinfo, 6, &opcode, &string);
  check_uint32("depinfo get 6 err", 1, err);

  depinfo_free(depinfo);
}

static void test_depinfo_write_read(void)
{
  int err;

  struct depinfo* depinfo_a = depinfo_alloc();
  struct depinfo* depinfo_b = NULL;
  if (check_nonnull("depinfo_a", depinfo_a))
    return;
  
  depinfo_add(depinfo_a, DEPINFO_INPUT_MISSING, "/usr/bad/libfoo.a");
  depinfo_add(depinfo_a, DEPINFO_INPUT_MISSING, "/usr/bad/goo.a");
  depinfo_add(depinfo_a, DEPINFO_INPUT_FOUND, "/usr/lib/libfoo.a");
  depinfo_add(depinfo_a, DEPINFO_INPUT_FOUND, "/usr/lib/libbar.a");
  depinfo_add(depinfo_a, DEPINFO_TOOL, "libstuff_test");
  depinfo_add(depinfo_a, DEPINFO_INPUT_FOUND, "/usr/lib/libbaz.a");
  check_uint32("depinfo_a count", 6, depinfo_count(depinfo_a));

  depinfo_sort(depinfo_a);
  check_uint32("depinfo_a count", 6, depinfo_count(depinfo_a));

  err = depinfo_write(depinfo_a, "/tmp/depinfo.data");
  check_uint32("depinfo_a write err", 0, err);
  if (!err) {
    depinfo_b = depinfo_read("/tmp/depinfo.data", 0);
    err = check_nonnull("depinfo_b", depinfo_b);
  }
  
  if (!err) {
    int count_a = depinfo_count(depinfo_a);
    int count_b = depinfo_count(depinfo_b);
    check_uint32("depinfo count", count_a, count_b);
    
    int count = count_a < count_b ? count_a : count_b; // min
    for (int i = 0; i < count; ++i) {
      uint8_t opcode_a, opcode_b;
      const char* string_a, *string_b;
      
      if (!depinfo_get(depinfo_a, i, &opcode_a, &string_a) &&
          !depinfo_get(depinfo_b, i, &opcode_b, &string_b))
      {
        check_set_prefix("depinfo [%d]", i);
        check_uint32("opcode", opcode_a, opcode_b);
        check_string("string", string_a, string_b);
      }
    }
    
    depinfo_free(depinfo_b);
  }

  if (!err)
    unlink("/tmp/depinfo.data");

  depinfo_free(depinfo_a);
}

static void test_depinfo_dump(void)
{
  int err;

  struct depinfo* depinfo_a = depinfo_alloc();
  struct depinfo* depinfo_b = NULL;
  if (check_nonnull("depinfo_a", depinfo_a))
    return;

  /* silence stdout ... */
  FILE* errcpy = stdout;
  stdout = fopen("/dev/null", "w");

  depinfo_add(depinfo_a, DEPINFO_INPUT_MISSING, "/usr/bad/libfoo.a");
  depinfo_add(depinfo_a, DEPINFO_INPUT_MISSING, "/usr/bad/goo.a");
  depinfo_add(depinfo_a, DEPINFO_INPUT_FOUND, "/usr/lib/libfoo.a");
  depinfo_add(depinfo_a, DEPINFO_INPUT_FOUND, "/usr/lib/libbar.a");
  depinfo_add(depinfo_a, DEPINFO_TOOL, "libstuff_test");
  depinfo_add(depinfo_a, DEPINFO_INPUT_FOUND, "/usr/lib/libbaz.a");
  check_uint32("depinfo_a count", 6, depinfo_count(depinfo_a));

  depinfo_sort(depinfo_a);
  check_uint32("depinfo_a count", 6, depinfo_count(depinfo_a));

  err = depinfo_write(depinfo_a, "/tmp/depinfo.data");
  check_uint32("depinfo_a write err", 0, err);
  if (!err) {
    /* test_printinfo("\n") */;
    depinfo_b = depinfo_read("/tmp/depinfo.data", DI_READ_LOG);
    err = check_nonnull("depinfo_b", depinfo_b);
    if (depinfo_b)
      depinfo_free(depinfo_b);
    depinfo_b = depinfo_read("/tmp/depinfo.data",
                                    DI_READ_LOG | DI_READ_NORETVAL);
    check_null("depinfo_b", depinfo_b);
  }

  if (!err)
    unlink("/tmp/depinfo.data");

  depinfo_free(depinfo_a);

  /* restore stdout */
  fclose(stdout);
  stdout = errcpy;
}

static int test_main(void)
{
  int err = 0;
  
  if (!err) err = test_add("test depinfo_alloc", test_depinfo_alloc);
  if (!err) err = test_add("test depinfo_add", test_depinfo_add);
  if (!err) err = test_add("test depinfo_get", test_depinfo_get);
  if (!err) err = test_add("test depinfo_sort", test_depinfo_sort);
  if (!err) err = test_add("test depinfo_write_read", test_depinfo_write_read);
  if (!err) err = test_add("test depinfo_dump", test_depinfo_dump);

  return err;
}
