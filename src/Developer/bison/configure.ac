# Configure template for GNU Bison.                   -*-Autoconf-*-
#
# Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006 Free Software
# Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

# This file uses AC_CONFIG_MACRO_DIR, so we need Autoconf 2.58 or better.
# But Autoconf 2.58 has a bug in the way that it generates tests: it
# puts the wrong absolute path names in the tests.  So we need at
# least Autoconf 2.59.
AC_PREREQ(2.59)

AC_INIT([GNU Bison], [2.3], [bug-bison@gnu.org])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

# We don't have a file m4/Makefile.am, so we need Automake 1.8 or later.
AM_INIT_AUTOMAKE([1.8 check-news readme-alpha dist-bzip2])
AC_CONFIG_HEADERS([config.h:config.hin])

gl_USE_SYSTEM_EXTENSIONS

# Checks for the compiler.
AC_PROG_CC
AM_PROG_CC_STDC
AC_ARG_ENABLE(gcc-warnings,
[  --enable-gcc-warnings   turn on lots of GCC warnings (not recommended)],
[case "${enableval}" in
   yes|no) ;;
   *)      AC_MSG_ERROR([bad value ${enableval} for gcc-warnings option]) ;;
 esac],
              [enableval=no])
if test "${enableval}" = yes; then
  BISON_WARNING(-Werror)
  AC_SUBST([WERROR_CFLAGS], [$WARNING_CFLAGS])
  WARNING_CFLAGS=
  BISON_WARNING(-W)
  BISON_WARNING(-Wall)
  BISON_WARNING(-Wcast-align)
  BISON_WARNING(-Wcast-qual)
  BISON_WARNING(-Wformat)
  BISON_WARNING(-Wwrite-strings)
  AC_SUBST([WARNING_CXXFLAGS], [$WARNING_CFLAGS])
  # The following warnings are not suitable for C++.
  BISON_WARNING(-Wbad-function-cast)
  BISON_WARNING(-Wmissing-declarations)
  BISON_WARNING(-Wmissing-prototypes)
  BISON_WARNING(-Wshadow)
  BISON_WARNING(-Wstrict-prototypes)
  AC_DEFINE([lint], 1, [Define to 1 if the compiler is checking for lint.])
fi

BISON_TEST_FOR_WORKING_C_COMPILER

AC_ARG_ENABLE([yacc],
  [AC_HELP_STRING([--disable-yacc],
     [do not build a yacc command or an -ly library])],
  , [enable_yacc=yes])
case $enable_yacc in
yes)
  YACC_SCRIPT=yacc
  YACC_LIBRARY=liby.a;;
*)
  YACC_SCRIPT=
  YACC_LIBRARY=;;
esac
AC_SUBST([YACC_SCRIPT])
AC_SUBST([YACC_LIBRARY])

# Checks for programs.
AC_PROG_LEX
AC_PROG_YACC
AC_PROG_RANLIB
BISON_PROG_GNU_M4
if test x"$ac_cv_prog_gnu_m4" != xyes; then
  AC_MSG_ERROR([GNU M4 1.4 is required])
fi
AC_DEFINE_UNQUOTED([M4], ["$M4"], [Define to the GNU M4 executable name.])

# Checks for header files.
AC_CHECK_HEADERS_ONCE([locale.h])
AM_STDBOOL_H

# Checks for compiler characteristics.
AC_C_INLINE

# Checks for types.
AC_CHECK_TYPES([uintptr_t])

# Checks for library functions.
AC_CHECK_FUNCS_ONCE([setlocale])
AM_WITH_DMALLOC
BISON_PREREQ_SUBPIPE
BISON_PREREQ_TIMEVAR

# gnulib and gettext.
GNULIB_AUTOCONF_SNIPPET
# We use po/Makevars, so we need at least gettext 0.12.
AM_GNU_GETTEXT_VERSION([0.12])
BISON_I18N

# Internationalized parsers.
AC_CONFIG_FILES([runtime-po/Makefile.in])
# Autoconf macros for packages using internationalized parsers.
aclocaldir='${datadir}/aclocal'
AC_SUBST([aclocaldir])

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AC_CONFIG_FILES([tests/Makefile tests/atlocal])
AC_CONFIG_FILES([tests/bison], [chmod +x tests/bison])
AC_CHECK_PROGS([VALGRIND], [valgrind])
AC_PROG_CXX
BISON_TEST_FOR_WORKING_CXX_COMPILER
AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
AC_SUBST([GCC])
O0CFLAGS=`echo $CFLAGS | sed 's/-O[[0-9]] *//'`
AC_SUBST([O0CFLAGS])
O0CXXFLAGS=`echo $CXXFLAGS | sed 's/-O[[0-9]] *//'`
AC_SUBST([O0CXXFLAGS])

AC_CONFIG_FILES([Makefile
                 build-aux/Makefile
                 po/Makefile.in
                 data/Makefile
		 examples/Makefile
		    examples/calc++/Makefile
                 lib/Makefile src/Makefile doc/Makefile])
AC_OUTPUT
