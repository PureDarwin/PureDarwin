#!/bin/ksh
# #!/usr/bin/ksh
#
# lastwords - print last few syscalls for dying processes.
#             Written using DTrace (Solaris 10 3/05).
#
# 20-Apr-2006, ver 0.71         (check for newer versions)
#
# This prints the last few system calls for processes matching
# the given name, when they exit. This makes use of a ring buffer
# so that the impact on the system is minimised.
#
# USAGE: lastwords command
#    eg,
#        lastwords netscape
#
# FIELDS:
#           TIME     Time of syscall return, ns
#           PID      Process ID
#           EXEC     Process name (execname)
#           SYSCALL  System call
#           RETURN   Return value for system call
#           ERR      errno for system call
#
# BASED ON: /usr/demo/dtrace/ring.d
#
# SEE ALSO: DTrace Guide "Buffers and Buffering" chapter (docs.sun.com)
#           dtruss (DTraceToolkit)
#
# PORTIONS: Copyright (c) 2005, 2006 Brendan Gregg.
#
# CDDL HEADER START
#
#  The contents of this file are subject to the terms of the
#  Common Development and Distribution License, Version 1.0 only
#  (the "License").  You may not use this file except in compliance
#  with the License.
#
#  You can obtain a copy of the license at Docs/cddl1.txt
#  or http://www.opensolaris.org/os/licensing.
#  See the License for the specific language governing permissions
#  and limitations under the License.
#
# CDDL HEADER END
#
# 09-Jun-2005   Brendan Gregg   Created this.
#

### Usage
function usage
{
	cat <<-END >&2
	USAGE: lastwords command
	   eg,
	       lastwords netscape
	END
	exit 1
}

### Process arguments
if (( $# != 1 )); then
        usage
fi
command=$1

print "Tracing... Waiting for $command to exit..."

### Run DTrace
/usr/sbin/dtrace -n '
 #pragma D option quiet
 #pragma D option bufpolicy=ring
 #pragma D option bufsize=16k

 syscall:::return
 /execname == strstr(execname, $$1) || $$1 == strstr($$1, execname)/
 {
	/* buffer syscall details */
	printf("%-18d %5d %12s %12s %10x %3d\n",
	    timestamp,pid,execname,probefunc,(int)arg0,errno);
 }

 /* SOLARIS: proc::proc_exit:exit */
 proc:::exit
 /execname == strstr(execname, $$1) || $$1 == strstr($$1, execname)/
 {
	/* print, erm, footer */
	printf("%-18s %5s %12s %12s %10s %3s\n",
	    "TIME","PID","EXEC","SYSCALL","RETURN","ERR");
	exit(0);
 }
' "$command"
