#include <fcntl.h>

#include "defs.h"

#include "structured_fmt_json.h"

static const char *s_syscall_type_names[] = {
	[S_SCT_SYSCALL] = "syscall",
	[S_SCT_SIGNAL]  = "signal",
};

JsonNode *root_node;

static void
s_print_xlat_json(enum s_type type, uint64_t value, uint64_t mask,
	const char *str, uint32_t flags, void *fn_data)
{
	JsonNode *parent = fn_data;
	JsonNode *obj;

	/* Corner case */
	if (!(flags & SPXF_FIRST) && (flags & SPXF_DEFAULT) && !value)
		return;

	obj = json_mkobject();

	json_append_member(obj, "default", json_mkbool(!!(flags & SPXF_DEFAULT)));
	json_append_member(obj, "val", json_mknumber(value));

	if (str && value)
		json_append_member(obj, "str", json_mkstring(str));

	json_append_element(parent, obj);
}

#ifndef AT_FDCWD
# define AT_FDCWD                -100
#endif

static JsonNode *
s_val_print(struct s_arg *arg)
{
	switch (arg->type) {
#define PRINT_INT(TYPE, ENUM) \
	case S_TYPE_ ## ENUM: return json_mknumber((TYPE) \
		(((struct s_num *)s_arg_to_type(arg))->val));

	PRINT_INT(int, d);
	PRINT_INT(long, ld);
	PRINT_INT(long long, lld);

	PRINT_INT(unsigned, u);
	PRINT_INT(unsigned long, lu);
	PRINT_INT(unsigned long long, llu);

	PRINT_INT(unsigned, x);
	PRINT_INT(unsigned long, lx);
	PRINT_INT(unsigned long long, llx);

	PRINT_INT(int, o);
	PRINT_INT(long, lo);
	PRINT_INT(long long, llo);

	PRINT_INT(int, wstatus);

	PRINT_INT(unsigned, rlim32);
	PRINT_INT(unsigned long long, rlim64);

#undef PRINT_INT

	case S_TYPE_uid:
	case S_TYPE_gid: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		if ((uid_t) -1U == (uid_t)p->val)
			return json_mknumber(-1);
		else
			return json_mknumber((uid_t)p->val);
	}

	case S_TYPE_time: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		return json_mkstring(sprinttime(p->val));
	}

	case S_TYPE_signo: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);
		JsonNode *res = json_mkobject();

		json_append_member(res, "type", json_mkstring("signo"));
		json_append_member(res, "val", json_mknumber(p->val));
		json_append_member(res, "name", json_mkstring(signame(p->val)));

		return res;
	}

	case S_TYPE_changeable: {
		struct s_changeable *s_ch = S_ARG_TO_TYPE(arg, changeable);

		if (!s_ch->entering && !s_ch->exiting)
			return json_mknull();

		JsonNode *chg_obj = json_mkobject();
		if (s_ch->entering)
			json_append_member(chg_obj, "entering", s_val_print(s_ch->entering));
		if (s_ch->exiting)
			if (!s_arg_equal(s_ch->entering, s_ch->exiting))
				json_append_member(chg_obj, "exiting", s_val_print(s_ch->exiting));

		return chg_obj;
	}
	case S_TYPE_str:
	case S_TYPE_path: {
		struct s_str *s_p = S_ARG_TO_TYPE(arg, str);
		char *outstr;
		unsigned int style = QUOTE_ELLIPSIS | (s_p->has_nul ? QUOTE_0_TERMINATED : 0);

		alloc_quoted_string(s_p->str, &outstr, s_p->len + 3, style);
		string_quote(s_p->str, outstr, (s_p->len ? s_p->len - 1 : 0) , style);

		return json_mkstring(outstr);
	}
	case S_TYPE_ptrace_uaddr:
	case S_TYPE_addr: {
		struct s_addr *p = S_ARG_TO_TYPE(arg, addr);

		JsonNode *addr_obj = json_mkobject();
		json_append_member(addr_obj, "addr", json_mknumber(p->addr));

		if (p->val)
			json_append_member(addr_obj, "value", s_val_print(p->val));

		return addr_obj;
	}
	case S_TYPE_dirfd:
	case S_TYPE_fd: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		if ((int)p->val == AT_FDCWD)
			return json_mkstring("AT_FDCWD");
		else
			return json_mknumber((int)p->val);
			// printfd(arg->syscall->tcp, (int)p->val); XXX

		break;
	}
	case S_TYPE_xlat:
	case S_TYPE_xlat_l:
	case S_TYPE_xlat_ll:
	case S_TYPE_xlat_d:
	case S_TYPE_xlat_ld:
	case S_TYPE_xlat_lld: {
		struct s_xlat *f_p = S_ARG_TO_TYPE(arg, xlat);
		JsonNode *arr = json_mkarray();

		s_process_xlat(f_p, s_print_xlat_json, arr);

		return arr;
	}
	case S_TYPE_array: {
		struct s_struct *p = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *field;
		struct s_arg *tmp;
		JsonNode *arr = json_mkarray();

		STAILQ_FOREACH_SAFE(field, &p->args.args, entry, tmp) {
			json_append_element(arr, s_val_print(field));
		}

		return arr;
	}
	case S_TYPE_struct: {
		struct s_struct *p = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *field;
		struct s_arg *tmp;
		JsonNode *arr = json_mkobject();

		STAILQ_FOREACH_SAFE(field, &p->args.args, entry, tmp) {
			json_append_member(arr, field->name, s_val_print(field));
		}

		return arr;
	}
	case S_TYPE_ellipsis:
		// ШАТ
		return json_mkstring("...");

	default:
		break;
	}

	return json_mkstring(">:[");
}

