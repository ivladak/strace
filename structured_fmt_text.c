#include <fcntl.h>
#include <stdarg.h>

#include "defs.h"
#include "printresource.h"
#include "process.h"
#include "structured_sigmask.h"
#include "xlat/ptrace_events.h"
#include "ptrace.h"

#include "structured_fmt_text.h"

static void
s_print_xlat_text(struct s_xlat *x, uint64_t value, uint64_t mask,
	const char *str, uint32_t flags, void *fn_data)
{
	uint64_t descaled_val = value >> abs(x->scale);
	const char *fmt;

	switch (x->arg.type) {
	case S_TYPE_xlat:
		fmt = "%#" PRIx32;
		break;
	case S_TYPE_xlat_l:
		fmt = (current_wordsize > sizeof(int)) ?
			"%#" PRIx64 : "%#" PRIx32;
		break;
	case S_TYPE_xlat_ll:
		fmt = "%#" PRIx64;
		break;
	case S_TYPE_xlat_d:
		fmt = "%" PRId32;
		break;
	case S_TYPE_xlat_ld:
		fmt = (current_wordsize > sizeof(int)) ?
			"%" PRId64 : "%" PRId32;
		break;
	case S_TYPE_xlat_lld:
		fmt = "%" PRId64;
		break;
	case S_TYPE_xlat_u:
		fmt = "%" PRIu32;
		break;
	default:
		fmt = "%#" PRIx64;
	}

	/* Corner case */
	if (!(flags & SPXF_FIRST) && (flags & SPXF_DEFAULT) && !value &&
	    x->flags)
		return;

	if (!(flags & SPXF_FIRST))
		tprints("|");

	if (flags & SPXF_DEFAULT) {
		tprintf(fmt, descaled_val);
		if (x->scale)
			tprintf("<<%d", (int)abs(x->scale));
		if (str && value)
			tprintf(" /* %s */", str);
	} else {
		if (str) {
			tprintf("%s", str);
			if (x->scale > 0)
				tprintf("<<%d", (int)x->scale);
		} else {
			tprintf(fmt, descaled_val);
			if (descaled_val && x->scale)
				tprintf("<<%d", (int)abs(x->scale));
		}
	}
}

void
s_print_sigmask_text(int bit, const char *str, bool set, void *data)
{
	enum sigmask_printer_text_flags {
		SPT_INVERT,
		SPT_SEPARATOR,
	};
	unsigned *flags = data;

	if (!((*flags) & (1 << SPT_INVERT)) ^ set)
		return;

	if ((*flags) & (1 << SPT_SEPARATOR))
		tprints(" ");
	else
		*flags |= 1 << SPT_SEPARATOR;

	tprints(str);
}

#ifndef AT_FDCWD
# define AT_FDCWD -100
#endif
#ifndef FAN_NOFD
# define FAN_NOFD -1
#endif

#if !defined WCOREFLAG && defined WCOREFLG
# define WCOREFLAG WCOREFLG
#endif
#ifndef WCOREFLAG
# define WCOREFLAG 0x80
#endif
#ifndef WCOREDUMP
# define WCOREDUMP(status)  ((status) & 0200)
#endif
#ifndef W_STOPCODE
# define W_STOPCODE(sig)  ((sig) << 8 | 0x7f)
#endif
#ifndef W_EXITCODE
# define W_EXITCODE(ret, sig)  ((ret) << 8 | (sig))
#endif
#ifndef W_CONTINUED
# define W_CONTINUED 0xffff
#endif

