/*
 * Copyright (c) 2015 Dmitry V. Levin <ldv@altlinux.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "defs.h"

#include DEF_MPERS_TYPE(timespec_t)

typedef struct timespec timespec_t;

#include MPERS_DEFS

#ifndef UTIME_NOW
# define UTIME_NOW ((1l << 30) - 1l)
#endif
#ifndef UTIME_OMIT
# define UTIME_OMIT ((1l << 30) - 2l)
#endif

static const char time_fmt[] = "{%jd, %jd}";

static ssize_t
fill_timespec_t(struct s_arg *arg, void *buf, long len, void *fn_data)
{
	timespec_t *t = buf;

	s_insert_lld("tv_sec", t->tv_sec);
	s_insert_lld("tv_nsec", t->tv_nsec);

	return 0;
}

static ssize_t
fetch_timespec_t(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	timespec_t t;

	if (s_umove_verbose(current_tcp, addr, &t))
		return -1;

	return fill_timespec_t(arg, &t,sizeof(t), fn_data);
}

static int
fill_timespec_t_utime(struct s_arg *arg, void *buf, unsigned long len, void *fn_data)
{
	timespec_t *t = buf;

	s_insert_lld("tv_sec", t->tv_sec);
	s_insert_lld("tv_nsec", t->tv_nsec);

	struct s_struct *struct_arg = S_ARG_TO_TYPE(arg, struct);

	if (t->tv_nsec == UTIME_NOW) {
		struct_arg->aux_str = "UTIME_NOW";
	} else if (t->tv_nsec == UTIME_NOW) {
		struct_arg->aux_str = "UTIME_OMIT";
	}
	return 0;
}
/*
static ssize_t
fetch_timespec_t_utime(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	timespec_t t;

	if (s_umove_verbose(current_tcp, addr, &t))
		return -1;

	return fill_timespec_t_utime(arg, &t, sizeof(t), fn_data);
}
*/
MPERS_PRINTER_DECL(ssize_t, s_fetch_fill_timespec,
	struct s_arg *arg, unsigned long addr, void *fn_data)
{
	return fetch_timespec_t(arg, addr, fn_data);
}

MPERS_PRINTER_DECL(void, push_timespec_utime_pair, const char *name)
{
	s_push_array_type(name, 2, sizeof(timespec_t), S_TYPE_struct,
		fill_timespec_t_utime, NULL);
}

MPERS_PRINTER_DECL(void, insert_itimerspec,
		   struct tcb *tcp, const long addr)
{
	s_insert_struct("itimerspec");
	s_insert_addr_type("it_interval", addr, S_TYPE_struct, fetch_timespec_t, NULL);
	s_insert_addr_type("it_value", addr + sizeof(timespec_t), S_TYPE_struct,
		fetch_timespec_t, NULL);
	s_struct_finish();
}

MPERS_PRINTER_DECL(const char *, sprint_timespec,
		   struct tcb *tcp, const long addr)
{
	timespec_t t;
	static char buf[sizeof(time_fmt) + 3 * sizeof(t)];

	if (!addr) {
		strcpy(buf, "NULL");
	} else if (!verbose(tcp) || (exiting(tcp) && syserror(tcp)) ||
		   umove(tcp, addr, &t)) {
		snprintf(buf, sizeof(buf), "%#lx", addr);
	} else {
		snprintf(buf, sizeof(buf), time_fmt,
			 (intmax_t) t.tv_sec, (intmax_t) t.tv_nsec);
	}

	return buf;
}
