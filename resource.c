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

#include <sys/resource.h>

#include "defs.h"
#include "printrusage.h"


#include "xlat/resources.h"

const char *
sprint_rlim64(uint64_t lim)
{
	static char buf[sizeof(uint64_t)*3 + sizeof("*1024")];

	if (lim == UINT64_MAX)
		return "RLIM64_INFINITY";

	if (lim > 1024 && lim % 1024 == 0)
		sprintf(buf, "%" PRIu64 "*1024", lim / 1024);
	else
		sprintf(buf, "%" PRIu64, lim);
	return buf;
}

static ssize_t
print_rlimit64(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct rlimit_64 {
		uint64_t rlim_cur;
		uint64_t rlim_max;
	} rlim;

	if (s_umove_verbose(current_tcp, addr, &rlim))
		return -1;

	s_insert_rlim64("rlim_cur", rlim.rlim_cur);
	s_insert_rlim64("rlim_max", rlim.rlim_max);

	return sizeof(rlim);
}

#if !defined(current_wordsize) || current_wordsize == 4

const char *
sprint_rlim32(uint32_t lim)
{
	static char buf[sizeof(uint32_t)*3 + sizeof("*1024")];

	if (lim == UINT32_MAX)
		return "RLIM_INFINITY";

	if (lim > 1024 && lim % 1024 == 0)
		sprintf(buf, "%" PRIu32 "*1024", lim / 1024);
	else
		sprintf(buf, "%" PRIu32, lim);
	return buf;
}

static ssize_t
print_rlimit32(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct rlimit_32 {
		uint32_t rlim_cur;
		uint32_t rlim_max;
	} rlim;

	if (s_umove_verbose(current_tcp, addr, &rlim))
		return -1;

	s_insert_rlim32("rlim_cur", rlim.rlim_cur);
	s_insert_rlim32("rlim_max", rlim.rlim_max);

	return sizeof(rlim);
}

static ssize_t
fetch_fill_rlimit(struct s_arg *arg, unsigned long addr, void *fn_data)
{
# if defined(X86_64) || defined(X32)
	/*
	 * i386 is the only personality on X86_64 and X32
	 * with 32-bit rlim_t.
	 * When current_personality is X32, current_wordsize
	 * equals to 4 but rlim_t is 64-bit.
	 */
	if (current_personality == 1)
# else
	if (current_wordsize == 4)
# endif
		return print_rlimit32(arg, addr, fn_data);
	else
		return print_rlimit64(arg, addr, fn_data);
}

#else /* defined(current_wordsize) && current_wordsize != 4 */

#define fetch_fill_rlimit print_rlimit64

#endif

static void
s_push_rlimit_addr(const char *name)
{
	s_push_fetch_fill_struct(name, fetch_fill_rlimit, NULL);
}

static void
s_push_rlimit64_addr(const char *name)
{
	s_push_fetch_fill_struct(name, print_rlimit64, NULL);
}

SYS_FUNC(getrlimit)
{
	if (entering(tcp)) {
		s_push_xlat_signed("resource", resources, "RLIMIT_???");
		s_changeable_void("rlim");
	} else {
		s_push_rlimit_addr("rlim");
	}
	return 0;
}

SYS_FUNC(setrlimit)
{
	s_push_xlat_signed("resource", resources, "RLIMIT_???");
	s_push_rlimit_addr("rlim");

	return RVAL_DECODED;
}

SYS_FUNC(prlimit64)
{
	if (entering(tcp)) {
		s_push_d("pid");
		s_push_xlat_signed("resource", resources, "RLIMIT_???");
		s_push_rlimit64_addr("new_limit");
		s_changeable_void("old_limit");
	} else {
		s_push_rlimit64_addr("old_limit");
	}
	return 0;
}

#include "xlat/usagewho.h"

SYS_FUNC(getrusage)
{
	if (entering(tcp)) {
		s_push_xlat_signed("who", usagewho, "RUSAGE_???");
		s_changeable_void("usage");
	} else {
		s_push_rusage("usage");
	}

	return 0;
}

#ifdef ALPHA
SYS_FUNC(osf_getrusage)
{
	if (entering(tcp)) {
		s_push_xlat_signed("who", usagewho, "RUSAGE_???");
		s_changeable_void("usage");
	} else {
		s_push_rusage32("usage");
	}

	return 0;
}
#endif /* ALPHA */

#include "xlat/priorities.h"

SYS_FUNC(getpriority)
{
	s_push_xlat_signed("which", priorities, "PRIO_???");
	s_push_d("who");

	return RVAL_DECODED;
}

SYS_FUNC(setpriority)
{
	s_push_xlat_signed("which", priorities, "PRIO_???");
	s_push_d("who");
	s_push_d("prio");

	return RVAL_DECODED;
}