int
printstatus(int status)
{
	int exited = 0;

	/*
	 * Here is a tricky presentation problem.  This solution
	 * is still not entirely satisfactory but since there
	 * are no wait status constructors it will have to do.
	 */
	if (WIFSTOPPED(status)) {
		int sig = WSTOPSIG(status);
		tprintf("{WIFSTOPPED(s) && WSTOPSIG(s) == %s%s}",
			signame(sig & 0x7f),
			sig & 0x80 ? " | 0x80" : "");
		status &= ~W_STOPCODE(sig);
	}
	else if (WIFSIGNALED(status)) {
		tprintf("{WIFSIGNALED(s) && WTERMSIG(s) == %s%s}",
			signame(WTERMSIG(status)),
			WCOREDUMP(status) ? " && WCOREDUMP(s)" : "");
		status &= ~(W_EXITCODE(0, WTERMSIG(status)) | WCOREFLAG);
	}
	else if (WIFEXITED(status)) {
		tprintf("{WIFEXITED(s) && WEXITSTATUS(s) == %d}",
			WEXITSTATUS(status));
		exited = 1;
		status &= ~W_EXITCODE(WEXITSTATUS(status), 0);
	}
#ifdef WIFCONTINUED
	else if (WIFCONTINUED(status)) {
		tprints("{WIFCONTINUED(s)}");
		status &= ~W_CONTINUED;
	}
#endif
	else {
		tprintf("%#x", status);
		return 0;
	}

	if (status) {
		unsigned int event = (unsigned int) status >> 16;
		if (event) {
			tprints(" | ");
			printxval(ptrace_events, event, "PTRACE_EVENT_???");
			tprints(" << 16");
			status &= 0xffff;
		}
		if (status)
			tprintf(" | %#x", status);
	}

	return exited;
}

static void
s_val_print(struct s_arg *arg)
{
	switch (arg->type) {
#define PRINT_INT(TYPE, ENUM, PR) \
	case S_TYPE_ ## ENUM : tprintf("%" PR, \
		(TYPE)(((struct s_num *)s_arg_to_type(arg))->val)); break

	PRINT_INT(char, c, "c");

	PRINT_INT(signed char, hhd, "hhd");
	PRINT_INT(short, hd, "hd");
	PRINT_INT(int, d, "d");
	PRINT_INT(long, ld, "ld");
	PRINT_INT(long long, lld, "lld");

	PRINT_INT(unsigned char, hhu, "hhu");
	PRINT_INT(unsigned short, hu, "hu");
	PRINT_INT(unsigned, u, "u");
	PRINT_INT(unsigned long, lu, "lu");
	PRINT_INT(unsigned long long, llu, "llu");

#undef PRINT_INT

#define PRINT_ALT_INT(TYPE, ENUM, PR) \
	case S_TYPE_ ## ENUM : tprintf("%#" PR, \
		(TYPE)(((struct s_num *)s_arg_to_type(arg))->val)); break

	PRINT_ALT_INT(unsigned char, hhx, "hhx");
	PRINT_ALT_INT(unsigned short, hx, "hx");
	PRINT_ALT_INT(unsigned, x, "x");
	PRINT_ALT_INT(unsigned long, lx, "lx");
	PRINT_ALT_INT(unsigned long long, llx, "llx");

	PRINT_ALT_INT(unsigned char, hho, "hho");
	PRINT_ALT_INT(unsigned short, ho, "ho");
	PRINT_ALT_INT(int, o, "o");
	PRINT_ALT_INT(long, lo, "lo");
	PRINT_ALT_INT(long long, llo, "llo");

#undef PRINT_ALT_INT

	case S_TYPE_uid:
	case S_TYPE_gid: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		if ((uid_t)-1U == (uid_t)p->val)
			tprints("-1");
		else
			tprintf("%u", (uid_t)p->val);

		break;
	}

	case S_TYPE_uid16:
	case S_TYPE_gid16: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		if ((uint16_t)-1U == (uint16_t)p->val)
			tprints("-1");
		else
			tprintf("%hu", (uint16_t)p->val);

		break;
	}

	case S_TYPE_time: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		tprintf("%s", sprinttime(p->val));

		break;
	}

	case S_TYPE_umask: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		print_numeric_long_umask(p->val);

		break;
	}

	case S_TYPE_umode_t: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		print_numeric_umode_t(p->val);

		break;
	}

	case S_TYPE_short_mode_t:
	case S_TYPE_mode_t: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		print_symbolic_mode_t(p->val);

		break;
	}

	case S_TYPE_signo: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		tprintf("%s", signame(p->val));

		break;
	}

	case S_TYPE_scno: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);
		const char *str = SCNO_IS_VALID(p->val) ? syscall_name(p->val) : NULL;

		if (str)
			tprintf("__NR_%s", str);
		else
			tprintf("%lu", (unsigned long)p->val);

		break;
	}

	case S_TYPE_wstatus: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		printstatus(p->val);

		break;
	}

	case S_TYPE_rlim64: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		tprintf("%s", sprint_rlim64(p->val));

		break;
	}

	case S_TYPE_rlim32: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

