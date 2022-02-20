//
//  args_test.c
//  libstuff_test
//
//  Created by Michael Trent on 5/31/19.
//

#include "test_main.h"
#include "test_util.h"

#include "stuff/args.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define countof(X) (sizeof(X) / sizeof(*X))

static char* masprintf(const char * __restrict format, ...)
{
  assert(format);
  
  va_list args;
  va_start(args, format);
  
  char* s;
  vasprintf(&s, format, args);
  
  va_end(args);
  
  return s;
}

static void test_args_expand_at_1(void)
{
  char* strs[2] = {0};
  strs[0] = strdup("/bin/ls");

  int argc = 1;
  char** argv = strs;
  int err = args_expand_at(&argc, &argv);

  check_uint32("args_expand_at result", 0, err);
  check_uint32("argc", 1, argc);
  check_string("argv[0]", "/bin/ls", argv[0]);
  check_null("argv[1]", argv[1]);

  for (int i = 0; i < countof(strs); ++i)
    if (strs[i])
      free(strs[i]);
}

static void test_args_expand_at_2(void)
{
  char* strs[4] = {0};
  strs[0] = strdup("/bin/ls");
  strs[1] = strdup("-ls");
  strs[2] = strdup("/");

  int argc = 3;
  char** argv = strs;
  int err = args_expand_at(&argc, &argv);

  check_uint32("args_expand_at result", 0, err);
  check_uint32("argc", 3, argc);
  check_string("argv[0]", "/bin/ls", argv[0]);
  check_string("argv[1]", "-ls", argv[1]);
  check_string("argv[2]", "/", argv[2]);
  check_null("argv[3]", argv[3]);

  for (int i = 0; i < countof(strs); ++i)
    if (strs[i])
      free(strs[i]);
}

static void test_args_expand_at_3(void)
{
  char* strs[5] = {0};
  strs[0] = strdup("/bin/ls");
  strs[1] = strdup("@/tmp/missing-ls-args");
  strs[2] = strdup("@/tmp/missing-ls-args");
  strs[3] = strdup("/");

  int argc = 4;
  char** argv = strs;
  int err = args_expand_at(&argc, &argv);

  check_uint32("args_expand_at result", 0, err);
  check_uint32("argc", 4, argc);
  check_string("argv[0]", "/bin/ls", argv[0]);
  check_string("argv[1]", "@/tmp/missing-ls-args", argv[1]);
  check_string("argv[2]", "@/tmp/missing-ls-args", argv[2]);
  check_string("argv[3]", "/", argv[3]);
  check_null("argv[4]", argv[4]);

  for (int i = 0; i < countof(strs); ++i)
    if (strs[i])
      free(strs[i]);
}

static void test_args_expand_at_4(void)
{
  const char* data = "-ls apple\tbanana\ncarrot    durian\\ fruit 'el burro'\n";
  char* tempfile = NULL;
  
  if (test_write_tmp_data(data, strlen(data), &tempfile)) {
    test_printerr("failed to write temp data: %s", strerror(errno));
    return;
  }

  char* strs[3] = {0};
  strs[0] = strdup("/bin/ls");
  strs[1] = masprintf("@%s", tempfile);

  int argc = 2;
  char** argv = strs;
  int err = args_expand_at(&argc, &argv);

  check_uint32("args_expand_at result", 0, err);
  check_uint32("argc", 7, argc);
  check_string("argv[0]", "/bin/ls", argv[0]);
  check_string("argv[1]", "-ls", argv[1]);
  check_string("argv[2]", "apple", argv[2]);
  check_string("argv[3]", "banana", argv[3]);
  check_string("argv[4]", "carrot", argv[4]);
  check_string("argv[5]", "durian fruit", argv[5]);
  check_string("argv[6]", "el burro", argv[6]);
  check_null("argv[7]", argv[7]);

  for (int i = 0; i < countof(strs); ++i)
    if (strs[i])
      free(strs[i]);

  unlink(tempfile);
  free(tempfile);
}

