/*
 * Copyright 2006-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H


#include <termios.h>

/* These only work on sockets for now */
#define FIONBIO 	0xbe000000
#define FIONREAD	0xbe000001

#define FIOSEEKDATA	0xbe000002
#define FIOSEEKHOLE	0xbe000003

#endif	/* _SYS_IOCTL_H */
