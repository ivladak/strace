#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>

#include "defs.h"
#include "structured_sigmask.h"

#include "xlat/sa_handler_values.h"

#include "structured_fmt_json.h"

static const char *s_syscall_type_names[] = {
	[S_SCT_SYSCALL] = "syscall",
	[S_SCT_SIGNAL]  = "signal",
};

JsonNode *root_node;

static void
s_print_xlat_json(struct s_xlat *x, uint64_t value, uint64_t mask,
	const char *str, uint32_t flags, void *fn_data)
{
	JsonNode *parent = fn_data;
	JsonNode *obj;

	/* Corner case */
	if (!(flags & SPXF_FIRST) && (flags & SPXF_DEFAULT) && !value)
		return;

	obj = json_mkobject();

	json_append_member(obj, "default", json_mkbool(!!(flags & SPXF_DEFAULT)));
	json_append_member(obj, "value", json_mknumber(value));

	if (str && value)
		json_append_member(obj, "str", json_mkstring_static(str));

	if (x->arg.comment)
		json_append_member(obj, "comment",
			json_mkstring_static(x->arg.comment));

	json_append_element(parent, obj);
}

void
s_print_sigmask_json(int bit, const char *str, bool set, void *data)
{
	JsonNode *root = data;
	JsonNode *ins_point = json_find_member(root, set ? "set" : "unset");
	char buf[sizeof(bit) * 3 + 1];

	if (!ins_point) {
		ins_point = json_mkobject();
		json_append_member(root, set ? "set" : "unset", ins_point);
	}

	snprintf(buf, sizeof(buf), "%u", bit);

	json_append_member(ins_point, buf, json_mkstring(str));
}

#ifndef AT_FDCWD
# define AT_FDCWD -100
#endif
#ifndef FAN_NOFD
# define FAN_NOFD -1
#endif

