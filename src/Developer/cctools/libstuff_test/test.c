//
//  test.cpp
//  libstuff_test
//
//  Created by Michael Trent on 1/19/19.
//

#include "test.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#include <string>
#endif

void test_breakpoint(void)
{
  
}

typedef enum {
  TEST_PASSED = 0,
  TEST_FAILED,
  TEST_ABORT,
} test_result_t;

static int s_print_called = 0;
static test_result_t s_test_result = TEST_PASSED;

static struct test *s_tests = NULL;
static uint32_t s_ntests = 0;

struct test_init {
  int order;
  test_initializer_func initializer;
};

static struct test_init* s_inits;
static uint32_t s_ninits;

static char* s_prefix;

int test_register_initializer(int order, test_initializer_func initializer)
{
  if (s_ntests < 0xFFFFFFFF)
  {
    s_inits = (struct test_init*)reallocf(s_inits, sizeof(*s_inits) *
                                          (s_ninits + 1));
    if (s_inits)
    {
      s_inits[s_ninits].order = order;
      s_inits[s_ninits].initializer = initializer;
      s_ninits++;
    }
    else
    {
      fprintf(stderr, "error: reallocf %s\n", strerror(errno));
      assert(s_inits);
    }
  }
  else
  {
    fprintf(stderr, "error: maximum number of test initializers reached\n");
    assert(0);
  }
  
  return 0;
}

static int test_init_sort(const void* a, const void* b)
{
  struct test_init* aa = (struct test_init*)a;
  struct test_init* bb = (struct test_init*)b;
  return aa->order - bb->order;
}

int test_add(const char* name, test_func test)
{
  int result = TEST_PASSED;
  
  if (s_ntests < 0xFFFFFFFF)
  {
    s_tests = (struct test *)reallocf(s_tests, sizeof(*s_tests) *
                                      (s_ntests + 1));
    if (s_tests)
    {
      s_tests[s_ntests].name = name;
      s_tests[s_ntests].testfunc = test;
      s_ntests += 1;
    }
    else
    {
      fprintf(stderr, "error: reallocf %s\n", strerror(errno));
      result = TEST_ABORT;
    }
  }
  else
  {
    fprintf(stderr, "error: maximum number of tests reached\n");
    result = TEST_ABORT;
  }
  
  return result;
}

int test_run(void)
{
  // if we've registered c++ initializers, sort them and run them.
  if (s_ninits) {
    qsort(s_inits, s_ninits, sizeof(*s_inits), test_init_sort);
    for (uint32_t i = 0; i < s_ninits; ++i)
    {
      int err = s_inits[i].initializer();
      if (err) return err;
    }
  }
  return test_run_tests(s_tests, s_ntests);
}

int test_run_tests(struct test tests[], uint32_t ntests)
{
  test_result_t run_result = TEST_PASSED;
  int passing = 0;
  
  printf("testing:\n");
  
  for (uint32_t i = 0; i < ntests; ++i)
  {
    s_test_result = TEST_PASSED;
    s_print_called = 0;
    
    check_set_prefix(NULL);
    
    printf("\t%s ... ", tests[i].name);
    
#ifdef __cplusplus
    try {
      tests[i].testfunc();
    }
    catch (int err) {
      test_printerr("uncaught exception: int %d (might be %s)", err,
                    strerror(err));
    }
    catch (const char* s) {
      test_printerr("uncaught exception: %s", s);
    }
    catch (std::string s) {
      test_printerr("uncaught exception: %s", s.c_str());
    }
    catch (...) {
      test_printerr("uncaught exception!");
    }
#else
    tests[i].testfunc();
#endif
    if (s_print_called)
      printf("\n\t... ");
    
    if (s_test_result == TEST_PASSED)
    {
      printf("passed\n");
      passing += 1;
    }
    else if (s_test_result == TEST_ABORT)
    {
      run_result = TEST_ABORT;
      printf("aborted!\n");
      break;
    }
    else
    {
      run_result = TEST_FAILED;
      printf("failed!\n");
    }
  }
  
  printf("tests completed (%d/%d passing)\n", passing, ntests);

  check_set_prefix(NULL);

  return run_result;
}

void test_abort(void)
{
  s_test_result = TEST_ABORT;
}

static void test_vprint(const char * __restrict fmt, va_list args)
{
  assert(fmt);
  
  s_print_called = 1;
  
  size_t len = strlen(fmt);
  char* format = (char*)calloc(1, len+4);
  if (format)
  {
    sprintf(format, "\n\t\t%s", fmt);
    vprintf(format, args);
    free(format);
  }
}

void test_printinfo(const char * __restrict format, ...)
{
  assert(format);
  
  va_list args;
  va_start(args, format);
  
  test_vprint(format, args);
  
  va_end(args);
}

void test_printerr(const char * __restrict format, ...)
{
  assert(format);
  
  test_breakpoint();
  
  if (s_test_result != TEST_ABORT)
    s_test_result = TEST_FAILED;
  
  va_list args;
  va_start(args, format);
  
  test_vprint(format, args);

  va_end(args);
}

void test_vprintinfo(const char * __restrict fmt, va_list args)
{
  test_vprint(fmt, args);
}

