#include <stdio.h>

#include <darwintest.h>

T_DECL(ctermid, "ctermid")
{
  char term[L_ctermid] = { '\0' };
  char *ptr = ctermid(term);
  T_EXPECT_EQ((void*)term, (void*)ptr, "ctermid should return the buffer it received");
  T_EXPECT_GT(strlen(ptr), 0ul, "the controlling terminal should have a name");
}

T_DECL(ctermid_null, "ctermid(NULL)")
{
  char *ptr = ctermid(NULL);
  T_EXPECT_GT(strlen(ptr), 0ul, "the controlling terminal should have a name");
}