static void test_args_expand_at_5(void)
{
  const char* data1 = "    -ls apple\tbanana\ncarrot     \n";
  const char* data2 = "\n\n\tdurian\\ fruit 'el burro'      \n";
  char* tempname1 = NULL;
  char* tempname2 = NULL;

  if (test_write_tmp_data(data1, strlen(data1), &tempname1)) {
    test_printerr("failed to write temp data: %s", strerror(errno));
    return;
  }
  if (test_write_tmp_data(data2, strlen(data2), &tempname2)) {
    unlink(tempname1);
    free(tempname1);
    test_printerr("failed to write temp data: %s", strerror(errno));
    return;
  }

  char* strs[4] = {0};
  strs[0] = strdup("/bin/ls");
  strs[1] = masprintf("@%s", tempname1);
  strs[2] = masprintf("@%s", tempname2);

  int argc = 3;
  char** argv = strs;
  int err = args_expand_at(&argc, &argv);

  check_uint32("args_expand_at result", 0, err);
  check_uint32("argc", 7, argc);
  check_string("argv[0]", "/bin/ls", argv[0]);
  check_string("argv[1]", "-ls", argv[1]);
  check_string("argv[2]", "apple", argv[2]);
  check_string("argv[3]", "banana", argv[3]);
  check_string("argv[4]", "carrot", argv[4]);
  check_string("argv[5]", "durian fruit", argv[5]);
  check_string("argv[6]", "el burro", argv[6]);
  check_null("argv[7]", argv[7]);

  for (int i = 0; i < countof(strs); ++i)
    if (strs[i])
      free(strs[i]);

  unlink(tempname1);
  free(tempname1);
  unlink(tempname2);
  free(tempname2);
}

static void test_args_expand_at_6(void)
{
  const char* data1 = "    -ls apple\tbanana\ncarrot     \n";
  const char* data2 = "\n\n\tdurian\\ fruit 'el burro'      \n";
  char* tempname1 = NULL;
  char* tempname2 = NULL;
  char* tempname3 = NULL;

  if (test_write_tmp_data(data1, strlen(data1), &tempname1)) {
    test_printerr("failed to write temp data: %s", strerror(errno));
    return;
  }
  if (test_write_tmp_data(data2, strlen(data2), &tempname2)) {
    unlink(tempname1);
    free(tempname1);
    test_printerr("failed to write temp data: %s", strerror(errno));
    return;
  }

  char* data3 = masprintf("@%s @%s\n", tempname1, tempname2);
  if (test_write_tmp_data(data3, strlen(data3), &tempname3)) {
    unlink(tempname1);
    free(tempname1);
    unlink(tempname2);
    free(tempname2);
    free(data3);
    test_printerr("failed to write temp data: %s", strerror(errno));
    return;
  }
  free(data3);

  char* strs[3] = {0};
  strs[0] = strdup("/bin/ls");
  strs[1] = masprintf("@%s", tempname3);

  int argc = 2;
  char** argv = strs;
  int err = args_expand_at(&argc, &argv);

  check_uint32("args_expand_at result", 0, err);
  check_uint32("argc", 7, argc);
  check_string("argv[0]", "/bin/ls", argv[0]);
  check_string("argv[1]", "-ls", argv[1]);
  check_string("argv[2]", "apple", argv[2]);
  check_string("argv[3]", "banana", argv[3]);
  check_string("argv[4]", "carrot", argv[4]);
  check_string("argv[5]", "durian fruit", argv[5]);
  check_string("argv[6]", "el burro", argv[6]);
  check_null("argv[7]", argv[7]);

  for (int i = 0; i < countof(strs); ++i)
    if (strs[i])
      free(strs[i]);

  unlink(tempname1);
  free(tempname1);
  unlink(tempname2);
  free(tempname2);
  unlink(tempname3);
  free(tempname3);
}

static int test_main(void)
{
  int err = 0;

  if (!err) err = test_add("test args_expand_at with one argument",
                           test_args_expand_at_1);
  if (!err) err = test_add("test args_expand_at with many arguments",
                           test_args_expand_at_2);
  if (!err) err = test_add("test args_expand_at with missing @ file",
                           test_args_expand_at_3);
  if (!err) err = test_add("test args_expand_at with one @ file",
                           test_args_expand_at_4);
  if (!err) err = test_add("test args_expand_at with two @ files",
                           test_args_expand_at_5);
  if (!err) err = test_add("test args_expand_at with nested @ files",
                           test_args_expand_at_6);

  return err;
}
