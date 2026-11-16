/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <dtrace.h>

/*ARGSUSED*/
const char *
dtrace_subrstr(dtrace_hdl_t *dtp, int subr)
{
#pragma unused(dtp)
	switch (subr) {
	case DIF_SUBR_RAND: return ("rand");
	case DIF_SUBR_MUTEX_OWNED: return ("mutex_owned");
	case DIF_SUBR_MUTEX_OWNER: return ("mutex_owner");
	case DIF_SUBR_MUTEX_TYPE_ADAPTIVE: return ("mutex_type_adaptive");
	case DIF_SUBR_MUTEX_TYPE_SPIN: return ("mutex_type_spin");
	case DIF_SUBR_RW_READ_HELD: return ("rw_read_held");
	case DIF_SUBR_RW_WRITE_HELD: return ("rw_write_held");
	case DIF_SUBR_RW_ISWRITER: return ("rw_iswriter");
	case DIF_SUBR_COPYIN: return ("copyin");
	case DIF_SUBR_COPYINSTR: return ("copyinstr");
	case DIF_SUBR_SPECULATION: return ("speculation");
	case DIF_SUBR_PROGENYOF: return ("progenyof");
	case DIF_SUBR_STRLEN: return ("strlen");
	case DIF_SUBR_COPYOUT: return ("copyout");
	case DIF_SUBR_COPYOUTSTR: return ("copyoutstr");
	case DIF_SUBR_ALLOCA: return ("alloca");
	case DIF_SUBR_BCOPY: return ("bcopy");
	case DIF_SUBR_COPYINTO: return ("copyinto");
	case DIF_SUBR_MSGDSIZE: return ("msgdsize");
	case DIF_SUBR_MSGSIZE: return ("msgsize");
	case DIF_SUBR_GETMAJOR: return ("getmajor");
	case DIF_SUBR_GETMINOR: return ("getminor");
	case DIF_SUBR_DDI_PATHNAME: return ("ddi_pathname");
	case DIF_SUBR_STRJOIN: return ("strjoin");
	case DIF_SUBR_LLTOSTR: return ("lltostr");
	case DIF_SUBR_BASENAME: return ("basename");
	case DIF_SUBR_DIRNAME: return ("dirname");
	case DIF_SUBR_CLEANPATH: return ("cleanpath");
	case DIF_SUBR_STRCHR: return ("strchr");
	case DIF_SUBR_STRRCHR: return ("strrchr");
	case DIF_SUBR_STRSTR: return ("strstr");
	case DIF_SUBR_STRTOK: return ("strtok");
	case DIF_SUBR_SUBSTR: return ("substr");
	case DIF_SUBR_INDEX: return ("index");
	case DIF_SUBR_RINDEX: return ("rindex");
	case DIF_SUBR_HTONS: return ("htons");
	case DIF_SUBR_HTONL: return ("htonl");
	case DIF_SUBR_HTONLL: return ("htonll");
	case DIF_SUBR_NTOHS: return ("ntohs");
	case DIF_SUBR_NTOHL: return ("ntohl");
	case DIF_SUBR_NTOHLL: return ("ntohll");
	case DIF_SUBR_INET_NTOP: return ("inet_ntop");
	case DIF_SUBR_INET_NTOA: return ("inet_ntoa");
	case DIF_SUBR_INET_NTOA6: return ("inet_ntoa6");
	case DIF_SUBR_KDEBUG_TRACE: return ("kdebug_trace");
	case DIF_SUBR_KDEBUG_TRACE_STRING: return ("kdebug_trace_string");
#if defined(DIF_SUBR_LIVEDUMP)
	case DIF_SUBR_LIVEDUMP: return ("livedump");
#endif /* defined(DIF_SUBR_LIVEDUMP) */
	default: return ("unknown");
	}
}
