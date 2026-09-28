/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */
/*
 * Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved.
 *
 * km.m - kernel keyboard/monitor module, procedural interface.
 *
 * HISTORY
 */
#include <sys/param.h>
#include <sys/tty.h>

#include <machine/cons.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/fcntl.h>          /* for kmopen */
#include <sys/errno.h>
#include <sys/proc.h>           /* for kmopen */
#include <sys/msgbuf.h>
#include <sys/time.h>
#include <dev/kmreg_com.h>
#include <pexpert/pexpert.h>
#include <console/serial_protos.h>

extern int      hz;

extern void     console_write_char(char);
extern void     console_write(char *, int);


void    kminit(void);
void    cons_cinput(char ch);

// 'Global' variables, shared only by this file and conf.c.
struct tty     *km_tty[1] = { 0 };

// 'Global' variables, shared only by this file and kmDevice.m.
int             initialized = 0;

static int      kmoutput(struct tty * tp);
static void     kmstart(struct tty * tp);

extern void     KeyboardOpen(void);

void
kminit(void)
{
	km_tty[0] = ttymalloc();
	km_tty[0]->t_dev = makedev(12, 0);
	initialized = 1;
}

// cdevsw interface to km driver.
int
kmopen(dev_t dev, __unused int flag, __unused int devtype, proc_t pp)
{
	int             unit;
	struct tty     *tp;
	struct winsize *wp;
	int             ret;

	unit = minor(dev);
	if (unit >= 1) {
		return ENXIO;
	}

	tp = km_tty[unit];

	tty_lock(tp);

	tp->t_oproc = kmstart;
	tp->t_param = NULL;
	tp->t_dev = dev;

	if (!(tp->t_state & TS_ISOPEN)) {
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = (CREAD | CS8 | CLOCAL);
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		termioschars(&tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) && proc_suser(pp)) {
		ret = EBUSY;
		goto out;
	}

	tp->t_state |= TS_CARR_ON;      /* lie and say carrier exists and is
	                                 * on. */
	ret = ((*linesw[tp->t_line].l_open)(dev, tp));
	{
		PE_Video        video;
		wp = &tp->t_winsize;
		// Magic numbers, CHARWIDTH and CHARHEIGHT from pexpert/i386/video_console.c.
		wp->ws_xpixel = 8;
		wp->ws_ypixel = 16;

		tty_unlock(tp);         /* XXX race window */

		bzero(&video, sizeof(video));
		PE_current_console(&video);

		tty_lock(tp);

		if (serialmode & SERIALMODE_OUTPUT) {
			wp->ws_col = 80;
			wp->ws_row = 24;
		} else if (video.v_width != 0 && video.v_height != 0) {
			unsigned long ws_col = video.v_width / wp->ws_xpixel;
			unsigned long ws_row = video.v_height / wp->ws_ypixel;

			assert((ws_col <= USHRT_MAX) && (ws_row <= USHRT_MAX));

			wp->ws_col = (unsigned short)ws_col;
			wp->ws_row = (unsigned short)ws_row;
		} else {
			wp->ws_col = 100;
			wp->ws_row = 36;
		}
	}

out:
	tty_unlock(tp);

	return ret;
}

