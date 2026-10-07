/*
 * Copyright (c) 2005-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/*
 * @APPLE_FREE_COPYRIGHT@
 */

#include <kern/spl.h>
#include <mach/std_types.h>
#include <types.h>
#include <kern/thread.h>
#include <console/serial_protos.h>
#include <libkern/section_keywords.h>

extern void cons_cinput(char ch);               /* The BSD routine that gets characters */

// a host line that starts with SOH (0x01) is a command for a driver (PDSerialHID's pointer and
// keys), not console input: it goes to the registered handler when its newline arrives
void pd_serial_line_register(void (*handler)(void *ctx, const char *line), void *ctx);

static void (*pd_serial_line_handler)(void *ctx, const char *line);
static void *pd_serial_line_ctx;

void
pd_serial_line_register(void (*handler)(void *ctx, const char *line), void *ctx)
{
	pd_serial_line_ctx = ctx;
	pd_serial_line_handler = handler;
}

static int
pd_serial_line_filter(int chr)
{
	static char line[128];
	static int len = -1;

	if (len < 0) {
		if (chr != 0x01) return 0;
		len = 0;
		return 1;
	}
	if (chr == '\r' || chr == '\n') {
		line[len] = 0;
		len = -1;
		if (pd_serial_line_handler != NULL) pd_serial_line_handler(pd_serial_line_ctx, line);
		return 1;
	}
	if (len < (int)sizeof(line) - 1) line[len++] = (char)chr;
	return 1;
}

SECURITY_READ_ONLY_LATE(unsigned int) serialmode;                               /* Serial mode keyboard and console control */

/*
 *  This routine will start a thread that polls the serial port, listening for
 *  characters that have been typed.
 */

void
serial_keyboard_init(void)
{
	kern_return_t   result;
	thread_t                thread;

	if (!(serialmode & SERIALMODE_INPUT)) { /* Leave if we do not want a serial console */
		return;
	}

	kprintf("Serial keyboard started\n");
	result = kernel_thread_start_priority((thread_continue_t)serial_keyboard_start, NULL, MAXPRI_KERNEL, &thread);
	if (result != KERN_SUCCESS) {
		panic("serial_keyboard_init");
	}

	thread_deallocate(thread);
}

void
serial_keyboard_start(void)
{
	/* Go see if there are any characters pending now */
	serial_keyboard_poll();
}

__dead2
void
serial_keyboard_poll(void)
{
	int chr;
	uint64_t next;

	while (1) {
		chr = _serial_getc(false); /* Get a character if there is one */
		if (chr < 0) { /* The serial buffer is empty */
			break;
		}
		if (pd_serial_line_filter(chr)) {
			continue;
		}
		cons_cinput((char)chr); /* Buffer up the character */
	}

	clock_interval_to_deadline(16, 1000000, &next); /* Get time of pop */

	assert_wait_deadline((event_t)serial_keyboard_poll, THREAD_UNINT, next); /* Show we are "waiting" */
	thread_block((thread_continue_t)serial_keyboard_poll); /* Wait for it */
	panic("serial_keyboard_poll: Shouldn't never ever get here...");
}

boolean_t
console_is_serial(void)
{
	return cons_ops_index == SERIAL_CONS_OPS;
}

int
switch_to_video_console(void)
{
	int old_cons_ops = cons_ops_index;
	cons_ops_index = VC_CONS_OPS;
	return old_cons_ops;
}

int
switch_to_serial_console(void)
{
	extern bool serial_console_enabled;
	int old_cons_ops = cons_ops_index;

	if (serial_console_enabled) {
		cons_ops_index = SERIAL_CONS_OPS;
	}

	return old_cons_ops;
}

/* The switch_to_{video,serial,kgdb}_console functions return a cookie that
 *  can be used to restore the console to whatever it was before, in the
 *  same way that splwhatever() and splx() work.  */
void
switch_to_old_console(int old_console)
{
	static boolean_t squawked;
	uint32_t ops = old_console;

	if ((ops >= nconsops) && !squawked) {
		squawked = TRUE;
		printf("switch_to_old_console: unknown ops %d\n", ops);
	} else {
		cons_ops_index = ops;
	}
}

void
console_printbuf_state_init(struct console_printbuf_state * data, int write_on_newline, int can_block)
{
	if (data == NULL) {
		return;
	}
	bzero(data, sizeof(struct console_printbuf_state));
	if (write_on_newline) {
		data->flags |= CONS_PB_WRITE_NEWLINE;
	}
	if (can_block) {
		data->flags |= CONS_PB_CANBLOCK;
	}
}

void
console_printbuf_putc(int ch, void * arg)
{
	struct console_printbuf_state * info = (struct console_printbuf_state *)arg;
	info->total += 1;
	if (info->pos < (SERIAL_CONS_BUF_SIZE - 1)) {
		info->str[info->pos] = (char)ch;
		info->pos += 1;
	} else {
		/*
		 * when len(line) > SERIAL_CONS_BUF_SIZE, we truncate the message
		 * if boot-arg 'drain_uart_sync=1' is set, then
		 * drain all the buffer right now and append new ch
		 */
		if (serialmode & SERIALMODE_SYNCDRAIN) {
			info->str[info->pos] = '\0';
			console_write(info->str, info->pos);
			info->pos            = 0;
			info->str[info->pos] = (char)ch;
			info->pos += 1;
		}
	}

	info->str[info->pos] = '\0';
	/* if newline, then try output to console */
	if (ch == '\n' && info->flags & CONS_PB_WRITE_NEWLINE) {
		console_write(info->str, info->pos);
		info->pos            = 0;
		info->str[info->pos] = '\0';
	}
}

void
console_printbuf_clear(struct console_printbuf_state * info)
{
	if (info->pos != 0) {
		console_write(info->str, info->pos);
	}
	info->pos = 0;
	info->str[info->pos] = '\0';
	info->total = 0;
}
