/* Functions and structures to replace strace's traditional
 * output with new structured output subsystem
 */

#include <assert.h>
#include <signal.h>

#include "defs.h"
#include "printsiginfo.h"

#include "structured.h"

#include "structured_fmt_text.h"
#include "structured_fmt_json.h"

/** List of printers used. */
static struct s_printer *s_printers[] = {
	//&s_printer_json,
	&s_printer_text,
	NULL
};


/* syscall representation */

#define S_TYPE_FUNC(S_TYPE_FUNC_NAME, S_TYPE_FUNC_ARG, S_TYPE_FUNC_RET, \
    S_TYPE_SWITCH) \
	S_TYPE_FUNC_RET \
	S_TYPE_FUNC_NAME(S_TYPE_FUNC_ARG arg) \
	{ \
		switch (S_TYPE_KIND(S_TYPE_SWITCH)) { \
		case S_TYPE_KIND_fd: \
		__ARG_TYPE_CASE(num); \
		\
		case S_TYPE_KIND_path: \
		__ARG_TYPE_CASE(str); \
		\
		__ARG_TYPE_CASE(addr); \
		__ARG_TYPE_CASE(xlat); \
		\
		case S_TYPE_KIND_array: \
		__ARG_TYPE_CASE(struct); \
		\
		__ARG_TYPE_CASE(ellipsis); \
		__ARG_TYPE_CASE(changeable); \
		\
		default: \
			assert(0); \
		} \
		\
		return (S_TYPE_FUNC_RET)0; \
	}

#define __ARG_TYPE_CASE(_type) \
	case S_TYPE_KIND_##_type: \
		return S_ARG_TO_TYPE(arg, _type)

S_TYPE_FUNC(s_arg_to_type, struct s_arg *, void *, arg->type)

#undef __ARG_TYPE_CASE

#define __ARG_TYPE_CASE(_type) \
	case S_TYPE_KIND_##_type: \
		return __offsetof(struct s_##_type, arg)

S_TYPE_FUNC(s_type_offs, enum s_type, size_t, arg)

#undef __ARG_TYPE_CASE

#define __ARG_TYPE_CASE(_type) \
	case S_TYPE_KIND_##_type: \
		return sizeof(struct s_##_type)

S_TYPE_FUNC(s_type_size, enum s_type, size_t, arg)

#undef __ARG_TYPE_CASE

struct s_arg *
s_type_to_arg(void *p, enum s_type type)
{
	return (struct s_arg *)((char *)p + s_type_offs(type));
}


struct s_arg *
s_arg_new(struct tcb *tcp, enum s_type type, const char *name)
{
	struct s_syscall *syscall = tcp->s_syscall;
	void *p = xcalloc(1, s_type_size(type));
	struct s_arg *arg = s_type_to_arg(p, type);

	arg->syscall = syscall;
	arg->type = type;
	arg->name = name;
	arg->arg_num = -1;

	return arg;
}

void
s_arg_insert(struct s_syscall *syscall, struct s_arg *arg, int force_arg)
{
	struct args_queue *ins_point = s_syscall_insertion_point(syscall);

	if (entering(syscall->tcp) || !syscall->last_changeable ||
	    (ins_point != &syscall->args.args)) {
		if (force_arg >= 0) {
			arg->arg_num = force_arg;
		} else if (ins_point == &syscall->args.args) {
			arg->arg_num = syscall->last_arg;
			syscall->last_arg = syscall->cur_arg;
		} else {
			arg->arg_num = -1;
		}

		STAILQ_INSERT_TAIL(ins_point, arg, entry);

		if (arg->type == S_TYPE_changeable)
			STAILQ_INSERT_TAIL(&syscall->changeable_args, arg,
				chg_entry);
	} else {
		struct s_arg *chg = syscall->last_changeable;
		struct s_changeable *s_ch = S_ARG_TO_TYPE(chg, changeable);

		syscall->last_changeable = STAILQ_NEXT(chg, chg_entry);

		s_ch->exiting = arg;
	}
}

void
s_arg_free(struct s_arg *arg)
{
	switch (arg->type) {
	case S_TYPE_str: {
		struct s_str *s_p = S_ARG_TO_TYPE(arg, str);

		if (s_p->str)
			free(s_p->str);

		break;
	}
	case S_TYPE_addr: {
		struct s_addr *p = S_ARG_TO_TYPE(arg, addr);

		if (p->val)
			s_arg_free(p->val);

		break;
	}
	case S_TYPE_struct: {
		struct s_struct *p = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *arg;
		struct s_arg *tmp;

		STAILQ_FOREACH_SAFE(arg, &p->args.args, entry, tmp)
			s_arg_free(arg);

		free(p->aux_str);

		break;
	}
	case S_TYPE_changeable: {
		struct s_changeable *p = S_ARG_TO_TYPE(arg, changeable);

		if (p->entering)
			s_arg_free(p->entering);
		if (p->exiting)
			s_arg_free(p->exiting);

		break;
	}
	default:
		break;
	}

	free(s_arg_to_type(arg));
}


struct s_num *
s_num_new(enum s_type type, const char *name, uint64_t value)
{
	struct s_num *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		num);

	res->val = value;

	return res;
}

/* XXX Accommodates parts of logic from fetchstr and printpathn  */
struct s_str *
s_str_new(enum s_type type, const char *name, long addr, long len, bool has_nul)
{
	struct s_str *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		str);
	char *buf = NULL;
	size_t size = max_strlen;
	int ret;

	if (len == -1)
		size = max_strlen;
	if (len >= 0)
		size = len;

	if (!addr)
		goto s_str_new_fail;

	buf = xmalloc(size + 1);

	if (has_nul)
		ret = umovestr(current_tcp, addr, size + 1, buf);
	else
		ret = umoven(current_tcp, addr, size, buf);

	if (ret < 0)
		goto s_str_new_fail;

	res->str = buf;
	res->len = size;
	res->has_nul = has_nul;

	return res;

s_str_new_fail:
	free(buf);
	s_arg_free(&res->arg);

	return NULL;
}

