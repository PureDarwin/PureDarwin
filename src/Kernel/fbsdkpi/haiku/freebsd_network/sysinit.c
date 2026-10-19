/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "sysinit.h"

#include <sys/cdefs.h>
#include <sys/kernel.h>


//#define TRACE_SYSINIT
#ifdef TRACE_SYSINIT
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...)
#endif


/* linker sets */
SET_DECLARE(__freebsd_sysinit, struct sysinit);
SET_DECLARE(__freebsd_sysuninit, struct sysinit);

/* make sure there's something in both linker sets, so we can link */
SYSINIT(__dummy, 0, 0, NULL, NULL);
SYSUNINIT(__dummy, 0, 0, NULL, NULL);

// run every entry of a set in (subsystem, order) order, reversed for uninit
static void
run_sysinit_set(struct sysinit* const* begin, struct sysinit* const* end,
	bool reverse)
{
	unsigned int lastSub = 0, lastOrder = 0;
	bool first = true;

	for (;;) {
		struct sysinit* next = NULL;
		struct sysinit* const* initee;

		for (initee = begin; initee < end; initee++) {
			struct sysinit* s = *initee;
			if (s->func == NULL)
				continue;

			// skip keys that already ran
			if (!first) {
				int cmp = s->subsystem != lastSub
					? (s->subsystem < lastSub ? -1 : 1)
					: (s->order != lastOrder ? (s->order < lastOrder ? -1 : 1) : 0);
				if (reverse ? cmp >= 0 : cmp <= 0)
					continue;
			}

			if (next == NULL)
				next = s;
			else if (reverse ? (s->subsystem > next->subsystem
					|| (s->subsystem == next->subsystem && s->order > next->order))
				: (s->subsystem < next->subsystem
					|| (s->subsystem == next->subsystem && s->order < next->order)))
				next = s;
		}

		if (next == NULL)
			break;

		lastSub = next->subsystem;
		lastOrder = next->order;
		first = false;

		// every entry sharing this key runs now
		for (initee = begin; initee < end; initee++) {
			struct sysinit* s = *initee;
			if (s->func == NULL || s->subsystem != lastSub || s->order != lastOrder)
				continue;
			TRACE("sysinit: %#x %#x %s\n", s->subsystem, s->order, s->name);
			s->func((void*)s->arg);
		}
	}
}


void
init_sysinit()
{
	run_sysinit_set(SET_BEGIN(__freebsd_sysinit), SET_LIMIT(__freebsd_sysinit),
		false);
}


void
uninit_sysinit()
{
	run_sysinit_set(SET_BEGIN(__freebsd_sysuninit),
		SET_LIMIT(__freebsd_sysuninit), true);
}