#if !defined(current_wordsize) || current_wordsize == 4
		tprintf("%s", sprint_rlim32(p->val));
#else /* !defined(current_wordsize) || current_wordsize == 4 */
		tprintf("%u", (unsigned)p->val);
#endif /* !defined(current_wordsize) || current_wordsize == 4 */

		break;
	}

	case S_TYPE_sa_handler: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		print_sa_handler(p->val);

		break;
	}

	case S_TYPE_clockid: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		tprintf("%s", sprintclockname((int)p->val));

		break;
	}

	case S_TYPE_changeable: {
		struct s_changeable *s_ch = S_ARG_TO_TYPE(arg, changeable);
		if (s_ch->entering)
			s_val_print(s_ch->entering);
		if (s_ch->exiting) {
			if (!s_arg_equal(s_ch->entering, s_ch->exiting)) {
				if (s_ch->entering)
					tprints(" => ");
				s_val_print(s_ch->exiting);
			}
		}
		if (!s_ch->entering && !s_ch->exiting)
			tprints("[x]");
		break;
	}
	case S_TYPE_str:
	case S_TYPE_path: {
		struct s_str *s_p = S_ARG_TO_TYPE(arg, str);

		if ((print_quoted_string(s_p->str, s_p->len,
		    s_p->has_nul ? QUOTE_0_TERMINATED : 0) > 0) && s_p->has_nul)
			tprints("...");

		break;
	}
	case S_TYPE_addr: {
		struct s_addr *p = S_ARG_TO_TYPE(arg, addr);
		bool print_brackets = false;

		if (p->val) {
			switch (S_TYPE_KIND(p->val->type)) {
			case S_TYPE_KIND_num:
			case S_TYPE_KIND_addr:
			case S_TYPE_KIND_xlat:
			case S_TYPE_KIND_changeable:
				print_brackets = true;
				break;
			default:
				break;
			}

			if (print_brackets)
				tprints("[");

			s_val_print(p->val);

			if (print_brackets)
				tprints("]");
		} else {
			printaddr(p->addr);
		}

		break;
	}
	case S_TYPE_ptrace_uaddr: {
		struct s_addr *p = S_ARG_TO_TYPE(arg, addr);

		print_user_offset_addr(p->addr);

		break;
	}
	case S_TYPE_fan_dirfd:
	case S_TYPE_dirfd:
	case S_TYPE_fd: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		if ((arg->type == S_TYPE_fan_dirfd) &&
		    ((int)p->val == FAN_NOFD))
			tprints("FAN_NOFD");
		else if ((arg->type != S_TYPE_fd) && ((int)p->val == AT_FDCWD))
			tprints("AT_FDCWD");
		else
			printfd(arg->syscall->tcp, (int)p->val);

		break;
	}
	case S_TYPE_xlat:
	case S_TYPE_xlat_l:
	case S_TYPE_xlat_ll:
	case S_TYPE_xlat_u:
	case S_TYPE_xlat_d:
	case S_TYPE_xlat_ld:
	case S_TYPE_xlat_lld: {
		struct s_xlat *f_p = S_ARG_TO_TYPE(arg, xlat);

		s_process_xlat(f_p, s_print_xlat_text, NULL);

		break;
	}
	case S_TYPE_sigmask: {
		struct s_sigmask *p = S_ARG_TO_TYPE(arg, sigmask);
		unsigned bitcount = popcount32(p->sigmask32,
			(p->bytes + 3) / 4);
		unsigned flags = (bitcount >= (p->bytes * 8 * 2 / 3));

		if (flags)
			tprints("~");
		tprints("[");
		s_process_sigmask(p, s_print_sigmask_text, &flags);
		tprints("]");

		break;
	}
	case S_TYPE_dev_t: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);
		tprintf("makedev(%u, %u)", major(p->val), minor(p->val));
		break;
	}
	case S_TYPE_array:
	case S_TYPE_struct: {
		struct s_struct *p = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *field;
		struct s_arg *tmp;

		if (p->aux_str) {
			tprints(p->aux_str);
			break;
		}

		tprints(arg->type == S_TYPE_array ? "[" : "{");

		STAILQ_FOREACH_SAFE(field, &p->args.args, entry, tmp) {
			if ((arg->type == S_TYPE_struct) && field->name)
				tprintf("%s=", field->name);

			s_val_print(field);

			if (tmp)
				tprints(", ");
		}

		tprints(arg->type == S_TYPE_array ? "]" : "}");

		break;
	}
	case S_TYPE_ellipsis:
		tprints("...");
		break;

	default:
		tprints("[!!! unknown value type]");
		break;
	}
}