struct s_addr *
s_addr_new(const char *name, long addr, struct s_arg *arg)
{
	struct s_addr *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, S_TYPE_addr,
		name), addr);

	res->addr = addr;
	res->val = arg;

	return res;
}

struct s_xlat *
s_xlat_new(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags)
{
	struct s_xlat *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		xlat);

	res->x = x;
	res->val = val;
	res->dflt = dflt;
	res->flags = flags;

	return res;
}

struct s_struct *
s_struct_new(enum s_type type, const char *name)
{
	struct s_struct *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		struct);

	STAILQ_INIT(&res->args.args);

	return res;
}

struct s_ellipsis *
s_ellipsis_new(void)
{
	struct s_ellipsis *res = S_ARG_TO_TYPE(s_arg_new(current_tcp,
		S_TYPE_ellipsis, NULL), ellipsis);

	return res;
}

struct s_changeable *
s_changeable_new(const char *name, struct s_arg *entering,
	struct s_arg *exiting)
{
	struct s_changeable *res = S_ARG_TO_TYPE(s_arg_new(current_tcp,
		S_TYPE_changeable, name), changeable);

	res->entering = entering;
	res->exiting = exiting;

	return res;
}

struct s_arg *
s_arg_new_init(struct tcb *tcp, enum s_type type, const char *name)
{
	switch (S_TYPE_KIND(type)) {
	case S_TYPE_KIND_fd:
	case S_TYPE_KIND_num:
		return &s_num_new(type, name, 0)->arg;

	case S_TYPE_KIND_path:
	case S_TYPE_KIND_str:
		/* Since s_str_new fails and returns NULL on empty string */
		return s_arg_new(tcp, type, name);

	case S_TYPE_KIND_addr:
		return &s_addr_new(name, 0, NULL)->arg;

	case S_TYPE_KIND_xlat:
		return &s_xlat_new(type, name, NULL, 0, NULL, false)->arg;

	case S_TYPE_KIND_array:
	case S_TYPE_KIND_struct:
		return &s_struct_new(type, name)->arg;

	case S_TYPE_KIND_changeable:
		return &s_changeable_new(name, NULL, NULL)->arg;

	default:
		return s_arg_new(tcp, type, name);
	}

	return s_arg_new(tcp, type, name);
}

