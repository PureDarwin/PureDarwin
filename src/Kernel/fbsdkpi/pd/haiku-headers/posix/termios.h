/*
 * Copyright 2004-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _TERMIOS_H_
#define _TERMIOS_H_


#include <sys/types.h>


typedef __haiku_uint16 tcflag_t;
typedef __haiku_uint32 speed_t;
typedef unsigned char cc_t;

#define NCCS	11		/* number of control characters */

struct termios {
	tcflag_t		c_iflag;			/* input modes */
	tcflag_t		c_ispeed;			/* input baudrate */
	tcflag_t		c_oflag;			/* output modes */
	tcflag_t		c_ospeed;			/* output baudrate */
	tcflag_t		c_cflag;			/* control modes */
	tcflag_t		c_ispeed_high;		/* high word of input baudrate */
	tcflag_t		c_lflag;			/* local modes */
	tcflag_t		c_ospeed_high;		/* high word of output baudrate */
	char			c_line;				/* line discipline */
	unsigned char	_padding;		/* unused */
	unsigned char	_padding2;		/* unused */
	cc_t			c_cc[NCCS];			/* control characters */
};

/* control characters */
#define	VINTR	0
#define	VQUIT	1
#define	VERASE	2
#define	VKILL	3
#define	VEOF	4
#define	VEOL	5
#define	VMIN	4
#define	VTIME	5
#define	VEOL2	6
#define	VSWTCH	7
#define VSTART  8
#define VSTOP   9
#define VSUSP   10

/* c_iflag - input control modes */
#define	IGNBRK		0x01		/* ignore break condition */
#define	BRKINT		0x02		/* break sends interrupt */
#define	IGNPAR		0x04		/* ignore characters with parity errors */
#define	PARMRK		0x08		/* mark parity errors */
#define	INPCK		0x10		/* enable input parity checking */
#define	ISTRIP		0x20		/* strip high bit from characters */
#define	INLCR		0x40		/* maps newline to CR on input */
#define	IGNCR		0x80		/* ignore carriage returns */
#define	ICRNL		0x100		/* map CR to newline on input */
#define	IUCLC		0x200		/* map all upper case to lower */
#define	IXON		0x400		/* enable input SW flow control */
#define	IXANY		0x800		/* any character will restart input */
#define	IXOFF		0x1000		/* enable output SW flow control */

/* c_oflag - output control modes */
#define	OPOST		0x01		/* enable postprocessing of output */
#define	OLCUC		0x02		/* map lowercase to uppercase */
#define	ONLCR		0x04		/* map NL to CR-NL on output */
#define	OCRNL		0x08		/* map CR to NL on output */
#define	ONOCR		0x10		/* no CR output when at column 0 */
#define	ONLRET		0x20		/* newline performs CR function */
#define	OFILL		0x40		/* use fill characters for delays */
#define	OFDEL		0x80		/* Fills are DEL, otherwise NUL */
#define	NLDLY		0x100		/* Newline delays: */
#define	NL0			0x000
#define	NL1			0x100
#define	CRDLY		0x600		/* Carriage return delays: */
#define	CR0			0x000
#define	CR1			0x200
#define	CR2			0x400
#define	CR3			0x600
#define	TABDLY		0x1800		/* Tab delays: */
#define	TAB0		0x0000
#define	TAB1		0x0800
#define	TAB2		0x1000
#define	TAB3		0x1800
#define	BSDLY		0x2000		/* Backspace delays: */
#define	BS0			0x0000
#define	BS1			0x2000
#define	VTDLY		0x4000		/* Vertical tab delays: */
#define	VT0			0x0000
#define	VT1			0x4000
#define	FFDLY		0x8000		/* Form feed delays: */
#define	FF0			0x0000
#define	FF1			0x8000

/* c_cflag - control modes */
#define	CBAUD		0x1F			/* line speed definitions */

#define	B0			0x00			/* hang up */
#define	B50			0x01			/* 50 baud */
#define	B75			0x02
#define	B110		0x03
#define	B134		0x04
#define	B150		0x05
#define	B200		0x06
#define	B300		0x07
#define	B600		0x08
#define	B1200		0x09
#define	B1800		0x0A
#define	B2400		0x0B
#define	B4800		0x0C
#define	B9600		0x0D
#define	B19200		0x0E
#define B38400		0x0F
#define B57600		0x10
#define B115200		0x11
#define B230400		0x12
#define	B31250		0x13			/* for MIDI */

#define	CSIZE		0x20			/* character size */
#define	CS5			0x00			/* only 7 and 8 bits supported */
#define	CS6			0x00			/* Note, it was not very wise to set all of these */
#define	CS7			0x00			/* to zero, but there is not much we can do about it*/
#define	CS8			0x20
#define	CSTOPB		0x40			/* send 2 stop bits, not 1 */
#define	CREAD		0x80			/* enable receiver */
#define	PARENB		0x100			/* parity enable */
#define	PARODD		0x200			/* odd parity, else even */
#define	HUPCL		0x400			/* hangs up on last close */
#define	CLOCAL		0x800			/* indicates local line */
#define	XLOBLK		0x1000			/* block layer output ?*/
#define	CTSFLOW		0x2000			/* enable CTS flow */
#define	RTSFLOW		0x4000			/* enable RTS flow */
#define	CRTSCTS		(RTSFLOW | CTSFLOW)

