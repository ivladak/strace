/*
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

#include "xlat/fan_classes.h"
#include "xlat/fan_init_flags.h"

#ifndef FAN_ALL_CLASS_BITS
# define FAN_ALL_CLASS_BITS (FAN_CLASS_NOTIF | FAN_CLASS_CONTENT | FAN_CLASS_PRE_CONTENT)
#endif
#ifndef FAN_NOFD
# define FAN_NOFD -1
#endif

SYS_FUNC(fanotify_init)
{
	unsigned int flags = s_get_cur_arg(S_TYPE_u);

	s_insert_xlat_int("flags", fan_classes, flags & FAN_ALL_CLASS_BITS,
		"FAN_CLASS_???");
	s_append_xlat_int_val("flags", fan_init_flags,
		flags & ~FAN_ALL_CLASS_BITS, "FAN_???");
	s_push_open_modes("event_f_flags");

	return RVAL_DECODED | RVAL_FD;
}

#include "xlat/fan_mark_flags.h"
#include "xlat/fan_event_flags.h"

SYS_FUNC(fanotify_mark)
{
	s_push_fd("fanotify_fd");
	s_push_flags_int("flags", fan_mark_flags, "FAN_MARK_???");

	/*
	 * the mask argument is defined as 64-bit,
	 * but kernel uses the lower 32 bits only.
	 */
	unsigned long long mask = s_get_cur_arg(S_TYPE_llu);
#ifdef HPPA
	/* Parisc is weird.  See arch/parisc/kernel/sys_parisc32.c.  */
	mask = (mask << 32) | (mask >> 32);
#endif

	s_insert_flags_64("mask", fan_event_flags, mask, "FAN_???");
	s_push_fan_dirfd("dirfd");
	s_push_path("pathname");

	return RVAL_DECODED;
}
