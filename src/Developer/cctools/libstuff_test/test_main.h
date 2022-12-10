//
//  test_main.h
//  cctools
//
//  Created by Michael Trent on 5/25/19.
//
//  When creating a new test, include this header, call the TEST_INTIALIZE
//  macro, then define a function test_main() like so:
//
//    static int test_main(void)
//    {
//      int err = 0;
//      if (!err) err = test_add("some test", test_some_test);
//      return err;
//   }
//
//  individal tests take and return void, and call check_* or test_* functions
//  to report failure status.

#ifndef test_case_h
#define test_case_h

#ifdef __cplusplus
extern "C" {
#endif

#include "test.h"

#define TEST_INITIALIZE \
static int test_main(void);\
__attribute__((constructor)) static void init(void)\
{\
test_register_initializer(0, test_main);\
}\

TEST_INITIALIZE

#ifdef __cplusplus
}
#endif

#endif /* test_case_h */