bool
s_arg_equal(struct s_arg *arg1, struct s_arg *arg2)
{
	if (!arg1 || !arg2)
		return false;

	/* This is fine for now */
	if (S_TYPE_KIND(arg1->type) != S_TYPE_KIND(arg2->type))
		return false;

	switch (S_TYPE_KIND(arg1->type)) {
	case S_TYPE_KIND_fd:
	case S_TYPE_KIND_num: {
		struct s_num *num1 = S_ARG_TO_TYPE(arg1, num);
		struct s_num *num2 = S_ARG_TO_TYPE(arg2, num);

		return num1->val == num2->val;
	}
	case S_TYPE_KIND_path:
	case S_TYPE_KIND_str: {
		struct s_str *str1 = S_ARG_TO_TYPE(arg1, str);
		struct s_str *str2 = S_ARG_TO_TYPE(arg2, str);

		if (str1->len != str2->len)
			return false;

		return strncmp(str1->str, str2->str, str1->len);
	}
	case S_TYPE_KIND_addr: {
		struct s_addr *addr1 = S_ARG_TO_TYPE(arg1, addr);
		struct s_addr *addr2 = S_ARG_TO_TYPE(arg2, addr);

		if (addr1->addr != addr2->addr)
			return false;
		if (!addr1->addr && !addr2->addr && !addr1->val && !addr2->val)
			return true;
		if (!addr1->val || !addr2->val)
			return false;

		return s_arg_equal(addr1->val, addr2->val);
	}
	case S_TYPE_KIND_xlat: {
		struct s_xlat *xlat1 = S_ARG_TO_TYPE(arg1, xlat);
		struct s_xlat *xlat2 = S_ARG_TO_TYPE(arg2, xlat);

		if ((xlat1->x != xlat2->x) || (xlat1->val != xlat2->val))
			return false;
		if (xlat1->next && xlat2->next)
			return s_arg_equal(S_TYPE_TO_ARG(xlat1->next),
				S_TYPE_TO_ARG(xlat2->next));
		if (!xlat1->next && !xlat2->next)
			return true;

		return false;
	}
	case S_TYPE_KIND_array:
	case S_TYPE_KIND_struct: {
		struct s_struct *struct1 = S_ARG_TO_TYPE(arg1, struct);
		struct s_struct *struct2 = S_ARG_TO_TYPE(arg2, struct);
		struct s_arg *field1 = STAILQ_FIRST(&struct1->args.args);
		struct s_arg *field2 = STAILQ_FIRST(&struct2->args.args);

		while (field1 && field2) {
			if (!s_arg_equal(field1, field2))
				return false;

			field1 = STAILQ_NEXT(field1, entry);
			field2 = STAILQ_NEXT(field2, entry);
		}

		if (field1 || field2)
			return false;

		return true;
	}
	case S_TYPE_KIND_changeable: {
		struct s_changeable *chg1 = S_ARG_TO_TYPE(arg1, changeable);
		struct s_changeable *chg2 = S_ARG_TO_TYPE(arg2, changeable);

		if (chg1->entering && chg2->entering) {
			if (!s_arg_equal(chg1->entering, chg2->entering))
				return false;
		} else {
			if (chg1->entering || chg2->entering)
				return false;
		}

		if (chg1->exiting && chg2->exiting) {
			if (!s_arg_equal(chg1->exiting, chg2->exiting))
				return false;
		} else {
			if (chg1->exiting || chg2->exiting)
				return false;
		}

		return true;
	}

	default:
		return false;
	}

	return false;
}

struct s_num *
s_num_new_and_insert(enum s_type type, const char *name, uint64_t value)
{
	struct s_num *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_num_new(type, name, value))->arg, -1);

	return res;
}

struct s_addr *
s_addr_new_and_insert(const char *name, long addr, struct s_arg *arg)
{
	struct s_addr *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_addr_new(name, addr, arg))->arg, -1);

	return res;
}

struct s_xlat *
s_xlat_new_and_insert(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags)
{
	struct s_xlat *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_xlat_new(type, name, x, val, dflt, flags))->arg, -1);

	return res;
}

struct s_struct *
s_struct_new_and_insert(enum s_type type, const char *name)
{
	struct s_struct *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_struct_new(type, name))->arg, -1);

	return res;
}

struct s_ellipsis *
s_ellipsis_new_and_insert(void)
{
	struct s_ellipsis *res;

	s_arg_insert(current_tcp->s_syscall, &(res = s_ellipsis_new())->arg,
		-1);

	return res;
}