static void
s_syscall_json_print_before(struct tcb *tcp)
{
	root_node = json_mkobject();
	json_append_member(root_node, "name", json_mkstring(tcp->s_ent->sys_name));
}

static void
s_syscall_json_print_entering(struct tcb *tcp)
{
	json_append_member(root_node, "type",
		json_mkstring(s_syscall_type_names[tcp->s_syscall->type]));
}

static void
s_syscall_json_print_exiting(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;
	JsonNode *args_node = json_mkarray();

	STAILQ_FOREACH_SAFE(arg, &syscall->args.args, entry, tmp) {
		json_append_element(args_node, s_val_print(arg));
	}

	json_append_member(root_node, "args", args_node);
}

static void
s_syscall_json_print_after(struct tcb *tcp)
{
	long u_error = tcp->u_error;
	int sys_res = tcp->sys_res;

	if (!(sys_res & RVAL_NONE) && u_error) {
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
			json_append_member(root_node, "return", json_mkstring("?"));
			json_append_member(root_node, "errno", json_mknumber(ERESTARTSYS));
			json_append_member(root_node, "errnostr", json_mkstring("ERESTARTSYS"));
			json_append_member(root_node, "retstring", json_mkstring("To be restarted if SA_RESTART is set"));
			break;
		case ERESTARTNOINTR:
			/* Rare. For example, fork() returns this if interrupted.
			 * SA_RESTART is ignored (assumed set): the restart is unconditional.
			 */
			json_append_member(root_node, "return", json_mkstring("?"));
			json_append_member(root_node, "errno", json_mknumber(ERESTARTNOINTR));
			json_append_member(root_node, "errnostr", json_mkstring("ERESTARTNOINTR"));
			json_append_member(root_node, "retstring", json_mkstring("To be restarted"));
			break;
		case ERESTARTNOHAND:
			/* pause(), rt_sigsuspend() etc use this code.
			 * SA_RESTART is ignored (assumed not set):
			 * syscall won't restart (will return EINTR instead)
			 * even after signal with SA_RESTART set. However,
			 * after SIG_IGN or SIG_DFL signal it will restart
			 * (thus the name "restart only if has no handler").
			 */
			json_append_member(root_node, "return", json_mkstring("?"));
			json_append_member(root_node, "errno", json_mknumber(ERESTARTNOHAND));
			json_append_member(root_node, "errnostr", json_mkstring("ERESTARTNOHAND"));
			json_append_member(root_node, "retstring", json_mkstring("To be restarted if no handler"));
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
			json_append_member(root_node, "return", json_mkstring("?"));
			json_append_member(root_node, "errno", json_mknumber(ERESTART_RESTARTBLOCK));
			json_append_member(root_node, "errnostr", json_mkstring("ERESTART_RESTARTBLOCK"));
			json_append_member(root_node, "retstring", json_mkstring("Interrupted by signal"));
			break;
		default:
			json_append_member(root_node, "return", json_mknumber(-1));
			json_append_member(root_node, "errno", json_mknumber(u_error));
			if ((unsigned long) u_error < nerrnos && errnoent[u_error]) {
				json_append_member(root_node, "errnostr", json_mkstring(errnoent[u_error]));
			} else {
				char buf[sizeof("ERRNO_%lu") + sizeof(long)*3];
				sprintf(buf, "ERRNO_%lu", u_error);
				json_append_member(root_node, "errnostr", json_mkstring(buf));
			}
			json_append_member(root_node, "retstring", json_mkstring(strerror(u_error)));
			break;
		}
		if ((sys_res & RVAL_STR) && tcp->auxstr)
			json_append_member(root_node, "auxstr", json_mkstring(tcp->auxstr));
	} else {
		if (sys_res & RVAL_NONE) {
			json_append_member(root_node, "return", json_mkstring("?"));
		} else {
			switch (sys_res & RVAL_MASK) {
			case RVAL_HEX:
			case RVAL_OCTAL:
			case RVAL_UDECIMAL:
			case RVAL_DECIMAL:
			case RVAL_FD:
				json_append_member(root_node, "return", json_mknumber(tcp->u_rval));
				break;
#if HAVE_STRUCT_TCB_EXT_ARG
			/*
			case RVAL_LHEX:
			case RVAL_LOCTAL:
			case RVAL_LDECIMAL:
			*/
			case RVAL_LUDECIMAL:
				json_append_member(root_node, "return", json_mknumber(tcp->u_lrval));
				break;
#endif /* HAVE_STRUCT_TCB_EXT_ARG */
			default:
				json_append_member(root_node, "return", json_mkstring("?"));
				json_append_member(root_node, "retstring", json_mkstring("Invalid rval format"));
				break;
			}
		}
		if ((sys_res & RVAL_STR) && tcp->auxstr)
			json_append_member(root_node, "auxstr", json_mkstring(tcp->auxstr));
	}

	tprints(json_stringify(root_node, "\t"));
	fflush(tcp->outf);
	json_delete(root_node);
}