static JsonNode *
s_val_print(struct s_arg *arg)
{
	JsonNode *new_obj = json_mkobject();

	json_append_member(new_obj, "name",
		arg->name ? json_mkstring_static(arg->name) : json_mknull());
	if (arg->comment)
		json_append_member(new_obj, "comment",
			json_mkstring_static(arg->comment));

	switch (arg->type) {
#define PRINT_INT(TYPE, ENUM) \
	case S_TYPE_ ## ENUM: \
		json_append_member(new_obj, "value", \
		json_mknumber((TYPE) (((struct s_num *)s_arg_to_type(arg))->val))); \
		break;

	PRINT_INT(signed char, hhd);
	PRINT_INT(short, hd);
	PRINT_INT(int, d);
	PRINT_INT(long, ld);
	PRINT_INT(long long, lld);

	PRINT_INT(unsigned char, hhu);
	PRINT_INT(unsigned short, hu);
	PRINT_INT(unsigned, u);
	PRINT_INT(unsigned long, lu);
	PRINT_INT(unsigned long long, llu);

	PRINT_INT(unsigned char, hhx);
	PRINT_INT(unsigned short, hx);
	PRINT_INT(unsigned, x);
	PRINT_INT(unsigned long, lx);
	PRINT_INT(unsigned long long, llx);

	PRINT_INT(unsigned char, hho);
	PRINT_INT(unsigned short, ho);
	PRINT_INT(int, o);
	PRINT_INT(long, lo);
	PRINT_INT(long long, llo);

	PRINT_INT(int, wstatus);

	PRINT_INT(unsigned, rlim32);
	PRINT_INT(unsigned long long, rlim64);

#undef PRINT_INT

	case S_TYPE_c: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);
		char buf[2] = { (char)p->val, '\0' };

		json_append_member(new_obj, "type",
			json_mkstring_static("char"));
		json_append_member(new_obj, "value", json_mkstring(buf));
		break;
	}

	case S_TYPE_dev_t: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);
		JsonNode *dev_obj = json_mkobject();
		json_append_member(new_obj, "type",
			json_mkstring_static("dev_t"));
		json_append_member(dev_obj, "major",
			json_mknumber(major((dev_t)p->val)));
		json_append_member(dev_obj, "minor",
			json_mknumber(minor((dev_t)p->val)));
		json_append_member(new_obj, "value", dev_obj);
		break;
	}

	case S_TYPE_uid:
	case S_TYPE_gid: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		json_append_member(new_obj, "type",
			json_mkstring_static((arg->type == S_TYPE_uid) ?
				"uid" : "gid"));

		if ((uid_t) -1U == (uid_t)p->val)
			json_append_member(new_obj, "value", json_mknumber(-1));
		else
			json_append_member(new_obj, "value",
				json_mknumber((uid_t)p->val));

		break;
	}

	case S_TYPE_uid16:
	case S_TYPE_gid16: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		json_append_member(new_obj, "type",
			json_mkstring_static((arg->type == S_TYPE_uid) ?
				"uid16" : "gid16"));

		if ((uint16_t)-1U == (uint16_t)p->val)
			json_append_member(new_obj, "value", json_mknumber(-1));
		else
			json_append_member(new_obj, "value",
				json_mknumber((uint16_t)p->val));

		break;
	}

	case S_TYPE_time: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		json_append_member(new_obj, "type",
			json_mkstring_static("time"));
		json_append_member(new_obj, "value",
			json_mkstring(sprinttime(p->val)));

		break;
	}

	case S_TYPE_signo: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		json_append_member(new_obj, "type",
			json_mkstring_static("signo"));
		json_append_member(new_obj, "value",
			json_mknumber(p->val));
		json_append_member(new_obj, "signame",
			json_mkstring(signame(p->val)));

		break;
	}

	case S_TYPE_clockid: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		json_append_member(new_obj, "type",
			json_mkstring_static("clockid"));
		json_append_member(new_obj, "value",
			json_mknumber((int)p->val));
		json_append_member(new_obj, "clockname",
			json_mkstring(sprintclockname((int)p->val)));

		break;
	}

	case S_TYPE_changeable: {
		struct s_changeable *s_ch = S_ARG_TO_TYPE(arg, changeable);

		json_append_member(new_obj, "type",
			json_mkstring_static("changeable"));

		if (!s_ch->entering && !s_ch->exiting) {
			json_append_member(new_obj, "value", json_mknull());
		} else {
			if (s_ch->entering) {
				if (!s_arg_equal(s_ch->entering, s_ch->exiting))
					json_append_member(new_obj,
						"entering_value",
						s_val_print(s_ch->entering));
			} else {
				json_append_member(new_obj, "entering_value",
					json_mknull());
			}
			if (s_ch->exiting) {
				json_append_member(new_obj, "exiting_value",
					s_val_print(s_ch->exiting));
			} else {
				json_append_member(new_obj, "exiting_value",
					json_mknull());
			}
		}

		break;
	}

	case S_TYPE_str:
	case S_TYPE_path: {
		struct s_str *s_p = S_ARG_TO_TYPE(arg, str);
		char *outstr;
		unsigned int style = (s_p->flags |
			QUOTE_OMIT_LEADING_TRAILING_QUOTES) & ~QUOTE_ELLIPSIS;
		bool truncated;

		alloc_quoted_string(s_p->str, &outstr, s_p->len, style);
		truncated = string_quote(s_p->str, outstr,
			(s_p->len ? s_p->len : 0), style);

		json_append_member(new_obj, "type",
			json_mkstring_static("str"));
		json_append_member(new_obj, "value", json_mkstring(outstr));
		json_append_member(new_obj, "size", json_mknumber(s_p->len));
		json_append_member(new_obj, "truncated",
			json_mkbool(truncated));

		break;
	}
	case S_TYPE_ptrace_uaddr:
	case S_TYPE_addr: {
		struct s_addr *p = S_ARG_TO_TYPE(arg, addr);

		json_append_member(new_obj, "type",
			json_mkstring_static("address"));
		json_append_member(new_obj, "addr", json_mknumber(p->addr));

		if (p->val)
			json_append_member(new_obj, "value", s_val_print(p->val));

		break;
	}
	case S_TYPE_fan_dirfd:
	case S_TYPE_dirfd:
	case S_TYPE_fd: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);

		json_append_member(new_obj, "type", json_mkstring_static("fd"));

		if ((arg->type == S_TYPE_fan_dirfd) &&
		    ((int)p->val == FAN_NOFD))
			json_append_member(new_obj, "string",
				json_mkstring_static("FAN_NOFD"));
		else if ((arg->type != S_TYPE_fd) && ((int)p->val == AT_FDCWD))
			json_append_member(new_obj, "string",
				json_mkstring_static("AT_FDCWD"));

		json_append_member(new_obj, "value",
			json_mknumber((int)p->val));

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

		json_append_member(new_obj, "type",
			json_mkstring_static("xlat"));
		json_append_member(new_obj, "value", arr);

		break;
	}
	case S_TYPE_sigmask: {
		const unsigned endian = 1;
		int little_endian = * (char *) (void *) &endian;
		unsigned pos_xor_mask = little_endian ? 0 :
			current_wordsize - 1;

		struct s_sigmask *p = S_ARG_TO_TYPE(arg, sigmask);
		char buf[sizeof(p->sigmask) * 8 + 1];
		char *ptr = buf;
		unsigned i;

		json_append_member(new_obj, "type",
			json_mkstring_static("sigmask"));

		for (i = 0; i < p->bytes; i++) {
			unsigned cur_byte = i ^ pos_xor_mask;

			ptr += snprintf(ptr, sizeof(buf) - (ptr - buf),
				"%d%d%d%d%d%d%d%d",
				!!(p->sigmask[cur_byte] >> 0),
				!!(p->sigmask[cur_byte] >> 1),
				!!(p->sigmask[cur_byte] >> 2),
				!!(p->sigmask[cur_byte] >> 3),
				!!(p->sigmask[cur_byte] >> 4),
				!!(p->sigmask[cur_byte] >> 5),
				!!(p->sigmask[cur_byte] >> 6),
				!!(p->sigmask[cur_byte] >> 7));
		}

		json_append_member(new_obj, "bitmask", json_mkstring(buf));

		s_process_sigmask(p, s_print_sigmask_json, new_obj);

		break;
	}
	case S_TYPE_sa_handler: {
		struct s_num *p = S_ARG_TO_TYPE(arg, num);
		const char *str = xlookup(sa_handler_values,
			(unsigned long)p->val);

		json_append_member(new_obj, "type",
			json_mkstring_static("sa_handler"));
		json_append_member(new_obj, "value", json_mknumber(p->val));

		if (str)
			json_append_member(new_obj, "str", json_mkstring(str));

		break;
	}
	case S_TYPE_array: {
		struct s_struct *p = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *field;
		JsonNode *arr = json_mkarray();

		list_foreach(field, &p->args.args, entry) {
			json_append_element(arr, s_val_print(field));
		}

		json_append_member(new_obj, "value", arr);

		break;
	}
	case S_TYPE_struct: {
		struct s_struct *p = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *field;
		JsonNode *arr = json_mkobject();

		list_foreach(field, &p->args.args, entry) {
			json_append_member(arr, field->name, s_val_print(field));
		}

		json_append_member(new_obj, "value", arr);

		break;
	}
	case S_TYPE_ellipsis:
		json_delete(new_obj);
		return json_mkstring_static("...");

	default:
		json_delete(new_obj);
		return json_mkstring_static(">:[");
	}

	return new_obj;
}

