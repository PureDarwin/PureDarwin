# bison-i18n.m4 serial 1 (bison-2.1)
dnl Copyright (C) 2005 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl Support for internationalization of bison-generated parsers.

dnl BISON_I18N
dnl should be used in configure.ac, after AM_GNU_GETTEXT. If USE_NLS is yes, it
dnl sets BISON_LOCALEDIR to indicate where to find the bison-runtime.mo files
dnl and defines YYENABLE_NLS if there are bison-runtime.mo files at all.
AC_DEFUN([BISON_I18N],
[
  if test -z "$USE_NLS"; then
    echo "The BISON-I18N macro is used without being preceded by AM-GNU-GETTEXT." 1>&2
    exit 1
  fi
  BISON_LOCALEDIR=
  if test "$USE_NLS" = yes; then
    dnl AC_PROG_YACC sets the YACC variable; other macros set the BISON variable.
    if test -n "$YACC"; then
      case "$YACC" in
        *bison*)
          if ($YACC --print-localedir) >/dev/null 2>&1; then
            BISON_LOCALEDIR=`$YACC --print-localedir`
          fi
          ;;
      esac
    else
      if test -n "$BISON"; then
        if test "$BISON" != ":"; then
          if ($BISON --print-localedir) >/dev/null 2>&1; then
            BISON_LOCALEDIR=`$BISON --print-localedir`
          fi
        fi
      fi
    fi
    AC_SUBST([BISON_LOCALEDIR])
    if test -n "$BISON_LOCALEDIR"; then
      USER_LINGUAS="${LINGUAS-%UNSET%}"
      if test -n "$USER_LINGUAS"; then
        BISON_USE_NLS=yes
      else
        BISON_USE_NLS=no
      fi
    else
      BISON_USE_NLS=no
    fi
  else
    BISON_USE_NLS=no
  fi
  if test $BISON_USE_NLS = yes; then
    AC_DEFINE([YYENABLE_NLS], 1,
      [Define to 1 to internationalize bison runtime messages.])
  fi
])
