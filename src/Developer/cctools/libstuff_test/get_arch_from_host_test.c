//
//  get_arch_from_host_test.c
//  libstuff_test
//
//  Created by Michael Trent on 1/21/19.
//

#include "test_main.h"

#include "stuff/arch.h"

static void test_get_arch_from_host(void)
{
  struct arch_flag family;
  struct arch_flag specific;
  
  // it's difficult to test the active host without knowing what it is.
  int res = get_arch_from_host(&family, &specific);
  check_nonzero("get_arch_from_host result", res);
  check_nonzero("family cputype", family.cputype);
  check_nonzero("specific cputype", specific.cputype);
}

static int test_main(void)
{
  int err = 0;
  
  if (!err) err = test_add("test get_arch_from_host", test_get_arch_from_host);
  
  return err;
}