static void
s_syscall_json_print_unfinished(struct tcb *tcp)
{
	if (root_node) {
		json_append_member(root_node, "unfinished", json_mkbool(true));

		tprints(json_stringify(root_node, "\t"));
		fflush(tcp->outf);
	}
}

static void
s_syscall_json_print_leader(struct tcb *tcp, struct timeval *tv,
	struct timeval *dtv)
{
	json_delete(root_node);
	root_node = json_mkobject();

	json_append_member(root_node, "pid", json_mknumber(tcp->pid));

	if (tflag) {
		if (rflag)
			json_append_member(root_node, "time_delta",
				json_mknumber(
					dtv->tv_sec + dtv->tv_usec / 1e6));

		if (tflag > 2)
			json_append_member(root_node, "timestamp",
				json_mknumber(tv->tv_sec + tv->tv_usec / 1e6));
		else {
			time_t local = tv->tv_sec;
			char *ts_str = xmalloc(sizeof("HH:MM:SS.123456"));

			strftime(ts_str, sizeof("HH:MM:SS"), "%T",
				localtime(&local));
			snprintf(ts_str + sizeof("HH:MM:SS") - 1,
				sizeof(".123456"), ".%06ld", tv->tv_usec);

			json_append_member(root_node, "timestamp",
				json_mkstring_own(ts_str));
		}
	}
}

