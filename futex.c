/*
 * Copyright (c) 2002-2003 Roland McGrath  <roland@redhat.com>
 * Copyright (c) 2007-2008 Ulrich Drepper <drepper@redhat.com>
 * Copyright (c) 2009 Andreas Schwab <schwab@redhat.com>
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

#ifndef FUTEX_PRIVATE_FLAG
# define FUTEX_PRIVATE_FLAG 128
#endif
#ifndef FUTEX_CLOCK_REALTIME
# define FUTEX_CLOCK_REALTIME 256
#endif

#include "xlat/futexops.h"
#include "xlat/futexwakeops.h"
#include "xlat/futexwakecmps.h"

#include "print_time_structured.h"

SYS_FUNC(futex)
{
	const int op = tcp->u_arg[1];
	const int cmd = op & 127;

	const long uaddr2 = tcp->u_arg[4];

	const unsigned int val3 = tcp->u_arg[5];

	s_set_comment_level(S_SAN_PREFERRED);

	s_push_addr("uaddr");
	s_push_xlat_int("futex_op", futexops, "FUTEX_???");

	switch (cmd) {
	case FUTEX_WAIT:
		s_push_u("val");
		s_push_timespec("timeout");
		break;
	case FUTEX_LOCK_PI:
		s_push_empty(S_TYPE_u);
		s_push_timespec("timeout");
		break;
	case FUTEX_WAIT_BITSET:
		s_push_u("val");
		s_push_timespec("timeout");
		s_insert_x("val3", val3);
		s_append_comment("bitset");
		break;
	case FUTEX_WAKE_BITSET:
		s_push_u("val");
		s_insert_x("val3", val3);
		s_append_comment("bitset");
		break;
	case FUTEX_REQUEUE:
		s_push_u("val");
		s_append_comment("processes to wake");
		s_push_u("val2");
		s_append_comment("processes to requeue");
		s_push_addr("uaddr2");
		s_append_comment("futex to reqeueue to");
		break;
	case FUTEX_CMP_REQUEUE:
	case FUTEX_CMP_REQUEUE_PI:
		s_push_u("val");
		s_append_comment("processes to wake");
		s_push_u("val2");
		s_append_comment("processes to requeue");
		s_push_addr("uaddr2");
		s_append_comment("futex to reqeueue to");
		s_push_u("val3");
		s_append_comment("value of requeue target");
		break;
	case FUTEX_WAKE_OP:
		s_push_u("val");
		s_push_u("val2");
		s_append_comment("processes to wake on uaddr2 if op is successful");
		s_push_addr("uaddr2");
		s_append_comment("futex to apply operation upon");

		s_insert_xlat_int_empty("val3");

		/* In order to avoid generation of leading "0|" */
		if (val3 & (8 << 28))
			s_append_xlat_int_ex("op", NULL, val3 & (8 << 28),
				"FUTEX_OP_OPARG_SHIFT", 28,
				"oparg should be treated as power of 2");
		s_append_xlat_int_ex("op", futexwakeops,
			val3 &   (0x7 << 28), "FUTEX_OP_???", 28, "op");
		s_append_xlat_int_ex("oparg", NULL,
			val3 & (0xfff << 12), NULL, 12, "oparg");
		s_append_xlat_int_ex("cmp", futexwakecmps,
			val3 &   (0xf << 24), "FUTEX_OP_CMP_???", 24, "cmp");
		s_append_xlat_int_ex("cmparg", NULL,
			val3 & (0xfff <<  0), NULL, 0, "cmparg");
		break;
	case FUTEX_WAIT_REQUEUE_PI:
		s_push_u("val");
		s_push_timespec("timeout");
		s_insert_addr("uaddr2", uaddr2);
		s_append_comment("PI futex to requeue processes on");
		break;
	case FUTEX_FD:
	case FUTEX_WAKE:
		s_push_u("val");
		break;
	case FUTEX_UNLOCK_PI:
	case FUTEX_TRYLOCK_PI:
		break;
	default:
		s_push_u("val");
		s_push_lx("val2");
		s_push_addr("uaddr2");
		s_push_x("val3");
		break;
	}

	return RVAL_DECODED;
}
