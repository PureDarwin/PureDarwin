# wcwidth.m4 serial 5
dnl Copyright (C) 2006 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_WCWIDTH],
[
  dnl Persuade glibc <wchar.h> to declare wcwidth().
  AC_REQUIRE([AC_GNU_SOURCE])

  AC_REQUIRE([AC_C_INLINE])
  AC_REQUIRE([gt_TYPE_WCHAR_T])

  AC_CHECK_HEADERS_ONCE([wchar.h wctype.h])
  AC_CHECK_FUNCS_ONCE([iswprint wcwidth])

  AC_CHECK_DECLS([wcwidth], [], [], [
/* AIX 3.2.5 declares wcwidth in <string.h>. */
#include <string.h>
#if HAVE_WCHAR_H
/* Tru64 with Desktop Toolkit C has a bug: <stdio.h> must be included before
   <wchar.h>.
   BSD/OS 4.1 has a bug: <stdio.h> and <time.h> must be included before
   <wchar.h>.  */
# include <stdio.h>
# include <time.h>
# include <wchar.h>
#endif
])])