static void
s_syscall_json_print_before(struct tcb *tcp)
{
	assert(root_node);

	json_append_member(root_node, "name",
		json_mkstring(tcp->s_ent->sys_name));
}

static void
s_syscall_json_print_entering(struct tcb *tcp)
{
	assert(root_node);

	json_append_member(root_node, "type",
		json_mkstring_static(
			s_syscall_type_names[tcp->s_syscall->type]));
}

static void
s_syscall_json_print_exiting(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	JsonNode *args_node = json_mkarray();

	assert(root_node);

	list_foreach(arg, &syscall->args.args, entry) {
		json_append_element(args_node, s_val_print(arg));
	}

	json_append_member(root_node, "args", args_node);
}

static void
s_syscall_json_print_after(struct tcb *tcp)
{
	long u_error = tcp->u_error;
	int sys_res = tcp->sys_res;

	assert(root_node);

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
			json_append_member(root_node, "return",
				json_mkstring_static("?"));
			json_append_member(root_node, "errno",
				json_mknumber(ERESTARTSYS));
			json_append_member(root_node, "errnostr",
				json_mkstring_static("ERESTARTSYS"));
			json_append_member(root_node, "retstring",
				json_mkstring_static("To be restarted if "
					"SA_RESTART is set"));
			break;
		case ERESTARTNOINTR:
			/* Rare. For example, fork() returns this if interrupted.
			 * SA_RESTART is ignored (assumed set): the restart is unconditional.
			 */
			json_append_member(root_node, "return",
				json_mkstring_static("?"));
			json_append_member(root_node, "errno",
				json_mknumber(ERESTARTNOINTR));
			json_append_member(root_node, "errnostr",
				json_mkstring_static("ERESTARTNOINTR"));
			json_append_member(root_node, "retstring",
				json_mkstring_static("To be restarted"));
			break;
		case ERESTARTNOHAND:
			/* pause(), rt_sigsuspend() etc use this code.
			 * SA_RESTART is ignored (assumed not set):
			 * syscall won't restart (will return EINTR instead)
			 * even after signal with SA_RESTART set. However,
			 * after SIG_IGN or SIG_DFL signal it will restart
			 * (thus the name "restart only if has no handler").
			 */
			json_append_member(root_node, "return",
				json_mkstring_static("?"));
			json_append_member(root_node, "errno",
				json_mknumber(ERESTARTNOHAND));
			json_append_member(root_node, "errnostr",
				json_mkstring_static("ERESTARTNOHAND"));
			json_append_member(root_node, "retstring",
				json_mkstring_static("To be restarted if no "
					"handler"));
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
			json_append_member(root_node, "return",
				json_mkstring_static("?"));
			json_append_member(root_node, "errno",
				json_mknumber(ERESTART_RESTARTBLOCK));
			json_append_member(root_node, "errnostr",
				json_mkstring_static("ERESTART_RESTARTBLOCK"));
			json_append_member(root_node, "retstring",
				json_mkstring_static("Interrupted by signal"));
			break;
		default:
			json_append_member(root_node, "return", json_mknumber(-1));
			json_append_member(root_node, "errno", json_mknumber(u_error));
			if ((unsigned long) u_error < nerrnos && errnoent[u_error]) {
				json_append_member(root_node, "errnostr",
					json_mkstring_static(
						errnoent[u_error]));
			}
			json_append_member(root_node, "retstring",
				json_mkstring(strerror(u_error)));
			break;
		}
		if ((sys_res & RVAL_STR) && tcp->auxstr)
			json_append_member(root_node, "auxstr",
				json_mkstring(tcp->auxstr));
	} else {
		if (sys_res & RVAL_NONE) {
			json_append_member(root_node, "return",
				json_mkstring_static("?"));
		} else {
			switch (sys_res & RVAL_MASK) {
			case RVAL_HEX:
			case RVAL_OCTAL:
			case RVAL_UDECIMAL:
			case RVAL_DECIMAL:
			case RVAL_FD:
				json_append_member(root_node, "return",
					json_mknumber(tcp->u_rval));
				break;
#if HAVE_STRUCT_TCB_EXT_ARG
			/*
			case RVAL_LHEX:
			case RVAL_LOCTAL:
			case RVAL_LDECIMAL:
			*/
			case RVAL_LUDECIMAL:
				json_append_member(root_node, "return",
					json_mknumber(tcp->u_lrval));
				break;
#endif /* HAVE_STRUCT_TCB_EXT_ARG */
			default:
				json_append_member(root_node, "return",
					json_mkstring_static("?"));
				json_append_member(root_node, "retstring",
					json_mkstring_static("Invalid rval format"));
				break;
			}
		}
		if ((sys_res & RVAL_STR) && tcp->auxstr)
			json_append_member(root_node, "auxstr",
				json_mkstring(tcp->auxstr));
	}

	tprints(json_stringify(root_node, "\t"));
	fflush(tcp->outf);
	json_delete(root_node);
	root_node = NULL;
}

