/*
 * Copyright (c) 2004 Ulrich Drepper <drepper@redhat.com>
 * Copyright (c) 2005-2015 Dmitry V. Levin <ldv@altlinux.org>
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
#include <fcntl.h>
#include "print_time_structured.h"

SYS_FUNC(mq_open)
{
	s_push_path("name");
	s_push_open_modes("oflag");
	if (tcp->u_arg[1] & O_CREAT) {
		s_insert_umode_t("mode", tcp->u_arg[2]);
		s_push_mqattr("attr");
	}
	return RVAL_DECODED;
}

SYS_FUNC(mq_timedsend)
{
	s_push_ld("mqdes");
	s_insert_str("msg", tcp->u_arg[1], tcp->u_arg[2]);
	s_push_empty(S_TYPE_lu);
	s_push_lu("msg_len");
	s_push_ld("msg_prio");
	s_push_timespec("abs_timeout");
	return RVAL_DECODED;
}

SYS_FUNC(mq_timedreceive)
{
	if (entering(tcp)) {
		s_push_ld("mqdes");
		s_push_addr("msg");
		s_changeable();
		s_push_lu("msg_len");
		s_push_ld("msg_prio");
		s_push_timespec("abs_timeout");
	} else {
		s_insert_str("msg", tcp->u_arg[1], tcp->u_arg[2]);
	}
	return 0;
}

SYS_FUNC(mq_notify)
{
	tprintf("%ld, ", tcp->u_arg[0]);
	print_sigevent(tcp, tcp->u_arg[1]);
	return RVAL_DECODED;
}

SYS_FUNC(mq_getsetattr)
{
	if (entering(tcp)) {
		s_push_ld("mqdes");
		s_push_mqattr("newattr");
		s_changeable_void("oldattr");
	} else
		s_push_mqattr("oldattr");
	return 0;
}