extern struct s_changeable *s_changeable_new_and_insert(const char *name,
	struct s_arg *entering, struct s_arg *exiting)
{
	struct s_changeable *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_changeable_new(name, entering, exiting))->arg,
		entering ? entering->arg_num : -1);

	return res;
}

struct s_xlat *
s_xlat_append(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags)
{
	struct s_arg *last_arg = STAILQ_LAST(&current_tcp->s_syscall->args.args,
		s_arg, entry);
	struct s_xlat *last_xlat;
	struct s_xlat *res;

	if (!last_arg && (last_arg->type == S_TYPE_addr))
		last_arg = S_ARG_TO_TYPE(last_arg, addr)->val;

	if (!last_arg || (S_TYPE_KIND(last_arg->type) != S_TYPE_KIND_xlat))
		return s_xlat_new_and_insert(type, last_arg->name, x, val, dflt,
			flags);

	res = s_xlat_new(type, name, x, val, dflt, flags);

	last_xlat = S_ARG_TO_TYPE(last_arg, xlat);

	if (last_xlat->last)
		last_xlat->last->next = res;
	else
		last_xlat->next = res;

	last_xlat->last = res;

	return res;
}

struct args_queue *
s_struct_enter(struct s_struct *s)
{
	SLIST_INSERT_HEAD(&s->arg.syscall->insertion_stack, &s->args, entry);

	return &s->args.args;
}

struct args_queue *
s_syscall_insertion_point(struct s_syscall *s)
{
	if (SLIST_EMPTY(&s->insertion_stack))
		return &s->args.args;

	return &(SLIST_FIRST(&s->insertion_stack)->args);
}

struct args_queue *
s_syscall_pop(struct s_syscall *s)
{
	SLIST_REMOVE_HEAD(&s->insertion_stack, entry);

	return s_syscall_insertion_point(s);
}

struct args_queue *
s_syscall_pop_all(struct s_syscall *s)
{
	SLIST_INIT(&s->insertion_stack);

	return &s->args.args;
}


struct s_syscall *
s_syscall_new(struct tcb *tcp, enum s_syscall_type sc_type)
{
	struct s_syscall *syscall = xmalloc(sizeof(*syscall));

	tcp->s_syscall = syscall;

	syscall->tcp = tcp;
	syscall->type = sc_type;
	syscall->last_arg = syscall->cur_arg = 0;

	STAILQ_INIT(&syscall->args.args);
	STAILQ_INIT(&syscall->changeable_args);
	SLIST_INIT(&syscall->insertion_stack);

	return syscall;
}

void
s_last_is_changeable(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *last_arg = STAILQ_LAST(&syscall->args.args, s_arg, entry);
	STAILQ_REMOVE(&syscall->args.args, last_arg, s_arg, entry);

	s_changeable_new_and_insert(last_arg->name, last_arg, NULL);
}

void
s_syscall_free(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	STAILQ_FOREACH_SAFE(arg, &syscall->args.args, entry, tmp) {
		s_arg_free(arg);
	}

	free(syscall);
}

int
s_syscall_cur_arg_advance(struct s_syscall *syscall, enum s_type type,
	unsigned long long *val)
{
	static unsigned long long dummy_val;
	unsigned long long *local_val = val ? val : &dummy_val;

	if (exiting(syscall->tcp) && syscall->last_changeable) {
		int chg_arg = syscall->last_changeable->arg_num;

		assert(chg_arg >= 0);

		syscall->cur_arg = chg_arg;
	}

	syscall->last_arg = syscall->cur_arg;

	if (S_TYPE_SIZE(type) == S_TYPE_SIZE_ll) {
		syscall->cur_arg = getllval(current_tcp, local_val,
			syscall->cur_arg);
	} else {
		unsigned long long mask = ~0LLU;

		switch (S_TYPE_SIZE(type)) {
		case S_TYPE_SIZE_c:
			mask = 0xFF;
			break;
		case S_TYPE_SIZE_h:
			mask = 0xFFFF;
			break;
		case S_TYPE_SIZE_i:
			mask = 0xFFFFFFFF;
			break;
		case S_TYPE_SIZE_l:
			if (current_wordsize == sizeof(int))
				mask = 0xFFFFFFFF;
			break;
		case S_TYPE_SIZE_ll:
		default:
			break;
		}

		*local_val = current_tcp->u_arg[syscall->cur_arg] & mask;
		syscall->cur_arg++;
	}

	return syscall->cur_arg;
}

