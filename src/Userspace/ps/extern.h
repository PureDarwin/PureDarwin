/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)extern.h	8.3 (Berkeley) 4/2/94
 *	$FreeBSD: extern.h,v 1.8 1998/05/25 05:07:17 steve Exp $
 */

struct kinfo;
struct nlist;
struct var;
struct varent;

extern fixpt_t ccpu;
extern int cflag, eval, fscale, nlistread, rawcpu;
#ifdef __APPLE__
extern uint64_t mempages;
#else
extern unsigned long mempages;
#endif
extern time_t now;
extern int showthreads, sumrusage, termwidth, totwidth;
extern STAILQ_HEAD(velisthead, varent) varlist;

__BEGIN_DECLS
int	 get_task_info(KINFO *);
void	 command(KINFO *, VARENT *);
void	 just_command(KINFO *, VARENT *);
void	 args(KINFO *, VARENT *);
int	 s_command(KINFO *);
int	 s_just_command(KINFO *);
int	 s_args(KINFO *);
void	 cputime(KINFO *, VARENT *);
void	 pstime(KINFO *, VARENT *);
void	 p_etime(KINFO *, VARENT *);
int	 s_etime(KINFO *);
void	 putime(KINFO *, VARENT *);
int	 donlist(void);
void	 evar(KINFO *, VARENT *);
VARENT	*find_varentry(VAR *);
const	 char *fmt_argv(char **, char *, size_t);
int	 getpcpu(KINFO *);
double	 getpmem(KINFO *);
void	 logname(KINFO *, VARENT *);
void	 longtname(KINFO *, VARENT *);
void	 lstarted(KINFO *, VARENT *);
void	 maxrss(KINFO *, VARENT *);
void	 nlisterr(struct nlist *);
void	 p_rssize(KINFO *, VARENT *);
void	 pagein(KINFO *, VARENT *);
void	 parsefmt(const char *, int);
void	 pcpu(KINFO *, VARENT *);
void	 pmem(KINFO *, VARENT *);
void	 pri(KINFO *, VARENT *);
void	 rtprior(KINFO *, VARENT *);
void	 printheader(void);
void	 pvar(KINFO *, VARENT *);
void	 runame(KINFO *, VARENT *);
void	 rvar(KINFO *, VARENT *);
int	 s_runame(KINFO *);
int	 s_uname(KINFO *);
void	 showkey(void);
void	 started(KINFO *, VARENT *);
void	 state(KINFO *, VARENT *);
void	 tdev(KINFO *, VARENT *);
void	 tname(KINFO *, VARENT *);
void	 tsize(KINFO *, VARENT *);
void	 ucomm(KINFO *, VARENT *);
void	 uname(KINFO *, VARENT *);
void	 uvar(KINFO *, VARENT *);
void	 vsize(KINFO *, VARENT *);
void	 wchan(KINFO *, VARENT *);
void	 wq(KINFO *, VARENT *);
__END_DECLS
