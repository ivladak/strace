/*
 * Copyright (c) 2008-2015 Dmitry V. Levin <ldv@altlinux.org>
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
#ifdef HAVE_SYS_SIGNALFD_H
# include <sys/signalfd.h>
#endif

#include "signal_structured.h"

#include "xlat/sfd_flags.h"

static int
do_signalfd(struct tcb *tcp, bool flags_arg)
{
	/* NB: kernel requires arg[2] == NSIG / 8 */
	s_push_fd("fd");
	s_push_sigset_addr_len("mask", tcp->u_arg[2]);
	s_push_lu("sizemask");
	if (flags_arg) {
		s_push_flags_int("flags", sfd_flags, "SFD_???");
	}

	return RVAL_DECODED | RVAL_FD;
}

SYS_FUNC(signalfd)
{
	return do_signalfd(tcp, false);
}

SYS_FUNC(signalfd4)
{
	return do_signalfd(tcp, true);
}
