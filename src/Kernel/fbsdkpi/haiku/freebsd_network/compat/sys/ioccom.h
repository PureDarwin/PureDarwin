/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_IOCCOM_H_
#define _FBSD_COMPAT_SYS_IOCCOM_H_


#define	_IOW(g,n,t)		SIOCEND + n
#define	_IOWR(g,n,t)	SIOCEND + n

#endif /* _FBSD_COMPAT_SYS_IOCCOM_H_ */