static void
s_syscall_text_print_before(struct tcb *tcp)
{
	tprintf("%s(", tcp->s_ent->sys_name);
}

static void
s_syscall_text_print_entering(struct tcb *tcp)
{
	fflush(tcp->outf);
}

static void
s_syscall_text_print_exiting(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	STAILQ_FOREACH_SAFE(arg, &syscall->args.args, entry, tmp) {
		s_val_print(arg);

		if (tmp)
			tprints(", ");
	}
}

static void
s_syscall_text_print_after(struct tcb *tcp)
{
	long u_error = tcp->u_error;
	int sys_res = tcp->sys_res;

	tprints(") ");
	tabto();

	if (tcp->qual_flg & QUAL_RAW) {
		if (u_error)
			tprintf("= -1 (errno %ld)", u_error);
		else
			tprintf("= %#lx", tcp->u_rval);
	}
	else if (!(sys_res & RVAL_NONE) && u_error) {
		switch (u_error) {
		/* Blocked signals do not interrupt any syscalls.
		 * In this case syscalls don't return ERESTARTfoo codes.
		 *
		 * Deadly signals set to SIG_DFL interrupt syscalls
		 * and kill the process regardless of which of the codes below
		 * is returned by the interrupted syscall.
		 * In some cases, kernel forces a kernel-generated deadly
		 * signal to be unblocked and set to SIG_DFL (and thus cause
		 * death) if it is blocked or SIG_IGNed: for example, SIGSEGV
		 * or SIGILL. (The alternative is to leave process spinning
		 * forever on the faulty instruction - not useful).
		 *
		 * SIG_IGNed signals and non-deadly signals set to SIG_DFL
		 * (for example, SIGCHLD, SIGWINCH) interrupt syscalls,
		 * but kernel will always restart them.
		 */
		case ERESTARTSYS:
			/* Most common type of signal-interrupted syscall exit code.
			 * The system call will be restarted with the same arguments
			 * if SA_RESTART is set; otherwise, it will fail with EINTR.
			 */
			tprints("= ? ERESTARTSYS (To be restarted if SA_RESTART is set)");
			break;
		case ERESTARTNOINTR:
			/* Rare. For example, fork() returns this if interrupted.
			 * SA_RESTART is ignored (assumed set): the restart is unconditional.
			 */
			tprints("= ? ERESTARTNOINTR (To be restarted)");
			break;
		case ERESTARTNOHAND:
			/* pause(), rt_sigsuspend() etc use this code.
			 * SA_RESTART is ignored (assumed not set):
			 * syscall won't restart (will return EINTR instead)
			 * even after signal with SA_RESTART set. However,
			 * after SIG_IGN or SIG_DFL signal it will restart
			 * (thus the name "restart only if has no handler").
			 */
			tprints("= ? ERESTARTNOHAND (To be restarted if no handler)");
			break;
		case ERESTART_RESTARTBLOCK:
			/* Syscalls like nanosleep(), poll() which can't be
			 * restarted with their original arguments use this
			 * code. Kernel will execute restart_syscall() instead,
			 * which changes arguments before restarting syscall.
			 * SA_RESTART is ignored (assumed not set) similarly
			 * to ERESTARTNOHAND. (Kernel can't honor SA_RESTART
			 * since restart data is saved in "restart block"
			 * in task struct, and if signal handler uses a syscall
			 * which in turn saves another such restart block,
			 * old data is lost and restart becomes impossible)
			 */
			tprints("= ? ERESTART_RESTARTBLOCK (Interrupted by signal)");
			break;
		default:
			if ((unsigned long) u_error < nerrnos
			    && errnoent[u_error])
				tprintf("= -1 %s (%s)", errnoent[u_error],
					strerror(u_error));
			else
				tprintf("= -1 ERRNO_%lu (%s)", u_error,
					strerror(u_error));
			break;
		}
		if ((sys_res & RVAL_STR) && tcp->auxstr)
			tprintf(" (%s)", tcp->auxstr);
	}
	else {
		if (sys_res & RVAL_NONE)
			tprints("= ?");
		else {
			switch (sys_res & RVAL_MASK) {
			case RVAL_HEX:
#if SUPPORTED_PERSONALITIES > 1
				if (current_wordsize < sizeof(long))
					tprintf("= %#x",
						(unsigned int) tcp->u_rval);
				else
#endif
					tprintf("= %#lx", tcp->u_rval);
				break;
			case RVAL_OCTAL:
				tprints("= ");
				print_numeric_long_umask(tcp->u_rval);
				break;
			case RVAL_UDECIMAL:
#if SUPPORTED_PERSONALITIES > 1
				if (current_wordsize < sizeof(long))
					tprintf("= %u",
						(unsigned int) tcp->u_rval);
				else
#endif
					tprintf("= %lu", tcp->u_rval);
				break;
			case RVAL_DECIMAL:
				tprintf("= %ld", tcp->u_rval);
				break;
			case RVAL_FD:
				if (show_fd_path) {
					tprints("= ");
					printfd(tcp, tcp->u_rval);
				}
				else
					tprintf("= %ld", tcp->u_rval);
				break;
#if HAVE_STRUCT_TCB_EXT_ARG
			/*
			case RVAL_LHEX:
				tprintf("= %#llx", tcp->u_lrval);
				break;
			case RVAL_LOCTAL:
				tprintf("= %#llo", tcp->u_lrval);
				break;
			*/
			case RVAL_LUDECIMAL:
				tprintf("= %llu", tcp->u_lrval);
				break;
			/*
			case RVAL_LDECIMAL:
				tprintf("= %lld", tcp->u_lrval);
				break;
			*/
#endif /* HAVE_STRUCT_TCB_EXT_ARG */
			default:
				error_msg("invalid rval format");
				break;
			}
		}
		if ((sys_res & RVAL_STR) && tcp->auxstr)
			tprintf(" (%s)", tcp->auxstr);
	}
}

