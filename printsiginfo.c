/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2001 John Hughes <john@Calva.COM>
 * Copyright (c) 2013 Denys Vlasenko <vda.linux@googlemail.com>
 * Copyright (c) 2011-2015 Dmitry V. Levin <ldv@altlinux.org>
 * Copyright (c) 2015 Elvira Khabirova <lineprinter0@gmail.com>
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

#include DEF_MPERS_TYPE(siginfo_t)

#include <signal.h>
#include <linux/audit.h>

#include MPERS_DEFS

#ifndef IN_MPERS
#include "printsiginfo.h"
#endif

#include "xlat/audit_arch.h"
#include "xlat/sigbus_codes.h"
#include "xlat/sigchld_codes.h"
#include "xlat/sigfpe_codes.h"
#include "xlat/sigill_codes.h"
#include "xlat/siginfo_codes.h"
#include "xlat/sigpoll_codes.h"
#include "xlat/sigprof_codes.h"
#include "xlat/sigsegv_codes.h"
#include "xlat/sigsys_codes.h"
#include "xlat/sigtrap_codes.h"

#ifdef SIGEMT
# include "xlat/sigemt_codes.h"
#endif

#ifndef SI_FROMUSER
# define SI_FROMUSER(sip)	((sip)->si_code <= 0)
#endif

static void
fill_sigsource(const siginfo_t *sip)
{
	s_insert_u("si_pid", sip->si_pid);
	s_insert_u("si_uid", sip->si_uid);
}

static void
fill_sigval(const siginfo_t *sip)
{
	s_insert_struct("si_value");
	s_insert_d("int", sip->si_int);
	s_insert_addr("ptr", (unsigned long)sip->si_ptr);
	s_struct_finish();
}

static void
fill_si_code(int si_signo, unsigned int si_code)
{
	const struct xlat *xlat_to_use = siginfo_codes;

	if (!xlookup(siginfo_codes, si_code)) {
		switch (si_signo) {
		case SIGTRAP:
			xlat_to_use = sigtrap_codes;
			break;
		case SIGCHLD:
			xlat_to_use = sigchld_codes;
			break;
		case SIGPOLL:
			xlat_to_use = sigpoll_codes;
			break;
		case SIGPROF:
			xlat_to_use = sigprof_codes;
			break;
		case SIGILL:
			xlat_to_use = sigill_codes;
			break;
#ifdef SIGEMT
		case SIGEMT:
			xlat_to_use = sigemt_codes;
			break;
#endif
		case SIGFPE:
			xlat_to_use = sigfpe_codes;
			break;
		case SIGSEGV:
			xlat_to_use = sigsegv_codes;
			break;
		case SIGBUS:
			xlat_to_use = sigbus_codes;
			break;
		case SIGSYS:
			xlat_to_use = sigsys_codes;
			break;
		}
	}

	s_insert_xlat_int("si_code", xlat_to_use, si_code, NULL);
}

static void
fill_si_info(const siginfo_t *sip)
{
	if (sip->si_errno) {
		const char *errno_str = NULL;

		if ((unsigned) sip->si_errno < nerrnos && errnoent[sip->si_errno])
			errno_str = errnoent[sip->si_errno];

		s_insert_xlat_signed("si_errno", NULL, sip->si_errno, errno_str);
	}

	if (SI_FROMUSER(sip)) {
		switch (sip->si_code) {
		case SI_USER:
			fill_sigsource(sip);
			break;
		case SI_TKILL:
			fill_sigsource(sip);
			break;
#if defined HAVE_SIGINFO_T_SI_TIMERID && defined HAVE_SIGINFO_T_SI_OVERRUN
		case SI_TIMER:
			s_insert_x("si_timerid", sip->si_timerid);
			s_insert_d("si_overrun", sip->si_overrun);
			fill_sigval(sip);
			break;
#endif
		default:
			fill_sigsource(sip);
			if (sip->si_ptr)
				fill_sigval(sip);
			break;
		}
	} else {
		switch (sip->si_signo) {
		case SIGCHLD:
			fill_sigsource(sip);
			if (sip->si_code == CLD_EXITED)
				s_insert_d("si_status", sip->si_status);
			else
				s_insert_signo("si_status", sip->si_status);
			s_insert_llu("si_utime", zero_extend_signed_to_ull(sip->si_utime));
			s_insert_llu("si_stime", zero_extend_signed_to_ull(sip->si_stime));
			break;
		case SIGILL: case SIGFPE:
		case SIGSEGV: case SIGBUS:
			s_insert_addr("si_addr", (unsigned long)sip->si_addr);
			break;
		case SIGPOLL:
			switch (sip->si_code) {
			case POLL_IN: case POLL_OUT: case POLL_MSG:
				s_insert_ld("si_band", (long)sip->si_band);
				/* XXX mpers? si_fd? */
				break;
			}
			break;
#ifdef HAVE_SIGINFO_T_SI_SYSCALL
		case SIGSYS: {
			s_insert_addr("si_call_addr",
				(unsigned long) sip->si_call_addr);
			s_insert_scno("si_syscall", (unsigned) sip->si_syscall);
			s_insert_xlat_int("si_arch", audit_arch, sip->si_arch,
				"AUDIT_ARCH_???");
			break;
		}
#endif
		default:
			if (sip->si_pid || sip->si_uid)
				fill_sigsource(sip);
			if (sip->si_ptr)
				fill_sigval(sip);
		}
	}
}

#ifdef IN_MPERS
static
#endif
void
printsiginfo(const siginfo_t *sip)
{
	if (sip->si_signo == 0)
		return 0;

	s_insert_signo("si_signo", sip->si_signo);
	fill_si_code(sip->si_signo, sip->si_code);

#ifdef SI_NOINFO
	if (sip->si_code != SI_NOINFO)
#endif
		fill_si_info(sip);

	return 0;
}

static int
fill_siginfo_t(struct s_arg *arg, void *buf, size_t len, void *fn_data)
{
	printsiginfo((const siginfo_t *)buf);
        return 0;
}

static ssize_t
fetch_fill_siginfo_t(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	siginfo_t si;

	if (s_umove_verbose(current_tcp, addr, &si))
		return -1;

	fill_siginfo_t(arg, &si, sizeof(si), fn_data);

	return sizeof(si);
}

MPERS_PRINTER_DECL(void, s_insert_siginfo, const char *name,
	const void *si_void)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_struct, name);
	s_struct_enter(s);
	fill_siginfo_t(&s->arg, (void *)si_void, sizeof(siginfo_t), NULL);
	s_struct_finish();
}

MPERS_PRINTER_DECL(void, s_insert_siginfo_addr, const char *name, unsigned long addr)
{
	s_insert_addr_type(name, addr, S_TYPE_struct, fetch_fill_siginfo_t, NULL);
}

MPERS_PRINTER_DECL(void, s_push_siginfo, const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, fetch_fill_siginfo_t, NULL);
}

MPERS_PRINTER_DECL(void, s_push_siginfo_array, const char *name,
	unsigned long nmemb)
{
	s_push_array_type("data", nmemb, sizeof(siginfo_t), S_TYPE_struct,
		fill_siginfo_t, NULL);
}