static void
s_syscall_json_print_tv(struct tcb *tcp, struct timeval *tv)
{
	JsonNode *tv_node = json_mkobject();

	assert(root_node);

	json_append_member(tv_node, "sec", json_mknumber(tv->tv_sec));
	json_append_member(tv_node, "usec", json_mknumber(tv->tv_usec));
	json_append_member(root_node, "time", tv_node);
}

static void
s_syscall_json_print_resumed(struct tcb *tcp)
{
	assert(root_node);

	json_append_member(root_node, "resumed", json_mkbool(true));
}

static void
s_syscall_json_print_unavailable_entering(struct tcb *tcp, int scno_good)
{
	assert(root_node);

	json_append_member(root_node, "type",
		json_mkstring_static(
			s_syscall_type_names[tcp->s_syscall->type]));
	json_append_member(root_node, "name",
		json_mkstring(scno_good == 1 ? tcp->s_ent->sys_name : "????"));
}

static void
s_syscall_json_print_unavailable_exiting(struct tcb *tcp)
{
	assert(root_node);

	json_append_member(root_node, "return", json_mkstring_static("?"));
	json_append_member(root_node, "retstring",
		json_mkstring_static("<unavailable>"));
}

static void
s_syscall_json_print_signal(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	JsonNode *signal_node = json_mkobject();

	json_append_member(signal_node, "type",
		json_mkstring_static(
			s_syscall_type_names[tcp->s_syscall->type]));

	list_foreach(arg, &syscall->args.args, entry) {
		json_append_member(signal_node, arg->name, s_val_print(arg));
	}

	tprints(json_stringify(signal_node, "\t"));
	json_delete(signal_node);
}

static void
s_json_print_message(struct tcb *tcp, enum s_msg_type type, const char *msg,
	va_list args)
{
	static const char *msg_type_names[] = {
		[S_MSG_INFO] = "info",
		[S_MSG_ERROR] = "error"
	};
	JsonNode *msg_node = json_mkobject();
	char *buf = NULL;
	ssize_t size;
	va_list args_copy;

	json_append_member(msg_node, "type", json_mkstring_static("message"));

	if ((type < ARRAY_SIZE(msg_type_names)) && msg_type_names[type])
		json_append_member(msg_node, "msg_type",
			json_mkstring_static(msg_type_names[type]));
	else
		json_append_member(msg_node, "msg_type",
			json_mkstring_static("unknown"));

	va_copy(args_copy, args);
	size = vsnprintf(buf, 0, msg, args_copy);
	va_end(args_copy);

	if (size < 0) {
		json_append_member(msg_node, "error",
			json_mkstring_static("printf"));
		json_append_member(msg_node, "msg_format",
			json_mkstring_static(msg));
	} else {
		buf = xmalloc(size + 1);
		vsnprintf(buf, size + 1, msg, args);

		json_append_member(msg_node, "msg", json_mkstring_own(buf));
	}

	tprints(json_stringify(msg_node, "\t"));
	json_delete(msg_node);
}

struct s_printer s_printer_json = {
	.name = "json",
	.print_unfinished = s_syscall_json_print_unfinished,
	.print_leader = s_syscall_json_print_leader,
	.print_before = s_syscall_json_print_before,
	.print_entering = s_syscall_json_print_entering,
	.print_exiting  = s_syscall_json_print_exiting,
	.print_after = s_syscall_json_print_after,
	.print_resumed = s_syscall_json_print_resumed,
	.print_tv = s_syscall_json_print_tv,
	.print_unavailable_entering = s_syscall_json_print_unavailable_entering,
	.print_unavailable_exiting = s_syscall_json_print_unavailable_exiting,
	.print_signal = s_syscall_json_print_signal,
	.print_message = s_json_print_message,
};