int
kmclose(dev_t dev, int flag, __unused int mode, __unused proc_t p)
{
	int ret;
	struct tty *tp = km_tty[minor(dev)];

	tty_lock(tp);
	ret = (*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	tty_unlock(tp);

	return ret;
}

int
kmread(dev_t dev, struct uio * uio, int ioflag)
{
	int ret;
	struct tty *tp = km_tty[minor(dev)];

	tty_lock(tp);
	ret = (*linesw[tp->t_line].l_read)(tp, uio, ioflag);
	tty_unlock(tp);

	return ret;
}

int
kmwrite(dev_t dev, struct uio * uio, int ioflag)
{
	int ret;
	struct tty *tp = km_tty[minor(dev)];

	tty_lock(tp);
	ret = (*linesw[tp->t_line].l_write)(tp, uio, ioflag);
	tty_unlock(tp);

	return ret;
}

int
kmioctl(dev_t dev, u_long cmd, caddr_t data, int flag, proc_t p)
{
	int             error = 0;
	struct tty *tp = km_tty[minor(dev)];
	struct winsize *wp;

	tty_lock(tp);

	switch (cmd) {
	case KMIOCSIZE:
		wp = (struct winsize *) data;
		*wp = tp->t_winsize;
		break;

	case TIOCSWINSZ:
		// Prevent console size changes so login doesn't revert to the termcap size.
		error = EINVAL;
		break;

	/* Bodge in the CLOCAL flag as the km device is always local */
	case TIOCSETA_32:
	case TIOCSETAW_32:
	case TIOCSETAF_32:
	{
		struct termios32 *t = (struct termios32 *)data;
		t->c_cflag |= CLOCAL;
		/* No Break */
	}
		goto fallthrough;
	case TIOCSETA_64:
	case TIOCSETAW_64:
	case TIOCSETAF_64:
	{
		struct user_termios *t = (struct user_termios *)data;
		t->c_cflag |= CLOCAL;
		/* No Break */
	}
fallthrough:
	default:
		error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
		if (ENOTTY != error) {
			break;
		}
		error = ttioctl_locked(tp, cmd, data, flag, p);
		break;
	}

	tty_unlock(tp);

	return error;
}


// output a character via the serial console driver's console_write_char().
// tp is locked on entry and exit, only called from kmoutput().
int
kmputc(__unused dev_t dev, char c)
{
	if (initialized) {
		/* OCRNL */
		if (c == '\n') {
			console_write_char('\r');
		}
		console_write_char(c);
	}

	return 0;
}


// Callouts from linesw.

#define KM_LOWAT_DELAY  ((ns_time_t)1000)

// t_oproc for this driver, called from the line discipline. tp locked on entry and exit.
static void
kmstart(struct tty *tp)
{
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		goto out;
	}
	if (tp->t_outq.c_cc == 0) {
		goto out;
	}
	tp->t_state |= TS_BUSY;
	if (tp->t_outq.c_cc > tp->t_lowat) {
		// Start immediately.
		kmoutput(tp);
	} else {
		// Wait a bit...
#if 0
		/* FIXME */
		timeout(kmtimeout, tp, hz);
#else
		kmoutput(tp);
#endif
	}
	return;

out:
	(*linesw[tp->t_line].l_start)(tp);
	return;
}

// One-shot retry timeout from kmoutput(), re-calls it until the tty output queue is empty.
// must hold tty_lock() around the kmoutput() call, the return value is ignored.
static void
kmtimeout(void *arg)
{
	struct tty     *tp = (struct tty *)arg;

	tty_lock(tp);
	(void)kmoutput(tp);
	tty_unlock(tp);
}

// tp locked on entry and exit. Called from kmstart() and kmtimeout(), which retries output
// not yet flushed, since at most sizeof(buf) chars go out per call.
static int
kmoutput(struct tty * tp)
{
	unsigned char   buf[80];        /* buffer; limits output per call */
	unsigned char   *cp;
	int     cc = -1;

	/* While there is data available to be output... */
	while (tp->t_outq.c_cc > 0) {
		cc = ndqb(&tp->t_outq, 0);
		if (cc == 0) {
			break;
		}
		// output as many characters as are available, up to the transfer buffer size.
		cc = min(cc, sizeof(buf));
		/* copy the output queue contents to the buffer */
		(void) q_to_b(&tp->t_outq, buf, cc);
		for (cp = buf; cp < &buf[cc]; cp++) {
			/* output the buffer one charatcer at a time */
			*cp = *cp & 0x7f;
		}
		if (cc > 1) {
			console_write((char *)buf, cc);
		} else {
			kmputc(tp->t_dev, *buf);
		}
	}
	// XXX likely unnecessary, the tty output queue isn't writeable while we hold tty_lock().
	if (tp->t_outq.c_cc > 0) {
		timeout(kmtimeout, tp, hz);
	}
	tp->t_state &= ~TS_BUSY;
	/* Start the output processing for the line discipline */
	(*linesw[tp->t_line].l_start)(tp);

	return 0;
}


// feeds a character from the polled serial console driver into the line discipline's l_rint().
// tty_lock() must NOT be held on tp by the caller. ECHOE is handled by the line discipline.
void
cons_cinput(char ch)
{
	struct tty *tp = km_tty[0];     /* XXX */

	/* IOHIDSystem can deliver input before bsd_autoconf calls kminit(). */
	if (tp == NULL) {
		return;
	}

	tty_lock(tp);
	(*linesw[tp->t_line].l_rint)(ch, tp);
	tty_unlock(tp);
}
