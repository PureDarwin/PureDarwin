/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#ifndef _USB_PROCESS_H_
#define	_USB_PROCESS_H_

/* defines */
#ifdef __HAIKU__
#define	USB_PRI_HIGHEST	20 /* B_URGENT_DISPLAY_PRIORITY */
#define	USB_PRI_HIGH	15 /* B_DISPLAY_PRIORITY */
#define	USB_PRI_MED		10 /* B_NORMAL_PRIORITY */
#endif

#define	USB_PROC_WAIT_TIMEOUT 2
#define	USB_PROC_WAIT_DRAIN   1
#define	USB_PROC_WAIT_NORMAL  0

/* structure prototypes */

struct usb_proc_msg;
typedef void (usb_proc_callback_t)(struct usb_proc_msg *);

struct usb_proc_msg {
	TAILQ_ENTRY(usb_proc_msg) pm_qentry;
	usb_proc_callback_t *pm_callback;
	usb_size_t pm_num;
};
struct usb_device;

#define	USB_PROC_MSG_ENQUEUED(msg)	((msg)->pm_qentry.tqe_prev != NULL)

/*
 * The following structure defines the USB process.
 */
struct usb_process {
	TAILQ_HEAD(, usb_proc_msg) up_qhead;
	struct cv up_cv;
	struct cv up_drain;

	struct thread *up_ptr;
	struct thread *up_curtd;
	struct mtx *up_mtx;

	usb_size_t up_msg_num;

	uint8_t	up_prio;
	uint8_t	up_gone;
	uint8_t	up_msleep;
	uint8_t	up_csleep;
	uint8_t	up_dsleep;
};

/* prototypes */

uint8_t	usb_proc_is_gone(struct usb_process *up);
int	usb_proc_create(struct usb_process *up, struct mtx *p_mtx,
	    const char *pmesg, uint8_t prio);
void	usb_proc_drain(struct usb_process *up);
void	usb_proc_mwait(struct usb_process *up, void *pm0, void *pm1);
int	usb_proc_mwait_sig(struct usb_process *up, void *pm0, void *pm1);
void	usb_proc_free(struct usb_process *up);
void   *usb_proc_msignal(struct usb_process *up, void *pm0, void *pm1);
void	usb_proc_rewakeup(struct usb_process *up);
int	usb_proc_is_called_from(struct usb_process *up);

void	usb_proc_explore_mwait(struct usb_device *, void *, void *);
void   *usb_proc_explore_msignal(struct usb_device *, void *, void *);
void	usb_proc_explore_lock(struct usb_device *);
void	usb_proc_explore_unlock(struct usb_device *);

#endif					/* _USB_PROCESS_H_ */
