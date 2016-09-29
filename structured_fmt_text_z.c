#include <fcntl.h>
#include <stdarg.h>

#include "defs.h"
#include "printresource.h"
#include "process.h"
#include "structured_sigmask.h"
#include "xlat/ptrace_events.h"
#include "ptrace.h"

#include "structured_fmt_text.h"
#include "structured_fmt_text_z.h"

static void
s_syscall_succ_print_unfinished(struct tcb *tcp)
{
}

static void
s_syscall_succ_print_leader(struct tcb *tcp, struct timeval *tv,
	struct timeval *dtv)
{
}

static void
s_syscall_succ_print_before(struct tcb *tcp)
{
}

static void
s_syscall_succ_print_entering(struct tcb *tcp)
{
}

static void
s_syscall_succ_print_exiting(struct tcb *tcp)
{
	if (tcp->u_error)
		return;

	s_printer_cur = &s_printer_text;
	printleader(tcp);
	s_printer_cur = &s_printer_text_z;
	s_printer_text.print_before(tcp);
	s_printer_text.print_entering(tcp);
	s_printer_text.print_exiting(tcp);
}

static void
s_syscall_succ_print_after(struct tcb *tcp)
{
	if (tcp->u_error)
		return;

	s_printer_text.print_after(tcp);
}

static void
s_syscall_succ_print_tv(struct tcb *tcp, struct timeval *tv)
{
	s_printer_text.print_tv(tcp, tv);
}

static void
s_syscall_succ_print_resumed(struct tcb *tcp)
{
}

static void
s_syscall_succ_print_unavailable_entering(struct tcb *tcp, int scno_good)
{
}

static void
s_syscall_succ_print_unavailable_exiting(struct tcb *tcp)
{
}

static void
s_syscall_succ_print_signal(struct tcb *tcp)
{
	s_printer_text.print_signal(tcp);
}

static void
s_text_print_message(struct tcb *tcp, enum s_msg_type type, const char *msg,
	va_list args)
{
	s_printer_text.print_message(tcp, type, msg, args);
}

struct s_printer s_printer_text_z = {
	.name = "succeeding",
	.print_unfinished = s_syscall_succ_print_unfinished,
	.print_leader = s_syscall_succ_print_leader,
	.print_before = s_syscall_succ_print_before,
	.print_entering = s_syscall_succ_print_entering,
	.print_exiting  = s_syscall_succ_print_exiting,
	.print_after = s_syscall_succ_print_after,
	.print_resumed = s_syscall_succ_print_resumed,
	.print_tv = s_syscall_succ_print_tv,
	.print_unavailable_entering = s_syscall_succ_print_unavailable_entering,
	.print_unavailable_exiting = s_syscall_succ_print_unavailable_exiting,
	.print_signal = s_syscall_succ_print_signal,
	.print_message = s_text_print_message,
};
