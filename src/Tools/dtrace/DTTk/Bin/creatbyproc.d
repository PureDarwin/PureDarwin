#!/usr/sbin/dtrace -s
/*
 * creatbyproc.d - file creat()s by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 11-Jun-2005	Brendan Gregg	Created this.
 */

syscall::open:entry /arg1 == 0x0200 | 0x0400 | 0x0001 /* O_CREAT | O_TRUNC | O_WRONLY */ / 
{ 
	printf("%s %s", execname, copyinstr(arg0)); 
}