/* c_lflag - local modes */
#define ISIG		0x01			/* enable signals */
#define ICANON		0x02			/* Canonical input */
#define XCASE		0x04			/* Canonical u/l case */
#define ECHO		0x08			/* Enable echo */
#define ECHOE		0x10			/* Echo erase as bs-sp-bs */
#define ECHOK		0x20			/* Echo nl after kill */
#define ECHONL		0x40			/* Echo nl */
#define NOFLSH		0x80			/* Disable flush after int or quit */
#define TOSTOP		0x100			/* stop bg processes that write to tty */
#define IEXTEN		0x200			/* implementation defined extensions */
#define ECHOCTL		0x400
#define ECHOPRT		0x800
#define ECHOKE		0x1000
#define FLUSHO		0x2000
#define PENDIN		0x4000

/* options to tcsetattr() */
#define TCSANOW		0x01			/* make change immediate */
#define TCSADRAIN	0x02			/* drain output, then change */
#define TCSAFLUSH	0x04			/* drain output, flush input */

/* actions for tcflow() */
#define TCOOFF		0x01			/* suspend output */
#define TCOON		0x02			/* restart output */
#define TCIOFF		0x04			/* transmit STOP character, intended to stop input data */
#define TCION		0x08			/* transmit START character, intended to resume input data */

/* values for tcflush() */
#define TCIFLUSH	0x01			/* flush pending input */
#define TCOFLUSH	0x02			/* flush untransmitted output */
#define TCIOFLUSH	0x03			/* flush both */


/* ioctl() identifiers to control the TTY */
#define TCGETA				0x8000
#define TCSETA				(TCGETA + 1)
#define TCSETAF				(TCGETA + 2)
#define TCSETAW				(TCGETA + 3)
/* TCWAITEVENT				(TCGETA + 4) */
#define TCSBRK				(TCGETA + 5)
#define TCFLSH				(TCGETA + 6)
#define TCXONC				(TCGETA + 7)
/* TCQUERYCONNECTED			(TCGETA + 8) */
#define TCGETBITS			(TCGETA + 9)	/* same as TIOCMGET */
#define	TCSETDTR			(TCGETA + 10)
#define TCSETRTS			(TCGETA + 11)
#define TIOCGWINSZ			(TCGETA + 12)	/* pass in a struct winsize */
#define TIOCSWINSZ			(TCGETA + 13)	/* pass in a struct winsize */
/* TCVTIME					(TCGETA + 14) */
#define TIOCGPGRP			(TCGETA + 15)	/* Gets the process group ID of the TTY device */
#define TIOCSPGRP			(TCGETA + 16)	/* Sets the process group ID ('pgid' in BeOS) */
#define TIOCSCTTY			(TCGETA + 17)	/* Become controlling TTY */
#define TIOCMGET			(TCGETA + 18)	/* get line state */
#define TIOCMSET			(TCGETA + 19)	/* does TCSETDTR/TCSETRTS */
#define TIOCSBRK			(TCGETA + 20)	/* set txd pin */
#define TIOCCBRK			(TCGETA + 21)	/* both are a frontend to TCSBRK */
#define TIOCMBIS			(TCGETA + 22)	/* set bits in line state */
#define TIOCMBIC			(TCGETA + 23)	/* clear bits in line state */
#define	TIOCGSID			(TCGETA + 24)	/* get session leader process group ID */
#define TIOCOUTQ			(TCGETA + 25)	/* get output queue size */
#define TIOCEXCL			(TCGETA + 26)	/* set exclusive use of tty */
#define TIOCNXCL			(TCGETA + 27)	/* clear exclusive use of tty */

/* for TIOCGWINSZ */
struct winsize {
	unsigned short	ws_row;
	unsigned short	ws_col;
	unsigned short	ws_xpixel;
	unsigned short	ws_ypixel;
};

/* Bits for the TCGETBITS control */
#define	TCGB_CTS		0x01
#define TCGB_DSR		0x02
#define TCGB_RI			0x04
#define TCGB_DCD		0x08

/* Bits for the TIOCMGET / TIOCMSET control */
#define TIOCM_CTS		TCGB_CTS	/* clear to send */
#define TIOCM_CD		TCGB_DCD	/* carrier detect */
#define TIOCM_CAR		TCGB_DCD
#define TIOCM_RI		TCGB_RI		/* ring indicator */
#define TIOCM_RNG		TCGB_RI
#define TIOCM_DSR		TCGB_DSR	/* dataset ready */
#define TIOCM_DTR		0x10		/* data terminal ready */
#define TIOCM_RTS		0x20		/* request to send */


#ifdef __cplusplus
extern "C" {
#endif

extern speed_t	cfgetispeed(const struct termios *termios);
extern speed_t	cfgetospeed(const struct termios *termios);
extern int		cfsetispeed(struct termios *termios, speed_t speed);
extern int		cfsetospeed(struct termios *termios, speed_t speed);
extern void		cfmakeraw(struct termios *termios);
extern int		tcgetattr(int fd, struct termios *termios);
extern int		tcsetattr(int fd, int option, const struct termios *termios);
extern int		tcsendbreak(int fd, int duration);
extern int		tcdrain(int fd);
extern int		tcflow(int fd, int action);
extern int		tcflush(int fd, int queueSelector);
extern pid_t	tcgetsid(int fd);
extern int		tcsetsid(int fd, pid_t pid);
extern int		tcgetwinsize(int fd, struct winsize* winsize);
extern int		tcsetwinsize(int fd, const struct winsize* winsize);

#ifdef __cplusplus
}
#endif

#endif /* _TERMIOS_H_ */
