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

		s_process_xlat(f_p, s_print_xlat_text);

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
