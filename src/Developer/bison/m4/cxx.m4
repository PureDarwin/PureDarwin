# -*- Autoconf -*-
# Sanity-test a C++ compiler.
#
# Copyright (C) 2004, 2006 Free Software Foundation, Inc.
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

# Written by Paul Eggert.

AC_DEFUN([BISON_TEST_FOR_WORKING_CXX_COMPILER],
[
 AC_CACHE_CHECK([whether $CXX builds executables that work],
   bison_cv_cxx_works,
   [AC_LANG_PUSH([C++])
    bison_cv_cxx_works=no
    AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
	 [#include <cstdlib>
	  #include <iostream>
	  #include <map>
	  #include <string>
	  using namespace std;],
         [std::cerr << "";
          cout << "";
	  typedef std::pair<unsigned int, int> uipair;
	  std::map<unsigned int, int> m;
	  std::map<unsigned int, int>::iterator i;
	  m.insert (uipair (4, -4));
	  for (i = m.begin (); i != m.end (); ++i)
	    if (i->first != 4)
	      return 1;])],
      [AS_IF([AC_TRY_COMMAND([$CXX -o conftest$ac_exeext $CXXFLAGS $CPPFLAGS $LDFLAGS conftest.$ac_objext $LIBS >&AS_MESSAGE_LOG_FD])],
	 [AS_IF([test "$cross_compiling" = yes],
	    [bison_cv_cxx_works=cross],
	    [AS_IF([AC_TRY_COMMAND(./conftest$ac_exeext)],
	       [bison_cv_cxx_works=yes])])])
       rm -f conftest$ac_exeext])
    AC_LANG_POP([C++])])

 case $bison_cv_cxx_works in
 yes)
    BISON_CXX_WORKS=':';;
 no | cross)
    BISON_CXX_WORKS='exit 77';;
 esac

 AC_SUBST([BISON_CXX_WORKS])
 AM_CONDITIONAL(BISON_CXX_WORKS, test $bison_cv_cxx_works = yes)
])
