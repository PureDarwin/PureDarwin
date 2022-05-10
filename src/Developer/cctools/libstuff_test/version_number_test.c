//
//  version_number_test.c
//  libstuff_test
//
//  Created by Michael Trent on 5/14/19.
//

#include "test_main.h"

#include "stuff/bool.h"
#include "stuff/version_number.h"

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>

static void check_version(const char* version, enum bool success,
uint32_t expected)
{
  check_set_prefix("get_version_number %s", version);
  
  uint32_t value;
  enum bool result = get_version_number("", version, &value);

  check_bool("result", success, result);
  check_uint32("version", expected, value);
}

static void test_get_version_number(void)
{
  // silence stderr, because it will cause the test to fail upstream.
  // this code needs to be refactored. And this test will help us do that =
  // some day.
  FILE* errcpy = stderr;
  stderr = fopen("/dev/null", "w");
  
  // You'd think passing a empty string to get_version_number would be an
  // error condition, but it is not. It means: 0.
  check_version("", TRUE, 0x0);
  check_version("0", TRUE, 0x0);
  check_version("0.", FALSE, 0x0);
  check_version("0.0", TRUE, 0x0);
  check_version("0.0.", FALSE, 0x0);
  check_version("0.0.0", TRUE, 0x0);
  // You'd think if passing "0." was an error that passing "0.0.0." would also
  // be an error, but it is not. Everything past the 3rd number is ignored.
  check_version("0.0.0.", TRUE, 0x0);
  check_version("0.0.0.0", TRUE, 0x0);
  // This probably qualifies as undefined behavior
  check_version("0.0.0.9999999happybirthday", TRUE, 0x0);

  check_version("1", TRUE, 0x00010000);
  check_version("1.2", TRUE, 0x00010200);
  check_version("1.2.4", TRUE, 0x00010204);

  check_version("65535.255.255", TRUE, 0xFFFFFFFF);
  check_version("65536.255.255", FALSE, 0);
  check_version("65535.256.255", FALSE, 0);
  check_version("65535.255.256", FALSE, 0);
  check_version("-1", FALSE, 0);
  check_version("0.-1", FALSE, 0);
  check_version("0.0.-1", FALSE, 0);
  check_version("0.-1000", FALSE, 0);
  
  check_version("a", FALSE, 0);
  check_version("0.a", FALSE, 0);
  check_version("0.0.a", FALSE, 0);
  
  // restore stderr
  fclose(stderr);
  stderr = errcpy;
}

static int test_main(void)
{
  int err = 0;
  
  if (!err) err = test_add("test get_version_number", test_get_version_number);
  
  return err;
}
