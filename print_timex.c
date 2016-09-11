/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 2006-2015 Dmitry V. Levin <ldv@altlinux.org>
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

#include DEF_MPERS_TYPE(struct_timex)

#include <sys/timex.h>
typedef struct timex struct_timex;

#include MPERS_DEFS

#include "xlat/adjtimex_modes.h"
#include "xlat/adjtimex_status.h"

static ssize_t
fetch_fill_timex(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct_timex tx;

	if (s_umove_verbose(current_tcp, addr, &tx))
		return -1;

	s_insert_flags_int("modes", adjtimex_modes, tx.modes, "ADJ_???");
	s_insert_lld("offset", tx.offset);
	s_insert_lld("freq", tx.freq);
	s_insert_lld("maxerror", tx.maxerror);
	s_insert_lld("esterror", tx.esterror);
	s_insert_flags_int("status", adjtimex_status, tx.status, "STA_???");
	s_insert_lld("constant", tx.constant);
	s_insert_lld("precision", tx.precision);
	s_insert_lld("tolerance", tx.tolerance);

	s_insert_struct("time");
	s_insert_lld("tv_sec", tx.time.tv_sec);
	s_insert_lld("tv_usec", tx.time.tv_usec);
	s_struct_finish();
	s_insert_lld("tick", tx.tick);


	s_insert_lld("ppsfreq", tx.ppsfreq);
	s_insert_lld("jitter", tx.jitter);
	s_insert_d("shift", tx.shift);
	s_insert_lld("stabil", tx.stabil);
	s_insert_lld("jitcnt", tx.jitcnt);
	s_insert_lld("calcnt", tx.calcnt);
	s_insert_lld("errcnt", tx.errcnt);
	s_insert_lld("stbcnt", tx.stbcnt);

#ifdef HAVE_STRUCT_TIMEX_TAI
	s_insert_d("tai", tx.tai);
#endif

	return sizeof(tx);
}

MPERS_PRINTER_DECL(int, s_insert_timex, unsigned long addr)
{
	return s_insert_fetch_fill_struct("timex", addr, fetch_fill_timex, NULL);
}

MPERS_PRINTER_DECL(int, s_push_timex, void)
{
	return s_push_fetch_fill_struct("timex", fetch_fill_timex, NULL);
}
