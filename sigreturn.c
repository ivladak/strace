#include "defs.h"
#include <signal.h>
#include "regs.h"
#include "ptrace.h"

#if defined HAVE_ASM_SIGCONTEXT_H && !defined HAVE_STRUCT_SIGCONTEXT
# include <asm/sigcontext.h>
#endif

#ifndef NSIG
# warning NSIG is not defined, using 32
# define NSIG 32
#elif NSIG < 32
# error NSIG < 32
#endif

#include "signal_structured.h"
#include "structured_sigmask.h"

void
s_insert_sigcontext(long addr)
{
	s_insert_struct("sigcontext");
	s_insert_sigset_addr_len("mask", addr, NSIG / 8);
	s_struct_finish();
}

#include "arch_sigreturn.c"

SYS_FUNC(sigreturn)
{
	arch_sigreturn(tcp);

	return RVAL_DECODED;
}
