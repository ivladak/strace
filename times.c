/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2012 H.J. Lu <hongjiu.lu@intel.com>
 * Copyright (c) 2015 Elvira Khabirova <lineprinter0@gmail.com>
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
#include DEF_MPERS_TYPE(tms_t)
#include <sys/times.h>
typedef struct tms tms_t;
#include MPERS_DEFS

MPERS_PRINTER_DECL(ssize_t, fill_tms_t, struct s_arg *arg, unsigned long addr,
	void *fn_data)
{
	tms_t tbuf;

	if (umove(current_tcp, addr, &tbuf)) {
		return -1;
	}
	s_insert_llu("tms_utime", zero_extend_signed_to_ull(tbuf.tms_utime));
	s_insert_llu("tms_stime", zero_extend_signed_to_ull(tbuf.tms_stime));
	s_insert_llu("tms_cutime", zero_extend_signed_to_ull(tbuf.tms_cutime));
	s_insert_llu("tms_cstime", zero_extend_signed_to_ull(tbuf.tms_cstime));

	return sizeof(tbuf);
}

SYS_FUNC(times)
{

	if (entering(tcp)) {
		s_changeable_void("buf");
		return 0;
	}

	s_push_addr_type("buf", S_TYPE_struct, MPERS_FUNC_NAME(fill_tms_t), NULL);

	return syserror(tcp) ? RVAL_DECIMAL :
#if defined(RVAL_LUDECIMAL) && !defined(IN_MPERS)
			       RVAL_LUDECIMAL;
#else
			       RVAL_UDECIMAL;
#endif
}
