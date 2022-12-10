/* Declaration for error-reporting function for Bison.

   Copyright (C) 2000, 2001, 2002, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

/* Based on error.c and error.h,
   written by David MacKenzie <djm@gnu.ai.mit.edu>.  */

#include <config.h>
#include "system.h"

#include <stdarg.h>

#include "complain.h"
#include "files.h"

/* The calling program should define program_name and set it to the
   name of the executing program.  */
extern char *program_name;

/* This variable is set each time `warn' is called.  */
bool warning_issued;

/* This variable is set each time `complain' is called.  */
bool complaint_issued;


/*--------------------------------.
| Report a warning, and proceed.  |
`--------------------------------*/

void
warn_at (location loc, const char *message, ...)
{
  va_list args;

  location_print (stderr, loc);
  fputs (": ", stderr);
  fputs (_("warning: "), stderr);

  va_start (args, message);
  vfprintf (stderr, message, args);
  va_end (args);

  warning_issued = true;
  putc ('\n', stderr);
}

void
warn (const char *message, ...)
{
  va_list args;

  fprintf (stderr, "%s: %s", current_file ? current_file : program_name, _("warning: "));

  va_start (args, message);
  vfprintf (stderr, message, args);
  va_end (args);

  warning_issued = true;
  putc ('\n', stderr);
}

/*-----------------------------------------------------------.
| An error has occurred, but we can proceed, and die later.  |
`-----------------------------------------------------------*/

void
complain_at (location loc, const char *message, ...)
{
  va_list args;

  location_print (stderr, loc);
  fputs (": ", stderr);

  va_start (args, message);
  vfprintf (stderr, message, args);
  va_end (args);

  complaint_issued = true;
  putc ('\n', stderr);
}

void
complain (const char *message, ...)
{
  va_list args;

  fprintf (stderr, "%s: ", current_file ? current_file : program_name);

  va_start (args, message);
  vfprintf (stderr, message, args);
  va_end (args);

  complaint_issued = true;
  putc ('\n', stderr);
}

/*-------------------------------------------------.
| A severe error has occurred, we cannot proceed.  |
`-------------------------------------------------*/

void
fatal_at (location loc, const char *message, ...)
{
  va_list args;

  location_print (stderr, loc);
  fputs (": ", stderr);
  fputs (_("fatal error: "), stderr);

  va_start (args, message);
  vfprintf (stderr, message, args);
  va_end (args);
  putc ('\n', stderr);
  exit (EXIT_FAILURE);
}

void
fatal (const char *message, ...)
{
  va_list args;

  fprintf (stderr, "%s: ", current_file ? current_file : program_name);

  fputs (_("fatal error: "), stderr);

  va_start (args, message);
  vfprintf (stderr, message, args);
  va_end (args);
  putc ('\n', stderr);
  exit (EXIT_FAILURE);
}
