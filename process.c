/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *                     Linux for s390 port by D.J. Barrow
 *                    <barrow_dj@mail.yahoo.com,djbarrow@de.ibm.com>
 * Copyright (c) 2000 PocketPenguins Inc.  Linux for Hitachi SuperH
 *                    port by Greg Banks <gbanks@pocketpenguins.com>
 *
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

#ifdef HAVE_ELF_H
# include <elf.h>
#endif
#include <signal.h>

#include "xlat/nt_descriptor_types.h"

#include "printsiginfo.h"
#include "ptrace.h"
#include "regs.h"
#include "seccomp_structured.h"
#include "structured_sigmask.h"
#include "xlat/ptrace_cmds.h"
#include "xlat/ptrace_setoptions_flags.h"
#include "xlat/ptrace_peeksiginfo_flags.h"

#define uoff(member)	offsetof(struct user, member)
#define XLAT_UOFF(member)	{ uoff(member), "offsetof(struct user, " #member ")" }

static const struct xlat struct_user_offsets[] = {
#include "userent.h"
	XLAT_END
};

void
print_user_offset_addr(const unsigned long addr)
{
	const struct xlat *x;

	for (x = struct_user_offsets; x->str; ++x) {
		if (x->val >= addr)
			break;
	}

	if (!x->str) {
		printaddr(addr);
	} else if (x->val > addr) {
		if (x == struct_user_offsets) {
			printaddr(addr);
		} else {
			--x;
			tprintf("%s + %lu",
				x->str, addr - (unsigned long) x->val);
		}
	} else {
		tprints(x->str);
	}
}

static ssize_t
fetch_fill_peeksiginfo(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct tcb *tcp = current_tcp;
	struct {
		uint64_t off;
		uint32_t flags;
		uint32_t nr;
	} psi;

	if (s_umove_verbose(tcp, addr, &psi))
		return -1;

	s_insert_llu("off", psi.off);
	s_insert_flags_int("flags", ptrace_peeksiginfo_flags, psi.flags,
		"PTRACE_PEEKSIGINFO_???");
	s_insert_u("nr", psi.nr);

	return sizeof(psi);
}

SYS_FUNC(ptrace)
{
	const unsigned long request = tcp->u_arg[0];
	const unsigned long addr = tcp->u_arg[2];

	if (entering(tcp)) {
		s_push_xlat_long("request", ptrace_cmds, "PTRACE_???");

		if (request == PTRACE_TRACEME) {
			/* pid, addr, and data are ignored. */
			return RVAL_DECODED;
		}

		/* pid */
		s_push_d("pid");

		/* addr */
		switch (request) {
		case PTRACE_ATTACH:
		case PTRACE_INTERRUPT:
		case PTRACE_KILL:
		case PTRACE_LISTEN:
			/* addr and data are ignored */
			return RVAL_DECODED;
		case PTRACE_PEEKUSER:
		case PTRACE_POKEUSER:
			s_push_ptrace_uaddr("addr");
			break;
		case PTRACE_GETREGSET:
		case PTRACE_SETREGSET:
			s_push_xlat_long("addr", nt_descriptor_types, "NT_???");
			break;
		case PTRACE_GETSIGMASK:
		case PTRACE_SETSIGMASK:
		case PTRACE_SECCOMP_GET_FILTER:
			s_push_lu("addr");
			break;
		case PTRACE_PEEKSIGINFO: {
			s_push_fetch_fill_struct("addr", fetch_fill_peeksiginfo, NULL);
			break;
		}
		default:
			s_push_addr("addr");
		}

# if defined IA64 || defined SPARC || defined SPARC64
		switch (request) {
#  ifdef IA64
		case PTRACE_PEEKDATA:
		case PTRACE_PEEKTEXT:
		case PTRACE_PEEKUSER:
			/* data is ignored */
			return RVAL_DECODED | RVAL_HEX;
#  endif /* IA64 */
#  if defined SPARC || defined SPARC64
		case PTRACE_GETREGS:
		case PTRACE_SETREGS:
		case PTRACE_GETFPREGS:
		case PTRACE_SETFPREGS:
			/* data is ignored */
			return RVAL_DECODED;
#  endif /* SPARC || SPARC64 */
		}
# endif /* IA64 || SPARC || SPARC64 */

		/* data */
		switch (request) {
		case PTRACE_CONT:
		case PTRACE_DETACH:
		case PTRACE_SYSCALL:
#ifdef PTRACE_SINGLESTEP
		case PTRACE_SINGLESTEP:
#endif
#ifdef PTRACE_SINGLEBLOCK
		case PTRACE_SINGLEBLOCK:
#endif
#ifdef PTRACE_SYSEMU
		case PTRACE_SYSEMU:
#endif
#ifdef PTRACE_SYSEMU_SINGLESTEP
		case PTRACE_SYSEMU_SINGLESTEP:
#endif
			s_push_signo("data");
			break;
		case PTRACE_SEIZE:
		case PTRACE_SETOPTIONS:
#ifdef PTRACE_OLDSETOPTIONS
		case PTRACE_OLDSETOPTIONS:
#endif
			s_push_flags_long("data", ptrace_setoptions_flags, "PTRACE_O_???");
			break;
		case PTRACE_SETSIGINFO:
			s_push_siginfo("data");
			break;
		case PTRACE_SETSIGMASK:
			s_push_sigmask("data", addr);
			break;
		case PTRACE_SETREGSET:
			// XXX fix: tprint_iov(tcp, /*len:*/ 1, data, IOV_DECODE_ADDR);
			s_push_addr("data");
			break;
#ifndef IA64
		case PTRACE_PEEKDATA:
		case PTRACE_PEEKTEXT:
		case PTRACE_PEEKUSER:
#endif
		case PTRACE_GETEVENTMSG:
		case PTRACE_GETREGSET:
		case PTRACE_GETSIGINFO:
		case PTRACE_GETSIGMASK:
		case PTRACE_PEEKSIGINFO:
		case PTRACE_SECCOMP_GET_FILTER:
			if (verbose(tcp)) {
				/* print data on exiting syscall */
				return 0;
			}
			/* fall through */
		default:
			s_push_addr("data");
			break;
		}

		return RVAL_DECODED;
	} else {
		switch (request) {
#ifndef IA64
		case PTRACE_PEEKDATA:
		case PTRACE_PEEKTEXT:
		case PTRACE_PEEKUSER:
			s_push_lu_addr("data");
			break;
#endif
		case PTRACE_GETEVENTMSG:
			s_push_lu_addr("data");
			break;
		case PTRACE_GETREGSET:
			// XXX tprint_iov(tcp, /*len:*/ 1, data, IOV_DECODE_ADDR);
			s_push_addr("data");
			break;
		case PTRACE_GETSIGINFO:
			s_push_siginfo("data");
			break;
		case PTRACE_GETSIGMASK:
			s_push_sigmask("data", addr);
			break;
		case PTRACE_PEEKSIGINFO:
			if (syserror(tcp))
				s_push_addr("data");
			else
				s_push_siginfo_array("data", tcp->u_rval);
			break;
		case PTRACE_SECCOMP_GET_FILTER:
			if (syserror(tcp))
				s_push_addr("data");
			else
				s_push_seccomp_fprog("data", tcp->u_rval);
			break;
		}
	}
	return 0;
}