static void
s_syscall_json_print_tv(struct tcb *tcp, struct timeval *tv)
{
	JsonNode *tv_node = json_mkobject();
	json_append_member(tv_node, "sec", json_mknumber(tv->tv_sec));
	json_append_member(tv_node, "usec", json_mknumber(tv->tv_usec));
	json_append_member(root_node, "time", tv_node);
}

static void
s_syscall_json_print_resumed(struct tcb *tcp)
{
}

static void
s_syscall_json_print_unavailable_entering(struct tcb *tcp, int scno_good)
{
	root_node = json_mkobject();
	json_append_member(root_node, "type",
		json_mkstring(s_syscall_type_names[tcp->s_syscall->type]));
	json_append_member(root_node, "name",
		json_mkstring(scno_good == 1 ? tcp->s_ent->sys_name : "????"));
}

static void
s_syscall_json_print_unavailable_exiting(struct tcb *tcp)
{
	json_append_member(root_node, "return", json_mkstring("?"));
	json_append_member(root_node, "retstring", json_mkstring("<unavailable>"));
}

static void
s_syscall_json_print_signal(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	root_node = json_mkobject();
	json_append_member(root_node, "type",
		json_mkstring(s_syscall_type_names[tcp->s_syscall->type]));

	STAILQ_FOREACH_SAFE(arg, &syscall->args.args, entry, tmp) {
		json_append_member(root_node, arg->name, s_val_print(arg));
	}

	tprints(json_stringify(root_node, "\t"));
}

struct s_printer s_printer_json = {
	.print_before = s_syscall_json_print_before,
	.print_entering = s_syscall_json_print_entering,
	.print_exiting  = s_syscall_json_print_exiting,
	.print_after = s_syscall_json_print_after,
	.print_resumed = s_syscall_json_print_resumed,
	.print_tv = s_syscall_json_print_tv,
	.print_unavailable_entering = s_syscall_json_print_unavailable_entering,
	.print_unavailable_exiting = s_syscall_json_print_unavailable_exiting,
	.print_signal = s_syscall_json_print_signal,
};
