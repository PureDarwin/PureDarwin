/* Bitset vectors.
   Copyright (C) 2001, 2002, 2004, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include "bitsetv-print.h"

/*--------------------------------------------------------.
| Display the MATRIX array of SIZE bitsets of size SIZE.  |
`--------------------------------------------------------*/

void
bitsetv_matrix_dump (FILE * out, const char *title, bitsetv bset)
{
  bitset_bindex i, j;
  bitset_bindex hsize = bitset_size (bset[0]);

  /* Title. */
  fprintf (out, "%s BEGIN\n", title);

  /* Column numbers. */
  fputs ("   ", out);
  for (i = 0; i < hsize; ++i)
    putc (i / 10 ? '0' + i / 10 : ' ', out);
  putc ('\n', out);
  fputs ("   ", out);
  for (i = 0; i < hsize; ++i)
    fprintf (out, "%d", (int) (i % 10));
  putc ('\n', out);

  /* Bar. */
  fputs ("  .", out);
  for (i = 0; i < hsize; ++i)
    putc ('-', out);
  fputs (".\n", out);

  /* Contents. */
  for (i = 0; bset[i]; ++i)
    {
      fprintf (out, "%2lu|", (unsigned long int) i);
      for (j = 0; j < hsize; ++j)
	fputs (bitset_test (bset[i], j) ? "1" : " ", out);
      fputs ("|\n", out);
    }

  /* Bar. */
  fputs ("  `", out);
  for (i = 0; i < hsize; ++i)
    putc ('-', out);
  fputs ("'\n", out);

  /* End title. */
  fprintf (out, "%s END\n\n", title);
}
