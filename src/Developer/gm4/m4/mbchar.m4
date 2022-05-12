# mbchar.m4 serial 4
dnl Copyright (C) 2005-2006 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl autoconf tests required for use of mbchar.m4
dnl From Bruno Haible.

AC_DEFUN([gl_MBCHAR],
[
  AC_REQUIRE([AC_GNU_SOURCE])
  dnl The following line is that so the user can test HAVE_WCHAR_H
  dnl before #include "mbchar.h".
  AC_CHECK_HEADERS_ONCE([wchar.h])
  dnl Compile mbchar.c only if HAVE_WCHAR_H.
  if test $ac_cv_header_wchar_h = yes; then
    AC_LIBOBJ([mbchar])
    dnl Prerequisites of mbchar.h and mbchar.c.
    AC_CHECK_HEADERS_ONCE([wctype.h])
    AC_CHECK_FUNCS([iswcntrl])
  fi
])
