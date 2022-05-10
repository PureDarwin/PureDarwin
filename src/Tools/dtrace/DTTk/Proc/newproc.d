#!/usr/sbin/dtrace -s
/*
 * newproc.d - snoop new processes as they are executed. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

/*
 * Updated to capture arguments in OS X. Unfortunately this isn't straight forward... nor inexpensive ...
 * Bound the size of copyinstr()'s and printf incrementally to prevent "out of scratch space errors"
 * print "(...)" if the length of an argument exceeds COPYINSTRLIMIT.
 * print "<...>" if argc exceeds 5.
 */

#pragma D option quiet

this unsigned long long argv_ptr; /* Wide enough for 64 bit user procs */
inline int COPYINSTRLIMIT = 128;

proc:::exec-success
{
	print_pid[pid] = 1; /* This pid emerged from an exec, make a note of that. */
}

/*
 * The "this" variables are local to (all) of the following syscall::bsdthread_register:return probes,
 * and only those probes. They must be initialized before use in each new firing.
 */
syscall::bsdthread_register:return
{
	this->argc = 0; /* Disable argument collection until we notice an exec-success */
	this->need_newline = 0;
}

syscall::bsdthread_register:return
/ print_pid[pid] /
{
	print_pid[pid] = 0;
	this->is64Bit = curpsinfo->pr_dmodel == PR_MODEL_ILP32 ? 0 : 1;
	this->wordsize = this->is64Bit ? 8 : 4;

	this->argc = curpsinfo->pr_argc; 
	this->argc = (this->argc < 0) ? 0 : this->argc; /* Safety */

	this->argv_ptr = curpsinfo->pr_argv;

	printf("%Y %d <%d> %s ", walltimestamp, pid, ppid, this->is64Bit ? "64b" : "32b");
	this->need_newline = 1;
}

syscall::bsdthread_register:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg, COPYINSTRLIMIT);
	printf(" %s%s", this->here_arg, COPYINSTRLIMIT - 1 < strlen(this->here_arg) ? " (...)" : "");

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::bsdthread_register:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg, COPYINSTRLIMIT);
	printf(" %s%s", this->here_arg, COPYINSTRLIMIT - 1 < strlen(this->here_arg) ? " (...)" : "");

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::bsdthread_register:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg, COPYINSTRLIMIT);
	printf(" %s%s", this->here_arg, COPYINSTRLIMIT - 1 < strlen(this->here_arg) ? " (...)" : "");

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::bsdthread_register:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg, COPYINSTRLIMIT);
	printf(" %s%s", this->here_arg, COPYINSTRLIMIT - 1 < strlen(this->here_arg) ? " (...)" : "");

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::bsdthread_register:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg, COPYINSTRLIMIT);
	printf(" %s%s", this->here_arg, COPYINSTRLIMIT - 1 < strlen(this->here_arg) ? " (...)" : "");

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::bsdthread_register:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg, COPYINSTRLIMIT);
	printf(" %s%s", this->here_arg, COPYINSTRLIMIT - 1 < strlen(this->here_arg) ? " (...)" : "");

	this->argv_ptr += this->wordsize;
	this->argc--;
}


syscall::bsdthread_register:return
/ this->argc /
{
	printf(" <...>");
	this->argc = 0;
}

syscall::bsdthread_register:return
/ this->need_newline /
{
	printf("\n");
	this->need_newline = 0;
}

ERROR
/ arg4 == DTRACEFLT_NOSCRATCH /
{
	printf(" <...>\n");
	this->argc = 0;
	this->need_newline = 0;
}