void test_vprinterr(const char * __restrict fmt, va_list args)
{
  if (s_test_result != TEST_ABORT)
    s_test_result = TEST_FAILED;
  
  test_vprint(fmt, args);
}

void check_set_prefix(const char* fmt, ...)
{
  va_list ap;
  size_t len;

  if (s_prefix) {
    free(s_prefix);
    s_prefix = NULL;
  }

  if (fmt) {
    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    
    // reserve space for an extra ' ' between the prefix and the check name
    s_prefix = malloc(len + 2);
    if (s_prefix) {
      va_start(ap, fmt);
      len = vsnprintf(s_prefix, len + 1, fmt, ap);
      va_end(ap);
      
      // add the space
      s_prefix[len++] = ' ';
      s_prefix[len++] = 0;
    }
  }
}

int check_bool (const char* name, bool orig, bool copy)
{
  int result = 0;
  if (orig != copy)
  {
    test_printerr("%s%s should be %d: %d", s_prefix ? s_prefix : "",
                  name, orig, copy);
    result = 1;
  }
  return result;
}

int check_uint32(const char* name, uint32_t orig, uint32_t copy)
{
  int result = 0;
  if (orig != copy)
  {
    test_printerr("%s%s should be %u: %u", s_prefix ? s_prefix : "",
                  name, orig, copy);
    result = 1;
  }
  return result;
}

int check_uint64(const char* name, uint64_t orig, uint64_t copy)
{
  int result = 0;
  if (orig != copy)
  {
    test_printerr("%s%s should be %llu: %llu", s_prefix ? s_prefix : "",
                  name, orig, copy);
    result = 1;
  }
  return result;
}

int check_pointer(const char* name, const void* orig, const void* copy)
{
  int result = 0;
  if (orig != copy)
  {
    test_printerr("%s%s should be 0x%p: 0x%p", s_prefix ? s_prefix : "",
                  name, orig, copy);
    result = 1;
  }
  return result;
}

int check_string(const char* name, const char* orig, const char* copy)
{
  int result = 0;
  if (NULL != orig && NULL == copy)
  {
    test_printerr("missing %s%s should be '%s'", s_prefix ? s_prefix : "",
                  name, orig);
    result = 1;
  }
  else if (NULL == orig && NULL != copy)
  {
    test_printerr("%s%s should be unset: '%s'", s_prefix ? s_prefix : "",
                  name, copy);
    result = 1;
  }
  else if (NULL != orig && NULL != copy && strcmp(copy, orig))
  {
    test_printerr("%s%s should be '%s': '%s'", s_prefix ? s_prefix : "",
                  name, orig, copy);
    result = 1;
  }
  return result;
}

int check_memory(const char* name, const void* vorig, const void* vcopy,
                 uint64_t size)
{
  const uint8_t* orig = (const uint8_t*)vorig;
  const uint8_t* copy = (const uint8_t*)vcopy;
  
  int result = 0;
  if (NULL == orig)
  {
    test_printerr("%s%s is missing input buffer: orig",
                  s_prefix ? s_prefix : "", name);
    result = 1;
  }
  else if (NULL == copy)
  {
    test_printerr("%s%s is missing input buffer: copy",
                  s_prefix ? s_prefix : "", name);
    result = 1;
  }
  else
  {
    for (uint64_t i = 0; i < size; ++i)
    {
      if (orig[i] != copy[i])
      {
        test_printerr("%s%s differ at offset %llu[0x%llx]: 0x%x != 0x%x",
                      s_prefix ? s_prefix : "", name, i, i, orig[i], copy[i]);
        result = 1;
        break;
      }
    }
  }
  return result;
}

int check_zerofill(const char* name, const unsigned char* buf, size_t size)
{
  int result = 0;
  
  for (int i = 0; i < size; ++i)
  {
    if (buf[i])
    {
      test_printerr("%s%s is not zero filled", s_prefix ? s_prefix : "", name);
      result = 1;
      break;
    }
  }
  
  return result;
}

int check_null(const char* name, void* copy)
{
  int result = 0;
  if (copy)
  {
    test_printerr("%s%s should be NULL: 0x%p", s_prefix ? s_prefix : "",
                  name, copy);
    result = 1;
  }
  return result;
}

int check_nonnull(const char* name, void* copy)
{
  int result = 0;
  if (!copy)
  {
    test_printerr("%s%s is NULL", s_prefix ? s_prefix : "", name);
    result = 1;
  }
  return result;
}

int check_count(const char* name, uint32_t orig, uint32_t copy)
{
  int result = 0;
  if (orig != copy)
  {
    test_printerr("number of %s%s should be %d: %d", s_prefix ? s_prefix : "",
                  name, orig, copy);
    result = 1;
  }
  return result;
}
/*
int check_zero(const char* name, uint32_t copy)
{
  int result = 0;
  if (copy)
  {
    test_printerr("%s should be 0: %d", name, copy);
    result = 1;
  }
  return result;
}
 */

int check_nonzero(const char* name, uint32_t copy)
{
  int result = 0;
  if (!copy)
  {
    test_printerr("%s%s is 0", s_prefix ? s_prefix : "", name);
    result = 1;
  }
  return result;
}

