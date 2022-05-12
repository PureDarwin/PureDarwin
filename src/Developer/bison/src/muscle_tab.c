/* Muscle table manager for Bison.

   Copyright (C) 2001, 2002, 2003, 2004, 2005 Free Software
   Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   Bison is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Bison is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Bison; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#include <config.h>
#include "system.h"

#include <hash.h>
#include <quotearg.h>

#include "files.h"
#include "muscle_tab.h"
#include "getargs.h"

typedef struct
{
  const char *key;
  char *value;
} muscle_entry;

/* An obstack used to create some entries.  */
struct obstack muscle_obstack;

/* Initial capacity of muscles hash table.  */
#define HT_INITIAL_CAPACITY 257

static struct hash_table *muscle_table = NULL;

static bool
hash_compare_muscles (void const *x, void const *y)
{
  muscle_entry const *m1 = x;
  muscle_entry const *m2 = y;
  return strcmp (m1->key, m2->key) == 0;
}

static size_t
hash_muscle (const void *x, size_t tablesize)
{
  muscle_entry const *m = x;
  return hash_string (m->key, tablesize);
}

/*-----------------------------------------------------------------.
| Create the MUSCLE_TABLE, and initialize it with default values.  |
| Also set up the MUSCLE_OBSTACK.                                  |
`-----------------------------------------------------------------*/

void
muscle_init (void)
{
  /* Initialize the muscle obstack.  */
  obstack_init (&muscle_obstack);

  muscle_table = hash_initialize (HT_INITIAL_CAPACITY, NULL, hash_muscle,
				  hash_compare_muscles, free);

  /* Version and input file.  */
  MUSCLE_INSERT_STRING ("version", VERSION);
  MUSCLE_INSERT_C_STRING ("file_name", grammar_file);
}


/*------------------------------------------------------------.
| Free all the memory consumed by the muscle machinery only.  |
`------------------------------------------------------------*/

void
muscle_free (void)
{
  hash_free (muscle_table);
  obstack_free (&muscle_obstack, NULL);
}



/*------------------------------------------------------------.
| Insert (KEY, VALUE).  If KEY already existed, overwrite the |
| previous value.                                             |
`------------------------------------------------------------*/

void
muscle_insert (const char *key, char *value)
{
  muscle_entry probe;
  muscle_entry *entry;

  probe.key = key;
  entry = hash_lookup (muscle_table, &probe);

  if (!entry)
    {
      /* First insertion in the hash. */
      entry = xmalloc (sizeof *entry);
      entry->key = key;
      hash_insert (muscle_table, entry);
    }
  entry->value = value;
}


/*-------------------------------------------------------------------.
| Append VALUE to the current value of KEY.  If KEY did not already  |
| exist, create it.  Use MUSCLE_OBSTACK.  De-allocate the previously |
| associated value.  Copy VALUE and SEPARATOR.                       |
`-------------------------------------------------------------------*/

void
muscle_grow (const char *key, const char *val, const char *separator)
{
  muscle_entry probe;
  muscle_entry *entry = NULL;

  probe.key = key;
  entry = hash_lookup (muscle_table, &probe);

  if (!entry)
    {
      /* First insertion in the hash. */
      entry = xmalloc (sizeof *entry);
      entry->key = key;
      hash_insert (muscle_table, entry);
      entry->value = xstrdup (val);
    }
  else
    {
      /* Grow the current value. */
      char *new_val;
      obstack_sgrow (&muscle_obstack, entry->value);
      free (entry->value);
      obstack_sgrow (&muscle_obstack, separator);
      obstack_sgrow (&muscle_obstack, val);
      obstack_1grow (&muscle_obstack, 0);
      new_val = obstack_finish (&muscle_obstack);
      entry->value = xstrdup (new_val);
      obstack_free (&muscle_obstack, new_val);
    }
}


/*------------------------------------------------------------------.
| Append VALUE to the current value of KEY, using muscle_grow.  But |
| in addition, issue a synchronization line for the location LOC.   |
`------------------------------------------------------------------*/

void
muscle_code_grow (const char *key, const char *val, location loc)
{
  char *extension = NULL;
  obstack_fgrow1 (&muscle_obstack, "]b4_syncline(%d, [[", loc.start.line);
  MUSCLE_OBSTACK_SGROW (&muscle_obstack,
			quotearg_style (c_quoting_style, loc.start.file));
  obstack_sgrow (&muscle_obstack, "]])[\n");
  obstack_sgrow (&muscle_obstack, val);
  obstack_1grow (&muscle_obstack, 0);
  extension = obstack_finish (&muscle_obstack);
  muscle_grow (key, extension, "");
}


/*-------------------------------------------------------------------.
| MUSCLE is an M4 list of pairs.  Create or extend it with the pair  |
| (A1, A2).  Note that because the muscle values are output *double* |
| quoted, one needs to strip the first level of quotes to reach the  |
| list itself.                                                       |
`-------------------------------------------------------------------*/

void muscle_pair_list_grow (const char *muscle,
			    const char *a1, const char *a2)
{
  char *pair;
  obstack_fgrow2 (&muscle_obstack, "[[[%s]], [[%s]]]", a1, a2);
  obstack_1grow (&muscle_obstack, 0);
  pair = obstack_finish (&muscle_obstack);
  muscle_grow (muscle, pair, ",\n");
  obstack_free (&muscle_obstack, pair);
}

/*-------------------------------.
| Find the value of muscle KEY.  |
`-------------------------------*/

char *
muscle_find (const char *key)
{
  muscle_entry probe;
  muscle_entry *result = NULL;

  probe.key = key;
  result = hash_lookup (muscle_table, &probe);
  return result ? result->value : NULL;
}


/*------------------------------------------------.
| Output the definition of ENTRY as a m4_define.  |
`------------------------------------------------*/

static inline bool
muscle_m4_output (muscle_entry *entry, FILE *out)
{
  fprintf (out, "m4_define([b4_%s],\n", entry->key);
  fprintf (out, "[[%s]])\n\n\n", entry->value);
  return true;
}

static bool
muscle_m4_output_processor (void *entry, void *out)
{
  return muscle_m4_output (entry, out);
}


/*----------------------------------------------------------------.
| Output the definition of all the current muscles into a list of |
| m4_defines.                                                     |
`----------------------------------------------------------------*/

void
muscles_m4_output (FILE *out)
{
  hash_do_for_each (muscle_table, muscle_m4_output_processor, out);
}
