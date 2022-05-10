//
//  test.h
//  libstuff_test
//
//  Created by Michael Trent on 1/19/19.
//
//  test.h defines two name spaces:
//
//  test_*
//    functions beginning with test_* are expected to match the test_func
//    type declaration. tests are assumed to pass unless they call one of:
//      test_printerr
//      test_vprinterr
//      test_abort
//    specific test implementations will provide their own test_* functions.
//
//  check_*
//    check functions are convenience front ends to test_printerr that unify
//    common patterns, such as checking array counts, strings, and other data
//    types. specific tests may provide public check_* functions. generally,
//    these will implement a type similar to:
//      int check_type(const char* label, int orig, int copy);
//    where label is a human-readable label describing the comparison (e.g.,
//    "item name"), orig is the expected value, and copy is the actual value.
//    check functions return 0 on success and non-zero on failure so that
//    callers can customize their logic, such as bailing early.
//

#ifndef test_h
#define test_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
    
typedef int (*test_initializer_func)(void);
int test_register_initializer(int order, test_initializer_func initializer);

typedef void (*test_func)(void);

struct test {
  const char* name;
  test_func testfunc;
};

#define TEST_ENTRY(NAME, FUNC) { NAME, FUNC }
    
int test_add(const char* name, test_func test);

int test_run(void);
int test_run_tests(struct test tests[], uint32_t ntests);

void test_abort(void);
void test_printinfo(const char * __restrict format, ...);
void test_printerr(const char * __restrict format, ...);

void test_vprintinfo(const char * __restrict format, va_list ap);
void test_vprinterr(const char * __restrict format, va_list ap);

void check_set_prefix(const char* fmt, ...);
  
int check_bool (const char* name, bool orig, bool copy);
int check_uint32(const char* name, uint32_t orig, uint32_t copy);
int check_uint64(const char* name, uint64_t orig, uint64_t copy);
int check_pointer(const char* name, const void* orig, const void* copy);

int check_string(const char* name, const char* orig, const char* copy);
int check_memory(const char* name, const void* orig, const void* copy,
                 uint64_t size);
int check_zerofill(const char* name, const unsigned char* buf, size_t size);

int check_null(const char* name, void* copy);
int check_nonnull(const char* name, void* copy);
int check_count(const char* name, uint32_t orig, uint32_t copy);
//int check_zero(const char* name, uint32_t copy);
int check_nonzero(const char* name, uint32_t copy);

#ifdef __cplusplus
}
#endif

#endif /* test_h */
