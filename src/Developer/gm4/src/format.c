/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

/* printf like formatting for m4.  */

#include "m4.h"
#include "xvasprintf.h"

/* Simple varargs substitute.  */

#define ARG_INT(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, atoi (TOKEN_DATA_TEXT (argv[-1]))))

#define ARG_UINT(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, (unsigned int) atoi (TOKEN_DATA_TEXT (argv[-1]))))

#define ARG_LONG(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, atol (TOKEN_DATA_TEXT (argv[-1]))))

#define ARG_ULONG(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, (unsigned long) atol (TOKEN_DATA_TEXT (argv[-1]))))

#define ARG_STR(argc, argv) \
	((argc == 0) ? "" : \
	 (--argc, argv++, TOKEN_DATA_TEXT (argv[-1])))

#define ARG_DOUBLE(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, atof (TOKEN_DATA_TEXT (argv[-1]))))


/*------------------------------------------------------------------------.
| The main formatting function.  Output is placed on the obstack OBS, the |
| first argument in ARGV is the formatting string, and the rest is	  |
| arguments for the string.						  |
`------------------------------------------------------------------------*/

void
format (struct obstack *obs, int argc, token_data **argv)
{
  char *fmt;			/* format control string */
  const char *fstart;		/* beginning of current format spec */
  int c;			/* a simple character */

  /* Flags.  */
  char flags;			/* 1 iff treating flags */

  /* Precision specifiers.  */
  int width;			/* minimum field width */
  int prec;			/* precision */
  char lflag;			/* long flag */
  char hflag;			/* short flag */

  /* Buffer and stuff.  */
  char *str;			/* malloc'd buffer of formatted text */
  enum {INT, UINT, LONG, ULONG, DOUBLE, STR} datatype;

  fmt = ARG_STR (argc, argv);
  for (;;)
    {
      while ((c = *fmt++) != '%')
	{
	  if (c == 0)
	    return;
	  obstack_1grow (obs, c);
	}

      fstart = fmt - 1;

      if (*fmt == '%')
	{
	  obstack_1grow (obs, '%');
	  fmt++;
	  continue;
	}

      /* Parse flags.  */
      flags = 1;
      do
	{
	  switch (*fmt)
	    {
	    case '-':		/* left justification */
	    case '+':		/* mandatory sign */
	    case ' ':		/* space instead of positive sign */
	    case '0':		/* zero padding */
	    case '#':		/* alternate output */
	      break;

	    default:
	      flags = 0;
	      break;
	    }
	}
      while (flags && fmt++);

      /* Minimum field width.  */
      width = -1;
      if (*fmt == '*')
	{
	  width = ARG_INT (argc, argv);
	  fmt++;
	}
      else if (isdigit (to_uchar (*fmt)))
	{
	  do
	    {
	      fmt++;
	    }
	  while (isdigit (to_uchar (*fmt)));
	}

      /* Maximum precision.  */
      prec = -1;
      if (*fmt == '.')
	{
	  if (*(++fmt) == '*')
	    {
	      prec = ARG_INT (argc, argv);
	      ++fmt;
	    }
	  else if (isdigit (to_uchar (*fmt)))
	    {
	      do
		{
		  fmt++;
		}
	      while (isdigit (to_uchar (*fmt)));
	    }
	}

      /* Length modifiers.  */
      lflag = (*fmt == 'l');
      hflag = (*fmt == 'h');
      if (lflag || hflag)
	fmt++;

      switch (*fmt++)
	{

	case '\0':
	  return;

	case 'c':
	  datatype = INT;
	  break;

	case 's':
	  datatype = STR;
	  break;

	case 'd':
	case 'i':
	  if (lflag)
	    {
	      datatype = LONG;
	    }
	  else
	    {
	      datatype = INT;
	    }
	  break;

	case 'o':
	case 'x':
	case 'X':
	case 'u':
	  if (lflag)
	    {
	      datatype = ULONG;
	    }
	  else
	    {
	      datatype = UINT;
	    }
	  break;

	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	  datatype = DOUBLE;
	  break;

	default:
	  continue;
	}

      c = *fmt;
      *fmt = '\0';

      switch(datatype)
	{
	case INT:
	  if (width != -1 && prec != -1)
	    str = xasprintf (fstart, width, prec, ARG_INT(argc, argv));
	  else if (width != -1)
	    str = xasprintf (fstart, width, ARG_INT(argc, argv));
	  else if (prec != -1)
	    str = xasprintf (fstart, prec, ARG_INT(argc, argv));
	  else
	    str = xasprintf (fstart, ARG_INT(argc, argv));
	  break;

	case UINT:
	  if (width != -1 && prec != -1)
	    str = xasprintf (fstart, width, prec, ARG_UINT(argc, argv));
	  else if (width != -1)
	    str = xasprintf (fstart, width, ARG_UINT(argc, argv));
	  else if (prec != -1)
	    str = xasprintf (fstart, prec, ARG_UINT(argc, argv));
	  else
	    str = xasprintf (fstart, ARG_UINT(argc, argv));
	  break;

	case LONG:
	  if (width != -1 && prec != -1)
	    str = xasprintf (fstart, width, prec, ARG_LONG(argc, argv));
	  else if (width != -1)
	    str = xasprintf (fstart, width, ARG_LONG(argc, argv));
	  else if (prec != -1)
	    str = xasprintf (fstart, prec, ARG_LONG(argc, argv));
	  else
	    str = xasprintf (fstart, ARG_LONG(argc, argv));
	  break;

	case ULONG:
	  if (width != -1 && prec != -1)
	    str = xasprintf (fstart, width, prec, ARG_ULONG(argc, argv));
	  else if (width != -1)
	    str = xasprintf (fstart, width, ARG_ULONG(argc, argv));
	  else if (prec != -1)
	    str = xasprintf (fstart, prec, ARG_ULONG(argc, argv));
	  else
	    str = xasprintf (fstart, ARG_ULONG(argc, argv));
	  break;

	case DOUBLE:
	  if (width != -1 && prec != -1)
	    str = xasprintf (fstart, width, prec, ARG_DOUBLE(argc, argv));
	  else if (width != -1)
	    str = xasprintf (fstart, width, ARG_DOUBLE(argc, argv));
	  else if (prec != -1)
	    str = xasprintf (fstart, prec, ARG_DOUBLE(argc, argv));
	  else
	    str = xasprintf (fstart, ARG_DOUBLE(argc, argv));
	  break;

	case STR:
	  if (width != -1 && prec != -1)
	    str = xasprintf (fstart, width, prec, ARG_STR(argc, argv));
	  else if (width != -1)
	    str = xasprintf (fstart, width, ARG_STR(argc, argv));
	  else if (prec != -1)
	    str = xasprintf (fstart, prec, ARG_STR(argc, argv));
	  else
	    str = xasprintf (fstart, ARG_STR(argc, argv));
	  break;

	default:
	  abort();
	}

      *fmt = c;

      /* NULL was returned on failure, such as invalid format string.  For
	 now, just silently ignore that bad specifier.  */
      if (str == NULL)
	continue;

      obstack_grow (obs, str, strlen (str));
      free (str);
    }
}
