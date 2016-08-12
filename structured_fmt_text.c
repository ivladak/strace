#include "defs.h"

#include "structured_fmt_text.h"

void
s_print_xlat_text(uint64_t value, uint64_t mask, const char *str,
	uint32_t flags)
{
	/* Corner case */
	if (!(flags & SPXF_FIRST) && (flags & SPXF_DEFAULT) && !value)
		return;

	if (!(flags & SPXF_FIRST))
		tprints("|");

	if (flags & SPXF_DEFAULT) {
		tprintf("%#" PRIx64, value);
		if (str && value)
			tprintf(" /* %s */", str);
	} else {
		if (str)
			tprintf("%s", str);
		else
			tprintf("%#" PRIx64, value);
	}
}

void
s_val_print(struct s_arg *arg)
{
	switch (arg->type) {
#define PRINT_INT(TYPE, ENUM, PR) \
	case S_TYPE_ ## ENUM : tprintf("%" PR, \
		(TYPE)(((struct s_num *)s_arg_to_type(arg))->val)); break

	PRINT_INT(int, d, "d");
	PRINT_INT(long, ld, "ld");
	PRINT_INT(long long, lld, "lld");

	PRINT_INT(unsigned, u, "u");
	PRINT_INT(unsigned long, lu, "lu");
	PRINT_INT(unsigned long long, llu, "llu");

#undef PRINT_INT

#define PRINT_ALT_INT(TYPE, ENUM, PR) \
	case S_TYPE_ ## ENUM : tprintf("%#" PR, \
		(TYPE)(((struct s_num *)s_arg_to_type(arg))->val)); break

	PRINT_ALT_INT(unsigned, x, "x");
	PRINT_ALT_INT(unsigned long, lx, "lx");
	PRINT_ALT_INT(unsigned long long, llx, "llx");

	PRINT_ALT_INT(int, o, "o");
	PRINT_ALT_INT(long, lo, "lo");
	PRINT_ALT_INT(long long, llo, "llo");

#undef PRINT_ALT_INT

	case S_TYPE_changeable_void:
		tprints("[x]");
		break;
	case S_TYPE_changeable: {
		struct s_changeable *s_ch = S_ARG_TO_TYPE(arg, changeable);
		if (s_ch->entering)
			s_val_print(s_ch->entering);
		else
			tprints("[x]");
		if (s_ch->exiting) {
			tprints(" => ");
			s_val_print(s_ch->exiting);
		}
		break;
	}
	case S_TYPE_str:
	case S_TYPE_path: {
		struct s_str *s_p = S_ARG_TO_TYPE(arg, str);

		if (s_p->has_nul && (print_quoted_string(s_p->str,
		    s_p->len, s_p->has_nul ? QUOTE_0_TERMINATED : 0) > 0))
			tprints("...");

		break;
	}
	case S_TYPE_addr: {
		struct s_addr *p = S_ARG_TO_TYPE(arg, addr);
		if (p->val)
			s_val_print(p->val);
		else
			printaddr(p->addr);

		break;
	}
	case S_TYPE_fd:
		printfd(arg->syscall->tcp,
			(int)(((struct s_num *)s_arg_to_type(arg))->val));
		break;
	case S_TYPE_xlat:
	case S_TYPE_xlat_l:
	case S_TYPE_xlat_ll: {
		struct s_xlat *f_p = S_ARG_TO_TYPE(arg, xlat);

		s_process_xlat(f_p, s_print_xlat_text);

		break;
	}
	case S_TYPE_array:
	case S_TYPE_struct: {
		struct s_struct *p = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *field;
		struct s_arg *tmp;

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

	default:
		tprints("[!!! unknown value type]");
		break;
	}
}

void
s_syscall_text_print_before(struct tcb *tcp)
{
	tprintf("%s(", tcp->s_ent->sys_name);
}

void
s_syscall_text_print_entering(struct tcb *tcp)
{
	fflush(tcp->outf);
}

void
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

void
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

void
s_syscall_text_print_unavailable(struct tcb *tcp)
{
	tprints(") ");
	tabto();
	tprints("= ? <unavailable>\n");
	line_ended();
}

struct s_printer s_printer_text = {
	.print_before = s_syscall_text_print_before,
	.print_entering = s_syscall_text_print_entering,
	.print_exiting  = s_syscall_text_print_exiting,
	.print_after = s_syscall_text_print_after,
	.print_unavailable = s_syscall_text_print_unavailable,
};
