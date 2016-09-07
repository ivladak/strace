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

#include DEF_MPERS_TYPE(timeval_t)

typedef struct timeval timeval_t;

#include MPERS_DEFS

static const char time_fmt[] = "{%jd, %jd}";

static int
fill_timeval_t(struct s_arg *arg, void *buf, unsigned long len, void *fn_data)
{
	timeval_t *t = buf;

	s_insert_lld("tv_sec", t->tv_sec);
	s_insert_lld("tv_usec", t->tv_usec);

	return 0;
}

static int
s_fill_timeval(struct s_arg *arg, void *buf, unsigned long len, void *fn_data)
{
	return fill_timeval_t(arg, buf, len, fn_data);
}

static ssize_t
fetch_fill_itimerval_t(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	ssize_t ret = s_insert_addr_type_sized("it_interval", addr, sizeof(timeval_t),
		S_TYPE_struct, fill_timeval_t, NULL);
	if (ret < 0) return ret;
	return ret + s_insert_addr_type_sized("it_value", addr + sizeof(timeval_t),
		sizeof(timeval_t), S_TYPE_struct, fill_timeval_t, NULL);
}

MPERS_PRINTER_DECL(const char *, sprint_timeval,
		   struct tcb *tcp, const long addr)
{
	timeval_t t;
	static char buf[sizeof(time_fmt) + 3 * sizeof(t)];

	if (!addr) {
		strcpy(buf, "NULL");
	} else if (!verbose(tcp) || (exiting(tcp) && syserror(tcp)) ||
		   umove(tcp, addr, &t)) {
		snprintf(buf, sizeof(buf), "%#lx", addr);
	} else {
		snprintf(buf, sizeof(buf), time_fmt,
			 (intmax_t) t.tv_sec, (intmax_t) t.tv_usec);
	}

	return buf;
}

MPERS_PRINTER_DECL(void, s_insert_timeval_addr, const char *name, long addr)
{
	s_insert_addr_type_sized(name, addr, sizeof(timeval_t), S_TYPE_struct,
		s_fill_timeval, NULL);
}

MPERS_PRINTER_DECL(void, s_push_timeval, const char *name)
{
	s_push_addr_type_sized(name, sizeof(timeval_t), S_TYPE_struct,
		s_fill_timeval, NULL);
}

MPERS_PRINTER_DECL(void, s_push_itimerval, const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, fetch_fill_itimerval_t, NULL);
}

MPERS_PRINTER_DECL(void, push_timeval_pair, const char *name)
{
	s_push_array_type(name, 2, sizeof(timeval_t), S_TYPE_struct,
		fill_timeval_t, NULL);
}

