#include "defs.h"

#include "structured_fmt_text.h"

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
		s_val_print(s_ch->entering);
		if (s_ch->exiting->type != S_TYPE_changeable_void) {
			tprints(" => ");
			s_val_print(s_ch->exiting);
		}
		break;
	}
	case S_TYPE_str: {
		struct s_str *s_p = S_ARG_TO_TYPE(arg, str);
		if (!s_p->str) {
			if (s_p->addr) {
				printaddr(s_p->addr);
			} else {
				tprints("NULL");
			}
		} else {
			tprints(s_p->str);
		}
		break;
	}
	case S_TYPE_addr:
		printaddr((long)(((struct s_num *)s_arg_to_type(arg))->val));
		break;
	case S_TYPE_fd:
		printfd(arg->syscall->tcp,
			(int)(((struct s_num *)s_arg_to_type(arg))->val));
		break;
	case S_TYPE_path:
		printpathcur((long)(((struct s_num *)s_arg_to_type(arg))->val));
		break;
	case S_TYPE_xlat: {
		struct s_xlat *f_p = S_ARG_TO_TYPE(arg, xlat);

		if (f_p->flags)
			printflags64(f_p->x, f_p->val, f_p->dflt);
		else
			printxval64(f_p->x, f_p->val, f_p->dflt);

		break;
	}
	default:
		tprints("[!!! unknown value type]");
		break;
	}
}

void
s_syscall_text_print_entering(struct tcb *tcp)
{
	tcp->s_syscall->last_changeable =
		STAILQ_FIRST(&tcp->s_syscall->changeable_args);
}

void
s_syscall_text_print_exiting(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	STAILQ_FOREACH_SAFE(arg, &syscall->args, entry, tmp) {
		s_val_print(arg);

		if (tmp)
			tprints(", ");
	}
}

struct s_printer s_printer_text = {
	.print_entering = s_syscall_text_print_entering,
	.print_exiting  = s_syscall_text_print_exiting,
};