static void
s_syscall_text_print_tv(struct tcb *tcp, struct timeval *tv)
{
	tprintf(" <%ld.%06ld>",
		(long) tv->tv_sec, (long) tv->tv_usec);
}

static void
s_syscall_text_print_resumed(struct tcb *tcp)
{
	tprintf("<... %s resumed> ", tcp->s_ent->sys_name);
}

static void
s_syscall_text_print_unavailable_entering(struct tcb *tcp, int scno_good)
{
	tprintf("%s(", scno_good == 1 ? tcp->s_ent->sys_name : "????");
}

static void
s_syscall_text_print_unavailable_exiting(struct tcb *tcp)
{
	tprints(") ");
	tabto();
	tprints("= ? <unavailable>\n");
	line_ended();
}

static void
s_syscall_text_print_signal(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	if (STAILQ_FIRST(&syscall->args.args) &&
	    STAILQ_NEXT(STAILQ_FIRST(&syscall->args.args), entry))
		tprints("--- ");
	else
		tprints("--- stopped by ");

	STAILQ_FOREACH_SAFE(arg, &syscall->args.args, entry, tmp) {
		s_val_print(arg);

		if (tmp)
			tprints(" ");
	}

	tprints(" ---\n");

	line_ended();
}

static void
s_text_print_message(struct tcb *tcp, enum s_msg_type type, const char *msg,
	va_list args)
{
	switch (type) {
	case S_MSG_INFO:
		printleader(tcp);
		tprints("+++ ");
		vtprintf(msg, args);
		tprints(" +++\n");
		line_ended();
		break;

	case S_MSG_ERROR:
	default:
		printleader(tcp);
		tprintf("Message of unknown type (%d): ", type);
		vtprintf(msg, args);
		tprints("\n");
		line_ended();
	}
}

struct s_printer s_printer_text = {
	.name = "text",
	.print_before = s_syscall_text_print_before,
	.print_entering = s_syscall_text_print_entering,
	.print_exiting  = s_syscall_text_print_exiting,
	.print_after = s_syscall_text_print_after,
	.print_resumed = s_syscall_text_print_resumed,
	.print_tv = s_syscall_text_print_tv,
	.print_unavailable_entering = s_syscall_text_print_unavailable_entering,
	.print_unavailable_exiting = s_syscall_text_print_unavailable_exiting,
	.print_signal = s_syscall_text_print_signal,
	.print_message = s_text_print_message,
};
