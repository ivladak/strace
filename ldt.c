/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2002-2004 Roland McGrath <roland@redhat.com>
 * Copyright (c) 2010 Andreas Schwab <schwab@linux-m68k.org>
 * Copyright (c) 2014-2015 Dmitry V. Levin <ldv@altlinux.org>
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

#if defined I386 || defined X86_64 || defined X32

# include <asm/ldt.h>

static int
fill_user_desc(struct s_arg *arg, void *buf, size_t len, void *fn_data)
{
	struct user_desc *desc = buf;
	bool set = (bool) fn_data;

	s_insert_u("entry_number", desc->entry_number);
	if (set)
		s_changeable();
	s_insert_addr("base_addr", desc->base_addr);
	s_insert_u("limit", desc->limit);
	s_insert_u("seg_32bit", desc->seg_32bit);
	s_insert_u("contents", desc->contents);
	s_insert_u("read_exec_only", desc->read_exec_only);
	s_insert_u("limit_in_pages", desc->limit_in_pages);
	s_insert_u("seg_not_present", desc->seg_not_present);
	s_insert_u("useable", desc->useable);

	return 0;
}

SYS_FUNC(modify_ldt)
{
	s_push_d("func");
	if (tcp->u_arg[2] != sizeof(struct user_desc))
		s_push_addr("ptr");
	else
		s_push_fill_struct("user_desc", sizeof(struct user_desc),
			fill_user_desc, NULL);
	s_push_lu("bytecount");

	return RVAL_DECODED;
}

SYS_FUNC(set_thread_area)
{
	if (entering(tcp)) {
		s_push_fill_struct("user_desc", sizeof(struct user_desc),
			fill_user_desc, (void *) true);
	} else {
		struct user_desc desc;

		if (!verbose(tcp) || syserror(tcp) ||
		    umove(tcp, tcp->u_arg[0], &desc) < 0) {
			/* returned entry_number is not available */
		} else {
			s_insert_u("entry_number", desc.entry_number);
		}
	}
	return 0;
}

SYS_FUNC(get_thread_area)
{
	if (entering(tcp)) {
		s_push_addr("u_info");
		s_changeable();
	} else
		s_push_fill_struct("user_desc", sizeof(struct user_desc),
			fill_user_desc, (void *) false);

	return 0;
}

#endif /* I386 || X86_64 || X32 */

#if defined(M68K) || defined(MIPS)
SYS_FUNC(set_thread_area)
{
	printaddr(tcp->u_arg[0]);

	return RVAL_DECODED;

}
#endif

#if defined(M68K)
SYS_FUNC(get_thread_area)
{
	return RVAL_DECODED | RVAL_HEX;
}
#endif
