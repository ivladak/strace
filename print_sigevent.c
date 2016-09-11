/*
 * Copyright (c) 2003, 2004 Ulrich Drepper <drepper@redhat.com>
 * Copyright (c) 2005-2016 Dmitry V. Levin <ldv@altlinux.org>
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

#include DEF_MPERS_TYPE(struct_sigevent)
#include "sigevent.h"
#include MPERS_DEFS

#include <signal.h>
#include "xlat/sigev_value.h"

static ssize_t
fetch_fill_sigevent(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct_sigevent sev;

	if (s_umove_verbose(current_tcp, addr, &sev))
		return -1;

	if (sev.sigev_value.sival_ptr) {
		s_insert_struct("sigev_value");
		s_insert_d("sival_int", sev.sigev_value.sival_int);
		s_insert_addr("sival_ptr",
			(unsigned long) sev.sigev_value.sival_ptr);
		s_struct_finish();
	}

	switch (sev.sigev_notify) {
	case SIGEV_SIGNAL:
	case SIGEV_THREAD:
	case SIGEV_THREAD_ID:
		s_insert_signo("sigev_signo", sev.sigev_signo);
		break;
	default:
		s_insert_u("sigev_signo", sev.sigev_signo);
	}

	s_insert_xlat_int("sigev_notify", sigev_value, sev.sigev_notify,
		"SIGEV_???");

	switch (sev.sigev_notify) {
	case SIGEV_THREAD_ID:
		s_insert_d("sigev_notify_thread_id", sev.sigev_un.tid);
		break;
	case SIGEV_THREAD:
		s_insert_addr("sigev_notify_function",
			(unsigned long) sev.sigev_un.sigev_thread.function);
		s_insert_addr("sigev_notify_attributes",
			(unsigned long) sev.sigev_un.sigev_thread.attribute);
		break;
	}

	return sizeof(sev);
}

MPERS_PRINTER_DECL(void, s_insert_sigevent, const char *name,
	unsigned long addr)
{
	s_insert_fetch_fill_struct(name, addr, fetch_fill_sigevent, NULL);
}

MPERS_PRINTER_DECL(void, s_push_sigevent, const char *name)
{
	s_push_fetch_fill_struct(name, fetch_fill_sigevent, NULL);
}
