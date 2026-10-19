/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_device.h>

/*------------------------------------------------------------------------*
 *	 usb_pause_mtx - factored out code
 *
 * This function will delay the code by the passed number of system
 * ticks. The passed mutex "mtx" will be dropped while waiting, if
 * "mtx" is different from NULL.
 *------------------------------------------------------------------------*/
void
usb_pause_mtx(struct mtx *mtx, int timo)
{
	if (mtx != NULL)
		mtx_unlock(mtx);

	/*
	 * Add one tick to the timeout so that we don't return too
	 * early! Note that pause() will assert that the passed
	 * timeout is positive and non-zero!
	 */
	pause("USBWAIT", timo + 1);

	if (mtx != NULL)
		mtx_lock(mtx);
}

/*------------------------------------------------------------------------*
 *	usb_printbcd
 *
 * This function will print the version number "bcd" to the string
 * pointed to by "p" having a maximum length of "p_len" bytes
 * including the terminating zero.
 *------------------------------------------------------------------------*/
void
usb_printbcd(char *p, uint16_t p_len, uint16_t bcd)
{
	if (snprintf(p, p_len, "%x.%02x", bcd >> 8, bcd & 0xff)) {
		/* ignore any errors */
	}
}

/*------------------------------------------------------------------------*
 *	usb_trim_spaces
 *
 * This function removes spaces at the beginning and the end of the string
 * pointed to by the "p" argument.
 *------------------------------------------------------------------------*/
void
usb_trim_spaces(char *p)
{
	char *q;
	char *e;

	if (p == NULL)
		return;
	q = e = p;
	while (*q == ' ')		/* skip leading spaces */
		q++;
	while ((*p = *q++))		/* copy string */
		if (*p++ != ' ')	/* remember last non-space */
			e = p;
	*e = 0;				/* kill trailing spaces */
}
