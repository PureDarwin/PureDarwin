//
//  allocate_test.c
//  libstuff_test
//
//  Created by Michael Trent on 1/20/19.
//

#include "test_main.h"

#include "stuff/allocate.h"

static void test_allocate(void)
{
  // as an interesting quirk, allocate returns NULL when allocating a buffer
  // of size 0. This is not what the actual malloc implementation does ...
  void* p = allocate(0);
  check_null("allocate(NULL)", p);
  if (p)
    free(p);
  
  p = allocate(1);
  check_nonnull("allocate(1)", p);
  if (p)
    free(p);
}

static void test_reallocate(void)
{
  void* p = reallocate(NULL, 0);
  check_null("reallocate(NULL, 0)", p);
  
  p = reallocate(NULL, 1);
  check_nonnull("reallocate(NULL, 1)", p);
  
  p = reallocate(p, 2);
  check_nonnull("reallocate(p, 2)", p);

  p = reallocate(p, 0);
  check_nonnull("reallocate(p, 0)", p);
  
  if (p)
    free(p);
}

static void test_savestr(void)
{
  // savestr is strdup with cctools error reporting.
  char* s = savestr("");
  check_nonnull("savestr(\"\")", s);
  if (s) {
    check_string("savestr(\"\")", "", s);
    free(s);
  }
  
  const char* cs = "A brilliant red Barchetta";
  s = savestr(cs);
  check_nonnull("savestr(cs)", s);
  if (s) {
    check_string("savestr(cs)", cs, s);
    free(s);
  }
}

static void test_makestr(void)
{
  // makestr is a vararg wrapper to strcat. it is not possible to verify all
  // the arguments to makestr are char*. 
  char* s = makestr(NULL);
  
  check_nonnull("makestr(NULL)", s);
  if (s) {
    check_string("makestr(NULL)", "", s);
    free(s);
  }
  
  s = makestr("", NULL);
  check_nonnull("makestr(\"\", NULL)", s);
  if (s) {
    check_string("makestr(\"\", NULL)", "", s);
    free(s);
  }

  s = makestr("A", " brilliant", " red", " Barchetta", NULL);
  check_nonnull("makestr(a, b, c, d)", s);
  if (s) {
    check_string("makestr(a, b, c, d)", "A brilliant red Barchetta", s);
    free(s);
  }
}

static int test_main(void)
{
  int err = 0;
  
  if (!err) err = test_add("test allocate", test_allocate);
  if (!err) err = test_add("test reallocate", test_reallocate);
  if (!err) err = test_add("test savestr", test_savestr);
  if (!err) err = test_add("test makestr", test_makestr);
  
  return err;
}