/* Similar to printxvals() */
static int
s_process_xlat_val(struct s_xlat *arg, s_print_xlat_fn cb, void *cb_data,
	bool first)
{
	const char *str = arg->x ? xlookup(arg->x, arg->val) : NULL;

	cb(arg->arg.type, arg->val, ~0, str ? str : arg->dflt,
		(first ? SPXF_FIRST : 0) | ((str || !arg->x) ? 0 : SPXF_DEFAULT),
		cb_data);

	return 1;
}

/* Similar to printflags64() */
static int
s_process_xlat_flags(struct s_xlat *arg, s_print_xlat_fn cb, void *cb_data,
	bool first)
{
	int n;
	const struct xlat *xlat;
	uint64_t flags;

	if (arg->val == 0 && arg->x && arg->x->val == 0 && arg->x->str) {
		cb(arg->arg.type, arg->val, 0, arg->x->str,
			first ? SPXF_FIRST : 0, cb_data);
		return 1;
	}

	if (!arg->x) {
		cb(arg->arg.type, arg->val, ~0, arg->dflt,
			(first ? SPXF_FIRST : 0) | SPXF_DEFAULT, cb_data);
		return 1;
	}

	xlat = arg->x;
	flags = arg->val;

	for (n = 0; xlat->str; xlat++) {
		if (xlat->val && (flags & xlat->val) == xlat->val) {
			cb(arg->arg.type, xlat->val, xlat->val, xlat->str,
				first ? SPXF_FIRST : 0, cb_data);
			first = false;
			flags &= ~xlat->val;
			n++;
		}
	}

	if (n) {
		if (flags) {
			cb(arg->arg.type, flags, ~0, NULL, 0, cb_data);
			n++;
		}
	} else {
		cb(arg->arg.type, flags, ~0, arg->dflt,
			(first ? SPXF_FIRST : 0) | SPXF_DEFAULT, cb_data);
	}

	return n;
}

void
s_process_xlat(struct s_xlat *arg, s_print_xlat_fn cb, void *cb_data)
{
	bool first = true;

	while (arg) {
		first = !(arg->flags ? s_process_xlat_flags :
			s_process_xlat_val)(arg, cb, cb_data, first) && first;
		arg = arg->next;
	}
}

void
s_syscall_print_before(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_before(tcp);
	s_syscall_new(tcp, S_SCT_SYSCALL);
}

void
s_syscall_print_entering(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	tcp->s_syscall->last_changeable =
		STAILQ_FIRST(&tcp->s_syscall->changeable_args);

	//for (; *cur; cur++)
		(*cur)->print_entering(tcp);
}

void
s_syscall_init_exiting(struct tcb *tcp)
{
	tcp->s_syscall->last_changeable =
		STAILQ_FIRST(&tcp->s_syscall->changeable_args);
}

void
s_syscall_print_exiting(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	for (; *cur; cur++)
		(*cur)->print_exiting(tcp);
}

void
s_syscall_print_after(struct tcb *tcp)
{
	s_syscall_free(tcp);
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_after(tcp);
}

void
s_syscall_print_resumed(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_resumed(tcp);
}

void
s_syscall_print_tv(struct tcb *tcp, struct timeval *tv)
{
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_tv(tcp, tv);
}

void
s_syscall_print_unavailable_entering(struct tcb *tcp, int scno_good)
{
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_unavailable_entering(tcp, scno_good);
}

void
s_syscall_print_unavailable_exiting(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_unavailable_exiting(tcp);
	s_syscall_free(tcp);
}

void
s_syscall_print_signal(struct tcb *tcp, const void *si_void, unsigned sig)
{
	const siginfo_t *si = si_void;
	struct s_printer **cur = s_printers;
	struct s_syscall *saved_syscall = tcp->s_syscall;

	s_syscall_new(tcp, S_SCT_SIGNAL);

	s_insert_signo("signal", sig);
	if (si) {
		s_insert_struct("siginfo");
		printsiginfo(si);
		s_struct_finish();
	}

	//for (; *cur; cur++)
		(*cur)->print_signal(tcp);

	s_syscall_free(tcp);

	tcp->s_syscall = saved_syscall;
}
