/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
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
#include <sys/resource.h>

#include DEF_MPERS_TYPE(rusage_t)

typedef struct rusage rusage_t;

#include MPERS_DEFS

static ssize_t
fetch_fill_rusage(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	rusage_t ru;

	if (s_umove_verbose(current_tcp, addr, &ru))
		return -1;

	s_insert_struct("ru_utime");
	s_insert_llu("tv_sec",  zero_extend_signed_to_ull(ru.ru_utime.tv_sec));
	s_insert_llu("tv_usec", zero_extend_signed_to_ull(ru.ru_utime.tv_usec));
	s_struct_finish();

	s_insert_struct("ru_stime");
	s_insert_llu("tv_sec",  zero_extend_signed_to_ull(ru.ru_stime.tv_sec));
	s_insert_llu("tv_usec", zero_extend_signed_to_ull(ru.ru_stime.tv_usec));
	s_struct_finish();

	if (abbrev(current_tcp)) {
		s_insert_ellipsis();
	} else {
		s_insert_llu("ru_maxrss",   zero_extend_signed_to_ull(ru.ru_maxrss));
		s_insert_llu("ru_ixrss",    zero_extend_signed_to_ull(ru.ru_ixrss));
		s_insert_llu("ru_idrss",    zero_extend_signed_to_ull(ru.ru_idrss));
		s_insert_llu("ru_isrss",    zero_extend_signed_to_ull(ru.ru_isrss));
		s_insert_llu("ru_minflt",   zero_extend_signed_to_ull(ru.ru_minflt));
		s_insert_llu("ru_majflt",   zero_extend_signed_to_ull(ru.ru_majflt));
		s_insert_llu("ru_nswap",    zero_extend_signed_to_ull(ru.ru_nswap));
		s_insert_llu("ru_inblock",  zero_extend_signed_to_ull(ru.ru_inblock));
		s_insert_llu("ru_oublock",  zero_extend_signed_to_ull(ru.ru_oublock));
		s_insert_llu("ru_msgsnd",   zero_extend_signed_to_ull(ru.ru_msgsnd));
		s_insert_llu("ru_msgrcv",   zero_extend_signed_to_ull(ru.ru_msgrcv));
		s_insert_llu("ru_nsignals", zero_extend_signed_to_ull(ru.ru_nsignals));
		s_insert_llu("ru_nvcsw",    zero_extend_signed_to_ull(ru.ru_nvcsw));
		s_insert_llu("ru_nivcsw",   zero_extend_signed_to_ull(ru.ru_nivcsw));
	}

	return sizeof(ru);
}

MPERS_PRINTER_DECL(void, s_insert_rusage, const char *name, unsigned long addr)
{
	s_insert_addr_type(name, addr, S_TYPE_struct, fetch_fill_rusage, NULL);
}

MPERS_PRINTER_DECL(void, s_push_rusage, const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, fetch_fill_rusage, NULL);
}

#ifdef ALPHA
static ssize_t
fetch_fill_rusage32(struct s_arg *arg, long addr, void *fn_data)
{
	struct timeval32 {
		unsigned tv_sec;
		unsigned tv_usec;
	};
	struct rusage32 {
		struct timeval32 ru_utime;	/* user time used */
		struct timeval32 ru_stime;	/* system time used */
		long	ru_maxrss;		/* maximum resident set size */
		long	ru_ixrss;		/* integral shared memory size */
		long	ru_idrss;		/* integral unshared data size */
		long	ru_isrss;		/* integral unshared stack size */
		long	ru_minflt;		/* page reclaims */
		long	ru_majflt;		/* page faults */
		long	ru_nswap;		/* swaps */
		long	ru_inblock;		/* block input operations */
		long	ru_oublock;		/* block output operations */
		long	ru_msgsnd;		/* messages sent */
		long	ru_msgrcv;		/* messages received */
		long	ru_nsignals;		/* signals received */
		long	ru_nvcsw;		/* voluntary context switches */
		long	ru_nivcsw;		/* involuntary " */
	} ru;

	if (s_umove_verbose(current_tcp, addr, &ru))
		return -1;

	s_insert_struct("ru_utime");
	s_insert_llu("tv_sec",  (long)ru.ru_utime.tv_sec);
	s_insert_llu("tv_usec", (long)ru.ru_utime.tv_usec);
	s_struct_finish();

	s_insert_struct("ru_stime");
	s_insert_llu("tv_sec",  (long)ru.ru_stime.tv_sec);
	s_insert_llu("tv_usec", (long)ru.ru_stime.tv_usec);
	s_struct_finish();

	if (abbrev(current_tcp)) {
		s_insert_ellipsis();
	} else {
		s_insert_lu("ru_maxrss",   ru.ru_maxrss);
		s_insert_lu("ru_ixrss",    ru.ru_ixrss);
		s_insert_lu("ru_idrss",    ru.ru_idrss);
		s_insert_lu("ru_isrss",    ru.ru_isrss);
		s_insert_lu("ru_minflt",   ru.ru_minflt);
		s_insert_lu("ru_majflt",   ru.ru_majflt);
		s_insert_lu("ru_nswap",    ru.ru_nswap);
		s_insert_lu("ru_inblock",  ru.ru_inblock);
		s_insert_lu("ru_oublock",  ru.ru_oublock);
		s_insert_lu("ru_msgsnd",   ru.ru_msgsnd);
		s_insert_lu("ru_msgrcv",   ru.ru_msgrcv);
		s_insert_lu("ru_nsignals", ru.ru_nsignals);
		s_insert_lu("ru_nvcsw",    ru.ru_nvcsw);
		s_insert_lu("ru_nivcsw",   ru.ru_nivcsw);
	}
}

void
s_insert_rusage32(const char *name, unsigned long addr)
{
	s_insert_addr_type(name, addr, S_TYPE_struct, fetch_fill_rusage32, NULL);
}

void
s_push_rusage32(const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, fetch_fill_rusage32, NULL);
}

#endif
