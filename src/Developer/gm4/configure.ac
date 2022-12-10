# Configure template for GNU M4.			-*-Autoconf-*-
# Copyright (C) 1991, 1993, 1994, 2004, 2005, 2006 Free Software
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

AC_PREREQ([2.60])
AC_INIT([GNU M4], [1.4.6], [bug-m4@gnu.org])
AM_INIT_AUTOMAKE([1.9.6 dist-bzip2 gnu])
PACKAGE=$PACKAGE_TARNAME; AC_SUBST([PACKAGE])
VERSION=$PACKAGE_VERSION; AC_SUBST([VERSION])

m4_pattern_forbid([^M4_])
m4_pattern_allow([^M4_cv_])

AC_CONFIG_SRCDIR([src/m4.h])
AC_CONFIG_HEADERS([config.h:config-h.in])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_CC
M4_EARLY

AC_CHECK_HEADERS_ONCE([limits.h siginfo.h sys/wait.h])
AC_CHECK_TYPES([siginfo_t], [], [],
[[#include <signal.h>
#if HAVE_SIGINFO_H
# include <siginfo.h>
#endif
]])
AC_CHECK_MEMBERS([struct sigaction.sa_sigaction], [], [],
[[#include <signal.h>
]])

AC_TYPE_SIGNAL
AC_TYPE_SIZE_T

AC_CHECK_FUNCS_ONCE([sigaction sigaltstack sigstack sigvec strerror])

M4_INIT

# Code from Jim Avera <jima@netcom.com>.
# stackovf.c requires:
#  1. Either sigaction with SA_ONSTACK, or sigvec with SV_ONSTACK
#  2. Either sigaltstack or sigstack
#  3. getrlimit, including support for RLIMIT_STACK
AC_CACHE_CHECK([if stack overflow is detectable], [M4_cv_use_stackovf],
[M4_cv_use_stackovf=no
if test "$ac_cv_func_sigaction" = yes || test "$ac_cv_func_sigvec" = yes; then
  if test "$ac_cv_func_sigaltstack" = yes || test "$ac_cv_func_sigstack" = yes; then
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
]], [[struct rlimit r; getrlimit (RLIMIT_STACK, &r);
#if (!defined(HAVE_SIGACTION) || !defined(SA_ONSTACK)) \
    && (!defined(HAVE_SIGVEC) || !defined(SV_ONSTACK))
choke me		/* SA_ONSTACK and/or SV_ONSTACK are not defined */
#endif
]])], [M4_cv_use_stackovf=yes])
  fi
fi])
AM_CONDITIONAL([STACKOVF], [test "$M4_cv_use_stackovf" = yes])
if test "$M4_cv_use_stackovf" = yes; then
  AC_DEFINE([USE_STACKOVF], [1],
    [Define to 1 if using stack overflow detection])
  AC_CHECK_TYPES([rlim_t], [],
    [AC_DEFINE([rlim_t], [int],
      [Define to int if rlim_t is not defined in sys/resource.h])],
    [[#include <sys/resource.h>
  ]])
  AC_CHECK_TYPES([stack_t], [],
    [AC_DEFINE([stack_t], [struct sigaltstack],
      [Define to struct sigaltstack if stack_t is not in signal.h])],
    [[#include <signal.h>
  ]])
  AC_CHECK_TYPES([sigcontext], [], [], [[#include <signal.h>
  ]])
fi

AC_CACHE_CHECK([if system() agrees with pclose()],
  [M4_cv_func_system_consistent],
  [AC_RUN_IFELSE([AC_LANG_PROGRAM([
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
], [int i1, i2;
  FILE *f;
  i1 = system ("exit 2");
  if (i1 == -1)
    return 1;
  f = popen ("exit 2", "r");
  if (!f)
    return 1;
  i2 = pclose (f);
  return i1 != i2;])],
  [M4_cv_func_system_consistent=yes], [M4_cv_func_system_consistent=no],
  [AC_COMPILE_IFELSE([
/* EMX on OS/2 defines WEXITSTATUS to be (x>>8)&0xff, and uses that for
   pclose(), but for system() it uses x&0xff instead.  Otherwise, we assume
   your system is sane and that pclose() and system() are consistent in their
   values.  If this heuristic is wrong for your platform, report it as a bug
   to bug-m4@gnu.org.  */
#ifdef __EMX__
choke me
#endif
], [M4_cv_func_system_consistent=yes], [M4_cv_func_system_consistent=no])])])
if test "$M4_cv_func_system_consistent" = no ; then
  AC_DEFINE([FUNC_SYSTEM_BROKEN], [1],
    [Define to 1 if the return value of system() disagrees with pclose().])
fi

dnl Don't let changeword get in our way, if bootstrapping with a version of
dnl m4 that already turned the feature on.
m4_ifdef([changeword], [m4_undefine([changeword])])dnl

AC_MSG_CHECKING([[if changeword is wanted]])
AC_ARG_ENABLE([changeword],
  [AS_HELP_STRING([--enable-changeword],
     [enable -W and changeword() builtin])],
  [if test "$enableval" = yes; then
    AC_MSG_RESULT([yes])
    AC_DEFINE([ENABLE_CHANGEWORD], [1],
      [Define to 1 if the changeword(REGEXP) functionality is wanted])
  else
    AC_MSG_RESULT([no])
  fi], [AC_MSG_RESULT([no])])

M4_WITH_DMALLOC

AC_CONFIG_COMMANDS([stamp-h], [[test -z "$CONFIG_HEADERS" || date > stamp-h]])

AC_CONFIG_FILES([Makefile
		 doc/Makefile
		 lib/Makefile
		 src/Makefile
		 checks/Makefile
		 examples/Makefile
])

AC_OUTPUT
